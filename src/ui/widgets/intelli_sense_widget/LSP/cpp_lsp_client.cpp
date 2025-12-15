#include "cpp_lsp_client.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QSettings>

CppLSPClient::CppLSPClient(QObject* parent)
    : LSPClient(parent)
{
    m_language = LanguageCpp;

    QSettings settings;
    QString savedPath = settings.value("LSP/Cpp/ClangdPath", "").toString();

    if (!savedPath.isEmpty() && QFileInfo::exists(savedPath)) {
        m_serverPath = savedPath;
    } else {
        QString clangdPath;

#ifdef Q_OS_WIN
        clangdPath = QStandardPaths::findExecutable("clangd.exe");
        if (clangdPath.isEmpty()) {
            clangdPath = "C:\\Program Files\\LLVM\\bin\\clangd.exe";
        }
#else
        clangdPath = QStandardPaths::findExecutable("clangd");
#endif

        if (QFileInfo::exists(clangdPath)) {
            m_serverPath = clangdPath;
            settings.setValue("LSP/Cpp/ClangdPath", clangdPath);
        }
    }
}

CppLSPClient::~CppLSPClient()
{
    if (installer) installer->deleteLater();
}

QStringList CppLSPClient::getDefaultServerArgs() const
{
    QStringList args;
    args << "--log=verbose";
    args << "--pretty";

    if (!m_compileCommandsPath.isEmpty()) {
        args << "--compile-commands-dir=" + m_compileCommandsPath;
    }

    args << "--limit-results=100";
    args << "--background-index";
    return args;
}

QJsonObject CppLSPClient::getDefaultInitOptions() const
{
    QJsonObject clangdParams;
    clangdParams["compilationDatabasePath"] = m_compileCommandsPath;

    QJsonObject fallbackFlags;
    QJsonArray includeFlags;

    for (const QString& path : m_includePaths) {
        includeFlags.append("-I" + path);
    }

    includeFlags.append("-std=" + m_cppStandard);
    includeFlags.append("-Wall");
    includeFlags.append("-Wextra");

    fallbackFlags["flags"] = includeFlags;
    clangdParams["fallbackFlags"] = fallbackFlags;

    return QJsonObject{{"clangd", clangdParams}};
}

void CppLSPClient::applyLanguageSpecificSettings(QJsonObject& initOptions)
{
    QJsonObject clangdSettings = initOptions["clangd"].toObject();

    if (clangdSettings.isEmpty()) {
        clangdSettings = getDefaultInitOptions()["clangd"].toObject();
    }

    QJsonObject fallbackFlags = clangdSettings["fallbackFlags"].toObject();
    QJsonArray flags = fallbackFlags["flags"].toArray();

    flags.append("-DQT_CORE_LIB");
    flags.append("-DQT_GUI_LIB");
    flags.append("-DQT_WIDGETS_LIB");

    fallbackFlags["flags"] = flags;
    clangdSettings["fallbackFlags"] = fallbackFlags;
    initOptions["clangd"] = clangdSettings;
}

void CppLSPClient::installClangd()
{
    if (!installer) {
        installer = new ClangdInstaller(this);
        connect(installer, &ClangdInstaller::finished, this, [this](LSPInstaller::InstallResult result, const QString& msg) {
            if (result == LSPInstaller::Success) {
                m_serverPath = installer->getInstallPath();
                QSettings().setValue("LSP/Cpp/ClangdPath", m_serverPath);
                restart();
            }
        });
    }
    installer->install();
}
