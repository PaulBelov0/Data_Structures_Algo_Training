#include "clangd_installer.h"
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
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

ClangdInstaller::ClangdInstaller(QObject *parent)
    : LSPInstaller(parent)
    , m_version("17.0.0") // Стандартная версия по умолчанию
{
    // Устанавливаем путь установки
    m_installDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/clangd";
    QDir().mkpath(m_installDir);

    emit logMessage("ClangdInstaller initialized. Install dir: " + m_installDir);
}

// Основной метод установки
LSPInstaller::InstallResult ClangdInstaller::install()
{
    emit progressChanged(0, "Начинаем установку Clangd...");
    emit logMessage("Starting Clangd installation...");

    // Проверяем, не установлен ли уже
    if (isInstalled()) {
        emit logMessage("Clangd already installed, checking version...");

        QString existingPath = getInstallPath();
        QString existingVersion = getDetectedVersion();

        QMessageBox msgBox;
        msgBox.setWindowTitle("Clangd уже установлен");
        msgBox.setText("Clangd уже установлен в системе.");
        msgBox.setInformativeText(QString("Текущая версия: %1\nПуть: %2\n\nХотите переустановить?")
                                      .arg(existingVersion).arg(existingPath));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::No);

        int result = msgBox.exec();

        if (result == QMessageBox::Cancel) {
            emit logMessage("Installation cancelled by user");
            return Cancelled;
        }

        if (result == QMessageBox::No) {
            emit logMessage("Using existing Clangd installation");
            return AlreadyInstalled;
        }

        // Если пользователь хочет переустановить, продолжаем
        emit logMessage("User chose to reinstall Clangd");
    }

    // Показываем информацию об установке
    QMessageBox infoBox;
    infoBox.setWindowTitle("Установка Clangd");
    infoBox.setText("Установить Language Server для C++ (clangd)?");
    infoBox.setInformativeText("Это потребует:\n"
                               "• ~200-300 МБ дискового пространства\n"
                               "• Доступ в интернет для загрузки\n"
                               "• Права администратора (на Windows)\n\n"
                               "Clangd будет установлен в:\n" + m_installDir);
    infoBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    infoBox.setDefaultButton(QMessageBox::Ok);

    if (infoBox.exec() != QMessageBox::Ok) {
        emit logMessage("Installation cancelled by user at confirmation dialog");
        return Cancelled;
    }

    emit progressChanged(10, "Определяем операционную систему...");

#ifdef Q_OS_WIN
    return installWindows();
#elif defined(Q_OS_LINUX)
    return installLinux();
#elif defined(Q_OS_MACOS)
    return installMacOS();
#else
    emit finished(Failed, "Неподдерживаемая операционная система");
    return Failed;
#endif
}

// Проверка установлен ли Clangd
bool ClangdInstaller::isInstalled() const
{
    // Проверяем в сохраненных настройках
    QSettings settings;
    QString savedPath = settings.value("LSP/Cpp/ClangdPath", "").toString();

    if (!savedPath.isEmpty()) {
        if (QFileInfo::exists(savedPath)) {
            return true;
        }
    }

    // Ищем в PATH
    QString clangdPath = findInPath("clangd");
    if (!clangdPath.isEmpty()) {

        // Сохраняем найденный путь
        const_cast<QSettings&>(settings).setValue("LSP/Cpp/ClangdPath", clangdPath);
        const_cast<QSettings&>(settings).sync();

        return true;
    }

    // Ищем в стандартных местах
    QStringList possiblePaths;

#ifdef Q_OS_WIN
    possiblePaths << "C:\\Program Files\\LLVM\\bin\\clangd.exe"
                  << "C:\\Program Files (x86)\\LLVM\\bin\\clangd.exe"
                  << "C:\\msys64\\mingw64\\bin\\clangd.exe"
                  << "C:\\msys64\\clang64\\bin\\clangd.exe";
#elif defined(Q_OS_MACOS)
    possiblePaths << "/usr/local/bin/clangd"
                  << "/opt/homebrew/bin/clangd"
                  << "/usr/bin/clangd";
#else
    possiblePaths << "/usr/bin/clangd"
                  << "/usr/local/bin/clangd"
                  << "/opt/local/bin/clangd";
#endif

    for (const QString& path : possiblePaths) {
        if (QFileInfo::exists(path)) {

            // Сохраняем найденный путь
            const_cast<QSettings&>(settings).setValue("LSP/Cpp/ClangdPath", path);
            const_cast<QSettings&>(settings).sync();

            return true;
        }
    }

    return false;
}

// Получение пути установки
QString ClangdInstaller::getInstallPath() const
{
    QSettings settings;
    QString savedPath = settings.value("LSP/Cpp/ClangdPath", "").toString();

    if (!savedPath.isEmpty()) {
        return savedPath;
    }

    // Ищем в PATH
    QString clangdPath = findInPath("clangd");
    if (!clangdPath.isEmpty()) {
        return clangdPath;
    }

    return QString();
}

// Получение обнаруженной версии
QString ClangdInstaller::getDetectedVersion() const
{
    QString clangdPath = getInstallPath();
    if (clangdPath.isEmpty()) {
        return "Not installed";
    }

    QProcess process;
    process.start(clangdPath, QStringList() << "--version");

    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        if (output.isEmpty()) {
            output = QString::fromUtf8(process.readAllStandardError());
        }

        // Используем QRegularExpression вместо QRegExp
        QRegularExpression versionRegex("clangd version (\\d+\\.\\d+\\.\\d+)");
        QRegularExpressionMatch match = versionRegex.match(output);

        if (match.hasMatch()) {
            return match.captured(1);
        }

        // Альтернативный паттерн, если первый не сработал
        QRegularExpression altVersionRegex("version (\\d+\\.\\d+\\.\\d+)");
        match = altVersionRegex.match(output);

        if (match.hasMatch()) {
            return match.captured(1);
        }

        // Еще один вариант
        QRegularExpression simpleVersionRegex("(\\d+\\.\\d+\\.\\d+)");
        match = simpleVersionRegex.match(output);

        if (match.hasMatch()) {
            return match.captured(1);
        }

        return "Unknown (output: " + output.left(50).trimmed() + "...)";
    }

    return "Unknown (failed to execute)";
}

// Получение последней версии (упрощенная реализация)
QString ClangdInstaller::getLatestVersion() const
{
    // В реальном приложении здесь бы был запрос к API GitHub
    // Пока возвращаем фиксированную версию
    return "17.0.0";
}

// Установка на Windows
LSPInstaller::InstallResult ClangdInstaller::installWindows()
{
    emit progressChanged(20, "Получаем информацию о последней версии...");

    QString downloadUrl = getWindowsDownloadUrl();
    if (downloadUrl.isEmpty()) {
        emit finished(Failed, "Не удалось получить URL для загрузки LLVM");
        return Failed;
    }

    emit logMessage("Download URL: " + downloadUrl);

    QString tempFile = getTempFilePath("llvm_installer_");
    if (tempFile.isEmpty()) {
        emit finished(Failed, "Не удалось создать временный файл");
        return Failed;
    }

    emit progressChanged(30, QString("Скачиваем установщик LLVM (%1)...").arg(m_version));
    emit logMessage("Downloading to: " + tempFile);

    // Скачиваем установщик
    QString downloadedFile = downloadFile(QUrl(downloadUrl), tempFile);
    if (downloadedFile.isEmpty()) {
        emit finished(Failed, "Ошибка загрузки установщика");
        return Failed;
    }

    emit progressChanged(60, "Запускаем установщик...");
    emit logMessage("Starting installer: " + downloadedFile);

    // Создаем скрипт для автоматической установки
    QString scriptFile = QDir::tempPath() + "/install_clangd.bat";
    QFile batFile(scriptFile);
    if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&batFile);
        out << "@echo off\n";
        out << "echo Installing Clangd (LLVM)...\n";
        out << "timeout /t 2 /nobreak >nul\n";
        out << "\"" << downloadedFile << "\" /S /D=" << m_installDir << "\n";
        out << "if %errorlevel% equ 0 (\n";
        out << "  echo Installation successful\n";
        out << "  pause\n";
        out << ") else (\n";
        out << "  echo Installation failed with error %errorlevel%\n";
        out << "  pause\n";
        out << ")\n";
        batFile.close();
    }

    // Запускаем установщик
    QProcess installer;
    installer.setProgram("cmd.exe");
    installer.setArguments(QStringList() << "/c" << scriptFile);

    // Показываем прогресс установки
    QTimer progressTimer;
    int progressValue = 60;
    connect(&progressTimer, &QTimer::timeout, [this, &progressValue]() {
        if (progressValue < 90) {
            progressValue += 2;
            emit progressChanged(progressValue, "Установка Clangd...");
        }
    });

    progressTimer.start(1000);

    installer.start();

    // Ждем завершения
    if (!installer.waitForFinished(300000)) { // 5 минут
        progressTimer.stop();
        emit finished(Failed, "Таймаут установки (5 минут)");
        return Failed;
    }

    progressTimer.stop();

    if (installer.exitCode() != 0) {
        QString error = QString::fromUtf8(installer.readAllStandardError());
        emit finished(Failed, QString("Ошибка установки. Код: %1\n%2")
                                  .arg(installer.exitCode())
                                  .arg(error));
        return Failed;
    }

    emit progressChanged(95, "Настраиваем окружение...");

    // Добавляем в PATH
    QString binDir = m_installDir + "/bin";
    if (addToPath(binDir)) {
        emit logMessage("Added to PATH: " + binDir);
    }

    // Проверяем установку
    if (verifyInstallation()) {
        // Сохраняем путь в настройках
        QSettings settings;
        QString clangdPath = binDir + "/clangd.exe";
        settings.setValue("LSP/Cpp/ClangdPath", clangdPath);
        settings.setValue("LSP/Cpp/InstallDir", m_installDir);
        settings.setValue("LSP/Cpp/Version", m_version);
        settings.sync();

        emit progressChanged(100, "Установка завершена!");
        emit logMessage("Clangd successfully installed at: " + clangdPath);
        emit finished(Success, "Clangd успешно установлен в " + m_installDir);

        // Удаляем временные файлы
        QFile::remove(downloadedFile);
        QFile::remove(scriptFile);

        return Success;
    } else {
        emit finished(Failed, "Установка завершена, но clangd не найден");
        return Failed;
    }
}

// Установка на Linux
LSPInstaller::InstallResult ClangdInstaller::installLinux()
{
    emit progressChanged(20, "Определяем дистрибутив Linux...");

    // Определяем дистрибутив
    QString distro = "unknown";
    QProcess lsbProcess;
    lsbProcess.start("lsb_release", QStringList() << "-i" << "-s");
    if (lsbProcess.waitForFinished(3000)) {
        distro = QString::fromUtf8(lsbProcess.readAllStandardOutput()).trimmed().toLower();
    }

    emit logMessage("Detected Linux distribution: " + distro);

    // Проверяем права sudo
    QProcess sudoCheck;
    sudoCheck.start("sudo", QStringList() << "-n" << "true");
    bool hasSudo = sudoCheck.waitForFinished(3000) && sudoCheck.exitCode() == 0;

    if (!hasSudo) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Требуются права администратора");
        msgBox.setText("Для установки clangd требуются права администратора.");
        msgBox.setInformativeText("Установить через пакетный менеджер?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);

        if (msgBox.exec() != QMessageBox::Yes) {
            emit finished(Failed, "Требуются права администратора для установки");
            return Failed;
        }

        // Запрашиваем пароль через графический интерфейс
        QProcess pkexecProcess;
        pkexecProcess.start("pkexec", QStringList() << "--version");
        hasSudo = pkexecProcess.waitForFinished(3000) && pkexecProcess.exitCode() == 0;
    }

    emit progressChanged(40, "Устанавливаем clangd через пакетный менеджер...");

    QString packageName = "clangd";

    // Для разных дистрибутивов разные команды
    QString command;
    QStringList args;

    if (distro.contains("ubuntu") || distro.contains("debian")) {
        command = hasSudo ? "sudo" : "pkexec";
        args << "apt-get" << "update" << "&&" << command << "apt-get" << "install" << "-y"
             << "clangd-17" << "||" << command << "apt-get" << "install" << "-y" << "clangd";
    } else if (distro.contains("fedora") || distro.contains("centos") || distro.contains("rhel")) {
        command = hasSudo ? "sudo" : "pkexec";
        args << "dnf" << "install" << "-y" << "clang-tools-extra";
    } else if (distro.contains("arch") || distro.contains("manjaro")) {
        command = hasSudo ? "sudo" : "pkexec";
        args << "pacman" << "-S" << "--noconfirm" << "clang";
    } else {
        // Общий случай
        command = hasSudo ? "sudo" : "pkexec";
        args << "sh" << "-c" << "\"which apt-get && (apt-get update && apt-get install -y clangd) || "
                                "which dnf && dnf install -y clang-tools-extra || "
                                "which pacman && pacman -S --noconfirm clang || "
                                "which zypper && zypper install -y clang-tools-extra\"";
    }

    emit logMessage("Running: " + command + " " + args.join(" "));

    QProcess installProcess;
    installProcess.start(command, args);

    // Показываем прогресс
    QTimer progressTimer;
    int progressValue = 40;
    connect(&progressTimer, &QTimer::timeout, [this, &progressValue]() {
        if (progressValue < 90) {
            progressValue += 5;
            emit progressChanged(progressValue, "Установка clangd...");
        }
    });

    progressTimer.start(2000);

    if (!installProcess.waitForFinished(300000)) { // 5 минут
        progressTimer.stop();
        emit finished(Failed, "Таймаут установки");
        return Failed;
    }

    progressTimer.stop();

    if (installProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(installProcess.readAllStandardError());
        emit finished(Failed, QString("Ошибка установки. Код: %1\n%2")
                                  .arg(installProcess.exitCode())
                                  .arg(error));
        return Failed;
    }

    emit progressChanged(95, "Проверяем установку...");

    // Проверяем установку
    if (verifyInstallation()) {
        QString clangdPath = findInPath("clangd");

        // Сохраняем в настройках
        QSettings settings;
        settings.setValue("LSP/Cpp/ClangdPath", clangdPath);
        settings.setValue("LSP/Cpp/Version", getDetectedVersion());
        settings.sync();

        emit progressChanged(100, "Установка завершена!");
        emit logMessage("Clangd successfully installed: " + clangdPath);
        emit finished(Success, "Clangd успешно установлен через пакетный менеджер");
        return Success;
    } else {
        // Пробуем установить через LLVM
        return installLinuxViaLLVM();
    }
}

// Альтернативная установка через LLVM на Linux
LSPInstaller::InstallResult ClangdInstaller::installLinuxViaLLVM()
{
    emit progressChanged(50, "Пробуем установить через LLVM...");

    QString downloadUrl = "https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.0/clang+llvm-17.0.0-x86_64-linux-gnu-ubuntu-22.04.tar.xz";
    QString tempFile = getTempFilePath("llvm_linux_");

    emit progressChanged(60, "Скачиваем LLVM для Linux...");

    QString downloadedFile = downloadFile(QUrl(downloadUrl), tempFile);
    if (downloadedFile.isEmpty()) {
        emit finished(Failed, "Ошибка загрузки LLVM");
        return Failed;
    }

    emit progressChanged(80, "Распаковываем LLVM...");

    QString extractDir = m_installDir + "/llvm";
    if (!extractArchive(downloadedFile, extractDir)) {
        emit finished(Failed, "Ошибка распаковки LLVM");
        return Failed;
    }

    // Ищем clangd в распакованной директории
    QDir llvmDir(extractDir);
    QStringList clangdFiles = llvmDir.entryList(QStringList() << "*/bin/clangd", QDir::Files, QDir::Name);

    if (clangdFiles.isEmpty()) {
        // Ищем рекурсивно
        QDirIterator it(extractDir, QStringList() << "clangd", QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            QString clangdPath = it.next();

            // Добавляем в PATH
            QFileInfo clangdInfo(clangdPath);
            QString binDir = clangdInfo.dir().absolutePath();

            if (addToPath(binDir)) {
                emit logMessage("Added to PATH: " + binDir);
            }

            // Сохраняем в настройках
            QSettings settings;
            settings.setValue("LSP/Cpp/ClangdPath", clangdPath);
            settings.setValue("LSP/Cpp/InstallDir", extractDir);
            settings.setValue("LSP/Cpp/Version", m_version);
            settings.sync();

            emit progressChanged(100, "Установка завершена!");
            emit finished(Success, "Clangd установлен через LLVM");
            return Success;
        }
    }

    emit finished(Failed, "Не удалось найти clangd в распакованных файлах");
    return Failed;
}

// Установка на macOS
LSPInstaller::InstallResult ClangdInstaller::installMacOS()
{
    emit progressChanged(20, "Проверяем наличие Homebrew...");

    // Проверяем Homebrew
    QProcess brewCheck;
    brewCheck.start("brew", QStringList() << "--version");
    bool hasBrew = brewCheck.waitForFinished(3000) && brewCheck.exitCode() == 0;

    if (!hasBrew) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Требуется Homebrew");
        msgBox.setText("Для установки clangd требуется Homebrew.");
        msgBox.setInformativeText("Установить Homebrew автоматически?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);

        if (msgBox.exec() == QMessageBox::Yes) {
            emit progressChanged(30, "Устанавливаем Homebrew...");

            // Установка Homebrew
            QProcess brewInstall;
            brewInstall.start("/bin/bash", QStringList()
                                               << "-c" << "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)");

            if (!brewInstall.waitForFinished(300000)) { // 5 минут
                emit finished(Failed, "Таймаут установки Homebrew");
                return Failed;
            }

            if (brewInstall.exitCode() != 0) {
                emit finished(Failed, "Ошибка установки Homebrew");
                return Failed;
            }

            hasBrew = true;
        } else {
            emit finished(Failed, "Требуется Homebrew для установки clangd");
            return Failed;
        }
    }

    emit progressChanged(50, "Устанавливаем clangd через Homebrew...");

    QProcess brewProcess;
    brewProcess.start("brew", QStringList() << "install" << "llvm");

    QTimer progressTimer;
    int progressValue = 50;
    connect(&progressTimer, &QTimer::timeout, [this, &progressValue]() {
        if (progressValue < 90) {
            progressValue += 5;
            emit progressChanged(progressValue, "Установка llvm через Homebrew...");
        }
    });

    progressTimer.start(3000);

    if (!brewProcess.waitForFinished(300000)) { // 5 минут
        progressTimer.stop();
        emit finished(Failed, "Таймаут установки через Homebrew");
        return Failed;
    }

    progressTimer.stop();

    if (brewProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(brewProcess.readAllStandardError());
        emit finished(Failed, QString("Ошибка установки. Код: %1\n%2")
                                  .arg(brewProcess.exitCode())
                                  .arg(error));
        return Failed;
    }

    emit progressChanged(95, "Настраиваем окружение...");

    // Путь к clangd после установки через Homebrew
    QString clangdPath = "/opt/homebrew/opt/llvm/bin/clangd";
    if (!QFileInfo::exists(clangdPath)) {
        clangdPath = "/usr/local/opt/llvm/bin/clangd";
    }

    // Добавляем в PATH
    QString binDir = QFileInfo(clangdPath).dir().absolutePath();
    if (addToPath(binDir)) {
        emit logMessage("Added to PATH: " + binDir);
    }

    if (verifyInstallation()) {
        // Сохраняем в настройках
        QSettings settings;
        settings.setValue("LSP/Cpp/ClangdPath", clangdPath);
        settings.setValue("LSP/Cpp/Version", getDetectedVersion());
        settings.sync();

        emit progressChanged(100, "Установка завершена!");
        emit finished(Success, "Clangd успешно установлен через Homebrew");
        return Success;
    }

    emit finished(Failed, "Установка завершена, но clangd не найден");
    return Failed;
}

// URL для загрузки на Windows
QString ClangdInstaller::getWindowsDownloadUrl() const
{
    // Определяем архитектуру
    QString arch = "x64";

    // Определяем разрядность системы
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
        arch = "arm64";
    } else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        // Уже x64 по умолчанию
    }

    // Формируем URL
    return QString("https://github.com/llvm/llvm-project/releases/download/llvmorg-%1/LLVM-%1-win%s.exe")
        .arg(m_version).arg(arch);
}

// Проверка установки
bool ClangdInstaller::verifyInstallation() const
{
    QString clangdPath;

    // Сначала проверяем в установочной директории
#ifdef Q_OS_WIN
    clangdPath = m_installDir + "/bin/clangd.exe";
#else
    clangdPath = m_installDir + "/bin/clangd";
#endif

    if (QFileInfo::exists(clangdPath)) {
        return true;
    }

    // Проверяем в PATH
    return !findInPath("clangd").isEmpty();
}

// Информация о системе
QString ClangdInstaller::getSystemInfo()
{
    QString info;

#ifdef Q_OS_WIN
    info = "Windows ";

    // Версия Windows
    QSettings winReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", QSettings::NativeFormat);
    QString productName = winReg.value("ProductName", "Unknown").toString();
    QString currentBuild = winReg.value("CurrentBuild", "").toString();

    info += productName + " (Build " + currentBuild + ")";

    // Архитектура
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        info += " x64";
    } else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
        info += " ARM64";
    } else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        info += " x86";
    }

#elif defined(Q_OS_MACOS)
    QProcess uname;
    uname.start("uname", QStringList() << "-a");
    if (uname.waitForFinished(3000)) {
        info = QString::fromUtf8(uname.readAllStandardOutput()).trimmed();
    } else {
        info = "macOS";
    }

#else
    QProcess uname;
    uname.start("uname", QStringList() << "-a");
    if (uname.waitForFinished(3000)) {
        info = QString::fromUtf8(uname.readAllStandardOutput()).trimmed();
    } else {
        info = "Linux";
    }
#endif

    return info;
}

// Проверка требований
bool ClangdInstaller::checkSystemRequirements(QString& errorMessage)
{
    // Проверяем дисковое пространство
    QString tempDir = QDir::tempPath();
    QStorageInfo storage(tempDir);

    qint64 availableSpace = storage.bytesAvailable();
    qint64 requiredSpace = 500 * 1024 * 1024; // 500 МБ

    if (availableSpace < requiredSpace) {
        errorMessage = QString("Недостаточно места на диске. Требуется: %1 МБ, доступно: %2 МБ")
                           .arg(requiredSpace / (1024 * 1024))
                           .arg(availableSpace / (1024 * 1024));
        return false;
    }

    // Для Windows проверяем версию
#ifdef Q_OS_WIN
    QSettings winReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", QSettings::NativeFormat);
    int majorVersion = winReg.value("CurrentMajorVersionNumber", 0).toInt();
    int minorVersion = winReg.value("CurrentMinorVersionNumber", 0).toInt();

    if (majorVersion < 10) {
        errorMessage = "Требуется Windows 10 или новее";
        return false;
    }
#endif

    return true;
}

// Метод для быстрой проверки и установки
LSPInstaller::InstallResult ClangdInstaller::checkAndInstall(QWidget* parent)
{
    ClangdInstaller installer;

    if (installer.isInstalled()) {
        return AlreadyInstalled;
    }

    // Проверяем требования
    QString errorMsg;
    if (!checkSystemRequirements(errorMsg)) {
        if (parent) {
            QMessageBox::critical(parent, "System Requirements Not Met", errorMsg);
        }
        return Failed;
    }

    return installer.install();
}

// Получение пути с автоматической установкой
QString ClangdInstaller::getClangdPathWithAutoInstall(bool autoInstall)
{
    ClangdInstaller installer;

    if (installer.isInstalled()) {
        return installer.getInstallPath();
    }

    if (autoInstall) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Clangd Not Found");
        msgBox.setText("Clangd (C++ Language Server) not found.");
        msgBox.setInformativeText("Install automatically?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);

        if (msgBox.exec() == QMessageBox::Yes) {
            if (installer.install() == Success) {
                return installer.getInstallPath();
            }
        }
    }

    return QString();
}
