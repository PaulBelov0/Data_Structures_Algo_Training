#include "lsp_client.h"

LSPClient::LSPClient(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_state(Disconnected)
    , m_language(LanguageUnknown)
    , m_requestIdCounter(0)
    , m_initializeTimer(new QTimer(this))
    , m_keepAliveTimer(new QTimer(this))
{
    // Настройка таймеров
    m_initializeTimer->setSingleShot(true);
    m_initializeTimer->setInterval(10000); // 10 секунд на инициализацию

    m_keepAliveTimer->setInterval(30000); // Отправлять keep-alive каждые 30 секунд

    // Подключение сигналов процесса
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &LSPClient::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError,
            this, [this]() {
                QString error = m_process->readAllStandardError();
                if (!error.trimmed().isEmpty()) {
                    emit logMessage(error, "stderr");
                }
            });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LSPClient::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &LSPClient::onProcessErrorOccurred);

    // Подключение таймеров
    connect(m_initializeTimer, &QTimer::timeout,
            this, &LSPClient::onInitializeTimeout);
    connect(m_keepAliveTimer, &QTimer::timeout,
            this, &LSPClient::onKeepAliveTimeout);
}

LSPClient::~LSPClient()
{
    stop();
}

bool LSPClient::start(const QString& projectPath)
{
    if (m_state != Disconnected) {
        qWarning() << "LSPClient already started";
        return false;
    }

    if (m_serverPath.isEmpty()) {
        m_errorString = "Server path not set";
        emit errorOccurred(m_errorString);
        return false;
    }

    if (!projectPath.isEmpty()) {
        m_rootPath = projectPath;
    }

    if (m_rootPath.isEmpty()) {
        m_rootPath = QDir::currentPath();
    }

    qDebug() << "Starting LSP server:" << m_serverPath;
    qDebug() << "Root path:" << m_rootPath;
    qDebug() << "Arguments:" << m_serverArgs;

    setState(Starting);

    // Собираем аргументы
    QStringList args = m_serverArgs;
    if (args.isEmpty()) {
        args = getDefaultServerArgs();
    }

    // Определяем команду для запуска
    QString program;
    QStringList programArgs;

    // Проверяем, нужно ли запускать через Java (для JDT LS)
    if (m_language == LanguageJava && QFileInfo(m_serverPath).isDir()) {
        // Запускаем через java
        program = "java";

        // Ищем java в JAVA_HOME или PATH
        QString javaHome = qgetenv("JAVA_HOME");
        if (!javaHome.isEmpty()) {
#ifdef Q_OS_WIN
            program = javaHome + "/bin/java.exe";
#else
            program = javaHome + "/bin/java";
#endif
        } else {
            // Ищем java в PATH
            QString javaPath = QStandardPaths::findExecutable("java");
            if (!javaPath.isEmpty()) {
                program = javaPath;
            }
        }

        // Проверяем, что java существует
        if (!QFileInfo::exists(program)) {
            m_errorString = "Java not found. Please set JAVA_HOME or add java to PATH";
            setState(Error);
            emit errorOccurred(m_errorString);
            return false;
        }

        // Добавляем аргументы JVM
        programArgs << "-Declipse.application=org.eclipse.jdt.ls.core.id1";
        programArgs << "-Dosgi.bundles.defaultStartLevel=4";
        programArgs << "-Declipse.product=org.eclipse.jdt.ls.core.product";
        programArgs << "-Dlog.protocol=true";
        programArgs << "-Dlog.level=ALL";
        programArgs << "-noverify";
        programArgs << "-Xmx1G";  // Память для JVM

        // Добавляем -jar если первый аргумент -jar
        for (int i = 0; i < args.size(); i++) {
            if (args[i] == "-jar" && i + 1 < args.size()) {
                programArgs << "-jar" << args[i + 1];
                i++; // Пропускаем путь к jar
            } else {
                programArgs << args[i];
            }
        }
    } else {
        // Обычный запуск исполняемого файла
        program = m_serverPath;
        programArgs = args;
    }

    qDebug() << "Starting program:" << program;
    qDebug() << "Program args:" << programArgs;

    // Запускаем процесс
    m_process->setProgram(program);
    m_process->setArguments(programArgs);
    m_process->setWorkingDirectory(m_rootPath);

    // Настраиваем переменные окружения
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C.UTF-8");
    env.insert("LANG", "C.UTF-8");

    // Для Java добавляем специфичные переменные
    if (m_language == LanguageJava) {
        QString javaHome = qgetenv("JAVA_HOME");
        if (!javaHome.isEmpty()) {
            env.insert("JAVA_HOME", javaHome);
        }
    }

    m_process->setProcessEnvironment(env);

    m_process->start();

    if (!m_process->waitForStarted(10000)) { // 10 секунд для Java
        m_errorString = QString("Failed to start server: %1").arg(m_process->errorString());
        setState(Error);
        emit errorOccurred(m_errorString);
        return false;
    }

    qDebug() << "LSP server process started, PID:" << m_process->processId();

    // Запускаем таймер инициализации
    m_initializeTimer->start();

    // Инициализируем сервер
    initializeServer();

    return true;
}

void LSPClient::stop()
{
    if (m_state == Disconnected) {
        return;
    }

    qDebug() << "Stopping LSP client";

    m_initializeTimer->stop();
    m_keepAliveTimer->stop();

    if (m_process->state() == QProcess::Running) {
        // Отправляем shutdown запрос если сервер поддерживает
        if (m_serverCapabilities.contains("shutdown")) {
            QJsonObject shutdownRequest = createRequest("shutdown", QJsonObject());
            sendMessage(shutdownRequest);

            // Даем время на корректное завершение
            if (m_process->waitForFinished(2000)) {
                m_process->terminate();
                if (!m_process->waitForFinished(1000)) {
                    m_process->kill();
                }
            }
        } else {
            m_process->terminate();
            if (!m_process->waitForFinished(1000)) {
                m_process->kill();
            }
        }
    }

    // Удаляем временные файлы
    for (const QString& tempFile : m_tempFiles.values()) {
        QFile::remove(tempFile);
    }
    m_tempFiles.clear();

    setState(Disconnected);
    m_openDocuments.clear();
    m_pendingRequests.clear();
    m_buffer.clear();
}

void LSPClient::restart()
{
    qDebug() << "Restarting LSP client";
    stop();
    QThread::msleep(500); // Небольшая задержка
    start(m_rootPath);
}

bool LSPClient::openDocument(const QString& filePath, const QString& text)
{
    if (m_state != Connected) {
        qWarning() << "Cannot open document: LSP client not connected";
        return false;
    }

    QString uri = pathToUri(filePath);

    // Создаем временный файл если путь не существует
    QString actualPath = filePath;
    if (!QFile::exists(filePath)) {
        QTemporaryFile* tempFile = new QTemporaryFile(QDir::tempPath() + "/lsp_XXXXXX." + getLanguageId());
        if (tempFile->open()) {
            tempFile->write(text.toUtf8());
            tempFile->close();
            actualPath = tempFile->fileName();
            m_tempFiles[uri] = actualPath;
            delete tempFile;
        } else {
            qWarning() << "Failed to create temporary file for document";
            return false;
        }
    }

    uri = pathToUri(actualPath);

    // Отправляем didOpen уведомление
    LSPTextDocumentItem docItem;
    docItem.uri = uri;
    docItem.languageId = getLanguageId();
    docItem.version = 1;
    docItem.text = text;

    m_openDocuments[uri] = 1;

    QJsonObject params = QJsonObject{
        {"textDocument", docItem.toJson()}
    };

    QJsonObject notification = createNotification("textDocument/didOpen", params);
    sendMessage(notification);

    qDebug() << "Document opened:" << uri;
    return true;
}

bool LSPClient::updateDocument(const QString& filePath, const QString& text, int version)
{
    if (m_state != Connected) {
        return false;
    }

    QString uri = pathToUri(filePath);
    if (!m_openDocuments.contains(uri)) {
        return openDocument(filePath, text);
    }

    if (version == -1) {
        version = m_openDocuments[uri] + 1;
    }

    m_openDocuments[uri] = version;

    QJsonObject params = QJsonObject{
        {"textDocument", QJsonObject{
                             {"uri", uri},
                             {"version", version}
                         }},
        {"contentChanges", QJsonArray{
                               QJsonObject{
                                   {"text", text}
                               }
                           }}
    };

    QJsonObject notification = createNotification("textDocument/didChange", params);
    sendMessage(notification);

    return true;
}

bool LSPClient::closeDocument(const QString& filePath)
{
    if (m_state != Connected) {
        return false;
    }

    QString uri = pathToUri(filePath);

    if (!m_openDocuments.contains(uri)) {
        return false;
    }

    QJsonObject params = QJsonObject{
        {"textDocument", QJsonObject{
                             {"uri", uri}
                         }}
    };

    QJsonObject notification = createNotification("textDocument/didClose", params);
    sendMessage(notification);

    m_openDocuments.remove(uri);

    // Удаляем временный файл если он был создан
    if (m_tempFiles.contains(uri)) {
        QFile::remove(m_tempFiles[uri]);
        m_tempFiles.remove(uri);
    }

    return true;
}

void LSPClient::requestCompletion(const QString& filePath, int line, int character)
{
    if (m_state != Connected) {
        emit completionReady("", line, character, {});
        return;
    }

    QString uri = pathToUri(filePath);

    QJsonObject params = QJsonObject{
        {"textDocument", QJsonObject{
                             {"uri", uri}
                         }},
        {"position", LSPPosition{line, character}.toJson()},
        {"context", QJsonObject{
                        {"triggerKind", 1} // Invoked
                    }}
    };

    int requestId = getNextRequestId();
    m_pendingRequests[requestId] = "textDocument/completion";

    QJsonObject request = createRequest("textDocument/completion", params);
    request["id"] = requestId;

    sendMessage(request);
}

void LSPClient::requestHover(const QString& filePath, int line, int character)
{
    if (m_state != Connected) {
        emit hoverReady("", line, character, "");
        return;
    }

    QString uri = pathToUri(filePath);

    QJsonObject params = QJsonObject{
        {"textDocument", QJsonObject{
                             {"uri", uri}
                         }},
        {"position", LSPPosition{line, character}.toJson()}
    };

    int requestId = getNextRequestId();
    m_pendingRequests[requestId] = "textDocument/hover";

    QJsonObject request = createRequest("textDocument/hover", params);
    request["id"] = requestId;

    sendMessage(request);
}

void LSPClient::onProcessReadyRead()
{
    m_buffer.append(m_process->readAllStandardOutput());

    // Парсим сообщения (LSP использует заголовки Content-Length)
    while (!m_buffer.isEmpty()) {
        // Ищем конец заголовка
        int headerEnd = m_buffer.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            break;
        }

        // Парсим заголовок
        QString header = m_buffer.left(headerEnd);
        QStringList headerLines = header.split("\r\n");

        int contentLength = -1;
        for (const QString& line : headerLines) {
            if (line.startsWith("Content-Length:", Qt::CaseInsensitive)) {
                contentLength = line.mid(15).trimmed().toInt();
                break;
            }
        }

        if (contentLength == -1) {
            qWarning() << "No Content-Length header found";
            m_buffer.clear();
            break;
        }

        // Проверяем, есть ли все данные
        int totalLength = headerEnd + 4 + contentLength; // +4 для "\r\n\r\n"
        if (m_buffer.length() < totalLength) {
            break;
        }

        // Извлекаем тело сообщения
        QByteArray jsonData = m_buffer.mid(headerEnd + 4, contentLength);
        m_buffer = m_buffer.mid(totalLength);

        // Парсим JSON
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);

        if (error.error != QJsonParseError::NoError) {
            qWarning() << "Failed to parse LSP message:" << error.errorString();
            continue;
        }

        QJsonObject message = doc.object();
        parseMessage(message);
    }
}

void LSPClient::parseMessage(const QJsonObject& message)
{
    if (message.contains("id")) {
        if (message.contains("result") || message.contains("error")) {
            handleResponse(message);
        } else {
            qWarning() << "Invalid LSP message: has id but no result/error";
        }
    } else {
        handleNotification(message);
    }
}

void LSPClient::handleNotification(const QJsonObject& message)
{
    QString method = message["method"].toString();
    QJsonValue params = message["params"];

    qDebug() << "LSP notification:" << method;

    if (method == "textDocument/publishDiagnostics") {
        processDiagnostics(params.toObject());
    }
    else if (method == "window/logMessage") {
        QJsonObject logParams = params.toObject();
        QString msg = logParams["message"].toString();
        QString type = logParams["type"].toString("log");
        emit logMessage(msg, type);
    }
    else if (method == "window/showMessage") {
        QJsonObject msgParams = params.toObject();
        QString msg = msgParams["message"].toString();
        QString type = msgParams["type"].toString("info");
        emit logMessage(msg, type);
    }
    else if (method == "telemetry/event") {
        // Игнорируем телеметрию
    }
}

void LSPClient::handleResponse(const QJsonObject& message)
{
    int id = message["id"].toInt();
    QString method = m_pendingRequests.take(id);

    if (method.isEmpty()) {
        qWarning() << "Received response for unknown request id:" << id;
        return;
    }

    if (message.contains("error")) {
        QJsonObject error = message["error"].toObject();
        QString errorMsg = QString("LSP error [%1]: %2")
                               .arg(error["code"].toInt())
                               .arg(error["message"].toString());
        emit errorOccurred(errorMsg);
        return;
    }

    QJsonValue result = message["result"];

    if (method == "initialize") {
        m_serverCapabilities = result.toObject();
        qDebug() << "Server initialized, capabilities received";
        sendInitialized();
    }
    else if (method == "textDocument/completion") {
        processCompletionResponse(id, result.toObject());
    }
    else if (method == "textDocument/hover") {
        QJsonObject hoverResult = result.toObject();
        QString content = "";
        if (hoverResult.contains("contents")) {
            QJsonValue contents = hoverResult["contents"];
            if (contents.isString()) {
                content = contents.toString();
            } else if (contents.isObject()) {
                content = contents.toObject()["value"].toString();
            } else if (contents.isArray()) {
                QJsonArray arr = contents.toArray();
                QStringList parts;
                for (const QJsonValue& val : arr) {
                    if (val.isString()) parts << val.toString();
                    else if (val.isObject()) parts << val.toObject()["value"].toString();
                }
                content = parts.join("\n");
            }
        }

        // Находим URI для этого запроса (упрощенно)
        QString uri;
        for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
            if (it.key() == id) {
                // Нужно хранить больше информации о запросах
                break;
            }
        }

        emit hoverReady(uri, 0, 0, content);
    }
    // ... обработка других методов
}

void LSPClient::processCompletionResponse(int requestId, const QJsonObject& result)
{
    QList<LSPCompletionItem> items;

    if (result["isIncomplete"].toBool()) {
        qDebug() << "Completion list is incomplete";
    }

    QJsonValue itemsValue = result["items"];
    if (itemsValue.isArray()) {
        QJsonArray itemsArray = itemsValue.toArray();
        for (const QJsonValue& itemValue : itemsArray) {
            if (itemValue.isObject()) {
                LSPCompletionItem item = LSPCompletionItem::fromJson(itemValue.toObject());
                items.append(item);
            }
        }
    } else if (QJsonValue(result).isArray()) {
        QJsonArray itemsArray = QJsonValue(result).toArray();
        for (const QJsonValue& itemValue : itemsArray) {
            if (itemValue.isObject()) {
                LSPCompletionItem item = LSPCompletionItem::fromJson(itemValue.toObject());
                items.append(item);
            }
        }
    }

    // Находим URI для этого запроса (упрощенно)
    QString uri;
    int line = 0, character = 0;

    emit completionReady(uri, line, character, items);
}

void LSPClient::processDiagnostics(const QJsonObject& params)
{
    QString uri = params["uri"].toString();
    QJsonArray diagnostics = params["diagnostics"].toArray();

    emit diagnosticsUpdated(uri, diagnostics);
}

QString LSPClient::pathToUri(const QString& path) const
{
    QFileInfo info(path);
    QString absolutePath = info.absoluteFilePath();

#ifdef Q_OS_WIN
    // На Windows file:///C:/path/to/file
    return "file:///" + absolutePath.replace("\\", "/");
#else
    // На Unix file:///path/to/file
    return "file://" + absolutePath;
#endif
}

QString LSPClient::uriToPath(const QString& uri) const
{
    if (uri.startsWith("file://")) {
        QString path = uri.mid(7);
#ifdef Q_OS_WIN
        if (path.startsWith("/")) {
            path = path.mid(1);
        }
#endif
        return QUrl::fromPercentEncoding(path.toUtf8());
    }
    return uri;
}

void LSPClient::initializeServer()
{
    setState(Initializing);
    sendInitialize();
}

void LSPClient::sendInitialize()
{
    QJsonObject params = QJsonObject{
        {"processId", QCoreApplication::applicationPid()},
        {"rootUri", pathToUri(m_rootPath)},
        {"capabilities", QJsonObject{
                             {"workspace", QJsonObject{
                                               {"applyEdit", true},
                                               {"workspaceEdit", QJsonObject{
                                                                     {"documentChanges", true}
                                                                 }},
                                               {"didChangeConfiguration", QJsonObject{
                                                                              {"dynamicRegistration", true}
                                                                          }},
                                               {"symbol", QJsonObject{
                                                              {"dynamicRegistration", true}
                                                          }}
                                           }},
                             {"textDocument", QJsonObject{
                                                  {"synchronization", QJsonObject{
                                                                          {"dynamicRegistration", true},
                                                                          {"willSave", true},
                                                                          {"willSaveWaitUntil", true},
                                                                          {"didSave", true}
                                                                      }},
                                                  {"completion", QJsonObject{
                                                                     {"dynamicRegistration", true},
                                                                     {"completionItem", QJsonObject{
                                                                                            {"snippetSupport", true},
                                                                                            {"commitCharactersSupport", true},
                                                                                            {"documentationFormat", QJsonArray{"markdown", "plaintext"}}
                                                                                        }}
                                                                 }},
                                                  {"hover", QJsonObject{
                                                                {"dynamicRegistration", true},
                                                                {"contentFormat", QJsonArray{"markdown", "plaintext"}}
                                                            }},
                                                  {"signatureHelp", QJsonObject{
                                                                        {"dynamicRegistration", true}
                                                                    }},
                                                  {"definition", QJsonObject{
                                                                     {"dynamicRegistration", true}
                                                                 }},
                                                  {"references", QJsonObject{
                                                                     {"dynamicRegistration", true}
                                                                 }},
                                                  {"documentHighlight", QJsonObject{
                                                                            {"dynamicRegistration", true}
                                                                        }},
                                                  {"documentSymbol", QJsonObject{
                                                                         {"dynamicRegistration", true}
                                                                     }},
                                                  {"codeAction", QJsonObject{
                                                                     {"dynamicRegistration", true}
                                                                 }},
                                                  {"formatting", QJsonObject{
                                                                     {"dynamicRegistration", true}
                                                                 }},
                                                  {"rename", QJsonObject{
                                                                 {"dynamicRegistration", true}
                                                             }}
                                              }}
                         }},
        {"initializationOptions", m_initOptions}
    };

    // Применяем языко-специфичные настройки
    applyLanguageSpecificSettings(params);

    int requestId = getNextRequestId();
    m_pendingRequests[requestId] = "initialize";

    QJsonObject request = createRequest("initialize", params);
    request["id"] = requestId;

    sendMessage(request);
}

void LSPClient::sendInitialized()
{
    QJsonObject params; // Пустые параметры
    QJsonObject notification = createNotification("initialized", params);
    sendMessage(notification);

    setState(Connected);
    m_initializeTimer->stop();
    m_keepAliveTimer->start();

    qDebug() << "LSP client connected successfully";
}

QJsonObject LSPClient::createRequest(const QString& method, const QJsonObject& params)
{
    return QJsonObject{
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
}

QJsonObject LSPClient::createNotification(const QString& method, const QJsonObject& params)
{
    return QJsonObject{
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
}

void LSPClient::sendMessage(const QJsonObject& message)
{
    QJsonDocument doc(message);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    QByteArray data = QString("Content-Length: %1\r\n\r\n").arg(json.length()).toUtf8() + json;

    if (m_process->state() == QProcess::Running) {
        m_process->write(data);
        m_process->waitForBytesWritten(1000);
    } else {
        qWarning() << "Cannot send message: process not running";
    }
}

void LSPClient::setState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

void LSPClient::onInitializeTimeout()
{
    if (m_state == Initializing) {
        m_errorString = "Server initialization timeout";
        setState(Error);
        emit errorOccurred(m_errorString);
        stop();
    }
}

void LSPClient::onKeepAliveTimeout()
{
    if (m_state == Connected) {
        // Можно отправлять ping или просто проверять соединение
        if (m_process->state() != QProcess::Running) {
            m_errorString = "Server process died";
            setState(Error);
            emit errorOccurred(m_errorString);
        }
    }
}

int LSPClient::getNextRequestId()
{
    return ++m_requestIdCounter;
}
