#ifndef CPPLSPCLIENT_H
#define CPPLSPCLIENT_H

#include "lsp_client.h"

class CppLSPClient : public LSPClient
{
    Q_OBJECT

public:
    explicit CppLSPClient(QObject* parent = nullptr);

    // LSPClient interface
    QString getLanguageId() const override { return "cpp"; }
    QString getServerName() const override { return "clangd"; }
    QStringList getDefaultServerArgs() const override;
    QJsonObject getDefaultInitOptions() const override;
    void applyLanguageSpecificSettings(QJsonObject& initOptions) override;

    // C++ специфичные методы
    void setCompilationDatabasePath(const QString& path);
    void setCompileCommands(const QJsonArray& commands);
    void addIncludePath(const QString& path);
    void setCppStandard(const QString& standard); // "c++11", "c++14", etc.

private:
    QString m_compileCommandsPath;
    QJsonArray m_compileCommands;
    QStringList m_includePaths;
    QString m_cppStandard;
};

#endif // CPPLSPCLIENT_H
