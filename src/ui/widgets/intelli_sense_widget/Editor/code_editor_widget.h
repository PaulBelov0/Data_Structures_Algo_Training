#ifndef CODEEDITORWIDGET_H
#define CODEEDITORWIDGET_H

#include <QWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QListWidget>
#include <QLabel>
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QToolTip>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QTextCursor>
#include <QScrollBar>
#include <QToolBar>
#include <QAction>
#include <QFontDatabase>
#include <QPalette>
#include <QApplication>
#include <QShortcut>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QPointer>

#include "../integration/ide_integrator.h"
#include "../LSP/LSP.hpp"
#include "auto_complete_widget.h"

enum LanguageMode {
    ModeCpp,
    ModeJava,
};

class CodeEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CodeEditorWidget(QWidget* parent = nullptr);
    ~CodeEditorWidget();

    void setLanguage(LanguageMode mode);
    void loadTemplate(LanguageMode mode);

    QString currentCode() const;
    void setCurrentCode(const QString& code);

    void setProjectPath(const QString& path);

signals:
    void codeChanged(const QString& code);
    void languageChanged(LanguageMode mode);
    void intelliSenseReady(bool ready);
    void errorOccurred(const QString& error);

public slots:
    void onTextChanged();
    void showCompletions();
    void showSignatureHelp();
    void gotoDefinition();
    void findReferences();
    void formatDocument();
    void connectToLSP();

private slots:
    void onCompletionReady(LSPClient::Language language, const QString& fileUri,
                           int line, int character, const QList<LSPCompletionItem>& items);

    void onHoverReady(LSPClient::Language language, const QString& fileUri,
                      int line, int character, const QString& content);

    void onDiagnosticsUpdated(LSPClient::Language language, const QString& fileUri,
                              const QJsonArray& diagnostics);

    void onClientStateChanged(LSPClient::Language language, LSPClient::State state);
    void onLogMessage(const QString& message, const QString& type);
    void onCompletionTimer();
    void onCursorPositionChanged();
    void onCompletionsDoubleClicked(QListWidgetItem* item);
    void onLSPConnected(bool connected);

private:
    void setupUI();
    void setupLSP();
    void applyCompletion(const QString& completion);
    void updateDiagnostics();
    void updateErrorMarkers();
    void updateEditorStyle();
    void showCompletionPopup(const QList<LSPCompletionItem>& items);
    void insertCompletion(const QString& completion);
    QString getCurrentWord() const;
    LSPClient::Language getCurrentLSPLanguage() const;
    QString getCurrentFilePath() const;
    void updateStatus(const QString& message, const QColor& color = Qt::black);
    bool eventFilter(QObject* obj, QEvent* event);

    // UI элементы
    QSplitter* m_mainSplitter;
    QTextEdit* m_editor;
    QListWidget* m_completionList;
    QListWidget* m_problemList;
    QLabel* m_statusLabel;
    QWidget* m_sidebar;

    // LSP
    QPointer<LSPClient> m_lspClient;
    IDEIntegrator* m_ideIntegrator;

    QPointer<AutoCompleteWidget> m_autoCompleteWidget;

    // Состояние
    LanguageMode m_currentLanguage;
    QString m_projectPath;
    QString m_currentFile;
    int m_documentVersion;

    // Autocomplete
    QTimer* m_completionTimer;
    QList<LSPCompletionItem> m_currentCompletions;
    int m_completionStartPos;

    // Diagnostics
    QList<QTextEdit::ExtraSelection> m_errorSelections;
    QList<QTextEdit::ExtraSelection> m_warningSelections;
    QList<QTextEdit::ExtraSelection> m_infoSelections;

    // settings
    bool m_autoCompleteEnabled;
    bool m_syntaxHighlightingEnabled;
    bool m_errorCheckingEnabled;

    // colors
    QColor m_errorColor;
    QColor m_warningColor;
    QColor m_infoColor;
    QColor m_currentLineColor;
};

#endif // CODEEDITORWIDGET_H
