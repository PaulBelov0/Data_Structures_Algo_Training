#ifndef CLANGDINSTALLER_H
#define CLANGDINSTALLER_H

#include "lsp_installer.h"
#include <QRegularExpression>
#include <QDirIterator>
#include <QStorageInfo>

class ClangdInstaller : public LSPInstaller
{
    Q_OBJECT

public:
    explicit ClangdInstaller(QObject *parent = nullptr);

    // LSPInstaller interface
    InstallResult install() override;
    bool isInstalled() const override;
    QString getInstallPath() const override;
    QString getName() const override { return "Clangd (C++ LSP)"; }

    // Дополнительные методы
    QString getDetectedVersion() const;
    QString getLatestVersion() const;

    // Получение информации о системе
    static QString getSystemInfo();

    // Проверка минимальных требований
    static bool checkSystemRequirements(QString& errorMessage);

    // Метод для быстрой проверки и установки
    static InstallResult checkAndInstall(QWidget* parent = nullptr);

    // Метод для получения пути к clangd с автоматической установкой
    static QString getClangdPathWithAutoInstall(bool autoInstall = true);

    LSPInstaller::InstallResult installLinuxViaLLVM();

private:
    QString m_installDir;
    QString m_version;

    InstallResult installWindows();
    InstallResult installLinux();
    InstallResult installMacOS();

    QString getWindowsDownloadUrl() const;
    bool verifyInstallation() const;
};

#endif // CLANGDINSTALLER_H
