#include "java_lsp_client.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>

JavaLSPClient::JavaLSPClient(QObject* parent)
    : LSPClient(parent)
    , m_javaVersion("17")
    , m_allowDeepSearch(true)
{
    qDebug() << "======JAVA_LSP_CONSTRUCTOR_START======";
    m_language = LanguageJava;

    // Загружаем настройки из предыдущих запусков
    initializeFromSettings();

    // Автоматический поиск Java Home если не установлен
    if (m_javaHome.isEmpty()) {
        m_javaHome = findJavaHome();
        if (m_javaHome.isEmpty()) {
            qWarning() << "Java not found. Please install Java 11+ and set JAVA_HOME";
        } else {
            qDebug() << "Found Java at:" << m_javaHome;
            // Сохраняем найденный путь
            QSettings settings;
            settings.setValue("LSP/Java/JavaHome", m_javaHome);
        }
    }

    // Проверяем JDT LS
    if (m_serverPath.isEmpty() || !QFileInfo::exists(m_serverPath)) {
        QString jdtlsPath = findJdtLs();
        if (!jdtlsPath.isEmpty()) {
            m_serverPath = jdtlsPath;
            qDebug() << "Found JDT LS at:" << m_serverPath;

            QSettings settings;
            settings.setValue("LSP/Java/JdtlsPath", m_serverPath);
        } else {
            qWarning() << "JDT LS not found in standard locations";

            // Предлагаем установку только если есть Java
            if (!m_javaHome.isEmpty()) {
                QTimer::singleShot(1000, this, [this]() {
                    checkAndInstallAutomatically();
                });
            }
        }
    }

    // Базовые настройки JDT LS
    if (m_jdtlsSettings.isEmpty()) {
        m_jdtlsSettings = QJsonObject{
            {"java", QJsonObject{
                         {"home", m_javaHome},
                         {"version", m_javaVersion},
                         {"configuration", QJsonObject{
                                               {"runtimes", QJsonArray{}}
                                           }}
                     }},
            {"extendedClientCapabilities", QJsonObject{
                                               {"progressReportProvider", true},
                                               {"classFileContentsSupport", true}
                                           }}
        };
    }

    qDebug() << "======JAVA_LSP_CONSTRUCTOR_END======";
}

JavaLSPClient::~JavaLSPClient()
{
    if (m_installer) {
        m_installer->deleteLater();
    }
}

void JavaLSPClient::initializeFromSettings()
{
    QSettings settings;

    // Загружаем сохранённые пути
    m_javaHome = settings.value("LSP/Java/JavaHome", "").toString();
    m_serverPath = settings.value("LSP/Java/JdtlsPath", "").toString();
    m_javaVersion = settings.value("LSP/Java/Version", "17").toString();

    // Загружаем настройки JDT LS
    QByteArray settingsJson = settings.value("LSP/Java/Settings", QByteArray()).toByteArray();
    if (!settingsJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(settingsJson);
        if (doc.isObject()) {
            m_jdtlsSettings = doc.object();
        }
    }

    qDebug() << "Loaded from settings:";
    qDebug() << "  JavaHome:" << m_javaHome;
    qDebug() << "  JdtlsPath:" << m_serverPath;
    qDebug() << "  Version:" << m_javaVersion;
}

QString JavaLSPClient::findJavaHome() const
{
    // 1. Проверяем переменную окружения JAVA_HOME
    QString javaHome = qgetenv("JAVA_HOME");
    if (!javaHome.isEmpty() && QDir(javaHome).exists()) {
        QString javaExe = javaHome + "/bin/java";
#ifdef Q_OS_WIN
        javaExe += ".exe";
#endif
        if (QFileInfo::exists(javaExe)) {
            return javaHome;
        }
    }

    // 2. Ищем java в PATH
    QString javaPath;
#ifdef Q_OS_WIN
    QProcess where;
    where.start("where", {"java"});
    if (where.waitForFinished(3000)) {
        javaPath = QString::fromUtf8(where.readAllStandardOutput()).split('\n').first().trimmed();
    }
#else
    QProcess which;
    which.start("which", {"java"});
    if (which.waitForFinished(3000)) {
        javaPath = QString::fromUtf8(which.readAllStandardOutput()).trimmed();
    }
#endif

    if (!javaPath.isEmpty() && QFileInfo::exists(javaPath)) {
        // Получаем JAVA_HOME из пути к java
        QFileInfo javaInfo(javaPath);
        QString binDir = javaInfo.dir().absolutePath();
        QString homeDir = QDir(binDir).absolutePath() + "/..";
        return QDir(homeDir).canonicalPath();
    }

    // 3. Проверяем стандартные пути
    QStringList possiblePaths;
#ifdef Q_OS_WIN
    possiblePaths << "C:\\Program Files\\Java\\jdk-" + m_javaVersion
                  << "C:\\Program Files\\Java\\jdk" + m_javaVersion
                  << "C:\\Program Files (x86)\\Java\\jdk-" + m_javaVersion
                  << "C:\\Program Files\\Eclipse Adoptium\\jdk-" + m_javaVersion + "-hotspot"
                  << "C:\\Program Files\\BellSoft\\LibericaJDK-" + m_javaVersion;
#elif defined(Q_OS_MACOS)
    possiblePaths << "/Library/Java/JavaVirtualMachines/jdk-" + m_javaVersion + ".jdk/Contents/Home"
                  << "/usr/local/opt/openjdk@" + m_javaVersion + "/libexec/openjdk.jdk/Contents/Home";
#else
    possiblePaths << "/usr/lib/jvm/java-" + m_javaVersion + "-openjdk"
                  << "/usr/lib/jvm/jdk-" + m_javaVersion
                  << "/usr/lib/jvm/java-" + m_javaVersion;
#endif

    for (const QString& path : possiblePaths) {
        QString javaExe = path + "/bin/java";
#ifdef Q_OS_WIN
        javaExe += ".exe";
#endif
        if (QFileInfo::exists(javaExe)) {
            return path;
        }
    }

    return QString();
}

bool JavaLSPClient::checkJavaInstallation() const
{
    return !m_javaHome.isEmpty() && QDir(m_javaHome).exists();
}

void JavaLSPClient::setJavaHome(const QString& path)
{
    if (m_javaHome != path) {
        m_javaHome = path;

        // Обновляем настройки JDT LS
        if (!m_jdtlsSettings["java"].isObject()) {
            m_jdtlsSettings["java"] = QJsonObject();
        }

        QJsonObject javaConfig = m_jdtlsSettings["java"].toObject();
        javaConfig["home"] = path;
        m_jdtlsSettings["java"] = javaConfig;

        // Сохраняем в настройках
        QSettings settings;
        settings.setValue("LSP/Java/JavaHome", path);

        emit javaInstallationCompleted(true, path);
    }
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

    QSettings settings;
    settings.setValue("LSP/Java/Version", version);
}

void JavaLSPClient::setJdtLsSettings(const QJsonObject& settings)
{
    m_jdtlsSettings = settings;

    // Сохраняем в настройках
    QJsonDocument doc(settings);
    QSettings appSettings;
    appSettings.setValue("LSP/Java/Settings", doc.toJson(QJsonDocument::Compact));
}

QStringList JavaLSPClient::getDefaultServerArgs() const
{
    QStringList args;

    if (m_serverPath.isEmpty() || !QDir(m_serverPath).exists()) {
        return args;
    }

    // Ищем launcher jar
    QDir serverDir(m_serverPath);
    QStringList launcherJars = serverDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files, QDir::Name);

    if (launcherJars.isEmpty()) {
        // Ищем в поддиректориях
        QDirIterator it(m_serverPath, {"org.eclipse.equinox.launcher_*.jar"},
                        QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            QString launcherPath = it.next();

            args << "-jar" << launcherPath;

            // Конфигурация
            QString configDir;
#ifdef Q_OS_WIN
            configDir = m_serverPath + "/config_win";
#elif defined(Q_OS_MACOS)
            configDir = m_serverPath + "/config_mac";
#else
            configDir = m_serverPath + "/config_linux";
#endif

            if (QDir(configDir).exists()) {
                args << "-configuration" << configDir;
            }

            // Data directory
            QString dataDir = m_rootPath.isEmpty() ?
                                  QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/jdtls-workspace" :
                                  m_rootPath + "/.jdtls-workspace";

            QDir().mkpath(dataDir);
            args << "-data" << dataDir;

            // JVM options
            args << "-noverify";
            args << "--add-modules=ALL-SYSTEM";
            args << "--add-opens" << "java.base/java.util=ALL-UNNAMED";
            args << "--add-opens" << "java.base/java.lang=ALL-UNNAMED";
        }
    }

    return args;
}

QJsonObject JavaLSPClient::getDefaultInitOptions() const
{
    QJsonObject jdtlsParams = m_jdtlsSettings;

    // Добавляем workspace folders если есть
    if (!m_workspaceFolders.isEmpty()) {
        QJsonArray workspaceArray;
        for (const QString& folder : m_workspaceFolders) {
            workspaceArray.append(QJsonObject{
                {"uri", pathToUri(folder)},
                {"name", QFileInfo(folder).fileName()}
            });
        }
        jdtlsParams["workspaceFolders"] = workspaceArray;
    }

    // Добавляем classpath если есть
    if (!m_classPath.isEmpty()) {
        QJsonArray classPathArray;
        for (const QString& path : m_classPath) {
            classPathArray.append(pathToUri(path));
        }

        if (!jdtlsParams["java"].isObject()) {
            jdtlsParams["java"] = QJsonObject();
        }

        QJsonObject javaConfig = jdtlsParams["java"].toObject();
        javaConfig["classpath"] = classPathArray;
        jdtlsParams["java"] = javaConfig;
    }

    return QJsonObject{{"jdtls", jdtlsParams}};
}

void JavaLSPClient::applyLanguageSpecificSettings(QJsonObject& initOptions)
{
    QJsonObject jdtlsSettings = initOptions["jdtls"].toObject();

    if (jdtlsSettings.isEmpty()) {
        jdtlsSettings = getDefaultInitOptions()["jdtls"].toObject();
    }

    // Объединяем настройки
    for (auto it = m_jdtlsSettings.begin(); it != m_jdtlsSettings.end(); ++it) {
        if (!jdtlsSettings.contains(it.key())) {
            jdtlsSettings[it.key()] = it.value();
        }
    }

    initOptions["jdtls"] = jdtlsSettings;
}

bool JavaLSPClient::downloadAndSetupJdtls(bool showProgress)
{
    // Создаем инсталлер если его нет
    if (!m_installer) {
        m_installer = new JdtlsInstaller(this);
        setupInstallerSignals();
    }


    // Показываем диалог подтверждения если нужно
    if (showProgress) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Установка JDT Language Server");
        msgBox.setText("Установить Language Server для Java?");
        msgBox.setInformativeText("Это потребует:\n"
                                  "• Java JDK 11+ (будет установлен если нужно)\n"
                                  "• ~100-200 МБ дискового пространства\n"
                                  "• Доступ в интернет");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Ok);

        if (msgBox.exec() != QMessageBox::Ok) {
            return false;
        }
    }

    emit jdtlsInstallationStarted();

    // Запускаем установку
    LSPInstaller::InstallResult result = m_installer->install();

    return result == LSPInstaller::Success || result == LSPInstaller::AlreadyInstalled;
}

void JavaLSPClient::checkAndInstallAutomatically()
{
    if (isJdtlsInstalled()) {
        qDebug() << "JDT LS already installed";
        return;
    }

    QMessageBox msgBox;
    msgBox.setWindowTitle("Java Language Server не найден");
    msgBox.setText("Для полноценной работы с Java кодом требуется JDT Language Server.");
    msgBox.setInformativeText("Установить автоматически?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() == QMessageBox::Yes) {
        downloadAndSetupJdtls(true);
    }
}

bool JavaLSPClient::isJdtlsInstalled() const
{
    return !m_serverPath.isEmpty() && QDir(m_serverPath).exists();
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

void JavaLSPClient::startAutoInstallation()
{
    // Сначала проверяем Java
    if (!checkJavaInstallation()) {
        emit javaInstallationRequired();

        QMessageBox msgBox;
        msgBox.setWindowTitle("Java не найден");
        msgBox.setText("Для работы Java Language Server требуется Java JDK 11+.");
        msgBox.setInformativeText("Установить Java автоматически?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);

        if (msgBox.exec() == QMessageBox::Yes) {
            if (!m_installer) {
                m_installer = new JdtlsInstaller(this);
                setupInstallerSignals();
            }

            // Устанавливаем Java через инсталлер
            connect(m_installer, &JdtlsInstaller::javaInstallationResult,
                    this, [this](LSPInstaller::InstallResult result, const QString& message) {
                        if (result == LSPInstaller::Success) {
                            // После установки Java устанавливаем JDT LS
                            QTimer::singleShot(2000, this, [this]() {
                                downloadAndSetupJdtls(true);
                            });
                        }
                    });

            m_installer->installJavaJDK();
        }
    } else {
        // Java есть, устанавливаем JDT LS
        downloadAndSetupJdtls(true);
    }
}

void JavaLSPClient::setupInstallerSignals()
{
    if (!m_installer) return;

    connect(m_installer, &JdtlsInstaller::progressChanged,
            this, &JavaLSPClient::onJdtlsInstallProgress);

    connect(m_installer, &JdtlsInstaller::finished,
            this, &JavaLSPClient::onJdtlsInstallFinished);

    connect(m_installer, &JdtlsInstaller::javaCheckCompleted,
            this, &JavaLSPClient::onJavaCheckCompleted);

    connect(m_installer, &JdtlsInstaller::javaInstallationResult,
            this, &JavaLSPClient::onJavaInstallationResult);

    connect(m_installer, &JdtlsInstaller::downloadProgress,
            this, &JavaLSPClient::onJdtlsDownloadProgress);

    connect(m_installer, &JdtlsInstaller::logMessage,
            [this](const QString& message) {
                qDebug() << "JDTLS Install:" << message;
            });
}

void JavaLSPClient::onJdtlsInstallProgress(int percent, const QString& message)
{
    emit installationProgress(percent, message);
}

void JavaLSPClient::onJdtlsInstallFinished(LSPInstaller::InstallResult result, const QString& message)
{
    handleInstallationResult(result, message);
}

void JavaLSPClient::onJavaCheckCompleted(bool javaInstalled, const QString& javaHome)
{
    if (javaInstalled && !javaHome.isEmpty()) {
        setJavaHome(javaHome);
    }
}

void JavaLSPClient::onJavaInstallationResult(LSPInstaller::InstallResult result, const QString& message)
{
    if (result == LSPInstaller::Success) {
        QString javaHome = m_installer->getDetectedJavaHome();
        if (!javaHome.isEmpty()) {
            setJavaHome(javaHome);
        }
    }
}

void JavaLSPClient::onJdtlsDownloadProgress(qint64 bytesReceived, qint64 totalBytes)
{
    emit downloadProgressChanged(bytesReceived, totalBytes);
}

void JavaLSPClient::handleInstallationResult(LSPInstaller::InstallResult result, const QString& message)
{
    switch (result) {
    case LSPInstaller::Success:
    {
        // Обновляем путь к серверу
        m_serverPath = m_installer->getInstallPath();

        // Сохраняем в настройках
        QSettings settings;
        settings.setValue("LSP/Java/JdtlsPath", m_serverPath);
        settings.sync();

        qDebug() << "JDT LS installed at:" << m_serverPath;
        emit jdtlsInstallationFinished(true, m_serverPath);

        // Автоматически переподключаем LSP
        QTimer::singleShot(2000, this, [this]() {
            restart();
        });
    }
        break;

    case LSPInstaller::AlreadyInstalled:
        // Просто обновляем путь
        m_serverPath = m_installer->getInstallPath();
        emit jdtlsInstallationFinished(true, m_serverPath);
        break;

    case LSPInstaller::Cancelled:
        qDebug() << "JDT LS installation cancelled:" << message;
        emit jdtlsInstallationFinished(false, "Installation cancelled");
        break;

    case LSPInstaller::Failed:
        qWarning() << "JDT LS installation failed:" << message;
        emit jdtlsInstallationFinished(false, message);

        // Показываем сообщение об ошибке
        QMessageBox::critical(nullptr, "Installation Failed",
                              "Failed to install JDT Language Server:\n" + message);
        break;
    }
}

QString JavaLSPClient::findJdtLs() const
{
    qDebug() << "Searching for JDT LS...";

    // 1. Проверяем сохраненный путь
    QSettings settings;
    QString savedPath = settings.value("LSP/Java/JdtlsPath", "").toString();
    if (!savedPath.isEmpty() && QDir(savedPath).exists()) {
        qDebug() << "Found JDT LS in settings:" << savedPath;
        return savedPath;
    }

    // 2. Проверяем переменную окружения
    QString envPath = qgetenv("JDTLS_HOME");
    if (!envPath.isEmpty() && QDir(envPath).exists()) {
        qDebug() << "Found JDT LS in JDTLS_HOME:" << envPath;
        return envPath;
    }

    // 3. Ищем в VS Code расширениях
    QStringList vscodePaths;

#ifdef Q_OS_WIN
    QString userProfile = qgetenv("USERPROFILE");
    if (!userProfile.isEmpty()) {
        vscodePaths << userProfile + "/.vscode/extensions"
                    << userProfile + "/AppData/Roaming/Code/User/extensions"
                    << "C:/Program Files/Microsoft VS Code/resources/app/extensions";
    }
#elif defined(Q_OS_MACOS)
    vscodePaths << QDir::homePath() + "/.vscode/extensions"
                << "/Applications/Visual Studio Code.app/Contents/Resources/app/extensions";
#else
    vscodePaths << QDir::homePath() + "/.vscode/extensions"
                << "/usr/share/code/resources/app/extensions"
                << "/opt/visual-studio-code/resources/app/extensions";
#endif

    for (const QString& vscodePath : vscodePaths) {
        QDir extensionsDir(vscodePath);
        if (extensionsDir.exists()) {
            // Ищем расширение redhat.java
            QStringList filters;
            filters << "redhat.java-*" << "vscjava.vscode-java-*";

            QStringList javaExtensions = extensionsDir.entryList(filters,
                                                                 QDir::Dirs | QDir::NoDotAndDotDot,
                                                                 QDir::Name | QDir::Reversed);

            for (const QString& ext : javaExtensions) {
                QString candidate = vscodePath + "/" + ext + "/server";
                if (QDir(candidate).exists()) {
                    // Проверяем наличие launcher jar
                    QDir serverDir(candidate);
                    QStringList launcherJars = serverDir.entryList(
                        {"org.eclipse.equinox.launcher_*.jar"},
                        QDir::Files, QDir::Name);

                    if (!launcherJars.isEmpty()) {
                        qDebug() << "Found JDT LS in VS Code extension:" << candidate;
                        return candidate;
                    }
                }
            }
        }
    }

    // 4. Ищем в стандартных путях установки
    QStringList standardPaths;

#ifdef Q_OS_WIN
    QString programFiles = qgetenv("ProgramFiles");
    if (!programFiles.isEmpty()) {
        standardPaths << programFiles + "/jdtls"
                      << "C:/jdtls";
    }
    standardPaths << QDir::homePath() + "/jdtls";
#elif defined(Q_OS_MACOS)
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
        if (QDir(path).exists()) {
            // Проверяем наличие launcher jar
            QDir dir(path);
            QStringList launcherJars = dir.entryList(
                {"org.eclipse.equinox.launcher_*.jar"},
                QDir::Files, QDir::Name);

            if (!launcherJars.isEmpty()) {
                qDebug() << "Found JDT LS in standard path:" << path;
                return path;
            }
        }
    }

    // 5. Ищем в Eclipse
    QStringList eclipsePaths;
#ifdef Q_OS_WIN
    eclipsePaths << "C:/Program Files/Eclipse"
                 << "C:/Program Files (x86)/Eclipse"
                 << qgetenv("ProgramFiles") + "/Eclipse"
                 << qgetenv("LOCALAPPDATA") + "/Eclipse";
#elif defined(Q_OS_MACOS)
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
        QDir eclipseDir(eclipsePath);
        if (eclipseDir.exists()) {
            // Ищем плагины JDT LS
            QString pluginsPath = eclipsePath + "/plugins";
            QDir pluginsDir(pluginsPath);

            if (pluginsDir.exists()) {
                QStringList jdtlsPlugins = pluginsDir.entryList(
                    {"org.eclipse.jdt.ls.core*"},
                    QDir::Dirs | QDir::NoDotAndDotDot,
                    QDir::Name | QDir::Reversed);

                for (const QString& plugin : jdtlsPlugins) {
                    QString candidate = pluginsPath + "/" + plugin;

                    // Проверяем наличие launcher jar
                    QDir pluginDir(candidate);
                    QStringList launcherJars = pluginDir.entryList(
                        {"org.eclipse.equinox.launcher_*.jar"},
                        QDir::Files, QDir::Name);

                    if (!launcherJars.isEmpty()) {
                        qDebug() << "Found JDT LS in Eclipse plugin:" << candidate;
                        return candidate;
                    }
                }
            }
        }
    }

    // 6. Ищем в домашней директории (глубокий поиск)
    if (m_allowDeepSearch) {
        qDebug() << "Performing deep search in home directory...";

        QString homePath = QDir::homePath();
        QDirIterator it(homePath,
                        QStringList() << "*jdtls*" << "*jdt*ls*" << "*eclipse.jdt.ls*",
                        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);

        int count = 0;
        while (it.hasNext() && count < 1000) { // Ограничиваем поиск
            QString candidate = it.next();
            count++;

            if (QFileInfo(candidate).isDir()) {
                QDir dir(candidate);
                QStringList launcherJars = dir.entryList(
                    {"org.eclipse.equinox.launcher_*.jar"},
                    QDir::Files, QDir::Name);

                if (!launcherJars.isEmpty()) {
                    qDebug() << "Found JDT LS via deep search:" << candidate;
                    return candidate;
                }
            }
        }

        qDebug() << "Deep search completed, checked" << count << "items";
    }

    qDebug() << "JDT LS not found";
    return QString();
}
