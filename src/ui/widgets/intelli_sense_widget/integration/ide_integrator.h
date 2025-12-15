#ifndef IDE_INTEGRATOR_H
#define IDE_INTEGRATOR_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QProcessEnvironment>
#include <QMessageBox>

class IDEIntegrator : public QObject
{
    Q_OBJECT

public:
    enum Language {
        Cpp,
        Java,
        Python,
        JavaScript
    };

    struct IDEPath {
        QString name;
        QString path;
        QString version;
        QString lspServerPath;
    };

    explicit IDEIntegrator(QObject* parent = nullptr);

    QMap<Language, QList<IDEPath>> findInstalledIDEs();

    QString findLSPServer(Language lang, const QString& preferredIDE = "");

    QString getEnvPath(const QString& envVar);

    QString findInRegistry(Language lang);

    QString findWithWinget(const QString& package);

    bool checkProgramAvailable(const QString& program, const QStringList& args = {"--version"});

    bool autoSetupLanguageServer(Language lang);

signals:
    void ideFound(Language lang, const IDEPath& ide);
    void lspServerFound(Language lang, const QString& path);
    void setupComplete(Language lang, bool success);

private:
    QList<IDEPath> findWindowsIDEs(Language lang);

    QList<IDEPath> findLinuxIDEs(Language lang);

    QList<IDEPath> findMacOSIDEs(Language lang);

    QString checkStandardPaths(Language lang);

    QProcessEnvironment m_env;
};


#endif // IDE_INTEGRATOR_H
