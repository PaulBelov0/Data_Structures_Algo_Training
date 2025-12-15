#include "ide_integrator.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QSettings>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

IDEIntegrator::IDEIntegrator(QObject* parent)
    : QObject(parent)
    , m_env(QProcessEnvironment::systemEnvironment())
{
}

QMap<IDEIntegrator::Language, QList<IDEIntegrator::IDEPath>> IDEIntegrator::findInstalledIDEs()
{
    QMap<Language, QList<IDEPath>> result;

    // Поиск для C++
    result[Cpp] = findWindowsIDEs(Cpp);

    // Поиск для Java
    result[Java] = findWindowsIDEs(Java);

    return result;
}

QList<IDEIntegrator::IDEPath> IDEIntegrator::findWindowsIDEs(Language lang)
{
    QList<IDEPath> ides;

    if (lang == Cpp) {
        // Visual Studio
        QStringList vsPaths = {
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\Llvm\\bin\\clangd.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\bin\\clangd.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\Llvm\\bin\\clangd.exe",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\Llvm\\bin\\clangd.exe",
            QStandardPaths::findExecutable("clangd"),
            QStandardPaths::findExecutable("clangd", {m_env.value("PATH")})
        };

        // Проверка реестра для Visual Studio
        QSettings vsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\Setup", QSettings::NativeFormat);
        QStringList vsKeys = vsReg.childGroups();

        for (const QString& key : vsKeys) {
            QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\Setup\\" + key, QSettings::NativeFormat);
            QString installPath = reg.value("InstallationPath").toString();
            if (!installPath.isEmpty()) {
                QString clangdPath = installPath + "\\VC\\Tools\\Llvm\\bin\\clangd.exe";
                if (QFile::exists(clangdPath)) {
                    IDEPath ide;
                    ide.name = "Visual Studio " + key;
                    ide.path = installPath;
                    ide.lspServerPath = clangdPath;
                    ides.append(ide);
                }
            }
        }

        // CLion
        QStringList clionPaths = {
            m_env.value("LOCALAPPDATA") + "\\Programs\\CLion\\bin\\clangd\\bin\\clangd.exe",
            "C:\\Program Files\\JetBrains\\CLion\\bin\\clangd\\bin\\clangd.exe"
        };

        for (const QString& path : clionPaths) {
            if (QFile::exists(path)) {
                IDEPath ide;
                ide.name = "CLion";
                ide.path = QFileInfo(path).absolutePath();
                ide.lspServerPath = path;
                ides.append(ide);
            }
        }

        // VS Code C++ расширение
        QString vscodeExtensions = m_env.value("USERPROFILE") + "\\.vscode\\extensions";
        QDir vscodeDir(vscodeExtensions);
        QStringList cppExtensions = vscodeDir.entryList({"ms-vscode.cpptools-*"}, QDir::Dirs);

        for (const QString& ext : cppExtensions) {
            QString clangdPath = vscodeExtensions + "\\" + ext + "\\debugAdapters\\bin\\clangd.exe";
            if (QFile::exists(clangdPath)) {
                IDEPath ide;
                ide.name = "VS Code C/C++";
                ide.path = vscodeExtensions + "\\" + ext;
                ide.lspServerPath = clangdPath;
                ides.append(ide);
            }
        }
    }
    else if (lang == Java) {
        // VS Code Java расширение
        QString vscodeExtensions = m_env.value("USERPROFILE") + "\\.vscode\\extensions";
        QDir vscodeDir(vscodeExtensions);
        QStringList javaExtensions = vscodeDir.entryList({"redhat.java-*"}, QDir::Dirs);

        for (const QString& ext : javaExtensions) {
            QString jdtlsPath = vscodeExtensions + "\\" + ext + "\\server";
            if (QDir(jdtlsPath).exists()) {
                IDEPath ide;
                ide.name = "VS Code Java";
                ide.path = vscodeExtensions + "\\" + ext;
                ide.lspServerPath = jdtlsPath;
                ides.append(ide);
            }
        }

        // Eclipse
        QStringList eclipsePaths = {
            "C:\\Program Files\\Eclipse\\eclipse.exe",
            "C:\\Program Files (x86)\\Eclipse\\eclipse.exe",
            m_env.value("ProgramFiles") + "\\Eclipse\\eclipse.exe"
        };

        for (const QString& path : eclipsePaths) {
            if (QFile::exists(path)) {
                QString eclipseDir = QFileInfo(path).absolutePath();
                QString pluginsDir = eclipseDir + "\\plugins";
                QDir plugins(pluginsDir);
                QStringList jdtlsPlugins = plugins.entryList({"org.eclipse.jdt.ls.core*"}, QDir::Dirs);

                if (!jdtlsPlugins.isEmpty()) {
                    IDEPath ide;
                    ide.name = "Eclipse";
                    ide.path = eclipseDir;
                    ide.lspServerPath = pluginsDir + "\\" + jdtlsPlugins.first();
                    ides.append(ide);
                }
            }
        }

        // IntelliJ IDEA
        QStringList ideaPaths = {
            "C:\\Program Files\\JetBrains\\IntelliJ IDEA Community Edition\\bin\\idea.exe",
            "C:\\Program Files\\JetBrains\\IntelliJ IDEA Ultimate\\bin\\idea.exe",
            m_env.value("LOCALAPPDATA") + "\\Programs\\IntelliJ IDEA Community Edition\\bin\\idea.exe"
        };

        for (const QString& path : ideaPaths) {
            if (QFile::exists(path)) {
                QString ideaDir = QFileInfo(path).absolutePath() + "\\..";
                QString pluginsDir = QDir(ideaDir).absolutePath() + "\\plugins";

                // IntelliJ может иметь встроенный LSP сервер
                IDEPath ide;
                ide.name = "IntelliJ IDEA";
                ide.path = QDir(ideaDir).absolutePath();
                ide.lspServerPath = pluginsDir; // Нужно искать конкретный плагин
                ides.append(ide);
            }
        }
    }

    return ides;
}

QString IDEIntegrator::findLSPServer(Language lang, const QString& preferredIDE)
{
    // 1. Проверить стандартные пути
    QString standardPath = checkStandardPaths(lang);
    if (!standardPath.isEmpty() && QFile::exists(standardPath)) {
        return standardPath;
    }

    // 2. Поиск установленных IDE
    QList<IDEPath> ides = findWindowsIDEs(lang);

    if (!preferredIDE.isEmpty()) {
        for (const IDEPath& ide : ides) {
            if (ide.name.contains(preferredIDE, Qt::CaseInsensitive)) {
                return ide.lspServerPath;
            }
        }
    }

    // 3. Вернуть первый найденный
    if (!ides.isEmpty()) {
        return ides.first().lspServerPath;
    }

// 4. Попробовать winget (только Windows)
#ifdef Q_OS_WIN
    if (lang == Cpp) {
        QString clangd = findWithWinget("LLVM.LLVM");
        if (!clangd.isEmpty()) {
            return clangd + "\\bin\\clangd.exe";
        }
    }
#endif

    return QString();
}

QString IDEIntegrator::checkStandardPaths(Language lang)
{
    if (lang == Cpp) {
        // Стандартные пути для clangd

        // 1. Проверка в PATH (самый простой способ)
        QString clangdPath = QStandardPaths::findExecutable("clangd");
        if (!clangdPath.isEmpty() && QFileInfo::exists(clangdPath)) {
            qDebug() << "Found clangd in PATH:" << clangdPath;
            return clangdPath;
        }

        // 2. Windows стандартные пути
#ifdef Q_OS_WIN
        QStringList winPaths = {
            "C:/Program Files/LLVM/bin/clangd.exe",
            "C:/Program Files (x86)/LLVM/bin/clangd.exe",
            "C:/LLVM/bin/clangd.exe",
            qgetenv("ProgramFiles") + "/LLVM/bin/clangd.exe",
            qgetenv("ProgramFiles(x86)") + "/LLVM/bin/clangd.exe",
            qgetenv("LOCALAPPDATA") + "/Programs/LLVM/bin/clangd.exe"
        };

        for (const QString& path : winPaths) {
            if (QFileInfo::exists(path)) {
                qDebug() << "Found clangd at standard Windows path:" << path;
                return path;
            }
        }

        // 3. Visual Studio пути (Visual Studio часто включает clangd)
        QStringList vsPaths = {
            "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clangd.exe",
            "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/bin/clangd.exe",
            "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/bin/clangd.exe",
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/Llvm/bin/clangd.exe",
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Tools/Llvm/bin/clangd.exe",
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Tools/Llvm/bin/clangd.exe",
            "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Tools/Llvm/bin/clangd.exe"
        };

        for (const QString& path : vsPaths) {
            if (QFileInfo::exists(path)) {
                qDebug() << "Found clangd in Visual Studio:" << path;
                return path;
            }
        }

        // 4. Проверка реестра для Visual Studio
        QSettings vsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\Setup", QSettings::NativeFormat);
        QStringList vsVersions = vsReg.childGroups();

        for (const QString& version : vsVersions) {
            QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\Setup\\" + version, QSettings::NativeFormat);
            QString installPath = reg.value("InstallationPath").toString();
            if (!installPath.isEmpty()) {
                QString clangdPath = installPath + "/VC/Tools/Llvm/bin/clangd.exe";
                if (QFileInfo::exists(clangdPath)) {
                    qDebug() << "Found clangd via registry:" << clangdPath;
                    return clangdPath;
                }
            }
        }

#elif defined(Q_OS_MAC)
        // macOS стандартные пути
        QStringList macPaths = {
            "/usr/local/opt/llvm/bin/clangd",          // Homebrew
            "/opt/homebrew/opt/llvm/bin/clangd",       // Homebrew Apple Silicon
            "/usr/local/bin/clangd",                   // MacPorts или ручная установка
            "/opt/local/bin/clangd",                   // MacPorts альтернативный
            "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clangd"
        };

        for (const QString& path : macPaths) {
            if (QFileInfo::exists(path)) {
                qDebug() << "Found clangd at standard macOS path:" << path;
                return path;
            }
        }

        // Проверка через Homebrew если установлен
        QProcess brewProcess;
        brewProcess.start("brew", {"--prefix", "llvm"});
        if (brewProcess.waitForFinished(3000)) {
            QString brewPrefix = QString::fromUtf8(brewProcess.readAllStandardOutput()).trimmed();
            if (!brewPrefix.isEmpty()) {
                QString brewClangd = brewPrefix + "/bin/clangd";
                if (QFileInfo::exists(brewClangd)) {
                    qDebug() << "Found clangd via Homebrew:" << brewClangd;
                    return brewClangd;
                }
            }
        }

#else
        // Linux стандартные пути
        QStringList linuxPaths = {
            "/usr/bin/clangd",                         // Дистрибутивные пакеты
            "/usr/local/bin/clangd",                   // Ручная установка
            "/usr/lib/llvm-*/bin/clangd",              // Ubuntu/Debian пакеты
            "/opt/clang+llvm*/bin/clangd",             // Официальные билды LLVM
            "/snap/bin/clangd"                         // Snap пакет
        };

        // Проверяем стандартные пути
        for (const QString& path : linuxPaths) {
            // Обрабатываем пути с wildcards
            if (path.contains('*')) {
                QDir dir(QFileInfo(path).absolutePath());
                QString nameFilter = QFileInfo(path).fileName();
                QStringList matches = dir.entryList({nameFilter}, QDir::Files);
                if (!matches.isEmpty()) {
                    QString fullPath = dir.absoluteFilePath(matches.first());
                    qDebug() << "Found clangd with wildcard:" << fullPath;
                    return fullPath;
                }
            } else if (QFileInfo::exists(path)) {
                qDebug() << "Found clangd at standard Linux path:" << path;
                return path;
            }
        }

        // Проверка через which (дополнительная проверка)
        QProcess whichProcess;
        whichProcess.start("which", {"clangd"});
        if (whichProcess.waitForFinished(1000)) {
            QString whichPath = QString::fromUtf8(whichProcess.readAllStandardOutput()).trimmed();
            if (!whichPath.isEmpty() && QFileInfo::exists(whichPath)) {
                qDebug() << "Found clangd via which:" << whichPath;
                return whichPath;
            }
        }
#endif

        qDebug() << "clangd not found in standard paths";
        return QString();
    }
    else if (lang == Java) {
        // Стандартные пути для JDT LS

        // 1. Проверка переменной среды JDTLS_HOME
        QString jdtlsHome = qgetenv("JDTLS_HOME");
        if (!jdtlsHome.isEmpty()) {
            qDebug() << "Checking JDTLS_HOME:" << jdtlsHome;

            // Проверяем наличие launcher jar
            QDir jdtlsDir(jdtlsHome);
            QStringList launcherJars = jdtlsDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);
            if (!launcherJars.isEmpty()) {
                qDebug() << "Found JDT LS in JDTLS_HOME:" << jdtlsHome;
                return jdtlsHome;
            }

            // Проверяем поддиректорию server
            QDir serverDir(jdtlsHome + "/server");
            if (serverDir.exists()) {
                launcherJars = serverDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);
                if (!launcherJars.isEmpty()) {
                    qDebug() << "Found JDT LS in JDTLS_HOME/server:" << serverDir.absolutePath();
                    return serverDir.absolutePath();
                }
            }
        }

        // 2. VS Code расширения (самый распространенный способ)
        QStringList vscodeExtensionsPaths;

#ifdef Q_OS_WIN
        vscodeExtensionsPaths = {
            qgetenv("USERPROFILE") + "/.vscode/extensions",
            qgetenv("APPDATA") + "/Code/User/extensions",
            qgetenv("LOCALAPPDATA") + "/Programs/Microsoft VS Code/resources/app/extensions"
        };
#else
        vscodeExtensionsPaths = {
            QDir::homePath() + "/.vscode/extensions",
            QDir::homePath() + "/.vscode-insiders/extensions",
            "/usr/share/code/resources/app/extensions"
        };
#endif

        for (const QString& vscodePath : vscodeExtensionsPaths) {
            QDir extensionsDir(vscodePath);
            if (extensionsDir.exists()) {
                qDebug() << "Checking VS Code extensions at:" << vscodePath;

                // Ищем расширение redhat.java (сортируем по версии, новейшая первая)
                QStringList javaExtensions = extensionsDir.entryList({"redhat.java-*"},
                                                                     QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

                for (const QString& ext : javaExtensions) {
                    // Сначала проверяем поддиректорию server
                    QString serverPath = vscodePath + "/" + ext + "/server";
                    QDir serverDir(serverPath);
                    if (serverDir.exists()) {
                        QStringList launcherJars = serverDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);
                        if (!launcherJars.isEmpty()) {
                            qDebug() << "Found JDT LS in VS Code extension server:" << serverPath;
                            return serverPath;
                        }
                    }

                    // Проверяем прямо в директории расширения
                    QString extPath = vscodePath + "/" + ext;
                    QDir extDir(extPath);
                    if (extDir.exists()) {
                        QStringList launcherJars = extDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);
                        if (!launcherJars.isEmpty()) {
                            qDebug() << "Found JDT LS in VS Code extension:" << extPath;
                            return extPath;
                        }
                    }
                }
            }
        }

        // 3. Стандартные пути установки
        QStringList standardPaths;

#ifdef Q_OS_WIN
        standardPaths = {
            "C:/jdtls",
            qgetenv("ProgramFiles") + "/jdtls",
            qgetenv("LOCALAPPDATA") + "/jdtls",
            QDir::homePath() + "/jdtls"
        };
#elif defined(Q_OS_MAC)
        standardPaths = {
            "/Applications/jdtls",
            "/usr/local/jdtls",
            "/opt/jdtls",
            QDir::homePath() + "/jdtls",
            "/usr/local/share/jdtls"
        };
#else
        standardPaths = {
            "/usr/lib/jdtls",
            "/usr/share/jdtls",
            "/usr/local/lib/jdtls",
            "/opt/jdtls",
            QDir::homePath() + "/jdtls",
            "/usr/local/share/jdtls"
        };
#endif

        for (const QString& path : standardPaths) {
            QDir dir(path);
            if (dir.exists()) {
                qDebug() << "Checking standard JDT LS path:" << path;

                QStringList launcherJars = dir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);
                if (!launcherJars.isEmpty()) {
                    qDebug() << "Found JDT LS at standard path:" << path;
                    return path;
                }

                // Проверяем поддиректорию server
                QDir serverDir(path + "/server");
                if (serverDir.exists()) {
                    launcherJars = serverDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);
                    if (!launcherJars.isEmpty()) {
                        qDebug() << "Found JDT LS at standard path/server:" << serverDir.absolutePath();
                        return serverDir.absolutePath();
                    }
                }
            }
        }

        // 4. Проверка в Eclipse
        QStringList eclipsePaths;

#ifdef Q_OS_WIN
        eclipsePaths = {
            "C:/Program Files/Eclipse",
            "C:/Program Files (x86)/Eclipse",
            qgetenv("ProgramFiles") + "/Eclipse",
            qgetenv("LOCALAPPDATA") + "/Eclipse"
        };
#elif defined(Q_OS_MAC)
        eclipsePaths = {
            "/Applications/Eclipse.app/Contents/Eclipse",
            "/usr/local/eclipse",
            QDir::homePath() + "/eclipse"
        };
#else
        eclipsePaths = {
            "/usr/lib/eclipse",
            "/usr/share/eclipse",
            "/opt/eclipse",
            QDir::homePath() + "/eclipse"
        };
#endif

        for (const QString& eclipsePath : eclipsePaths) {
            QDir eclipseDir(eclipsePath);
            if (eclipseDir.exists()) {
                qDebug() << "Checking Eclipse at:" << eclipsePath;

                // Проверяем плагины
                QString pluginsPath = eclipsePath + "/plugins";
                QDir pluginsDir(pluginsPath);
                if (pluginsDir.exists()) {
                    // Ищем плагин JDT LS
                    QStringList jdtlsPlugins = pluginsDir.entryList({"org.eclipse.jdt.ls.core*"},
                                                                    QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

                    if (!jdtlsPlugins.isEmpty()) {
                        QString candidate = pluginsPath + "/" + jdtlsPlugins.first();
                        QDir pluginDir(candidate);
                        QStringList launcherJars = pluginDir.entryList({"org.eclipse.equinox.launcher_*.jar"}, QDir::Files);

                        if (!launcherJars.isEmpty()) {
                            qDebug() << "Found JDT LS in Eclipse plugin:" << candidate;
                            return candidate;
                        }
                    }
                }
            }
        }

        qDebug() << "JDT LS not found in standard paths";
        return QString();
    }

    // Для неизвестного языка
    qWarning() << "Unknown language requested:" << lang;
    return QString();
}

QString IDEIntegrator::findWithWinget(const QString& package)
{
    QProcess winget;
    winget.start("winget", {"list", "--id", package, "--exact"});

    if (winget.waitForFinished(5000)) {
        QString output = winget.readAllStandardOutput();
        if (output.contains(package)) {
            // Если пакет установлен, получим его путь
            winget.start("where", {package});
            if (winget.waitForFinished(3000)) {
                return QString::fromLocal8Bit(winget.readAllStandardOutput()).trimmed();
            }
        }
    }

    return QString();
}

bool IDEIntegrator::autoSetupLanguageServer(Language lang)
{
    QString serverPath = findLSPServer(lang);

    if (serverPath.isEmpty()) {
        qWarning() << "No LSP server found for language" << lang;

// Предложить установить
#ifdef Q_OS_WIN
        if (lang == Cpp) {
            int result = QMessageBox::question(nullptr,
                                               "Установить Clangd",
                                               "Clangd не найден. Хотите установить через winget?",
                                               QMessageBox::Yes | QMessageBox::No);

            if (result == QMessageBox::Yes) {
                QProcess::startDetached("winget", {"install", "LLVM.LLVM"});
            }
        }
#endif

        emit setupComplete(lang, false);
        return false;
    }

    // Сохранить путь в настройках
    QSettings settings;
    settings.setValue(QString("LSP/%1/Path").arg(lang == Cpp ? "Cpp" : "Java"), serverPath);

    qDebug() << "LSP server found:" << serverPath;
    emit lspServerFound(lang, serverPath);
    emit setupComplete(lang, true);

    return true;
}
