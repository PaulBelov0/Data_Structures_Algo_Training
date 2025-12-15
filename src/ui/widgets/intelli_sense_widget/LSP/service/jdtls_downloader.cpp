#include "jdtls_downloader.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QThread>
#include <QCoreApplication>
#include <QTimer>
#include <QTemporaryDir>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

JdtlsDownloader::JdtlsDownloader(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_downloadFile(nullptr)
    , m_isDownloading(false)
    , m_bytesReceived(0)
    , m_totalBytes(0)
    , m_extractProcess(nullptr)
{
}

bool JdtlsDownloader::downloadJdtls(const QString& targetDir, DownloadSource source)
{
    if (m_isDownloading) {
        emit downloadError("Already downloading");
        return false;
    }

    if (targetDir.isEmpty()) {
        emit downloadError("Target directory is empty");
        return false;
    }

    QDir dir(targetDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emit downloadError("Cannot create target directory");
            return false;
        }
    }

    m_targetDir = targetDir;
    m_currentSource = source;
    m_isDownloading = true;
    m_bytesReceived = 0;
    m_totalBytes = 0;

    // Выбираем источник для загрузки
    switch (source) {
    case SourceEclipseSnapshots:
        startDownloadFromEclipse();
        break;
    case SourceGitHubReleases:
        startDownloadFromGitHub();
        break;
    case SourceMavenCentral:
        startDownloadFromMaven();
        break;
    }

    return true;
}

QString JdtlsDownloader::getDefaultDownloadPath()
{
    QString downloadPath;

#ifdef Q_OS_WIN
    downloadPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#else
    downloadPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/jdtls";
#endif

    QDir dir(downloadPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return downloadPath;
}

QString JdtlsDownloader::getLatestVersionUrl(DownloadSource source)
{
    switch (source) {
    case SourceEclipseSnapshots:
        return "https://download.eclipse.org/jdtls/snapshots/jdt-language-server-latest.tar.gz";
    case SourceGitHubReleases:
        return "https://api.github.com/repos/eclipse-jdtls/eclipse.jdt.ls/releases/latest";
    case SourceMavenCentral:
        return "https://repo1.maven.org/maven2/org/eclipse/jdtls/org.eclipse.jdt.ls.product/";
    }

    return QString();
}

void JdtlsDownloader::startDownloadFromEclipse()
{
    m_downloadUrl = "https://download.eclipse.org/jdtls/snapshots/jdt-language-server-latest.tar.gz";

    QNetworkRequest request(m_downloadUrl);
    request.setRawHeader("User-Agent", "QT-LSP-Client/1.0");

    QString filename = "jdtls-latest.tar.gz";
    m_downloadFile = new QFile(m_targetDir + "/" + filename);

    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit downloadError("Cannot open file for writing");
        cleanup();
        return;
    }

    m_currentReply = m_networkManager->get(request);

    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &JdtlsDownloader::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &JdtlsDownloader::onDownloadFinished);
    connect(m_currentReply, &QNetworkReply::readyRead,
            this, &JdtlsDownloader::onReadyRead);
}

void JdtlsDownloader::startDownloadFromGitHub()
{
    // Сначала получаем информацию о последнем релизе
    QNetworkRequest request(QUrl("https://api.github.com/repos/eclipse-jdtls/eclipse.jdt.ls/releases/latest"));
    request.setRawHeader("User-Agent", "QT-LSP-Client/1.0");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    QNetworkReply* apiReply = m_networkManager->get(request);

    connect(apiReply, &QNetworkReply::finished, this, [this, apiReply]() {
        apiReply->deleteLater();

        if (apiReply->error() != QNetworkReply::NoError) {
            emit downloadError("Failed to get release info: " + apiReply->errorString());
            cleanup();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(apiReply->readAll());
        if (doc.isNull()) {
            emit downloadError("Invalid JSON response");
            cleanup();
            return;
        }

        QJsonObject release = doc.object();
        QJsonArray assets = release["assets"].toArray();

        // Ищем asset с архивом
        QString downloadUrl;
        QString filename;

        for (const QJsonValue& asset : assets) {
            QJsonObject assetObj = asset.toObject();
            QString name = assetObj["name"].toString();
            QString url = assetObj["browser_download_url"].toString();

            if (name.contains("tar.gz") || name.contains("zip")) {
                downloadUrl = url;
                filename = name;
                break;
            }
        }

        if (downloadUrl.isEmpty()) {
            emit downloadError("No suitable download found in release");
            cleanup();
            return;
        }

        // Начинаем загрузку
        m_downloadUrl = downloadUrl;
        m_downloadFile = new QFile(m_targetDir + "/" + filename);

        if (!m_downloadFile->open(QIODevice::WriteOnly)) {
            emit downloadError("Cannot open file for writing");
            cleanup();
            return;
        }

        QNetworkRequest downloadRequest(downloadUrl);
        downloadRequest.setRawHeader("User-Agent", "QT-LSP-Client/1.0");

        m_currentReply = m_networkManager->get(downloadRequest);

        connect(m_currentReply, &QNetworkReply::downloadProgress,
                this, &JdtlsDownloader::onDownloadProgress);
        connect(m_currentReply, &QNetworkReply::finished,
                this, &JdtlsDownloader::onDownloadFinished);
        connect(m_currentReply, &QNetworkReply::readyRead,
                this, &JdtlsDownloader::onReadyRead);
    });
}

void JdtlsDownloader::startDownloadFromMaven()
{
    // Maven Central требует сложного парсинга metadata
    // Упростим и возьмем конкретную версию
    QString version = "1.32.0"; // Пример версии
    m_downloadUrl = QString("https://repo1.maven.org/maven2/org/eclipse/jdtls/org.eclipse.jdt.ls.product/%1/org.eclipse.jdt.ls.product-%1.tar.gz")
                        .arg(version);

    QNetworkRequest request(m_downloadUrl);
    request.setRawHeader("User-Agent", "QT-LSP-Client/1.0");

    QString filename = QString("jdtls-%1.tar.gz").arg(version);
    m_downloadFile = new QFile(m_targetDir + "/" + filename);

    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit downloadError("Cannot open file for writing");
        cleanup();
        return;
    }

    m_currentReply = m_networkManager->get(request);

    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &JdtlsDownloader::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &JdtlsDownloader::onDownloadFinished);
    connect(m_currentReply, &QNetworkReply::readyRead,
            this, &JdtlsDownloader::onReadyRead);
}

void JdtlsDownloader::onDownloadProgress(qint64 bytesReceived, qint64 totalBytes)
{
    m_bytesReceived = bytesReceived;
    m_totalBytes = totalBytes;
    emit downloadProgressChanged(bytesReceived, totalBytes);
}

void JdtlsDownloader::onDownloadFinished()
{
    if (!m_currentReply || !m_downloadFile) {
        cleanup();
        return;
    }

    if (m_currentReply->error() != QNetworkReply::NoError) {
        emit downloadError("Download failed: " + m_currentReply->errorString());
        cleanup();
        return;
    }

    m_downloadFile->close();

    QString archivePath = m_downloadFile->fileName();
    QString extractDir = m_targetDir + "/extracted";

    qDebug() << "Download finished, extracting to:" << extractDir;

    // Распаковываем архив
    if (extractArchive(archivePath, extractDir)) {
        emit extractionProgress(100);
        emit extractionFinished(true);

        // Ищем launcher jar в распакованной директории
        QDir dir(extractDir);
        QStringList launcherJars = dir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files, QDir::Name);

        if (!launcherJars.isEmpty()) {
            emit downloadFinished(extractDir);
        } else {
            // Проверяем вложенные директории
            QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& subdir : subdirs) {
                QDir subdirPath(extractDir + "/" + subdir);
                launcherJars = subdirPath.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files, QDir::Name);
                if (!launcherJars.isEmpty()) {
                    emit downloadFinished(extractDir + "/" + subdir);
                    break;
                }
            }
        }
    } else {
        emit downloadError("Failed to extract archive");
    }

    cleanup();
}

void JdtlsDownloader::onReadyRead()
{
    if (m_currentReply && m_downloadFile) {
        m_downloadFile->write(m_currentReply->readAll());
    }
}

bool JdtlsDownloader::extractArchive(const QString& archivePath, const QString& targetDir)
{
    QDir dir(targetDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Определяем команду для распаковки в зависимости от ОС и типа архива
    QString program;
    QStringList args;

    if (archivePath.endsWith(".tar.gz") || archivePath.endsWith(".tgz")) {
#ifdef Q_OS_WIN
        // На Windows используем tar из Git или 7-zip
        program = "tar";
        args << "-xzf" << archivePath << "-C" << targetDir;

        QProcess checkTar;
        checkTar.start("tar", {"--version"});
        if (!checkTar.waitForFinished(3000)) {
            // tar не найден, пробуем 7-zip
            program = "7z";
            args = QStringList() << "x" << archivePath << "-o" + targetDir << "-y";
        }
#else
        program = "tar";
        args << "-xzf" << archivePath << "-C" << targetDir;
#endif
    } else if (archivePath.endsWith(".zip")) {
#ifdef Q_OS_WIN
        program = "powershell";
        args << "-Command" << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                                  .arg(archivePath).arg(targetDir);
#else
        program = "unzip";
        args << "-o" << archivePath << "-d" << targetDir;
#endif
    } else {
        qWarning() << "Unsupported archive format:" << archivePath;
        return false;
    }

    m_extractProcess = new QProcess(this);
    m_extractProcess->setProgram(program);
    m_extractProcess->setArguments(args);

    connect(m_extractProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JdtlsDownloader::onExtractFinished);

    m_extractProcess->start();

    // Ждем завершения с таймаутом
    if (!m_extractProcess->waitForFinished(300000)) { // 5 минут
        qWarning() << "Extraction timeout";
        m_extractProcess->kill();
        return false;
    }

    return m_extractProcess->exitCode() == 0;
}

void JdtlsDownloader::onExtractFinished(int exitCode)
{
    if (m_extractProcess) {
        if (exitCode != 0) {
            QString error = QString::fromUtf8(m_extractProcess->readAllStandardError());
            qWarning() << "Extraction failed:" << error;
        }
        m_extractProcess->deleteLater();
        m_extractProcess = nullptr;
    }
}

void JdtlsDownloader::cancelDownload()
{
    if (m_currentReply) {
        m_currentReply->abort();
    }
    cleanup();
}

void JdtlsDownloader::cleanup()
{
    m_isDownloading = false;

    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_downloadFile) {
        if (m_downloadFile->isOpen()) {
            m_downloadFile->close();
        }
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }

    if (m_extractProcess) {
        if (m_extractProcess->state() == QProcess::Running) {
            m_extractProcess->kill();
        }
        m_extractProcess->deleteLater();
        m_extractProcess = nullptr;
    }
}
