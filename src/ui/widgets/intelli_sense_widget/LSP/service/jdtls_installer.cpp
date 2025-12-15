#include "jdtls_installer.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QCoreApplication>
#include <QSettings>
#include <QProcessEnvironment>
#include <qpushbutton.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

JdtlsInstaller::JdtlsInstaller(QObject* parent)
    : LSPInstaller(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_downloadFile(nullptr)
    , m_extractProcess(nullptr)
    , m_javaInstallProcess(nullptr)
    , m_isDownloading(false)
    , m_bytesReceived(0)
    , m_totalBytes(0)
    , m_currentSource(SourceEclipseSnapshots)
{
    // Устанавливаем путь установки
    m_installDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/jdtls";
    QDir().mkpath(m_installDir);

    // Проверяем Java при создании
    checkJavaInstallation();
}

JdtlsInstaller::~JdtlsInstaller()
{
    cleanup();
}

LSPInstaller::InstallResult JdtlsInstaller::install()
{
    emit progressChanged(0, "Начинаем установку JDT Language Server...");

    // Шаг 1: Проверяем Java
    if (!checkJavaInstallation()) {
        emit progressChanged(20, "Java не найдена. Требуется установка JDK...");

        QMessageBox msgBox;
        msgBox.setWindowTitle("Требуется Java");
        msgBox.setText("Для работы JDT Language Server требуется Java JDK 11 или выше.");
        msgBox.setInformativeText("Установить Java JDK автоматически?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);

        int result = msgBox.exec();

        if (result == QMessageBox::Cancel) {
            return Cancelled;
        }

        if (result == QMessageBox::Yes) {
            InstallResult javaResult = installJavaJDK();
            if (javaResult != Success) {
                return javaResult;
            }
        } else {
            // Пользователь хочет установить Java вручную
            emit progressChanged(30, "Установите Java JDK вручную и перезапустите установку...");
            return Failed;
        }
    }

    emit progressChanged(40, "Java проверена: " + m_javaHome);

    // Шаг 2: Проверяем, не установлен ли уже JDT LS
    if (isInstalled()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("JDT LS уже установлен");
        msgBox.setText("JDT Language Server уже установлен.");
        msgBox.setInformativeText("Хотите переустановить?\nТекущий путь: " + getInstallPath());
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);

        if (msgBox.exec() == QMessageBox::No) {
            return AlreadyInstalled;
        }
    }

    // Шаг 3: Выбор источника загрузки
    QMessageBox sourceBox;
    sourceBox.setWindowTitle("Выбор источника загрузки");
    sourceBox.setText("Откуда скачать JDT Language Server?");
    sourceBox.setIcon(QMessageBox::Question);

    QPushButton* eclipseBtn = sourceBox.addButton("Eclipse Snapshots (рекомендуется)", QMessageBox::ActionRole);
    QPushButton* githubBtn = sourceBox.addButton("GitHub Releases", QMessageBox::ActionRole);
    QPushButton* cancelBtn = sourceBox.addButton("Отмена", QMessageBox::RejectRole);

    sourceBox.exec();

    QAbstractButton* clickedButton = sourceBox.clickedButton();

    if (clickedButton == cancelBtn) {
        return Cancelled;
    }

    if (clickedButton == eclipseBtn) {
        m_currentSource = SourceEclipseSnapshots;
    } else if (clickedButton == githubBtn) {
        m_currentSource = SourceGitHubReleases;
    }

    // Шаг 4: Выполняем установку
    return performJdtlsInstallation();
}

bool JdtlsInstaller::isInstalled() const
{
    // Проверяем в сохраненных настройках
    QSettings settings;
    QString savedPath = settings.value("LSP/Java/JdtlsPath", "").toString();

    if (!savedPath.isEmpty() && QDir(savedPath).exists()) {
        // Проверяем наличие launcher jar
        QString launcherJar = findLauncherJar(savedPath);
        return !launcherJar.isEmpty();
    }

    // Проверяем в стандартных местах
    QStringList possiblePaths = {
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/jdtls",
        QDir::homePath() + "/.local/share/jdtls",
        QDir::homePath() + "/.vscode/extensions/redhat.java-*/server"
    };

    for (const QString& path : possiblePaths) {
        if (QDir(path).exists() && !findLauncherJar(path).isEmpty()) {
            return true;
        }
    }

    return false;
}

QString JdtlsInstaller::getInstallPath() const
{
    // Возвращаем путь из настроек или найденный
    QSettings settings;
    QString savedPath = settings.value("LSP/Java/JdtlsPath", "").toString();

    if (!savedPath.isEmpty() && QDir(savedPath).exists()) {
        return savedPath;
    }

    // Ищем в установочной директории
    if (QDir(m_installDir).exists() && !findLauncherJar(m_installDir).isEmpty()) {
        return m_installDir;
    }

    return QString();
}

bool JdtlsInstaller::checkJavaInstallation()
{
    // Проверяем переменную окружения JAVA_HOME
    QString javaHome = qgetenv("JAVA_HOME");

    if (!javaHome.isEmpty() && QDir(javaHome).exists()) {
        // Проверяем наличие java в bin
        QString javaExe = javaHome + "/bin/java";
#ifdef Q_OS_WIN
        javaExe += ".exe";
#endif

        if (QFileInfo::exists(javaExe)) {
            m_javaHome = javaHome;
            return true;
        }
    }

    // Ищем java в PATH
    QProcess process;
#ifdef Q_OS_WIN
    process.start("where", {"java"});
#else
    process.start("which", {"java"});
#endif

    if (process.waitForFinished(3000)) {
        QString javaPath = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!javaPath.isEmpty()) {
            // Получаем JAVA_HOME из пути к java
            QFileInfo javaInfo(javaPath);
            m_javaHome = javaInfo.dir().absolutePath(); // bin
            m_javaHome = QDir(m_javaHome).absolutePath() + "/.."; // JDK root

            return true;
        }
    }

    return false;
}


LSPInstaller::InstallResult JdtlsInstaller::performJdtlsInstallation()
{
    emit progressChanged(50, "Загружаем JDT Language Server...");

    if (!downloadJdtls(m_currentSource)) {
        emit finished(Failed, "Не удалось начать загрузку");
        return Failed;
    }

    // Ждем завершения загрузки (сигнал finished будет отправлен)
    return Success;
}

bool JdtlsInstaller::downloadJdtls(DownloadSource source)
{
    if (m_isDownloading) {
        emit logMessage("Загрузка уже выполняется");
        return false;
    }

    m_currentSource = source;
    m_isDownloading = true;
    m_bytesReceived = 0;
    m_totalBytes = 0;

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

void JdtlsInstaller::startDownloadFromEclipse()
{
    m_downloadUrl = "https://download.eclipse.org/jdtls/snapshots/jdt-language-server-latest.tar.gz";

    QNetworkRequest request(m_downloadUrl);
    request.setRawHeader("User-Agent", "QT-LSP-Client/1.0");

    QString filename = "jdtls-latest.tar.gz";
    m_downloadFile = new QFile(m_installDir + "/" + filename);

    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        cleanup();
        return;
    }

    m_currentReply = m_networkManager->get(request);

    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &JdtlsInstaller::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &JdtlsInstaller::onDownloadFinished);
    connect(m_currentReply, &QNetworkReply::readyRead,
            [this]() {
                if (m_currentReply && m_downloadFile) {
                    m_downloadFile->write(m_currentReply->readAll());
                }
            });
}

void JdtlsInstaller::onDownloadProgress(qint64 bytesReceived, qint64 totalBytes)
{
    m_bytesReceived = bytesReceived;
    m_totalBytes = totalBytes;

    int percent = totalBytes > 0 ? (bytesReceived * 100) / totalBytes : 0;
    emit progressChanged(50 + (percent / 2),
                         QString("Загрузка: %1% (%2/%3 MB)")
                             .arg(percent)
                             .arg(bytesReceived / (1024 * 1024))
                             .arg(totalBytes / (1024 * 1024)));
    emit downloadProgress(bytesReceived, totalBytes);
}

void JdtlsInstaller::onDownloadFinished()
{
    if (!m_currentReply || !m_downloadFile) {
        cleanup();
        return;
    }

    if (m_currentReply->error() != QNetworkReply::NoError) {
        emit finished(Failed, "Ошибка загрузки: " + m_currentReply->errorString());
        cleanup();
        return;
    }

    m_downloadFile->close();

    emit progressChanged(75, "Распаковываем архив...");

    QString archivePath = m_downloadFile->fileName();
    QString extractDir = m_installDir + "/latest";

    // Очищаем старую директорию
    QDir dir(extractDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    dir.mkpath(".");

    // Распаковываем
    if (extractArchive(archivePath, extractDir)) {
        emit progressChanged(90, "Настраиваем JDT LS...");

        if (setupJdtlsEnvironment()) {
            // Сохраняем путь в настройках
            QSettings settings;
            settings.setValue("LSP/Java/JdtlsPath", extractDir);
            settings.setValue("LSP/Java/JavaHome", m_javaHome);

            emit progressChanged(100, "Установка завершена!");
            emit finished(Success, "JDT Language Server успешно установлен");
        } else {
            emit finished(Failed, "Не удалось настроить окружение JDT LS");
        }
    } else {
        emit finished(Failed, "Ошибка распаковки архива");
    }

    cleanup();
}

bool JdtlsInstaller::setupJdtlsEnvironment()
{
    // Находим launcher jar
    QString launcherJar = findLauncherJar(m_installDir + "/latest");
    if (launcherJar.isEmpty()) {
        emit logMessage("Не найден launcher.jar в распакованной директории");
        return false;
    }

    emit logMessage("Найден launcher: " + launcherJar);

    // Создаем скрипт запуска для разных ОС
    QString scriptPath;

#ifdef Q_OS_WIN
    scriptPath = m_installDir + "/start_jdtls.bat";
    QFile batFile(scriptPath);
    if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&batFile);
        out << "@echo off\n";
        out << "set JAVA_HOME=" << m_javaHome << "\n";
        out << "set JDTLS_HOME=" << m_installDir << "\\latest\n";
        out << "java -jar \"" << launcherJar << "\" -configuration \"%JDTLS_HOME%\\config_win\" -data \"%cd%\\.jdtls-workspace\"\n";
        batFile.close();
    }
#else
    scriptPath = m_installDir + "/start_jdtls.sh";
    QFile shFile(scriptPath);
    if (shFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&shFile);
        out << "#!/bin/bash\n";
        out << "export JAVA_HOME=\"" << m_javaHome << "\"\n";
        out << "export JDTLS_HOME=\"" << m_installDir << "/latest\"\n";

#ifdef Q_OS_MACOS
        out << "java -jar \"" << launcherJar << "\" -configuration \"$JDTLS_HOME/config_mac\" -data \"$(pwd)/.jdtls-workspace\" \"$@\"\n";
#else
        out << "java -jar \"" << launcherJar << "\" -configuration \"$JDTLS_HOME/config_linux\" -data \"$(pwd)/.jdtls-workspace\" \"$@\"\n";
#endif

        shFile.close();

        // Делаем скрипт исполняемым
        chmod(scriptPath.toUtf8().constData(), 0755);
    }
#endif

    emit logMessage("Создан скрипт запуска: " + scriptPath);
    return true;
}

QString JdtlsInstaller::findLauncherJar(const QString& dirPath) const
{
    QDir dir(dirPath);

    // Ищем launcher jar
    QStringList launcherJars = dir.entryList({"org.eclipse.equinox.launcher_*.jar"},
                                             QDir::Files, QDir::Name | QDir::Reversed);

    if (!launcherJars.isEmpty()) {
        return dir.absoluteFilePath(launcherJars.first());
    }

    // Ищем в поддиректориях
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subdir : subdirs) {
        QString subPath = dirPath + "/" + subdir;
        launcherJars = QDir(subPath).entryList({"org.eclipse.equinox.launcher_*.jar"},
                                               QDir::Files, QDir::Name | QDir::Reversed);
        if (!launcherJars.isEmpty()) {
            return subPath + "/" + launcherJars.first();
        }
    }

    return QString();
}

void JdtlsInstaller::cleanup()
{
    m_isDownloading = false;
    m_bytesReceived = 0;
    m_totalBytes = 0;

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_downloadFile) {
        if (m_downloadFile->isOpen()) {
            m_downloadFile->close();
        }
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }

    if (m_extractProcess) {
        if (m_extractProcess->state() == QProcess::Running) {
            m_extractProcess->kill();
        }
        m_extractProcess->deleteLater();
        m_extractProcess = nullptr;
    }

    if (m_javaInstallProcess) {
        if (m_javaInstallProcess->state() == QProcess::Running) {
            m_javaInstallProcess->kill();
        }
        m_javaInstallProcess->deleteLater();
        m_javaInstallProcess = nullptr;
    }
}

void JdtlsInstaller::startDownloadFromMaven()
{
    // Maven Central обычно требует сложного парсинга metadata.xml
    // Используем конкретную стабильную версию
    QString version = "1.32.0"; // Пример стабильной версии
    m_downloadUrl = QString("https://repo1.maven.org/maven2/org/eclipse/jdtls/org.eclipse.jdt.ls.product/%1/org.eclipse.jdt.ls.product-%1.tar.gz")
                        .arg(version);

    QNetworkRequest request(m_downloadUrl);
    request.setRawHeader("User-Agent", "QT-LSP-Client/1.0");

    QString filename = QString("jdtls-%1.tar.gz").arg(version);
    QString tempFile = QDir::tempPath() + "/" + filename;
    m_downloadFile = new QFile(tempFile);

    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit logMessage("Cannot open file for writing: " + tempFile);
        cleanup();
        return;
    }

    m_currentReply = m_networkManager->get(request);

    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &JdtlsInstaller::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &JdtlsInstaller::onDownloadFinished);
    connect(m_currentReply, &QNetworkReply::readyRead,
            [this]() {
                if (m_currentReply && m_downloadFile) {
                    m_downloadFile->write(m_currentReply->readAll());
                }
            });

    emit logMessage("Downloading from Maven Central: " + m_downloadUrl);
}

LSPInstaller::InstallResult JdtlsInstaller::installJavaJDK()
{
#ifdef Q_OS_WIN
    return installJavaWindows();
#elif defined(Q_OS_LINUX)
    return installJavaLinux();
#elif defined(Q_OS_MACOS)
    return installJavaMacOS();
#else
    emit finished(Failed, "Unsupported OS for Java installation");
    return Failed;
#endif
}

LSPInstaller::InstallResult JdtlsInstaller::installJavaWindows()
{
    emit progressChanged(10, "Downloading Java JDK for Windows...");

    // Используем Adoptium (бывший AdoptOpenJDK) - открытый дистрибутив
    QString downloadUrl = "https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.10%2B7/OpenJDK17U-jdk_x64_windows_hotspot_17.0.10_7.msi";

    QString tempFile = QDir::tempPath() + "/java-jdk-installer.msi";

    // Скачиваем установщик
    emit progressChanged(20, "Downloading Java installer...");
    QString downloadedFile = downloadFile(QUrl(downloadUrl), tempFile);
    if (downloadedFile.isEmpty()) {
        emit finished(Failed, "Failed to download Java JDK");
        return Failed;
    }

    emit progressChanged(50, "Installing Java JDK...");

    // Устанавливаем Java через msiexec
    m_javaInstallProcess = new QProcess(this);
    m_javaInstallProcess->setProgram("msiexec");

    // Устанавливаем в Program Files\Eclipse Adoptium
    QString installDir = "C:\\Program Files\\Eclipse Adoptium\\jdk-17.0.10.7-hotspot";
    m_javaInstallProcess->setArguments(QStringList()
                                       << "/i" << downloadedFile
                                       << "/quiet"
                                       << "/norestart"
                                       << "INSTALLDIR=\"" + installDir + "\""
                                       << "ADDLOCAL=FeatureMain,FeatureEnvironment,FeatureJarFileRunWith,FeatureJavaHome"
                                       << "INSTALLTYPE=Typical");

    connect(m_javaInstallProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JdtlsInstaller::onJavaInstallationFinished);

    m_javaInstallProcess->start();

    // Ждем завершения с прогрессом
    QTimer progressTimer;
    int progress = 50;
    connect(&progressTimer, &QTimer::timeout, [this, &progress]() {
        if (progress < 90) {
            progress += 2;
            emit progressChanged(progress, "Installing Java...");
        }
    });
    progressTimer.start(2000);

    if (!m_javaInstallProcess->waitForFinished(300000)) { // 5 минут
        progressTimer.stop();
        emit finished(Failed, "Java installation timeout");
        return Failed;
    }

    progressTimer.stop();
    return m_javaInstallProcess->exitCode() == 0 ? Success : Failed;
}

LSPInstaller::InstallResult JdtlsInstaller::installJavaLinux()
{
    emit progressChanged(10, "Detecting Linux distribution...");

    // Определяем дистрибутив
    QString distro = "ubuntu"; // по умолчанию

    QProcess lsbProcess;
    lsbProcess.start("lsb_release", QStringList() << "-i" << "-s");
    if (lsbProcess.waitForFinished(3000)) {
        distro = QString::fromUtf8(lsbProcess.readAllStandardOutput()).trimmed().toLower();
    }

    emit logMessage("Detected Linux distribution: " + distro);

    // Команда установки в зависимости от дистрибутива
    QString command;
    QStringList args;

    if (distro.contains("ubuntu") || distro.contains("debian")) {
        emit progressChanged(20, "Installing OpenJDK 17 via apt...");

        // Добавляем репозиторий если нужно
        QProcess addRepo;
        addRepo.start("sudo", QStringList() << "apt-get" << "update");
        addRepo.waitForFinished(30000);

        command = "sudo";
        args << "apt-get" << "install" << "-y" << "openjdk-17-jdk";
    }
    else if (distro.contains("fedora") || distro.contains("centos") || distro.contains("rhel")) {
        emit progressChanged(20, "Installing OpenJDK 17 via dnf...");
        command = "sudo";
        args << "dnf" << "install" << "-y" << "java-17-openjdk-devel";
    }
    else if (distro.contains("arch") || distro.contains("manjaro")) {
        emit progressChanged(20, "Installing OpenJDK 17 via pacman...");
        command = "sudo";
        args << "pacman" << "-S" << "--noconfirm" << "jdk17-openjdk";
    }
    else {
        // Общий случай
        command = "sudo";
        args << "sh" << "-c"
             << "\"which apt-get && (apt-get update && apt-get install -y openjdk-17-jdk) || "
                "which dnf && dnf install -y java-17-openjdk-devel || "
                "which pacman && pacman -S --noconfirm jdk17-openjdk || "
                "which zypper && zypper install -y java-17-openjdk-devel\"";
    }

    emit logMessage("Running: " + command + " " + args.join(" "));

    QProcess installProcess;
    installProcess.start(command, args);

    // Показываем прогресс
    QTimer progressTimer;
    int progress = 20;
    connect(&progressTimer, &QTimer::timeout, [this, &progress]() {
        if (progress < 80) {
            progress += 5;
            emit progressChanged(progress, "Installing Java...");
        }
    });
    progressTimer.start(5000);

    if (!installProcess.waitForFinished(300000)) { // 5 минут
        progressTimer.stop();
        emit finished(Failed, "Java installation timeout");
        return Failed;
    }

    progressTimer.stop();

    if (installProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(installProcess.readAllStandardError());
        emit finished(Failed, QString("Java installation failed. Code: %1\n%2")
                                  .arg(installProcess.exitCode())
                                  .arg(error));
        return Failed;
    }

    // Устанавливаем JAVA_HOME
    emit progressChanged(90, "Setting up JAVA_HOME...");

    // Ищем установленную Java
    QProcess findJava;
    findJava.start("update-alternatives", QStringList() << "--list" << "java");
    if (findJava.waitForFinished(5000)) {
        QString javaPath = QString::fromUtf8(findJava.readAllStandardOutput()).split('\n').first().trimmed();
        if (!javaPath.isEmpty()) {
            QFileInfo javaInfo(javaPath);
            m_javaHome = javaInfo.dir().absolutePath() + "/..";

            // Устанавливаем переменную окружения
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("JAVA_HOME", m_javaHome);

            // Добавляем в .bashrc или .profile
            QString homeDir = QDir::homePath();
            QString shellConfig = homeDir + "/.bashrc";

            QFile configFile(shellConfig);
            if (configFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&configFile);
                out << "\n# Java Home set by JDTLS Installer\n";
                out << "export JAVA_HOME=\"" << m_javaHome << "\"\n";
                out << "export PATH=\"$JAVA_HOME/bin:$PATH\"\n";
                configFile.close();
            }

            emit logMessage("Java installed at: " + m_javaHome);
        }
    }

    emit progressChanged(100, "Java installation completed");
    return Success;
}

LSPInstaller::InstallResult JdtlsInstaller::installJavaMacOS()
{
    emit progressChanged(10, "Checking for Homebrew...");

    // Проверяем Homebrew
    QProcess brewCheck;
    brewCheck.start("brew", QStringList() << "--version");
    bool hasBrew = brewCheck.waitForFinished(3000) && brewCheck.exitCode() == 0;

    if (!hasBrew) {
        emit progressChanged(20, "Installing Homebrew...");

        // Устанавливаем Homebrew
        QProcess brewInstall;
        brewInstall.start("/bin/bash", QStringList()
                                           << "-c"
                                           << "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)");

        if (!brewInstall.waitForFinished(300000)) { // 5 минут
            emit finished(Failed, "Homebrew installation timeout");
            return Failed;
        }

        if (brewInstall.exitCode() != 0) {
            emit finished(Failed, "Homebrew installation failed");
            return Failed;
        }
    }

    emit progressChanged(50, "Installing OpenJDK via Homebrew...");

    QProcess brewProcess;
    brewProcess.start("brew", QStringList() << "install" << "--cask" << "temurin");

    QTimer progressTimer;
    int progress = 50;
    connect(&progressTimer, &QTimer::timeout, [this, &progress]() {
        if (progress < 90) {
            progress += 5;
            emit progressChanged(progress, "Installing Java...");
        }
    });
    progressTimer.start(5000);

    if (!brewProcess.waitForFinished(300000)) {
        progressTimer.stop();
        emit finished(Failed, "Java installation timeout");
        return Failed;
    }

    progressTimer.stop();

    if (brewProcess.exitCode() != 0) {
        emit finished(Failed, "Java installation failed via Homebrew");
        return Failed;
    }

    // На macOS Java обычно устанавливается в /Library/Java/JavaVirtualMachines
    QString javaHome = "/Library/Java/JavaVirtualMachines/temurin-17.jdk/Contents/Home";
    if (QDir(javaHome).exists()) {
        m_javaHome = javaHome;

        // Устанавливаем переменные окружения
        QProcessEnvironment::systemEnvironment().insert("JAVA_HOME", javaHome);

        // Добавляем в shell конфигурацию
        QString shellConfig = QDir::homePath() + "/.zshrc"; // macOS использует zsh по умолчанию
        QFile configFile(shellConfig);
        if (configFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&configFile);
            out << "\n# Java Home set by JDTLS Installer\n";
            out << "export JAVA_HOME=\"" << javaHome << "\"\n";
            out << "export PATH=\"$JAVA_HOME/bin:$PATH\"\n";
            configFile.close();
        }
    }

    emit progressChanged(100, "Java installation completed");
    return Success;
}

void JdtlsInstaller::onJavaInstallationFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);

    if (exitCode == 0) {
        // Успешная установка, устанавливаем JAVA_HOME
        QString javaHome = "C:\\Program Files\\Eclipse Adoptium\\jdk-17.0.10.7-hotspot";

        // Устанавливаем переменную окружения (для текущего процесса)
        QProcessEnvironment::systemEnvironment().insert("JAVA_HOME", javaHome);

        // Также устанавливаем в реестр для будущих сессий
#ifdef Q_OS_WIN
        QSettings envSettings("HKEY_CURRENT_USER\\Environment", QSettings::NativeFormat);
        envSettings.setValue("JAVA_HOME", javaHome);

        // Добавляем в PATH
        QString currentPath = envSettings.value("Path", "").toString();
        QString newPath = javaHome + "\\bin;" + currentPath;
        envSettings.setValue("Path", newPath);

        // Уведомляем систему об изменениях
        SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                           (LPARAM)"Environment", SMTO_ABORTIFHUNG,
                           5000, nullptr);
#endif

        m_javaHome = javaHome;
        emit javaInstallationResult(Success, "Java JDK successfully installed at " + javaHome);
        emit javaCheckCompleted(true, javaHome);
    } else {
        QString error = "Java installation failed with exit code: " + QString::number(exitCode);
        emit javaInstallationResult(Failed, error);
        emit javaCheckCompleted(false, "");
    }

    if (m_javaInstallProcess) {
        m_javaInstallProcess->deleteLater();
        m_javaInstallProcess = nullptr;
    }
}

void JdtlsInstaller::onExtractFinished(int exitCode)
{
    if (m_extractProcess) {
        if (exitCode == 0) {
            emit logMessage("Archive extracted successfully");
        } else {
            QString error = QString::fromUtf8(m_extractProcess->readAllStandardError());
            emit logMessage("Extraction failed. Error: " + error);
        }

        m_extractProcess->deleteLater();
        m_extractProcess = nullptr;
    }
}

void JdtlsInstaller::checkAndInstall()
{
    // Проверяем Java
    if (!checkJavaInstallation()) {
        emit logMessage("Java not found, installing...");
        installJavaJDK();
        return;
    }

    // Проверяем JDT LS
    if (isInstalled()) {
        emit logMessage("JDT LS already installed");
        emit finished(AlreadyInstalled, "JDT LS already installed at " + getInstallPath());
        return;
    }

    // Если всё проверено, запускаем установку
    install();
}

bool JdtlsInstaller::extractArchive(const QString& archivePath, const QString& targetDir)
{
    emit logMessage("Extracting archive: " + archivePath + " to " + targetDir);

    if (!QFileInfo::exists(archivePath)) {
        emit logMessage("Archive file does not exist: " + archivePath);
        return false;
    }

    QDir target(targetDir);
    if (!target.exists()) {
        if (!target.mkpath(".")) {
            emit logMessage("Failed to create target directory: " + targetDir);
            return false;
        }
    }

    QString program;
    QStringList args;
    bool useTar = false;

    // Определяем команду в зависимости от типа архива
    if (archivePath.endsWith(".tar.gz") || archivePath.endsWith(".tgz")) {
#ifdef Q_OS_WIN
        // Проверяем наличие tar
        QProcess checkTar;
        checkTar.start("tar", {"--version"});
        if (checkTar.waitForFinished(3000) && checkTar.exitCode() == 0) {
            program = "tar";
            args << "-xzf" << archivePath << "-C" << targetDir;
            useTar = true;
        } else {
            // Используем 7-zip
            program = "7z";
            args << "x" << archivePath << "-o" + targetDir << "-y" << "-tgz";
        }
#else
        program = "tar";
        args << "-xzf" << archivePath << "-C" << targetDir;
        useTar = true;
#endif
    }
    else if (archivePath.endsWith(".zip")) {
#ifdef Q_OS_WIN
        program = "powershell";
        args << "-Command"
             << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                    .arg(archivePath).arg(targetDir);
#else
        program = "unzip";
        args << "-o" << archivePath << "-d" << targetDir;
#endif
    }
    else {
        emit logMessage("Unsupported archive format: " + archivePath);
        return false;
    }

    // Проверяем наличие программы
    QProcess checkProcess;
    checkProcess.start(program, {"--version"});
    if (!checkProcess.waitForFinished(3000)) {
        emit logMessage("Extraction program not found: " + program);
        return false;
    }

    emit logMessage("Running: " + program + " " + args.join(" "));

    m_extractProcess = new QProcess(this);
    m_extractProcess->setProgram(program);
    m_extractProcess->setArguments(args);

    // Для tar на Windows устанавливаем кодировку
#ifdef Q_OS_WIN
    if (useTar) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LC_ALL", "C.UTF-8");
        env.insert("LANG", "C.UTF-8");
        m_extractProcess->setProcessEnvironment(env);
    }
#endif

    connect(m_extractProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JdtlsInstaller::onExtractFinished);

    m_extractProcess->start();

    // Ждем завершения
    if (!m_extractProcess->waitForFinished(300000)) { // 5 минут
        emit logMessage("Extraction timeout");
        m_extractProcess->kill();
        return false;
    }

    return m_extractProcess->exitCode() == 0;
}

void JdtlsInstaller::startDownloadFromGitHub()
{
    // Сначала получаем информацию о релизах
    QNetworkRequest request(QUrl("https://api.github.com/repos/eclipse-jdtls/eclipse.jdt.ls/releases/latest"));
    request.setRawHeader("User-Agent", "Qt-LSP-Client/1.0");

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit finished(Failed, "GitHub API error: " + reply->errorString());
            cleanup();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject release = doc.object();
        QJsonArray assets = release["assets"].toArray();

        // Ищем архив для скачивания
        QString downloadUrl;
        for (const QJsonValue& asset : assets) {
            QString name = asset.toObject()["name"].toString();
            if (name.contains("tar.gz") || name.contains("zip")) {
                downloadUrl = asset.toObject()["browser_download_url"].toString();
                break;
            }
        }

        if (downloadUrl.isEmpty()) {
            emit finished(Failed, "No download found in release");
            cleanup();
            return;
        }

        // Начинаем скачивание
        QString tempFile = QDir::tempPath() + "/jdtls_download.tar.gz";
        m_downloadFile = new QFile(tempFile);

        if (!m_downloadFile->open(QIODevice::WriteOnly)) {
            emit finished(Failed, "Cannot create temp file");
            cleanup();
            return;
        }

        QNetworkRequest downloadRequest(downloadUrl);
        m_currentReply = m_networkManager->get(downloadRequest);

        connect(m_currentReply, &QNetworkReply::downloadProgress,
                this, &JdtlsInstaller::onDownloadProgress);
        connect(m_currentReply, &QNetworkReply::finished,
                this, &JdtlsInstaller::onDownloadFinished);
        connect(m_currentReply, &QNetworkReply::readyRead, [this]() {
            if (m_downloadFile) m_downloadFile->write(m_currentReply->readAll());
        });
    });
}
