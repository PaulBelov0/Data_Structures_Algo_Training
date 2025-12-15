#include "java_lsp_client.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>

JavaLSPClient::JavaLSPClient(QObject* parent)
    : LSPClient(parent)
    , m_javaVersion("11")
{
    m_language = LanguageJava;

    // Автоматический поиск Java Home
    if (m_javaHome.isEmpty()) {
        m_javaHome = findJavaHome();
    }

    // Автоматический поиск jdtls
    if (m_serverPath.isEmpty()) {
        QString jdtlsPath = findJdtLs();
        if (!jdtlsPath.isEmpty()) {
            m_serverPath = jdtlsPath;
        }
    }

    // Базовые настройки JDT LS
    m_jdtlsSettings = QJsonObject{
        {"java", QJsonObject{
                     {"home", m_javaHome},
                     {"configuration", QJsonObject{
                                           {"runtimes", QJsonArray{}}
                                       }}
                 }},
        {"extendedClientCapabilities", QJsonObject{
                                           {"progressReportProvider", true},
                                           {"classFileContentsSupport", true}
                                       }}
    };

    m_downloader = new JdtlsDownloader(this);
}

QString JavaLSPClient::findJavaHome() const
{
#ifdef Q_OS_WIN
    // Проверка переменной среды JAVA_HOME
    QString javaHome = qgetenv("JAVA_HOME");
    if (!javaHome.isEmpty() && QDir(javaHome).exists()) {
        return javaHome;
    }

    // Проверка стандартных путей
    QStringList possiblePaths = {
        "C:\\Program Files\\Java\\jdk-" + m_javaVersion,
        "C:\\Program Files\\Java\\jdk" + m_javaVersion,
        "C:\\Program Files (x86)\\Java\\jdk-" + m_javaVersion,
        QStandardPaths::findExecutable("java.exe").section('/', 0, -2)
    };

    for (const QString& path : possiblePaths) {
        if (QFileInfo::exists(path + "\\bin\\java.exe")) {
            return path;
        }
    }
#else
    // Linux/macOS
    QString javaHome = qgetenv("JAVA_HOME");
    if (!javaHome.isEmpty() && QDir(javaHome).exists()) {
        return javaHome;
    }

    // Попробуем найти через which java
    QProcess process;
    process.start("which", {"java"});
    if (process.waitForFinished(1000)) {
        QString javaPath = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!javaPath.isEmpty()) {
            // Получаем домашнюю директорию Java
            QFileInfo javaInfo(javaPath);
            return javaInfo.canonicalPath() + "/..";
        }
    }

    // Проверка стандартных путей
    QStringList possiblePaths = {
        "/usr/lib/jvm/java-" + m_javaVersion + "-openjdk",
        "/usr/lib/jvm/jdk-" + m_javaVersion,
        "/Library/Java/JavaVirtualMachines/jdk-" + m_javaVersion + ".jdk/Contents/Home"
    };

    for (const QString& path : possiblePaths) {
        if (QFileInfo::exists(path + "/bin/java")) {
            return path;
        }
    }
#endif

    return QString();
}

QString JavaLSPClient::findJdtLs() const
{
    // 1. Проверка переменной среды JDTLS_HOME
    QString jdtlsPath = qgetenv("JDTLS_HOME");
    if (!jdtlsPath.isEmpty() && isValidJdtLsPath(jdtlsPath)) {
        return jdtlsPath;
    }

    // 2. Проверка сохраненного пути
    QSettings settings;
    QString savedPath = settings.value("LSP/Java/Path").toString();
    if (!savedPath.isEmpty() && isValidJdtLsPath(savedPath)) {
        return savedPath;
    }

    // 3. Поиск в VS Code расширениях (Windows)
#ifdef Q_OS_WIN
    QStringList vscodePaths = {
        qgetenv("USERPROFILE") + "/.vscode/extensions",
        qgetenv("APPDATA") + "/Code/User/extensions",
        qgetenv("LOCALAPPDATA") + "/Programs/Microsoft VS Code/resources/app/extensions"
    };

    for (const QString& vscodePath : vscodePaths) {
        QDir extensionsDir(vscodePath);
        if (extensionsDir.exists()) {
            // Ищем расширение redhat.java (новейшая версия первая)
            QStringList javaExtensions = extensionsDir.entryList({"redhat.java-*"},
                                                                 QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

            for (const QString& ext : javaExtensions) {
                QString candidate = vscodePath + "/" + ext;

                // Проверяем прямо в директории расширения
                if (isValidJdtLsPath(candidate)) {
                    settings.setValue("LSP/Java/Path", candidate);
                    return candidate;
                }

                // Проверяем в поддиректории server
                QString serverPath = candidate + "/server";
                if (isValidJdtLsPath(serverPath)) {
                    settings.setValue("LSP/Java/Path", serverPath);
                    return serverPath;
                }
            }
        }
    }
#endif

    // 4. Поиск в Eclipse (все платформы)
    QStringList eclipsePaths;

#ifdef Q_OS_WIN
    eclipsePaths << "C:/Program Files/Eclipse"
                 << "C:/Program Files (x86)/Eclipse"
                 << qgetenv("ProgramFiles") + "/Eclipse"
                 << qgetenv("LOCALAPPDATA") + "/Eclipse";
#elif defined(Q_OS_MAC)
    eclipsePaths << "/Applications/Eclipse.app/Contents/Eclipse"
                 << "/usr/local/eclipse"
                 << QDir::homePath() + "/eclipse";
#else
    eclipsePaths << "/usr/lib/eclipse"
                 << "/usr/share/eclipse"
                 << "/opt/eclipse"
                 << QDir::homePath() + "/eclipse";
#endif

    for (const QString& eclipsePath : eclipsePaths) {
        if (QDir(eclipsePath).exists()) {
            // Проверяем плагины JDT LS
            QString pluginsPath = eclipsePath + "/plugins";
            QDir pluginsDir(pluginsPath);

            if (pluginsDir.exists()) {
                // Ищем плагин JDT LS
                QStringList jdtlsPlugins = pluginsDir.entryList({"org.eclipse.jdt.ls.core*"},
                                                                QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

                if (!jdtlsPlugins.isEmpty()) {
                    QString candidate = pluginsPath + "/" + jdtlsPlugins.first();
                    if (isValidJdtLsPath(candidate)) {
                        settings.setValue("LSP/Java/Path", candidate);
                        return candidate;
                    }
                }
            }
        }
    }

    // 5. Поиск в стандартных путях установки
    QStringList standardPaths;

#ifdef Q_OS_WIN
    standardPaths << "C:/jdtls"
                  << qgetenv("ProgramFiles") + "/jdtls"
                  << qgetenv("LOCALAPPDATA") + "/jdtls"
                  << QDir::homePath() + "/jdtls";
#elif defined(Q_OS_MAC)
    standardPaths << "/Applications/jdtls"
                  << "/usr/local/jdtls"
                  << "/opt/jdtls"
                  << QDir::homePath() + "/jdtls";
#else
    standardPaths << "/usr/lib/jdtls"
                  << "/usr/share/jdtls"
                  << "/usr/local/lib/jdtls"
                  << "/opt/jdtls"
                  << QDir::homePath() + "/jdtls";
#endif

    for (const QString& path : standardPaths) {
        if (isValidJdtLsPath(path)) {
            settings.setValue("LSP/Java/Path", path);
            return path;
        }
    }

    // 6. Проверка в ранее загруженных версиях
    QString downloadPath = JdtlsDownloader::getDefaultDownloadPath() + "/jdtls";
    if (QDir(downloadPath).exists()) {
        // Ищем последнюю загруженную версию
        QDir dir(downloadPath);
        QStringList versions = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

        for (const QString& version : versions) {
            QString candidate = downloadPath + "/" + version;
            if (isValidJdtLsPath(candidate)) {
                settings.setValue("LSP/Java/Path", candidate);
                return candidate;
            }
        }
    }

    // 7. Проверка текущей директории
    if (isValidJdtLsPath(QDir::currentPath())) {
        settings.setValue("LSP/Java/Path", QDir::currentPath());
        return QDir::currentPath();
    }

    // 8. Рекурсивный поиск в домашней директории (последнее средство)
    if (m_allowDeepSearch) {
        QString homePath = QDir::homePath();
        QDirIterator it(homePath, QStringList() << "*jdtls*" << "*jdt*ls*" << "*eclipse.jdt.ls*",
                        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);

        while (it.hasNext()) {
            QString candidate = it.next();
            if (isValidJdtLsPath(candidate)) {
                qDebug() << "Found JDT LS via deep search:" << candidate;
                settings.setValue("LSP/Java/Path", candidate);
                return candidate;
            }
        }
    }

    // Не нашли
    return QString();
}

bool JavaLSPClient::isValidJdtLsPath(const QString& path) const
{
    if (path.isEmpty()) {
        return false;
    }

    QFileInfo info(path);
    if (!info.exists()) {
        return false;
    }

    // Путь может быть как директорией, так и архивом
    if (info.isDir()) {
        QDir dir(path);

        // Ищем launcher jar - основной признак JDT LS
        QStringList launcherJars = findLauncherJars(dir);
        if (!launcherJars.isEmpty()) {
            return true;
        }

        // Проверяем структуру директорий JDT LS
        QStringList entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

        // Должны быть типичные директории JDT LS
        bool hasPlugins = false;
        bool hasFeatures = false;
        bool hasConfig = false;
        bool hasLauncher = false;

        for (const QString& entry : entries) {
            if (entry.startsWith("plugins") || entry == "plugins") {
                hasPlugins = true;
            } else if (entry.startsWith("features") || entry == "features") {
                hasFeatures = true;
            } else if (entry.startsWith("config_") || entry == "configuration") {
                hasConfig = true;
            } else if (entry.contains("launcher", Qt::CaseInsensitive)) {
                hasLauncher = true;
            } else if (entry.endsWith(".jar") && entry.contains("equinox.launcher", Qt::CaseInsensitive)) {
                hasLauncher = true;
            }
        }

        // JDT LS обычно имеет хотя бы плагины и конфигурацию
        if ((hasPlugins && hasConfig) || (hasLauncher && hasPlugins)) {
            return true;
        }

        // Проверяем вложенные директории
        QDirIterator it(dir.absolutePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QDir subDir(it.next());
            launcherJars = findLauncherJars(subDir);
            if (!launcherJars.isEmpty()) {
                return true;
            }
        }
    }
    else if (info.isFile()) {
        // Проверяем, является ли файл архивом JDT LS
        QString fileName = info.fileName().toLower();
        if (fileName.endsWith(".tar.gz") || fileName.endsWith(".tgz") ||
            fileName.endsWith(".zip") || fileName.endsWith(".jar")) {

            // Проверяем имя файла
            if (fileName.contains("jdt", Qt::CaseInsensitive) ||
                fileName.contains("jdtls", Qt::CaseInsensitive) ||
                fileName.contains("eclipse.jdt.ls", Qt::CaseInsensitive)) {
                return true;
            }
        }
    }

    return false;
}

QStringList JavaLSPClient::findLauncherJars(const QDir& dir) const
{
    QStringList launcherJars;

    // Ищем стандартный launcher JDT LS
    launcherJars = dir.entryList({"org.eclipse.equinox.launcher_*.jar"},
                                 QDir::Files, QDir::Name);

    // Ищем другие возможные названия
    if (launcherJars.isEmpty()) {
        QStringList filters;
        filters << "*launcher*.jar"
                << "*equinox*.jar"
                << "launcher*.jar";

        launcherJars = dir.entryList(filters, QDir::Files, QDir::Name);
    }

    // Фильтруем результаты
    QStringList validLaunchers;
    for (const QString& jar : launcherJars) {
        // Проверяем, что это действительно launcher jar
        if (jar.contains("equinox", Qt::CaseInsensitive) ||
            jar.contains("launcher", Qt::CaseInsensitive)) {

            // Проверяем размер (launcher jar обычно > 1MB)
            QFileInfo jarInfo(dir.absoluteFilePath(jar));
            if (jarInfo.size() > 1000000) { // > 1MB
                validLaunchers.append(jar);
            }
        }
    }

    return validLaunchers;
}

QStringList JavaLSPClient::getDefaultServerArgs() const
{
    QStringList args;

    // JDT LS запускается через Java, а не как отдельный процесс
    // Нужно найти правильный launcher jar

    if (QFileInfo(m_serverPath).isDir()) {
        // Ищем launcher jar в директории сервера
        QDir serverDir(m_serverPath);

        // Проверяем различные возможные структуры

        // 1. VS Code расширение структура
        QStringList launcherJars = serverDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);

        // 2. Eclipse плагин структура
        if (launcherJars.isEmpty()) {
            QDir pluginsDir(m_serverPath);
            launcherJars = pluginsDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);
        }

        if (!launcherJars.isEmpty()) {
            // Нашли launcher jar
            QString launcherPath = m_serverPath + "/" + launcherJars.first();

            // Базовые аргументы для запуска
            args << "-jar" << launcherPath;

            // Конфигурация (зависит от ОС)
            QString configDir;
#ifdef Q_OS_WIN
            configDir = m_serverPath + "/config_win";
#elif defined(Q_OS_MAC)
            configDir = m_serverPath + "/config_mac";
#else
            configDir = m_serverPath + "/config_linux";
#endif

            // Проверяем существование конфигурации
            if (QDir(configDir).exists()) {
                args << "-configuration" << configDir;
            } else {
                // Попробуем найти любую конфигурацию
                QStringList configs = serverDir.entryList({"config_*"}, QDir::Dirs);
                if (!configs.isEmpty()) {
                    args << "-configuration" << (m_serverPath + "/" + configs.first());
                }
            }

            // Директория для данных
            QString dataDir = m_rootPath.isEmpty() ? QDir::tempPath() + "/jdtls-workspace" : m_rootPath + "/.jdtls-workspace";
            QDir().mkpath(dataDir);
            args << "-data" << dataDir;

            // Дополнительные параметры
            args << "-noverify";  // Не проверять JAR подписи
            args << "--add-modules=ALL-SYSTEM";
            args << "--add-opens";
            args << "java.base/java.util=ALL-UNNAMED";
            args << "--add-opens";
            args << "java.base/java.lang=ALL-UNNAMED";
        }
    }

    return args;
}

QJsonObject JavaLSPClient::getDefaultInitOptions() const
{
    QJsonObject jdtlsParams = m_jdtlsSettings;

    // Настройки workspace
    if (!m_workspaceFolders.isEmpty()) {
        QJsonArray workspaceFolders;
        for (const QString& folder : m_workspaceFolders) {
            workspaceFolders.append(QJsonObject{
                {"uri", pathToUri(folder)},
                {"name", QFileInfo(folder).fileName()}
            });
        }
        jdtlsParams["workspaceFolders"] = workspaceFolders;
    }

    // Настройки classpath
    if (!m_classPath.isEmpty()) {
        QJsonArray classPathArray;
        for (const QString& path : m_classPath) {
            classPathArray.append(path);
        }
        QJsonObject javaConfig = jdtlsParams["java"].toObject();
        javaConfig["classpath"] = classPathArray;
        jdtlsParams["java"] = javaConfig;
    }

    // Версия Java
    if (!m_javaVersion.isEmpty()) {
        QJsonObject javaConfig = jdtlsParams["java"].toObject();
        javaConfig["version"] = m_javaVersion;
        jdtlsParams["java"] = javaConfig;
    }

    return QJsonObject{
        {"jdtls", jdtlsParams}
    };
}

void JavaLSPClient::applyLanguageSpecificSettings(QJsonObject& initOptions)
{
    // Добавляем настройки для Java
    QJsonObject jdtlsSettings = initOptions["jdtls"].toObject();

    if (jdtlsSettings.isEmpty()) {
        jdtlsSettings = getDefaultInitOptions()["jdtls"].toObject();
    }

    // Объединяем с пользовательскими настройками
    for (auto it = m_jdtlsSettings.begin(); it != m_jdtlsSettings.end(); ++it) {
        if (!jdtlsSettings.contains(it.key())) {
            jdtlsSettings[it.key()] = it.value();
        }
    }

    initOptions["jdtls"] = jdtlsSettings;
}

void JavaLSPClient::setJavaHome(const QString& path)
{
    m_javaHome = path;

    // Обновляем настройки
    if (!m_jdtlsSettings["java"].isObject()) {
        m_jdtlsSettings["java"] = QJsonObject();
    }

    QJsonObject javaConfig = m_jdtlsSettings["java"].toObject();
    javaConfig["home"] = path;
    m_jdtlsSettings["java"] = javaConfig;
}

void JavaLSPClient::setWorkspaceFolders(const QStringList& folders)
{
    m_workspaceFolders = folders;
}

void JavaLSPClient::addClassPath(const QString& path)
{
    if (!m_classPath.contains(path)) {
        m_classPath.append(path);
    }
}

// Реализация в java_lsp_client.cpp
bool JavaLSPClient::downloadAndSetupJdtls(bool showProgress)
{
    if (!m_downloader) {
        m_downloader = new JdtlsDownloader(this);

        connect(m_downloader, &JdtlsDownloader::downloadProgressChanged,
                this, &JavaLSPClient::onDownloadProgress);
        connect(m_downloader, &JdtlsDownloader::downloadFinished,
                this, &JavaLSPClient::onDownloadFinished);
        connect(m_downloader, &JdtlsDownloader::downloadError,
                this, &JavaLSPClient::onDownloadError);
    }

    QString downloadPath = JdtlsDownloader::getDefaultDownloadPath() + "/jdtls";
    return m_downloader->downloadJdtls(downloadPath, JdtlsDownloader::SourceEclipseSnapshots);
}

void JavaLSPClient::onDownloadProgress(qint64 bytesReceived, qint64 totalBytes)
{
    emit downloadProgress(bytesReceived, totalBytes);

    if (totalBytes > 0) {
        int percent = (bytesReceived * 100) / totalBytes;
        qDebug() << "Downloading JDT LS:" << percent << "%";
    }
}

void JavaLSPClient::onDownloadFinished(const QString& jdtlsPath)
{
    m_serverPath = jdtlsPath;
    qDebug() << "JDT LS downloaded to:" << jdtlsPath;

    // Сохраняем путь в настройках
    QSettings settings;
    settings.setValue("LSP/Java/Path", jdtlsPath);

    emit downloadFinished(true, jdtlsPath);
}

void JavaLSPClient::onDownloadError(const QString& error)
{
    qWarning() << "Failed to download JDT LS:" << error;
    emit downloadFinished(false, error);
}

void JavaLSPClient::setJavaVersion(const QString& version)
{
    m_javaVersion = version;

    if (!m_jdtlsSettings["java"].isObject()) {
        m_jdtlsSettings["java"] = QJsonObject();
    }

    QJsonObject javaConfig = m_jdtlsSettings["java"].toObject();
    javaConfig["version"] = version;
    m_jdtlsSettings["java"] = javaConfig;
}

void JavaLSPClient::setJdtLsSettings(const QJsonObject& settings)
{
    m_jdtlsSettings = settings;
}
