#ifndef CODE_EDITOR_WIDGET_H
#define CODE_EDITOR_WIDGET_H

#ifndef LSPCODEEDITOR_H
#define LSPCODEEDITOR_H

#include <QWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QListWidget>

#include "../integration/ide_integrator.h"
#include "../LSP/lsp_client.h"

class LSPCodeEditor : public QWidget
{
    Q_OBJECT

public:
    enum LanguageMode {
        ModeCpp,
        ModeJava,
        ModeAuto
    };

    explicit LSPCodeEditor(QWidget* parent = nullptr);
    ~LSPCodeEditor();

    void setLanguage(LanguageMode mode);
    void loadTemplate(LanguageMode mode);

    QString currentCode() const;
    void setCurrentCode(const QString& code);

    void setProjectPath(const QString& path);

signals:
    void codeChanged(const QString& code);
    void languageChanged(LanguageMode mode);
    void intelliSenseReady(bool ready);

public slots:
    void onTextChanged();
    void showCompletions();
    void showSignatureHelp();
    void gotoDefinition();
    void findReferences();
    void formatDocument();

private slots:
    void onCompletionReady(const QJsonArray& items);
    void onSignatureHelpReady(const QJsonObject& help);
    void onHoverReady(const QString& content);
    void onLSPConnected(bool connected);

private:
    void setupUI();
    void setupLSP();
    void applyCompletion(const QString& completion);
    void updateDiagnostics();

    QTextEdit* m_editor;
    QListWidget* m_completionList;
    QListWidget* m_problemList;
    QLabel* m_statusLabel;

    IDEIntegrator* m_ideIntegrator;
    LSPClient* m_lspClient;

    LanguageMode m_currentLanguage;
    QString m_projectPath;

    // Для автодополнения
    QStringList m_completionCache;
    QTimer* m_completionTimer;
};

#endif // LSPCODEEDITOR_H
#endif // CODE_EDITOR_WIDGET_H
