#include "lsp_manager.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSettings>

LSPManager::LSPManager(QObject* parent)
    : QObject(parent)
{
    // Загружаем сохраненные пути серверов
    QSettings settings;
    m_workspacePath = settings.value("LSP/WorkspacePath", QDir::currentPath()).toString();

    QString cppPath = settings.value("LSP/Cpp/Path").toString();
    if (!cppPath.isEmpty()) {
        m_serverPaths[LSPClient::LanguageCpp] = cppPath;
    }

    QString javaPath = settings.value("LSP/Java/Path").toString();
    if (!javaPath.isEmpty()) {
        m_serverPaths[LSPClient::LanguageJava] = javaPath;
    }
}

LSPManager::~LSPManager()
{
    // Останавливаем и удаляем всех клиентов
    for (LSPClient* client : m_clients) {
        if (client) {
            client->stop();
            disconnect(client, nullptr, this, nullptr);
            delete client;
        }
    }
    m_clients.clear();
}

bool LSPManager::createClient(LSPClient::Language language)
{
    // Проверяем, есть ли уже клиент для этого языка
    if (m_clients.contains(language)) {
        qDebug() << "Client for language" << language << "already exists";
        return true;
    }

    LSPClient* client = nullptr;

    switch (language) {
    case LSPClient::LanguageCpp:
        client = createCppClient();
        break;
    case LSPClient::LanguageJava:
        client = createJavaClient();
        break;
    default:
        qWarning() << "Unsupported language:" << language;
        emit clientCreated(language, false);
        return false;
    }

    if (!client) {
        emit clientCreated(language, false);
        return false;
    }

    // Подключаем сигналы клиента к менеджеру
    connect(client, &LSPClient::stateChanged,
            this, &LSPManager::onClientStateChanged);
    connect(client, &LSPClient::completionReady,
            this, &LSPManager::onCompletionReady);
    connect(client, &LSPClient::hoverReady,
            this, &LSPManager::onHoverReady);
    connect(client, &LSPClient::diagnosticsUpdated,
            this, &LSPManager::onDiagnosticsUpdated);
    connect(client, &LSPClient::logMessage,
            this, &LSPManager::onLogMessage);
    connect(client, &LSPClient::errorOccurred,
            this, &LSPManager::onErrorOccurred);

    m_clients[language] = client;

    // Сохраняем путь сервера если он был установлен
    if (!client->serverPath().isEmpty()) {
        m_serverPaths[language] = client->serverPath();

        QSettings settings;
        if (language == LSPClient::LanguageCpp) {
            settings.setValue("LSP/Cpp/Path", client->serverPath());
        } else if (language == LSPClient::LanguageJava) {
            settings.setValue("LSP/Java/Path", client->serverPath());
        }
    }

    qDebug() << "Created LSP client for language:" << language;
    emit clientCreated(language, true);

    return true;
}

bool LSPManager::removeClient(LSPClient::Language language)
{
    if (!m_clients.contains(language)) {
        return false;
    }

    LSPClient* client = m_clients.take(language);
    if (client) {
        client->stop();
        disconnect(client, nullptr, this, nullptr);
        delete client;
    }

    qDebug() << "Removed LSP client for language:" << language;
    return true;
}

LSPClient* LSPManager::getClient(LSPClient::Language language) const
{
    return m_clients.value(language, nullptr);
}

bool LSPManager::autoSetup(LSPClient::Language language)
{
    // Создаем клиент если его нет
    if (!m_clients.contains(language)) {
        if (!createClient(language)) {
            return false;
        }
    }

    LSPClient* client = m_clients[language];
    if (!client) {
        return false;
    }

    // Если путь сервера не установлен, пытаемся найти
    if (client->serverPath().isEmpty()) {
        // Здесь можно добавить логику автоматического поиска
        // Например, через IDEIntegrator
        qDebug() << "Server path not set for language:" << language;
        return false;
    }

    // Запускаем клиент
    bool success = client->start(m_workspacePath);

    if (success) {
        qDebug() << "Auto-setup successful for language:" << language;
    } else {
        qDebug() << "Auto-setup failed for language:" << language
                 << "Error:" << client->errorString();
    }

    return success;
}

bool LSPManager::openDocument(LSPClient::Language language, const QString& filePath, const QString& text)
{
    LSPClient* client = getClient(language);
    if (!client) {
        qWarning() << "No client for language:" << language;
        return false;
    }

    if (!client->isConnected()) {
        qWarning() << "Client not connected for language:" << language;
        return false;
    }

    return client->openDocument(filePath, text);
}

bool LSPManager::updateDocument(LSPClient::Language language, const QString& filePath, const QString& text)
{
    LSPClient* client = getClient(language);
    if (!client || !client->isConnected()) {
        return false;
    }

    return client->updateDocument(filePath, text);
}

bool LSPManager::closeDocument(LSPClient::Language language, const QString& filePath)
{
    LSPClient* client = getClient(language);
    if (!client || !client->isConnected()) {
        return false;
    }

    return client->closeDocument(filePath);
}

void LSPManager::requestCompletion(LSPClient::Language language, const QString& filePath, int line, int character)
{
    LSPClient* client = getClient(language);
    if (!client || !client->isConnected()) {
        emit completionReady(language, "", line, character, {});
        return;
    }

    client->requestCompletion(filePath, line, character);
}

void LSPManager::requestHover(LSPClient::Language language, const QString& filePath, int line, int character)
{
    LSPClient* client = getClient(language);
    if (!client || !client->isConnected()) {
        emit hoverReady(language, "", line, character, "");
        return;
    }

    client->requestHover(filePath, line, character);
}

bool LSPManager::isLanguageSupported(LSPClient::Language language) const
{
    return (language == LSPClient::LanguageCpp || language == LSPClient::LanguageJava);
}

QStringList LSPManager::getSupportedLanguages() const
{
    return {"C++", "Java"};
}

void LSPManager::setWorkspacePath(const QString& path)
{
    if (m_workspacePath == path) {
        return;
    }

    m_workspacePath = path;

    // Сохраняем в настройках
    QSettings settings;
    settings.setValue("LSP/WorkspacePath", path);

    // Перезапускаем всех клиентов с новым путем
    for (LSPClient* client : m_clients) {
        if (client && client->isConnected()) {
            client->stop();
            client->start(path);
        }
    }
}

void LSPManager::setServerPath(LSPClient::Language language, const QString& path)
{
    m_serverPaths[language] = path;

    // Сохраняем в настройках
    QSettings settings;
    if (language == LSPClient::LanguageCpp) {
        settings.setValue("LSP/Cpp/Path", path);
    } else if (language == LSPClient::LanguageJava) {
        settings.setValue("LSP/Java/Path", path);
    }

    // Обновляем путь в существующем клиенте если есть
    if (m_clients.contains(language)) {
        LSPClient* client = m_clients[language];
        client->setServerPath(path);

        // Перезапускаем если был запущен
        if (client->isConnected()) {
            client->stop();
            client->start(m_workspacePath);
        }
    }
}

void LSPManager::onClientStateChanged(LSPClient::State state)
{
    // Находим какой клиент отправил сигнал
    LSPClient* senderClient = qobject_cast<LSPClient*>(sender());
    if (!senderClient) {
        return;
    }

    // Находим язык клиента
    LSPClient::Language language = LSPClient::LanguageUnknown;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value() == senderClient) {
            language = it.key();
            break;
        }
    }

    if (language != LSPClient::LanguageUnknown) {
        emit clientStateChanged(language, state);

        if (state == LSPClient::Connected) {
            qDebug() << "LSP client connected for language:" << language;
        } else if (state == LSPClient::Error) {
            qWarning() << "LSP client error for language:" << language
                       << "Error:" << senderClient->errorString();
        }
    }
}

void LSPManager::onCompletionReady(const QString& fileUri, int line, int character, const QList<LSPCompletionItem>& items)
{
    // Находим какой клиент отправил сигнал
    LSPClient* senderClient = qobject_cast<LSPClient*>(sender());
    if (!senderClient) {
        return;
    }

    // Находим язык клиента
    LSPClient::Language language = LSPClient::LanguageUnknown;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value() == senderClient) {
            language = it.key();
            break;
        }
    }

    if (language != LSPClient::LanguageUnknown) {
        emit completionReady(language, fileUri, line, character, items);
    }
}

void LSPManager::onHoverReady(const QString& fileUri, int line, int character, const QString& content)
{
    LSPClient* senderClient = qobject_cast<LSPClient*>(sender());
    if (!senderClient) {
        return;
    }

    LSPClient::Language language = LSPClient::LanguageUnknown;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value() == senderClient) {
            language = it.key();
            break;
        }
    }

    if (language != LSPClient::LanguageUnknown) {
        emit hoverReady(language, fileUri, line, character, content);
    }
}

void LSPManager::onDiagnosticsUpdated(const QString& fileUri, const QJsonArray& diagnostics)
{
    LSPClient* senderClient = qobject_cast<LSPClient*>(sender());
    if (!senderClient) {
        return;
    }

    LSPClient::Language language = LSPClient::LanguageUnknown;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value() == senderClient) {
            language = it.key();
            break;
        }
    }

    if (language != LSPClient::LanguageUnknown) {
        emit diagnosticsUpdated(language, fileUri, diagnostics);
    }
}

void LSPManager::onLogMessage(const QString& message, const QString& type)
{
    emit logMessage(message, type);
}

void LSPManager::onErrorOccurred(const QString& error)
{
    // Находим клиента по отправителю
    LSPClient* senderClient = qobject_cast<LSPClient*>(sender());
    if (senderClient) {
        qWarning() << "LSP client error:" << error;
    }

    emit logMessage(error, "error");
}

LSPClient* LSPManager::createCppClient()
{
    CppLSPClient* client = new CppLSPClient(this);

    // Устанавливаем сохраненный путь если есть
    if (m_serverPaths.contains(LSPClient::LanguageCpp)) {
        client->setServerPath(m_serverPaths[LSPClient::LanguageCpp]);
    }

    // Устанавливаем рабочую директорию
    client->setRootPath(m_workspacePath);

    // Настраиваем дополнительные параметры для C++
    client->setCppStandard("c++17");

    // Добавляем стандартные include пути
    client->addIncludePath(m_workspacePath);
    client->addIncludePath(m_workspacePath + "/include");

    return client;
}

LSPClient* LSPManager::createJavaClient()
{
    JavaLSPClient* client = new JavaLSPClient(this);

    // Устанавливаем сохраненный путь если есть
    if (m_serverPaths.contains(LSPClient::LanguageJava)) {
        client->setServerPath(m_serverPaths[LSPClient::LanguageJava]);
    }

    // Устанавливаем рабочую директорию
    client->setRootPath(m_workspacePath);

    // Настраиваем workspace папки
    QStringList workspaceFolders = {m_workspacePath};
    client->setWorkspaceFolders(workspaceFolders);

    return client;
}

LSPClient::Language LSPManager::getLanguageFromFilePath(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "cpp" || suffix == "cc" || suffix == "cxx" ||
        suffix == "h" || suffix == "hpp" || suffix == "hh") {
        return LSPClient::LanguageCpp;
    }
    else if (suffix == "java") {
        return LSPClient::LanguageJava;
    }

    return LSPClient::LanguageUnknown;
}

// Реализация в lsp_manager.cpp:

bool LSPManager::openDocumentByFilePath(const QString& filePath, const QString& text)
{
    LSPClient::Language language = getLanguageFromFilePath(filePath);
    if (language == LSPClient::LanguageUnknown) {
        qWarning() << "Unknown file type:" << filePath;
        return false;
    }

    // Создаем клиент если его нет
    if (!m_clients.contains(language)) {
        if (!createClient(language)) {
            return false;
        }

        // Автоматически запускаем клиент
        if (!autoSetup(language)) {
            qWarning() << "Failed to auto-setup client for:" << filePath;
            return false;
        }
    }

    return openDocument(language, filePath, text);
}

bool LSPManager::updateDocumentByFilePath(const QString& filePath, const QString& text)
{
    LSPClient::Language language = getLanguageFromFilePath(filePath);
    if (language == LSPClient::LanguageUnknown) {
        return false;
    }

    return updateDocument(language, filePath, text);
}

bool LSPManager::closeDocumentByFilePath(const QString& filePath)
{
    LSPClient::Language language = getLanguageFromFilePath(filePath);
    if (language == LSPClient::LanguageUnknown) {
        return false;
    }

    return closeDocument(language, filePath);
}

void LSPManager::requestCompletionByFilePath(const QString& filePath, int line, int character)
{
    LSPClient::Language language = getLanguageFromFilePath(filePath);
    if (language == LSPClient::LanguageUnknown) {
        emit completionReady(LSPClient::LanguageUnknown, "", line, character, {});
        return;
    }

    requestCompletion(language, filePath, line, character);
}

void LSPManager::requestHoverByFilePath(const QString& filePath, int line, int character)
{
    LSPClient::Language language = getLanguageFromFilePath(filePath);
    if (language == LSPClient::LanguageUnknown) {
        emit hoverReady(LSPClient::LanguageUnknown, "", line, character, "");
        return;
    }

    requestHover(language, filePath, line, character);
}

int LSPManager::activeClientsCount() const
{
    int count = 0;
    for (LSPClient* client : m_clients) {
        if (client && client->isConnected()) {
            count++;
        }
    }
    return count;
}

bool LSPManager::hasActiveClient(LSPClient::Language language) const
{
    LSPClient* client = getClient(language);
    return client && client->isConnected();
}
