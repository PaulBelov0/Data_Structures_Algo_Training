#include "code_editor_widget.h"

CodeEditorWidget::CodeEditorWidget(QWidget* parent)
    : QWidget(parent)
    , m_currentLanguage(ModeCpp)
    , m_documentVersion(0)
    , m_autoCompleteEnabled(true)
    , m_syntaxHighlightingEnabled(true)
    , m_errorCheckingEnabled(true)
    , m_errorColor(Qt::red)
    , m_warningColor(QColor(255, 165, 0)) // Оранжевый
    , m_infoColor(Qt::blue)
    , m_currentLineColor(QColor(240, 240, 255))
{
    setupUI();
    setupLSP();

    // Начальный шаблон
    loadTemplate(ModeCpp);
}

CodeEditorWidget::~CodeEditorWidget()
{
    if (m_lspClient) {
        m_lspClient->stop();
        delete m_lspClient;
    }

    if (m_ideIntegrator) {
        delete m_ideIntegrator;
    }
}

void CodeEditorWidget::setupUI()
{
    // Основной layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    // Панель инструментов
    QToolBar* toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));

    // Выбор языка
    QComboBox* languageCombo = new QComboBox(toolBar);
    languageCombo->addItem("C++", ModeCpp);
    languageCombo->addItem("Java", ModeJava);
    languageCombo->setCurrentIndex(0);

    connect(languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, languageCombo](int index) {
                LanguageMode mode = static_cast<LanguageMode>(languageCombo->itemData(index).toInt());
                setLanguage(mode);
                loadTemplate(mode);
            });

    toolBar->addWidget(new QLabel("Language:"));
    toolBar->addWidget(languageCombo);
    toolBar->addSeparator();

    // Кнопки LSP
    QAction* connectAction = toolBar->addAction("Connect LSP");
    connect(connectAction, &QAction::triggered, this, &CodeEditorWidget::connectToLSP);

    QAction* completeAction = toolBar->addAction("Complete");
    completeAction->setShortcut(QKeySequence("Ctrl+Space"));
    connect(completeAction, &QAction::triggered, this, &CodeEditorWidget::showCompletions);

    QAction* formatAction = toolBar->addAction("Format");
    formatAction->setShortcut(QKeySequence("Ctrl+Shift+F"));
    connect(formatAction, &QAction::triggered, this, &CodeEditorWidget::formatDocument);

    toolBar->addSeparator();

    // Кнопки навигации
    QAction* gotoDefAction = toolBar->addAction("Go to Definition");
    gotoDefAction->setShortcut(QKeySequence("F12"));
    connect(gotoDefAction, &QAction::triggered, this, &CodeEditorWidget::gotoDefinition);

    QAction* findRefsAction = toolBar->addAction("Find References");
    findRefsAction->setShortcut(QKeySequence("Shift+F12"));
    connect(findRefsAction, &QAction::triggered, this, &CodeEditorWidget::findReferences);

    mainLayout->addWidget(toolBar);

    // Основной сплиттер
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Редактор кода
    m_editor = new QTextEdit(this);
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_editor->setFontPointSize(12);
    m_editor->setTabStopDistance(40);
    m_editor->setTextColor(QColor(200, 200, 200));

    // Настройка подсветки текущей строки
    updateEditorStyle();

    connect(m_editor, &QTextEdit::textChanged, this, &CodeEditorWidget::onTextChanged);
    connect(m_editor, &QTextEdit::cursorPositionChanged,
            this, &CodeEditorWidget::onCursorPositionChanged);

    // Боковая панель (проблемы и автодополнение)
    m_sidebar = new QWidget(this);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(5);

    // Список автодополнения
    m_autoCompleteWidget = new AutoCompleteWidget(this);
    m_autoCompleteWidget->hide();

    // Подключаем сигналы
    connect(m_autoCompleteWidget, &AutoCompleteWidget::completionSelected,
            this, &CodeEditorWidget::insertCompletion);
    connect(m_autoCompleteWidget, &AutoCompleteWidget::canceled,
            [this]() {
                m_editor->setFocus();
            });

    // Список проблем
    QLabel* problemsLabel = new QLabel("Problems:", this);
    m_problemList = new QListWidget(this);
    m_problemList->setMaximumHeight(200);

    sidebarLayout->addWidget(new QLabel("Completions:", this));
    sidebarLayout->addWidget(m_completionList);
    sidebarLayout->addWidget(problemsLabel);
    sidebarLayout->addWidget(m_problemList);

    // Добавляем в сплиттер
    m_mainSplitter->addWidget(m_editor);
    m_mainSplitter->addWidget(m_sidebar);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_mainSplitter, 1);

    // Статус бар
    m_statusLabel = new QLabel(this);
    m_statusLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_statusLabel->setMaximumHeight(20);
    updateStatus("Ready");

    mainLayout->addWidget(m_statusLabel);

    // Таймер для автодополнения
    m_completionTimer = new QTimer(this);
    m_completionTimer->setSingleShot(true);
    m_completionTimer->setInterval(500); // 500ms задержка
    connect(m_completionTimer, &QTimer::timeout, this, &CodeEditorWidget::onCompletionTimer);

    // Настраиваем текущий файл
    m_currentFile = m_projectPath.isEmpty() ?
                        QDir::tempPath() + "/temp_code.cpp" :
                        m_projectPath + "/temp_code.cpp";
}

void CodeEditorWidget::setupLSP()
{
    m_ideIntegrator = new IDEIntegrator(this);

    // Пока не создаем LSPClient - создадим при подключении
    m_lspClient = new CppLSPClient(this);
}

void CodeEditorWidget::setLanguage(LanguageMode mode)
{
    if (m_currentLanguage == mode) {
        return;
    }

    m_currentLanguage = mode;

    // Обновляем текущий файл
    QString extension = (mode == ModeCpp) ? ".cpp" : ".java";
    m_currentFile = m_projectPath.isEmpty() ?
                        QDir::tempPath() + "/temp_code" + extension :
                        m_projectPath + "/temp_code" + extension;

    // Если LSP клиент существует, пересоздаем его
    if (m_lspClient) {
        m_lspClient->stop();
        delete m_lspClient;
        m_lspClient = nullptr;

        // Переоткрываем документ с новым клиентом
        if (!m_editor->toPlainText().isEmpty()) {
            connectToLSP();
        }
    }

    emit languageChanged(mode);
}

void CodeEditorWidget::loadTemplate(LanguageMode mode)
{
    QString templateCode;

    if (mode == ModeCpp) {
        templateCode =
            R"(#include <iostream>

void balance()
{
    //... Your code
}

int main() {

    balance();

    return 0;
})";
    }
    else if (mode == ModeJava)
    {
        templateCode =
            R"(import java.util.*;

public class Main {
    public static void main(String[] args) {
        balance();
    }

    private void balance(){
    //...Your code
    }
})";
    }

    m_editor->clear();
    m_editor->setPlainText(templateCode);
    m_documentVersion = 0;
}

QString CodeEditorWidget::currentCode() const
{
    return m_editor->toPlainText();
}

void CodeEditorWidget::setCurrentCode(const QString& code)
{
    m_editor->setPlainText(code);
    m_documentVersion = 0;

    if (m_lspClient && m_lspClient->isConnected()) {
        m_lspClient->openDocument(m_currentFile, code);
    }
}

void CodeEditorWidget::setProjectPath(const QString& path)
{
    if (m_projectPath == path) {
        return;
    }

    m_projectPath = path;

    // Обновляем текущий файл
    QString extension = (m_currentLanguage == ModeCpp) ? ".cpp" : ".java";
    m_currentFile = m_projectPath.isEmpty() ?
                        QDir::tempPath() + "/temp_code" + extension :
                        m_projectPath + "/temp_code" + extension;

    // Переподключаем LSP если был подключен
    if (m_lspClient && m_lspClient->isConnected()) {
        m_lspClient->stop();
        m_lspClient->start(m_projectPath);
    }
}

void CodeEditorWidget::onTextChanged()
{
    QString code = m_editor->toPlainText();
    m_documentVersion++;

    emit codeChanged(code);

    // Обновляем документ в LSP если подключен
    if (m_lspClient && m_lspClient->isConnected()) {
        m_lspClient->updateDocument(m_currentFile, code, m_documentVersion);
    }

    // Запускаем таймер для автодополнения
    if (m_autoCompleteEnabled) {
        m_completionTimer->start();
    }
}

void CodeEditorWidget::showCompletions()
{
    if (!m_lspClient || !m_lspClient->isConnected()) {
        updateStatus("LSP not connected", Qt::red);
        return;
    }

    QTextCursor cursor = m_editor->textCursor();
    int line = cursor.blockNumber();
    int character = cursor.positionInBlock();

    m_lspClient->requestCompletion(m_currentFile, line, character);
}

void CodeEditorWidget::showSignatureHelp()
{
    if (!m_lspClient || !m_lspClient->isConnected()) {
        return;
    }

    QTextCursor cursor = m_editor->textCursor();
    int line = cursor.blockNumber();
    int character = cursor.positionInBlock();

    // В LSPClient нужно добавить метод requestSignatureHelp
    // m_lspClient->requestSignatureHelp(m_currentFile, line, character);
}

void CodeEditorWidget::gotoDefinition()
{
    if (!m_lspClient || !m_lspClient->isConnected()) {
        updateStatus("LSP not connected", Qt::red);
        return;
    }

    QTextCursor cursor = m_editor->textCursor();
    int line = cursor.blockNumber();
    int character = cursor.positionInBlock();

    m_lspClient->requestDefinition(m_currentFile, line, character);
}

void CodeEditorWidget::findReferences()
{
    if (!m_lspClient || !m_lspClient->isConnected()) {
        updateStatus("LSP not connected", Qt::red);
        return;
    }

    QTextCursor cursor = m_editor->textCursor();
    int line = cursor.blockNumber();
    int character = cursor.positionInBlock();

    m_lspClient->requestReferences(m_currentFile, line, character);
}

void CodeEditorWidget::formatDocument()
{
    if (!m_lspClient || !m_lspClient->isConnected()) {
        updateStatus("LSP not connected", Qt::red);
        return;
    }

    m_lspClient->requestFormatting(m_currentFile);
}

void CodeEditorWidget::connectToLSP()
{
    qDebug() << "=== Starting LSP Connection ===";
    qDebug() << "Current language:" << m_currentLanguage;
    qDebug() << "Project path:" << m_projectPath;
    qDebug() << "Current file:" << m_currentFile;

    if (m_lspClient) {
        if (m_lspClient->isConnected()) {
            qDebug() << "Already connected, state:" << m_lspClient->state();
            updateStatus("Already connected", Qt::blue);
            return;
        }

        qDebug() << "Stopping existing LSP client";
        m_lspClient->stop();
        delete m_lspClient;
        m_lspClient = nullptr;
    }

    // Создаем соответствующий LSP клиент
    LSPClient::Language lspLang = getCurrentLSPLanguage();
    qDebug() << "LSP Language enum:" << lspLang;

    // Показываем, какой клиент создаётся
    QString clientName = "Unknown";
    if (lspLang == LSPClient::LanguageCpp)
    {
        clientName = "C++";
        m_lspClient = new CppLSPClient(this);
        qDebug() << "Created CppLSPClient";

        // Проверяем путь к clangd
        CppLSPClient* cppClient = qobject_cast<CppLSPClient*>(m_lspClient);
        if (cppClient) {
            qDebug() << "Clangd path:" << cppClient->getServerPath();
            qDebug() << "Clangd args:" << cppClient->getServerArgs();

            // ЕСЛИ CLANGD НЕ НАЙДЕН - ПРЕДЛАГАЕМ УСТАНОВКУ
            if (cppClient->getServerPath().isEmpty()) {
                qDebug() << "Clangd not found, offering installation";

                QMessageBox msgBox;
                msgBox.setWindowTitle("Clangd Not Found");
                msgBox.setText("Clangd (C++ Language Server) not found.");
                msgBox.setInformativeText("Install automatically?");
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                msgBox.setDefaultButton(QMessageBox::Yes);

                if (msgBox.exec() == QMessageBox::Yes) {
                    // Подключаем сигнал завершения установки
                    connect(cppClient, &CppLSPClient::clangdInstallationFinished,
                            this, [this](bool success, const QString& path) {
                                if (success) {
                                    qDebug() << "Clangd installed, reconnecting...";
                                    QTimer::singleShot(2000, this, &CodeEditorWidget::connectToLSP);
                                }
                            });

                    // Запускаем установку
                    cppClient->installClangd();
                    updateStatus("Installing Clangd...", QColor(255, 165, 0));
                    return; // Выходим - установка запущена
                } else {
                    updateStatus("Clangd installation cancelled", Qt::yellow);
                    delete m_lspClient;
                    m_lspClient = nullptr;
                    return;
                }
            }
        }
    }
    else if (lspLang == LSPClient::LanguageJava)
    {
        clientName = "Java";
        m_lspClient = new JavaLSPClient(this);
        qDebug() << "Created JavaLSPClient";

        JavaLSPClient* javaClient = qobject_cast<JavaLSPClient*>(m_lspClient);
        if (javaClient) {
            qDebug() << "JDT LS path:" << javaClient->getServerPath();
            qDebug() << "Java home:" << javaClient->getJavaHome();
        }
    }
    else
    {
        qWarning() << "Unknown language for LSP";
        updateStatus("Unknown language for LSP", Qt::red);
        return;
    }

    if (!m_lspClient) {
        qCritical() << "Failed to create LSP client";
        updateStatus("Failed to create LSP client", Qt::red);
        return;
    }

    // Подключаем сигналы
    connect(m_lspClient, &LSPClient::stateChanged,
            [this](LSPClient::State state) {
                qDebug() << "LSP State changed to:" << state;
                bool connected = (state == LSPClient::Connected);
                onLSPConnected(connected);

                if (state == LSPClient::Error) {
                    updateStatus("LSP error: " + m_lspClient->errorString(), Qt::red);
                }
            });

    connect(m_lspClient, &LSPClient::logMessage,
            [this](const QString& message, const QString& type) {
                qDebug() << "LSP [" << type << "]:" << message;
                if (type == "error") {
                    updateStatus("LSP: " + message, Qt::red);
                }
            });

    connect(m_lspClient, &LSPClient::errorOccurred,
            [this](const QString& error) {
                qWarning() << "LSP error:" << error;
                updateStatus("LSP error: " + error, Qt::red);
                emit errorOccurred(error);
            });

    connect(m_lspClient, &LSPClient::completionReady, [this, lspLang](const QString& fileUri, int line, int character, const QList<LSPCompletionItem>& items){
        onCompletionReady(lspLang, fileUri, line, character, items);
    });

    connect(m_lspClient, &LSPClient::diagnosticsUpdated, [this, lspLang](const QString& fileUri, const QJsonArray& diagnostics){
        onDiagnosticsUpdated(lspLang, fileUri, diagnostics);
    });

    connect(m_lspClient, &LSPClient::hoverReady, [this, lspLang](const QString& fileUri, int line, int character, const QString& content){
        onHoverReady(lspLang, fileUri, line, character, content);
    });

    qDebug() << "Attempting to start LSP client for" << clientName;

    QString workspace = m_projectPath.isEmpty() ? QDir::currentPath() : m_projectPath;
    qDebug() << "Workspace:" << workspace;

    if (m_lspClient->start(workspace)) {
        qDebug() << "LSP client start() returned true";
        updateStatus("Connecting to " + clientName + " LSP...", QColor(255, 165, 0));

        // Проверяем состояние процесса
        QTimer::singleShot(1000, this, [this, clientName]() {
            qDebug() << "Checking LSP state after 1 second...";
            if (m_lspClient) {
                qDebug() << "Current state:" << m_lspClient->state();
                qDebug() << "Is connected:" << m_lspClient->isConnected();
                qDebug() << "Error string:" << m_lspClient->errorString();

                if (m_lspClient->isConnected()) {
                    // Открываем текущий документ
                    QString code = m_editor->toPlainText();
                    m_lspClient->openDocument(m_currentFile, code);
                    updateStatus(clientName + " LSP connected", Qt::darkGreen);
                }
            }
        });
    } else {
        qWarning() << "LSP client start() failed";
        updateStatus("Failed to start " + clientName + " LSP: " + m_lspClient->errorString(), Qt::red);
    }
}

void CodeEditorWidget::onCompletionReady(LSPClient::Language language, const QString& fileUri,
                                         int line, int character, const QList<LSPCompletionItem>& items)
{
    Q_UNUSED(fileUri);

    // Проверяем, что это для текущего языка
    if (language != getCurrentLSPLanguage()) {
        return;
    }

    m_currentCompletions = items;

    if (items.isEmpty()) {
        m_completionList->clear();
        m_completionList->setVisible(false);
        return;
    }

    // Показываем список автодополнений
    showCompletionPopup(items);
}

void CodeEditorWidget::onHoverReady(LSPClient::Language language, const QString& fileUri,
                                    int line, int character, const QString& content)
{
    Q_UNUSED(fileUri);
    Q_UNUSED(line);
    Q_UNUSED(character);

    if (language != getCurrentLSPLanguage()) {
        return;
    }

    // Показываем подсказку во всплывающем окне или статус баре
    if (!content.isEmpty()) {
        QString shortContent = content;
        if (shortContent.length() > 100) {
            shortContent = shortContent.left(100) + "...";
        }
        updateStatus(shortContent, Qt::darkBlue);

        // Можно показать tooltip
        QToolTip::showText(m_editor->mapToGlobal(QPoint(10, 10)), content, m_editor);
    }
}

void CodeEditorWidget::onDiagnosticsUpdated(LSPClient::Language language, const QString& fileUri,
                                            const QJsonArray& diagnostics)
{
    Q_UNUSED(fileUri);

    if (language != getCurrentLSPLanguage()) {
        return;
    }

    m_problemList->clear();

    for (const QJsonValue& diagnostic : diagnostics) {
        QJsonObject obj = diagnostic.toObject();

        QString severity = obj["severity"].toString();
        QString message = obj["message"].toString();
        QJsonObject range = obj["range"].toObject();
        QJsonObject start = range["start"].toObject();

        int line = start["line"].toInt() + 1; // LSP uses 0-based, we show 1-based
        int character = start["character"].toInt() + 1;

        QString itemText = QString("Line %1:%2 - %3: %4")
                               .arg(line)
                               .arg(character)
                               .arg(severity)
                               .arg(message);

        QListWidgetItem* item = new QListWidgetItem(itemText, m_problemList);

        // Цвета в зависимости от серьезности
        if (severity == "error") {
            item->setForeground(m_errorColor);
        } else if (severity == "warning") {
            item->setForeground(m_warningColor);
        } else {
            item->setForeground(m_infoColor);
        }

        // Сохраняем позицию для навигации
        item->setData(Qt::UserRole, QPoint(line - 1, character - 1));
    }

    // Обновляем маркеры в редакторе
    updateDiagnostics();

    if (diagnostics.isEmpty()) {
        updateStatus("No problems found", Qt::darkGreen);
    } else {
        updateStatus(QString("Found %1 problems").arg(diagnostics.size()), Qt::red);
    }
}

void CodeEditorWidget::onClientStateChanged(LSPClient::Language language, LSPClient::State state)
{
    Q_UNUSED(language);

    bool connected = (state == LSPClient::Connected);
    onLSPConnected(connected);

    if (state == LSPClient::Error && m_lspClient) {
        updateStatus("LSP error: " + m_lspClient->errorString(), Qt::red);
    }
}

void CodeEditorWidget::onLogMessage(const QString& message, const QString& type)
{
    qDebug() << "LSP" << type << ":" << message;

    if (type == "error") {
        updateStatus("LSP: " + message, Qt::red);
    }
}

void CodeEditorWidget::onCompletionTimer()
{
    if (!m_autoCompleteEnabled || !m_lspClient || !m_lspClient->isConnected()) {
        return;
    }

    // Получаем текущее слово
    QString word = getCurrentWord();
    if (word.length() < 2) { // Минимальная длина для автодополнения
        return;
    }

    QTextCursor cursor = m_editor->textCursor();
    int line = cursor.blockNumber();
    int character = cursor.positionInBlock();

    m_lspClient->requestCompletion(m_currentFile, line, character);
}

void CodeEditorWidget::onCursorPositionChanged()
{
    // Подсветка текущей строки
    updateEditorStyle();

    // Показываем информацию о текущей позиции
    QTextCursor cursor = m_editor->textCursor();
    int line = cursor.blockNumber() + 1;
    int column = cursor.positionInBlock() + 1;

    QString position = QString("Line %1, Col %2").arg(line).arg(column);
    m_statusLabel->setText(position + " | " + m_statusLabel->text().split(" | ").last());
}

void CodeEditorWidget::onCompletionsDoubleClicked(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    QString completion = item->text().split(" ").first(); // Берем первое слово (имя)
    insertCompletion(completion);
}

void CodeEditorWidget::applyCompletion(const QString& completion)
{
    QTextCursor cursor = m_editor->textCursor();

    // Удаляем текущее частично написанное слово
    cursor.movePosition(QTextCursor::StartOfWord);
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    // Вставляем завершение
    cursor.insertText(completion);
    m_editor->setTextCursor(cursor);
}

void CodeEditorWidget::updateDiagnostics()
{
    if (!m_errorCheckingEnabled) {
        return;
    }

    // Очищаем предыдущие выделения
    m_editor->setExtraSelections({});

    QList<QTextEdit::ExtraSelection> extraSelections;

    // Добавляем подсветку текущей строки
    QTextEdit::ExtraSelection currentLine;
    currentLine.format.setBackground(m_currentLineColor);
    currentLine.format.setProperty(QTextFormat::FullWidthSelection, true);
    currentLine.cursor = m_editor->textCursor();
    currentLine.cursor.clearSelection();
    extraSelections.append(currentLine);

    // Можно добавить подсветку синтаксиса здесь

    m_editor->setExtraSelections(extraSelections);
}

void CodeEditorWidget::updateEditorStyle()
{
    QFont font("Consolas", 11);
    if (QFontInfo(font).fixedPitch()) {
        m_editor->setFont(font);
    }

    QPalette palette = m_editor->palette();
    palette.setColor(QPalette::Base, QColor(30, 30, 30));
    palette.setColor(QPalette::Text, QColor(180, 180, 180));
    m_editor->setPalette(palette);
}

void CodeEditorWidget::showCompletionPopup(const QList<LSPCompletionItem>& items)
{
    if (items.isEmpty()) {
        m_autoCompleteWidget->hideCompletions();
        return;
    }

    // Преобразуем LSPCompletionItem в CompletionItem
    QList<CompletionItem> completions;
    for (const LSPCompletionItem& lspItem : items) {
        CompletionItem item;
        item.label = lspItem.label;
        item.detail = lspItem.detail;
        item.kind = lspItem.kind;
        item.documentation = lspItem.documentation;
        completions.append(item);
    }

    // Получаем позицию курсора
    QTextCursor cursor = m_editor->textCursor();
    QRect cursorRect = m_editor->cursorRect(cursor);

    // Показываем автодополнение
    m_autoCompleteWidget->showCompletions(completions, cursorRect, m_editor);
}

void CodeEditorWidget::insertCompletion(const QString& completion)
{
    applyCompletion(completion);
    m_autoCompleteWidget->hideCompletions();
    m_editor->setFocus();
}

QString CodeEditorWidget::getCurrentWord() const
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

bool CodeEditorWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_editor && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        // Если автодополнение видимо, обрабатываем навигацию
        if (m_autoCompleteWidget->isVisible()) {
            switch (keyEvent->key()) {
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
            case Qt::Key_Escape:
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Tab:
                m_autoCompleteWidget->setFocus();
                QCoreApplication::sendEvent(m_autoCompleteWidget, keyEvent);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

LSPClient::Language CodeEditorWidget::getCurrentLSPLanguage() const
{
    if (m_currentLanguage == ModeCpp) {
        return LSPClient::LanguageCpp;
    } else if (m_currentLanguage == ModeJava) {
        return LSPClient::LanguageJava;
    }
    return LSPClient::LanguageUnknown;
}

QString CodeEditorWidget::getCurrentFilePath() const
{
    return m_currentFile;
}

void CodeEditorWidget::updateStatus(const QString& message, const QColor& color)
{
    QString html = QString("<span style='color: %1;'>%2</span>")
    .arg(color.name())
        .arg(message);

    m_statusLabel->setText(html);

    // Автоматически очищаем через 5 секунд если это не ошибка
    if (color != Qt::red) {
        QTimer::singleShot(5000, this, [this, originalText = m_statusLabel->text()]() {
            if (m_statusLabel->text() == originalText) {
                // Оставляем только позицию курсора
                QTextCursor cursor = m_editor->textCursor();
                int line = cursor.blockNumber() + 1;
                int column = cursor.positionInBlock() + 1;
                m_statusLabel->setText(QString("Line %1, Col %2").arg(line).arg(column));
            }
        });
    }
}

void CodeEditorWidget::onLSPConnected(bool connected)
{
    emit intelliSenseReady(connected);

    if (connected) {
        updateStatus("LSP connected", Qt::darkGreen);

        // Открываем текущий документ
        if (m_lspClient) {
            m_lspClient->openDocument(m_currentFile, m_editor->toPlainText());
        }
    } else {
        updateStatus("LSP disconnected", Qt::red);
    }
}
