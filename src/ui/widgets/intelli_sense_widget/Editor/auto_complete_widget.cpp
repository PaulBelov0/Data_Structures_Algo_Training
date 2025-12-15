#include "auto_complete_widget.h"

AutoCompleteWidget::AutoCompleteWidget(QWidget *parent)
    : QListWidget(parent)
    , m_hideTimer(new QTimer(this))
    , m_editor(nullptr)
{
    setupUI();
}

AutoCompleteWidget::~AutoCompleteWidget()
{
}

void AutoCompleteWidget::setupUI()
{
    // Настройка внешнего вида
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setUniformItemSizes(true);
    setMaximumHeight(300);
    setMinimumWidth(300);

    // Стиль
    setStyleSheet(R"(
        QListWidget {
            background-color: #2b2b2b;
            color: #a9b7c6;
            border: 1px solid #3c3f41;
            border-radius: 4px;
            padding: 2px;
        }
        QListWidget::item {
            padding: 4px 8px;
            border-radius: 2px;
        }
        QListWidget::item:selected {
            background-color: #214283;
            color: white;
        }
        QListWidget::item:hover:!selected {
            background-color: #323232;
        }
        QScrollBar:vertical {
            background: #2b2b2b;
            width: 12px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #606060;
            border-radius: 6px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #707070;
        }
    )");

    // Настройка таймера скрытия
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(200);
    connect(m_hideTimer, &QTimer::timeout, this, &AutoCompleteWidget::hideCompletions);

    // Подключение сигналов
    connect(this, &QListWidget::itemDoubleClicked,
            this, &AutoCompleteWidget::onItemDoubleClicked);
}

void AutoCompleteWidget::showCompletions(const QList<CompletionItem>& items,
                                         const QRect& cursorRect,
                                         QWidget* parentWidget)
{
    if (items.isEmpty()) {
        hideCompletions();
        return;
    }

    clear();
    m_currentItems = items;

    // Добавляем элементы в список
    for (const CompletionItem& item : items) {
        QString displayText = item.label;

        // Добавляем детали если есть
        if (!item.detail.isEmpty()) {
            QString detail = item.detail;
            // Обрезаем слишком длинные детали
            if (detail.length() > 50) {
                detail = detail.left(47) + "...";
            }
            displayText += " - " + detail;
        }

        QListWidgetItem* listItem = new QListWidgetItem(displayText, this);

        // Можно добавить иконки в зависимости от типа
        // Например: функция, переменная, класс и т.д.
        listItem->setData(Qt::UserRole, item.label);
        listItem->setData(Qt::UserRole + 1, item.documentation);

        // Цвет в зависимости от типа
        QColor itemColor = Qt::white;
        if (item.kind.contains("function", Qt::CaseInsensitive)) {
            itemColor = QColor(0, 220, 220); // Голубой для функций
        } else if (item.kind.contains("variable", Qt::CaseInsensitive)) {
            itemColor = QColor(200, 200, 100); // Жёлтый для переменных
        } else if (item.kind.contains("class", Qt::CaseInsensitive) ||
                   item.kind.contains("struct", Qt::CaseInsensitive)) {
            itemColor = QColor(100, 200, 100); // Зелёный для классов
        } else if (item.kind.contains("keyword", Qt::CaseInsensitive)) {
            itemColor = QColor(200, 100, 200); // Фиолетовый для ключевых слов
        }

        listItem->setForeground(itemColor);
    }

    // Настраиваем размер
    int itemHeight = sizeHintForRow(0);
    int visibleItems = qMin(10, items.count());
    int height = itemHeight * visibleItems + 10; // + padding

    // Рассчитываем позицию (под курсором)
    QPoint globalPos = parentWidget->mapToGlobal(cursorRect.bottomLeft());

    // Исправление: используем QScreen вместо QDesktopWidget
    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) {
        // Если курсор за пределами экранов, используем основной экран
        screen = QGuiApplication::primaryScreen();
    }

    QRect screenRect = screen->availableGeometry();

    // Проверяем, чтобы не выйти за границы экрана
    if (globalPos.y() + height > screenRect.bottom()) {
        // Показываем над курсором
        globalPos = parentWidget->mapToGlobal(cursorRect.topLeft());
        globalPos.setY(globalPos.y() - height);
    }

    // Проверяем, чтобы не выйти за левую/правую границы
    if (globalPos.x() + width() > screenRect.right()) {
        globalPos.setX(screenRect.right() - width());
    }
    if (globalPos.x() < screenRect.left()) {
        globalPos.setX(screenRect.left());
    }

    // Устанавливаем позицию и размер
    move(globalPos);
    resize(400, height);

    // Выбираем первый элемент
    if (count() > 0) {
        setCurrentRow(0);
        scrollToTop();
    }

    // Показываем и устанавливаем фокус
    show();
    raise();
    setFocus();

    // Сбрасываем таймер скрытия
    m_hideTimer->stop();
}

void AutoCompleteWidget::hideCompletions()
{
    clear();
    m_currentItems.clear();
    hide();
    emit canceled();
}

void AutoCompleteWidget::insertSelectedCompletion()
{
    if (currentItem() && !m_currentItems.isEmpty()) {
        int index = currentRow();
        if (index >= 0 && index < m_currentItems.size()) {
            QString completion = m_currentItems[index].label;
            emit completionSelected(completion);
            hideCompletions();
        }
    }
}

void AutoCompleteWidget::moveSelectionUp()
{
    if (currentRow() > 0) {
        setCurrentRow(currentRow() - 1);
    } else {
        setCurrentRow(count() - 1);
    }
}

void AutoCompleteWidget::moveSelectionDown()
{
    if (currentRow() < count() - 1) {
        setCurrentRow(currentRow() + 1);
    } else {
        setCurrentRow(0);
    }
}

void AutoCompleteWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        hideCompletions();
        event->accept();
        break;

    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Tab:
        insertSelectedCompletion();
        event->accept();
        break;

    case Qt::Key_Up:
        moveSelectionUp();
        event->accept();
        break;

    case Qt::Key_Down:
        moveSelectionDown();
        event->accept();
        break;

    case Qt::Key_PageUp:
        for (int i = 0; i < 5 && currentRow() > 0; ++i) {
            moveSelectionUp();
        }
        event->accept();
        break;

    case Qt::Key_PageDown:
        for (int i = 0; i < 5 && currentRow() < count() - 1; ++i) {
            moveSelectionDown();
        }
        event->accept();
        break;

    case Qt::Key_Home:
        setCurrentRow(0);
        event->accept();
        break;

    case Qt::Key_End:
        setCurrentRow(count() - 1);
        event->accept();
        break;

    default:
        // Передаем остальные клавиши родительскому виджету
        if (m_editor) {
            m_editor->setFocus();
            QCoreApplication::sendEvent(m_editor, event);
        }
        QListWidget::keyPressEvent(event);
        break;
    }
}

void AutoCompleteWidget::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event)
    // Не скрываем сразу, даем время на клик мышкой
    m_hideTimer->start();
}

void AutoCompleteWidget::onItemDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item)
    insertSelectedCompletion();
}

QString AutoCompleteWidget::getFullCompletionText(int index) const
{
    if (index >= 0 && index < m_currentItems.size()) {
        return m_currentItems[index].label;
    }
    return QString();
}
