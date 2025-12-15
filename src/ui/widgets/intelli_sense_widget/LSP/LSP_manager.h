#ifndef LSPMANAGER_H
#define LSPMANAGER_H

#include <QObject>
#include <QMap>
#include "lsp_client.h"
#include "cpp_lsp_client.h"
#include "java_lsp_client.h"

class LSPManager : public QObject
{
    Q_OBJECT

public:
    explicit LSPManager(QObject* parent = nullptr);
    ~LSPManager();

    // Управление клиентами
    bool createClient(LSPClient::Language language);
    bool removeClient(LSPClient::Language language);
    LSPClient* getClient(LSPClient::Language language) const;

    // Автоматическая настройка
    bool autoSetup(LSPClient::Language language);

    // Управление документами
    bool openDocument(LSPClient::Language language, const QString& filePath, const QString& text);
    bool updateDocument(LSPClient::Language language, const QString& filePath, const QString& text);
    bool closeDocument(LSPClient::Language language, const QString& filePath);

    // Запросы
    void requestCompletion(LSPClient::Language language, const QString& filePath,
                           int line, int character);
    void requestHover(LSPClient::Language language, const QString& filePath,
                      int line, int character);

    // Состояние
    bool isLanguageSupported(LSPClient::Language language) const;
    QStringList getSupportedLanguages() const;

    // Настройки
    void setWorkspacePath(const QString& path);
    void setServerPath(LSPClient::Language language, const QString& path);

    // Удобные методы для работы с файлами
    bool openDocumentByFilePath(const QString& filePath, const QString& text);
    bool updateDocumentByFilePath(const QString& filePath, const QString& text);
    bool closeDocumentByFilePath(const QString& filePath);
    void requestCompletionByFilePath(const QString& filePath, int line, int character);
    void requestHoverByFilePath(const QString& filePath, int line, int character);

    // Статистика
    int activeClientsCount() const;
    bool hasActiveClient(LSPClient::Language language) const;

signals:
    void clientCreated(LSPClient::Language language, bool success);
    void clientStateChanged(LSPClient::Language language, LSPClient::State state);
    void completionReady(LSPClient::Language language, const QString& fileUri,
                         int line, int character, const QList<LSPCompletionItem>& items);
    void hoverReady(LSPClient::Language language, const QString& fileUri,
                    int line, int character, const QString& content);
    void diagnosticsUpdated(LSPClient::Language language, const QString& fileUri,
                            const QJsonArray& diagnostics);
    void logMessage(const QString& message, const QString& type);

private slots:
    void onClientStateChanged(LSPClient::State state);
    void onCompletionReady(const QString& fileUri, int line, int character,
                           const QList<LSPCompletionItem>& items);
    void onHoverReady(const QString& fileUri, int line, int character,
                      const QString& content);
    void onDiagnosticsUpdated(const QString& fileUri, const QJsonArray& diagnostics);
    void onLogMessage(const QString& message, const QString& type);
    void onErrorOccurred(const QString& error);

private:
    LSPClient* createCppClient();
    LSPClient* createJavaClient();
    LSPClient::Language getLanguageFromFilePath(const QString& filePath) const;

    QMap<LSPClient::Language, LSPClient*> m_clients;
    QString m_workspacePath;
    QMap<LSPClient::Language, QString> m_serverPaths;
};

#endif // LSPMANAGER_H
