#ifndef JDTLSDOWNLOADER_H
#define JDTLSDOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QJsonObject>

class JdtlsDownloader : public QObject
{
    Q_OBJECT

public:
    enum DownloadSource {
        SourceEclipseSnapshots,
        SourceGitHubReleases,
        SourceMavenCentral
    };

    explicit JdtlsDownloader(QObject* parent = nullptr);

    bool downloadJdtls(const QString& targetDir, DownloadSource source = SourceEclipseSnapshots);
    bool isDownloading() const { return m_isDownloading; }
    qint64 downloadProgress() const { return m_bytesReceived; }
    qint64 totalSize() const { return m_totalBytes; }

    static QString getDefaultDownloadPath();
    static QString getLatestVersionUrl(DownloadSource source);
    bool extractArchive(const QString& archivePath, const QString& targetDir);

signals:
    void downloadProgressChanged(qint64 bytesReceived, qint64 totalBytes);
    void downloadFinished(const QString& jdtlsPath);
    void downloadError(const QString& error);
    void extractionProgress(int percentage);
    void extractionFinished(bool success);

public slots:
    void cancelDownload();

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 totalBytes);
    void onDownloadFinished();
    void onReadyRead();
    void onExtractFinished(int exitCode);

private:
    void startDownloadFromEclipse();
    void startDownloadFromGitHub();
    void startDownloadFromMaven();
    void cleanup();

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QFile* m_downloadFile;

    QString m_targetDir;
    QString m_downloadUrl;
    DownloadSource m_currentSource;

    bool m_isDownloading;
    qint64 m_bytesReceived;
    qint64 m_totalBytes;

    QProcess* m_extractProcess;
};

#endif // JDTLSDOWNLOADER_H
