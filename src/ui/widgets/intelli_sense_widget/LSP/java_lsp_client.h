#ifndef JAVALSPCLIENT_H
#define JAVALSPCLIENT_H

#include "lsp_client.h"
#include "service/jdtls_installer.h"
#include <QSettings>
#include <QDirIterator>

class JavaLSPClient : public LSPClient
{
    Q_OBJECT

public:
    explicit JavaLSPClient(QObject* parent = nullptr);
    ~JavaLSPClient() override;

    // LSPClient interface
    QStringList getDefaultServerArgs() const override;
    QJsonObject getDefaultInitOptions() const override;
    void applyLanguageSpecificSettings(QJsonObject& initOptions) override;

    // Java-specific methods
    QString findJavaHome() const;
    bool checkJavaInstallation() const;
    void setJavaHome(const QString& path);
    void setJavaVersion(const QString& version);
    QString getJavaHome() const { return m_javaHome; }
    QString getJavaVersion() const { return m_javaVersion; }

    // JDT LS installation
    bool downloadAndSetupJdtls(bool showProgress = true);
    void checkAndInstallAutomatically();
    bool isJdtlsInstalled() const;

    // Workspace and classpath management
    void setWorkspaceFolders(const QStringList& folders);
    void addClassPath(const QString& path);
    void setJdtLsSettings(const QJsonObject& settings);

    virtual QString getLanguageId() const { return QString(); };
    virtual QString getServerName() const { return QString(); };

    QString getServerPath() const { return m_serverPath; }

signals:
    void installationProgress(int percent, const QString& message);
    void javaInstallationRequired();
    void javaInstallationCompleted(bool success, const QString& javaHome);
    void jdtlsInstallationStarted();
    void jdtlsInstallationFinished(bool success, const QString& path);
    void downloadProgressChanged(qint64 bytesReceived, qint64 totalBytes);

public slots:
    void startAutoInstallation();

private slots:
    void onJdtlsInstallProgress(int percent, const QString& message);
    void onJdtlsInstallFinished(LSPInstaller::InstallResult result, const QString& message);
    void onJavaCheckCompleted(bool javaInstalled, const QString& javaHome);
    void onJavaInstallationResult(LSPInstaller::InstallResult result, const QString& message);
    void onJdtlsDownloadProgress(qint64 bytesReceived, qint64 totalBytes);

private:
    QString m_javaHome;
    QString m_javaVersion;
    QStringList m_workspaceFolders;
    QStringList m_classPath;
    QJsonObject m_jdtlsSettings;
    bool m_allowDeepSearch = true;

    JdtlsInstaller* m_installer = nullptr;

    // Helper methods
    QString findJdtLs() const;
    bool isValidJdtLsPath(const QString& path) const;
    QStringList findLauncherJars(const QDir& dir) const;

    void initializeFromSettings();
    void setupInstallerSignals();
    void handleInstallationResult(LSPInstaller::InstallResult result, const QString& message);
};

#endif // JAVALSPCLIENT_H
