#ifndef CPPLSPCLIENT_H
#define CPPLSPCLIENT_H

#include "lsp_client.h"
#include "service/clangd_installer.h"

class CppLSPClient : public LSPClient
{
    Q_OBJECT
public:
    explicit CppLSPClient(QObject* parent = nullptr);
    ~CppLSPClient() override;

    QStringList getDefaultServerArgs() const override;
    QJsonObject getDefaultInitOptions() const override;
    void applyLanguageSpecificSettings(QJsonObject& initOptions) override;

    QString getLanguageId() const override { return "cpp"; }
    QString getServerName() const override { return "clangd"; }

    QString getServerPath() { return m_serverPath; }
    QStringList getServerArgs() { return m_serverArgs; };

    void setCppStandard(QString str) { m_cppStandard = str; }
    void addIncludePath(QString str) { m_includePaths.append(str); }

    void installClangd();

signals:
    void clangdInstallationFinished(bool success, const QString& path);

private:
    QString m_cppStandard = "c++17";
    QString m_compileCommandsPath;
    QStringList m_includePaths;
    ClangdInstaller* installer = nullptr;
};

#endif // CPPLSPCLIENT_H
