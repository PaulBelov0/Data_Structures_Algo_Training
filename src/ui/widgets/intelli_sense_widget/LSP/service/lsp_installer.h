#ifndef LSPINSTALLER_H
#define LSPINSTALLER_H

#include <QObject>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QTime>
#include <QTimer>
#include <QSettings>
#include <QTemporaryFile>

class LSPInstaller : public QObject
{
    Q_OBJECT

public:
    enum InstallResult {
        Success,
        Failed,
        Cancelled,
        AlreadyInstalled
    };

    explicit LSPInstaller(QObject *parent = nullptr);
    virtual ~LSPInstaller() = default;

    // Основной метод установки
    virtual InstallResult install() = 0;

    // Проверка установлен ли сервер
    virtual bool isInstalled() const = 0;

    // Путь к установленному серверу
    virtual QString getInstallPath() const = 0;

    // Название сервера для UI
    virtual QString getName() const = 0;

signals:
    void progressChanged(int percent, const QString& message);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void finished(LSPInstaller::InstallResult result, const QString& message);
    void logMessage(const QString& message);

protected:
    QNetworkAccessManager* m_networkManager;

    // Вспомогательные методы
    QString downloadFile(const QUrl& url, const QString& destPath);
    bool extractArchive(const QString& archivePath, const QString& destDir);
    bool addToPath(const QString& dirPath);
    QString findInPath(const QString& executable) const;


    // Новые методы
    bool runProcess(const QString& program, const QStringList& args,
                    const QString& workingDir = QString(), int timeoutMs = 300000);
    QString getTempFilePath(const QString& prefix = "lsp_install_");
    QString getInstallBaseDir() const;
    bool setEnvironmentVariable(const QString& name, const QString& value);
};

#endif // LSPINSTALLER_H
