#include "lsp_installer.h"
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#else
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

LSPInstaller::LSPInstaller(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

// Метод для скачивания файла с прогрессом
QString LSPInstaller::downloadFile(const QUrl& url, const QString& destPath)
{
    if (url.isEmpty() || destPath.isEmpty()) {
        emit logMessage("Ошибка: пустой URL или путь назначения");
        return QString();
    }

    emit logMessage(QString("Начинаем загрузку: %1 -> %2").arg(url.toString(), destPath));

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Qt-LSP-Client/1.0");

    QFile* file = new QFile(destPath);
    if (!file->open(QIODevice::WriteOnly)) {
        emit logMessage("Ошибка: не удалось открыть файл для записи: " + destPath);
        delete file;
        return QString();
    }

    QNetworkReply* reply = m_networkManager->get(request);

    // Подключаем сигналы прогресса
    connect(reply, &QNetworkReply::downloadProgress,
            [this](qint64 bytesReceived, qint64 bytesTotal) {
                emit downloadProgress(bytesReceived, bytesTotal);

                if (bytesTotal > 0) {
                    int percent = (bytesReceived * 100) / bytesTotal;
                    emit progressChanged(percent,
                                         QString("Загрузка: %1% (%2/%3 MB)")
                                             .arg(percent)
                                             .arg(bytesReceived / (1024 * 1024))
                                             .arg(bytesTotal / (1024 * 1024)));
                }
            });

    // Обработка данных
    connect(reply, &QNetworkReply::readyRead, [reply, file]() {
        if (file->isOpen()) {
            file->write(reply->readAll());
        }
    });

    // Обработка завершения
    QEventLoop loop;
    bool success = false;

    connect(reply, &QNetworkReply::finished, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            file->flush();
            success = true;
            emit logMessage("Загрузка завершена успешно");
        } else {
            emit logMessage("Ошибка загрузки: " + reply->errorString());
            file->remove(); // Удаляем частично скачанный файл
        }
        file->close();
        delete file;
        reply->deleteLater();
        loop.quit();
    });

    // Ждем завершения с таймаутом
    QTimer::singleShot(300000, &loop, &QEventLoop::quit); // 5 минут таймаут

    loop.exec();

    return success ? destPath : QString();
}

// Метод для распаковки архивов
bool LSPInstaller::extractArchive(const QString& archivePath, const QString& destDir)
{
    if (!QFileInfo::exists(archivePath)) {
        emit logMessage("Ошибка: архив не найден: " + archivePath);
        return false;
    }

    emit logMessage(QString("Распаковываем архив: %1 -> %2").arg(archivePath, destDir));

    QDir dir(destDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emit logMessage("Ошибка: не удалось создать директорию: " + destDir);
            return false;
        }
    }

    QString program;
    QStringList args;
    bool useTar = false;

    // Определяем команду в зависимости от типа архива и ОС
    if (archivePath.endsWith(".tar.gz") || archivePath.endsWith(".tgz")) {
#ifdef Q_OS_WIN
        // Проверяем наличие tar (из Git for Windows или MSYS2)
        QProcess checkTar;
        checkTar.start("tar", {"--version"});
        if (checkTar.waitForFinished(3000) && checkTar.exitCode() == 0) {
            program = "tar";
            args << "-xzf" << archivePath << "-C" << destDir;
            useTar = true;
        } else {
            // Используем 7-zip или другой архиватор
            program = "7z";
            args << "x" << archivePath << "-o" + destDir << "-y" << "-tgz";
        }
#else
        program = "tar";
        args << "-xzf" << archivePath << "-C" << destDir;
        useTar = true;
#endif
    } else if (archivePath.endsWith(".zip")) {
#ifdef Q_OS_WIN
        program = "powershell";
        args << "-Command"
             << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                    .arg(archivePath).arg(destDir);
#else
        program = "unzip";
        args << "-o" << archivePath << "-d" << destDir;
#endif
    } else if (archivePath.endsWith(".exe") || archivePath.endsWith(".msi")) {
        // Это установщик, не архив
        return true;
    } else {
        emit logMessage("Ошибка: неподдерживаемый формат архива: " + archivePath);
        return false;
    }

    // Проверяем наличие программы
    QProcess checkProcess;
    checkProcess.start(program, {"--version"});
    if (!checkProcess.waitForFinished(3000)) {
        // Пробуем alternative
        if (program == "tar") {
#ifdef Q_OS_WIN
            program = "7z";
            args = QStringList() << "x" << archivePath << "-o" + destDir << "-y";
#else
            emit logMessage("Ошибка: не найден архиватор tar/unzip");
            return false;
#endif
        }
    }

    emit logMessage(QString("Запускаем: %1 %2").arg(program, args.join(" ")));

    QProcess extractProcess;
    extractProcess.setProgram(program);
    extractProcess.setArguments(args);

    // Для tar на Windows может потребоваться установить кодировку
#ifdef Q_OS_WIN
    if (useTar) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LC_ALL", "C.UTF-8");
        env.insert("LANG", "C.UTF-8");
        extractProcess.setProcessEnvironment(env);
    }
#endif

    extractProcess.start();

    // Ждем завершения с прогрессом
    QEventLoop loop;
    bool extractionSuccess = false;

    connect(&extractProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [&](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus);
                if (exitCode == 0) {
                    extractionSuccess = true;
                    emit logMessage("Распаковка завершена успешно");
                } else {
                    QString error = QString::fromUtf8(extractProcess.readAllStandardError());
                    emit logMessage("Ошибка распаковки. Код: " + QString::number(exitCode));
                    emit logMessage("Stderr: " + error);
                }
                loop.quit();
            });

    // Показываем прогресс распаковки
    QTimer progressTimer;
    int progressCounter = 0;
    connect(&progressTimer, &QTimer::timeout, [&]() {
        progressCounter++;
        int progress = 50 + (progressCounter % 50); // От 50 до 100
        emit progressChanged(progress, "Распаковка...");
    });

    progressTimer.start(1000);

    // Таймаут 10 минут
    QTimer::singleShot(600000, &loop, &QEventLoop::quit);

    loop.exec();
    progressTimer.stop();

    return extractionSuccess;
}

// Метод для добавления директории в PATH (только для текущего пользователя)
bool LSPInstaller::addToPath(const QString& dirPath)
{
    if (!QDir(dirPath).exists()) {
        emit logMessage("Ошибка: директория не существует: " + dirPath);
        return false;
    }

    QString normalizedPath = QDir::toNativeSeparators(QDir(dirPath).absolutePath());

#ifdef Q_OS_WIN
    // Добавляем в PATH через реестр (для текущего пользователя)
    QSettings envSettings("HKEY_CURRENT_USER\\Environment", QSettings::NativeFormat);

    // Получаем текущий PATH
    QString currentPath = envSettings.value("Path", "").toString();

    // Проверяем, нет ли уже этой директории в PATH
    QStringList pathEntries = currentPath.split(';', Qt::SkipEmptyParts);
    for (const QString& entry : pathEntries) {
        if (QDir::fromNativeSeparators(entry).compare(QDir::fromNativeSeparators(normalizedPath),
                                                      Qt::CaseInsensitive) == 0) {
            emit logMessage("Директория уже в PATH: " + normalizedPath);
            return true;
        }
    }

    // Добавляем в начало PATH
    QString newPath = normalizedPath + ";" + currentPath;
    envSettings.setValue("Path", newPath);

    // Уведомляем систему об изменении переменных окружения
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                       (LPARAM)"Environment", SMTO_ABORTIFHUNG,
                       5000, nullptr);

    emit logMessage("Добавлено в PATH: " + normalizedPath);
    return true;

#else
    // Для Linux/macOS: добавляем в ~/.bashrc, ~/.zshrc или ~/.profile
    QString homeDir = QDir::homePath();
    QString shellConfig;

    // Определяем текущую оболочку
    QString shell = qgetenv("SHELL");
    if (shell.contains("zsh")) {
        shellConfig = homeDir + "/.zshrc";
    } else if (shell.contains("bash")) {
        shellConfig = homeDir + "/.bashrc";
    } else {
        shellConfig = homeDir + "/.profile";
    }

    // Проверяем, нет ли уже этой директории в PATH
    QFile configFile(shellConfig);
    if (configFile.exists() && configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&configFile);
        QString content = in.readAll();
        configFile.close();

        if (content.contains("PATH=\"" + normalizedPath + "\"") ||
            content.contains("PATH='" + normalizedPath + "'") ||
            content.contains("PATH=$PATH:" + normalizedPath)) {
            emit logMessage("Директория уже в PATH: " + normalizedPath);
            return true;
        }
    }

    // Добавляем в конфигурационный файл
    if (configFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&configFile);
        out << "\n# Added by Qt LSP Client\n";
        out << "export PATH=\"" << normalizedPath << ":$PATH\"\n";
        configFile.close();

        emit logMessage("Добавлено в " + shellConfig + ": " + normalizedPath);
        emit logMessage("Перезапустите терминал для применения изменений");
        return true;
    }
#endif

    emit logMessage("Предупреждение: не удалось добавить в PATH");
    return false;
}

// Поиск исполняемого файла в PATH
QString LSPInstaller::findInPath(const QString& executable) const
{
#ifdef Q_OS_WIN
    // На Windows используем where
    QProcess process;
    process.start("where", {executable});
#else
    // На Unix используем which
    QProcess process;
    process.start("which", {executable});
#endif

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            return lines.first();
        }
    }

    // Также проверяем стандартные места
    QStringList standardPaths;

#ifdef Q_OS_WIN
    if (executable.endsWith(".exe")) {
        standardPaths << QString("C:\\Program Files\\LLVM\\bin\\%1").arg(executable)
        << QString("C:\\Program Files (x86)\\LLVM\\bin\\%1").arg(executable)
        << QString("C:\\msys64\\mingw64\\bin\\%1").arg(executable);
    }
#else
    standardPaths << QString("/usr/bin/%1").arg(executable)
                  << QString("/usr/local/bin/%1").arg(executable)
                  << QString("/opt/homebrew/bin/%1").arg(executable);
#endif

    for (const QString& path : standardPaths) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }

    return QString();
}

bool LSPInstaller::runProcess(const QString& program, const QStringList& args,
                              const QString& workingDir, int timeoutMs)
{
    emit logMessage(QString("Запуск: %1 %2").arg(program, args.join(" ")));

    QProcess process;

    if (!workingDir.isEmpty()) {
        process.setWorkingDirectory(workingDir);
    }

    process.setProgram(program);
    process.setArguments(args);

    process.start();

    if (!process.waitForStarted(5000)) {
        emit logMessage("Ошибка: процесс не запустился: " + program);
        return false;
    }

    if (!process.waitForFinished(timeoutMs)) {
        emit logMessage("Ошибка: таймаут выполнения: " + program);
        process.kill();
        return false;
    }

    if (process.exitCode() != 0) {
        QString error = QString::fromUtf8(process.readAllStandardError());
        emit logMessage("Ошибка выполнения. Код: " + QString::number(process.exitCode()));
        if (!error.isEmpty()) {
            emit logMessage("Stderr: " + error);
        }
        return false;
    }

    return true;
}

// Получение временного файла
QString LSPInstaller::getTempFilePath(const QString& prefix)
{
    QString tempDir = QDir::tempPath();
    QString templateStr = prefix + "XXXXXX";

    QTemporaryFile tempFile(tempDir + "/" + templateStr);
    tempFile.setAutoRemove(false); // Не удалять автоматически

    if (tempFile.open()) {
        QString path = tempFile.fileName();
        tempFile.close();
        return path;
    }

    return QString();
}

// Базовая директория для установки
QString LSPInstaller::getInstallBaseDir() const
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    // Создаем поддиректорию для LSP
    baseDir += "/lsp_servers";
    QDir().mkpath(baseDir);

    return baseDir;
}

// Установка переменной окружения (только для текущего процесса)
bool LSPInstaller::setEnvironmentVariable(const QString& name, const QString& value)
{
#ifdef Q_OS_WIN
    return _putenv_s(name.toUtf8().constData(), value.toUtf8().constData()) == 0;
#else
    return setenv(name.toUtf8().constData(), value.toUtf8().constData(), 1) == 0;
#endif
}
