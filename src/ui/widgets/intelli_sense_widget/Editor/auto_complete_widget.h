#ifndef AUTOCOMPLETEWIDGET_H
#define AUTOCOMPLETEWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QTimer>
#include <QTextEdit>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QApplication>
#include <QScrollBar>
#include <QScreen>

struct CompletionItem {
    QString label;
    QString detail;
    QString kind;
    QString documentation;
};

class AutoCompleteWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit AutoCompleteWidget(QWidget *parent = nullptr);
    ~AutoCompleteWidget();

    // Показать автодополнения
    void showCompletions(const QList<CompletionItem>& items,
                         const QRect& cursorRect,
                         QWidget* parentWidget);

    // Скрыть виджет
    void hideCompletions();

    // Вставить выбранное завершение
    void insertSelectedCompletion();

    // Навигация по списку
    void moveSelectionUp();
    void moveSelectionDown();

    bool isVisible() const { return QListWidget::isVisible(); }

signals:
    void completionSelected(const QString& completion);
    void canceled();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    QTimer* m_hideTimer;
    QTextEdit* m_editor;
    QList<CompletionItem> m_currentItems;

    void setupUI();
    QString getFullCompletionText(int index) const;
};

#endif // AUTOCOMPLETEWIDGET_H
