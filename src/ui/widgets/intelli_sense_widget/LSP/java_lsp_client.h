#ifndef JAVALSPCLIENT_H
#define JAVALSPCLIENT_H

#include <QSettings>
#include <QDirIterator>

#include "lsp_client.h"
#include "service/jdtls_downloader.h"

class JavaLSPClient : public LSPClient
{
    Q_OBJECT

public:
    explicit JavaLSPClient(QObject* parent = nullptr);

    // LSPClient interface
    QString getLanguageId() const override { return "java"; }
    QString getServerName() const override { return "jdtls"; }

    QStringList getDefaultServerArgs() const override;
    QJsonObject getDefaultInitOptions() const override;
    void applyLanguageSpecificSettings(QJsonObject& initOptions) override;

    void setJavaHome(const QString& path);
    void setWorkspaceFolders(const QStringList& folders);
    void addClassPath(const QString& path);
    void setJavaVersion(const QString& version); // "8", "11", "17", etc.

    void setJdtLsSettings(const QJsonObject& settings);

    bool downloadAndSetupJdtls(bool showProgress = false);

    void setAllowDeepSearch(bool allow) { m_allowDeepSearch = allow; }
    bool allowDeepSearch() const { return m_allowDeepSearch; }

signals:
    void downloadProgress(qint64 bytesReceived, qint64 totalBytes);
    void downloadFinished(bool success, const QString& path);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 totalBytes);
    void onDownloadFinished(const QString& jdtlsPath);
    void onDownloadError(const QString& error);

private:
    QString m_javaHome;
    QStringList m_workspaceFolders;
    QStringList m_classPath;
    QString m_javaVersion;
    QJsonObject m_jdtlsSettings;

    bool m_allowDeepSearch = false;

    JdtlsDownloader* m_downloader;

    QString findJavaHome() const;
    QString findJdtLs() const;

    bool isValidJdtLsPath(const QString& path) const;
    QStringList findLauncherJars(const QDir& dir) const;
};

#endif // JAVALSPCLIENT_H
