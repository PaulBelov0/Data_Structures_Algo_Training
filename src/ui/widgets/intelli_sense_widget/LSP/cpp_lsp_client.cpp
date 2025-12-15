#include "cpp_lsp_client.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

CppLSPClient::CppLSPClient(QObject* parent)
    : LSPClient(parent)
    , m_cppStandard("c++17")
{
    m_language = LanguageCpp;

    // Автоматический поиск clangd
    if (m_serverPath.isEmpty()) {
#ifdef Q_OS_WIN
        QStringList possiblePaths = {
            "clangd.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\Llvm\\bin\\clangd.exe",
            "C:\\Program Files\\LLVM\\bin\\clangd.exe",
            QStandardPaths::findExecutable("clangd")
        };
#else
        QStringList possiblePaths = {
            "clangd",
            "/usr/bin/clangd",
            "/usr/local/bin/clangd"
        };
#endif

        for (const QString& path : possiblePaths) {
            if (QFileInfo::exists(path)) {
                m_serverPath = path;
                break;
            }
        }
    }
}

QStringList CppLSPClient::getDefaultServerArgs() const
{
    QStringList args;

    // Логирование
    args << "--log=verbose";
    args << "--pretty";

    // Команда компиляции если есть
    if (!m_compileCommandsPath.isEmpty()) {
        args << QString("--compile-commands-dir=%1").arg(m_compileCommandsPath);
    }

    // Предельное количество запросов
    args << "--limit-results=100";

    // Background index
    args << "--background-index";

    return args;
}

QJsonObject CppLSPClient::getDefaultInitOptions() const
{
    QJsonObject clangdParams;

    // Настройки clangd
    clangdParams["compilationDatabasePath"] = m_compileCommandsPath;

    QJsonObject fallbackFlags;
    QJsonArray includeFlags;

    for (const QString& path : m_includePaths) {
        includeFlags.append("-I" + path);
    }

    includeFlags.append("-std=" + m_cppStandard);
    includeFlags.append("-Wall");
    includeFlags.append("-Wextra");

    fallbackFlags["compilers"] = QJsonArray{"/usr/bin/gcc", "/usr/bin/clang"};
    fallbackFlags["flags"] = includeFlags;

    clangdParams["fallbackFlags"] = fallbackFlags;

    return QJsonObject{
        {"clangd", clangdParams}
    };
}

void CppLSPClient::applyLanguageSpecificSettings(QJsonObject& initOptions)
{
    // Добавляем настройки для C++
    QJsonObject clangdSettings = initOptions["clangd"].toObject();

    if (clangdSettings.isEmpty()) {
        clangdSettings = getDefaultInitOptions()["clangd"].toObject();
    }

    // Добавляем дополнительные флаги
    QJsonObject fallbackFlags = clangdSettings["fallbackFlags"].toObject();
    QJsonArray flags = fallbackFlags["flags"].toArray();

    // Флаги для работы с Qt если нужно
    flags.append("-DQT_CORE_LIB");
    flags.append("-DQT_GUI_LIB");
    flags.append("-DQT_WIDGETS_LIB");

    fallbackFlags["flags"] = flags;
    clangdSettings["fallbackFlags"] = fallbackFlags;

    initOptions["clangd"] = clangdSettings;
}

void CppLSPClient::setCompilationDatabasePath(const QString& path)
{
    m_compileCommandsPath = path;
}

void CppLSPClient::setCompileCommands(const QJsonArray& commands)
{
    m_compileCommands = commands;

    // Сохраняем в файл compile_commands.json
    if (!m_rootPath.isEmpty()) {
        QString compileCommandsPath = m_rootPath + "/compile_commands.json";
        QFile file(compileCommandsPath);
        if (file.open(QIODevice::WriteOnly)) {
            QJsonDocument doc(commands);
            file.write(doc.toJson());
            file.close();

            m_compileCommandsPath = m_rootPath;
        }
    }
}

void CppLSPClient::addIncludePath(const QString& path)
{
    if (!m_includePaths.contains(path)) {
        m_includePaths.append(path);
    }
}

void CppLSPClient::setCppStandard(const QString& standard)
{
    m_cppStandard = standard;
}
