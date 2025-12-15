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
