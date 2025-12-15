#ifndef JDTLSINSTALLER_H
#define JDTLSINSTALLER_H

#include "lsp_installer.h"
#include <QNetworkAccessManager>
#include <QProcess>
#include <QFile>
#include <QAbstractButton>
#include <QNetworkReply>

class JdtlsInstaller : public LSPInstaller
{
    Q_OBJECT

public:
    enum DownloadSource {
        SourceEclipseSnapshots,
        SourceGitHubReleases,
        SourceMavenCentral
    };

    explicit JdtlsInstaller(QObject* parent = nullptr);
    ~JdtlsInstaller() override;

    // LSPInstaller interface
    InstallResult install() override;
    bool isInstalled() const override;
    QString getInstallPath() const override;
    QString getName() const override { return "JDT Language Server (Java)"; }

    // Java-specific methods
    bool checkJavaInstallation();
    InstallResult installJavaJDK();
    QString getDetectedJavaHome() const { return m_javaHome; }

    // Downloader functionality
    bool downloadJdtls(DownloadSource source = SourceEclipseSnapshots);
    void cancelDownload();

public slots:
    void checkAndInstall();

signals:
    void javaCheckCompleted(bool javaInstalled, const QString& javaHome);
    void javaInstallationResult(InstallResult result, const QString& message);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 totalBytes);
    void onDownloadFinished();
    void onExtractFinished(int exitCode);
    void onJavaInstallationFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString m_installDir;
    QString m_javaHome;
    QString m_version;
    QString m_downloadUrl;

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QFile* m_downloadFile;
    QProcess* m_extractProcess;
    QProcess* m_javaInstallProcess;

    bool m_isDownloading;
    qint64 m_bytesReceived;
    qint64 m_totalBytes;

    DownloadSource m_currentSource;

    // Installation methods
    InstallResult performJdtlsInstallation();
    void startDownloadFromEclipse();
    void startDownloadFromGitHub();
    void startDownloadFromMaven();
    bool extractArchive(const QString& archivePath, const QString& targetDir);
    QString findLauncherJar(const QString& dirPath) const;
    bool setupJdtlsEnvironment();

    // Java installation methods
    InstallResult installJavaWindows();
    InstallResult installJavaLinux();
    InstallResult installJavaMacOS();
    QString getJavaDownloadUrl() const;

    void cleanup();
};

#endif // JDTLSINSTALLER_H
