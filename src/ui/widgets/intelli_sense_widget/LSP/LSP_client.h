#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextDocument>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QTemporaryFile>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>
#include <QStandardPaths>


// DON'T TOUCH !!!!!

struct LSPPosition {
    int line;
    int character;

    QJsonObject toJson() const {
        return QJsonObject{
            {"line", line},
            {"character", character}
        };
    }
};

struct LSPRange {
    LSPPosition start;
    LSPPosition end;

    QJsonObject toJson() const {
        return QJsonObject{
            {"start", start.toJson()},
            {"end", end.toJson()}
        };
    }
};

struct LSPTextDocumentItem {
    QString uri;
    QString languageId;
    int version;
    QString text;

    QJsonObject toJson() const {
        return QJsonObject{
            {"uri", uri},
            {"languageId", languageId},
            {"version", version},
            {"text", text}
        };
    }
};

struct LSPCompletionItem {
    QString label;
    QString kind;
    QString detail;
    QString documentation;
    QString insertText;
    QString filterText;

    static LSPCompletionItem fromJson(const QJsonObject& json) {
        LSPCompletionItem item;
        item.label = json["label"].toString();
        item.kind = json["kind"].toString();
        item.detail = json.value("detail").toString();
        item.documentation = json.value("documentation").toString();
        item.insertText = json.value("insertText").toString(item.label);
        item.filterText = json.value("filterText").toString();
        return item;
    }
};

class LSPClient : public QObject
{
    Q_OBJECT

public:
    enum State {
        Disconnected,
        Starting,
        Initializing,
        Connected,
        Error
    };

    enum Language {
        LanguageUnknown,
        LanguageCpp,
        LanguageJava,
        LanguagePython,
        LanguageJavaScript
    };

    explicit LSPClient(QObject* parent = nullptr);
    virtual ~LSPClient();

    // Основные методы
    virtual bool start(const QString& projectPath = "");
    virtual void stop();
    virtual void restart();

    // Состояние
    State state() const { return m_state; }
    bool isConnected() const { return m_state == Connected; }
    QString errorString() const { return m_errorString; }
    Language language() const { return m_language; }

    // Методы документов
    bool openDocument(const QString& filePath, const QString& text);
    bool updateDocument(const QString& filePath, const QString& text, int version = -1);
    bool closeDocument(const QString& filePath);
    bool saveDocument(const QString& filePath);

    // Запросы к серверу
    void requestCompletion(const QString& filePath, int line, int character);
    void requestSignatureHelp(const QString& filePath, int line, int character);
    void requestHover(const QString& filePath, int line, int character);

    void requestDefinition(const QString& filePath, int line, int character) {}; //todo
    void requestReferences(const QString& filePath, int line, int character) {}; //todo
    void requestFormatting(const QString& filePath) {}; //todo

    void requestDocumentSymbols(const QString& filePath);
    void requestWorkspaceSymbols(const QString& query);
    void requestCodeAction(const QString& filePath, const LSPRange& range);
    void requestRename(const QString& filePath, int line, int character, const QString& newName);

    // Настройки
    void setRootPath(const QString& path) { m_rootPath = path; }
    QString rootPath() const { return m_rootPath; }

    void setServerPath(const QString& path) { m_serverPath = path; }
    QString serverPath() const { return m_serverPath; }

    void setServerArguments(const QStringList& args) { m_serverArgs = args; }
    QStringList serverArguments() const { return m_serverArgs; }

    void setInitializationOptions(const QJsonObject& options) { m_initOptions = options; }
    QJsonObject initializationOptions() const { return m_initOptions; }

signals:
    void stateChanged(LSPClient::State state);
    void completionReady(const QString& fileUri, int line, int character, const QList<LSPCompletionItem>& items);
    void signatureHelpReady(const QString& fileUri, int line, int character, const QJsonObject& help);
    void hoverReady(const QString& fileUri, int line, int character, const QString& content);
    void definitionReady(const QString& fileUri, int line, int character, const QList<QString>& locations);
    void referencesReady(const QString& fileUri, int line, int character, const QList<QString>& locations);
    void documentSymbolsReady(const QString& fileUri, const QJsonArray& symbols);
    void workspaceSymbolsReady(const QString& query, const QJsonArray& symbols);
    void codeActionReady(const QString& fileUri, const LSPRange& range, const QJsonArray& actions);
    void formattingReady(const QString& fileUri, const QJsonArray& edits);
    void renameReady(const QString& fileUri, int line, int character, const QJsonObject& result);
    void diagnosticsUpdated(const QString& fileUri, const QJsonArray& diagnostics);
    void logMessage(const QString& message, const QString& type = "log");
    void errorOccurred(const QString& error);

protected:
    // Виртуальные методы для переопределения наследниками
    virtual QString getLanguageId() const = 0;
    virtual QString getServerName() const = 0;
    virtual QStringList getDefaultServerArgs() const = 0;
    virtual QJsonObject getDefaultInitOptions() const = 0;
    virtual void applyLanguageSpecificSettings(QJsonObject& initOptions) = 0;

    // Вспомогательные методы
    QString pathToUri(const QString& path) const;
    QString uriToPath(const QString& uri) const;
    int getNextRequestId();

    // Обработчики LSP сообщений
    void handleNotification(const QJsonObject& message);
    void handleResponse(const QJsonObject& message);

    // LSP протокол сообщения
    QJsonObject createRequest(const QString& method, const QJsonObject& params);
    QJsonObject createNotification(const QString& method, const QJsonObject& params);

    // Обработка специфичных сообщений
    void processCompletionResponse(int requestId, const QJsonObject& result);
    void processDiagnostics(const QJsonObject& params);
    void parseMessage(const QJsonObject& message);

    QString m_rootPath;
    Language m_language;
    QString m_serverPath;
    QStringList m_serverArgs;

private slots:
    void onProcessReadyRead();
    void onProcessErrorOccurred(QProcess::ProcessError error) {};
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {};
    void onInitializeTimeout();
    void onKeepAliveTimeout();

private:
    void setState(State newState);
    void sendMessage(const QJsonObject& message);
    void initializeServer();
    void initializedServer();
    void parseMessage(const QByteArray& data);
    void sendInitialize();
    void sendInitialized();

    QProcess* m_process;
    State m_state;
    QJsonObject m_initOptions;
    QString m_errorString;

    // Буфер для чтения сообщений
    QByteArray m_buffer;

    // Для отслеживания запросов
    QMap<int, QString> m_pendingRequests;
    int m_requestIdCounter;

    // Открытые документы
    QMap<QString, int> m_openDocuments; // URI -> version

    // Таймеры
    QTimer* m_initializeTimer;
    QTimer* m_keepAliveTimer;

    // Capabilities сервера
    QJsonObject m_serverCapabilities;

    // Временные файлы для документов без сохранения
    QMap<QString, QString> m_tempFiles;
};

#endif // LSPCLIENT_H
