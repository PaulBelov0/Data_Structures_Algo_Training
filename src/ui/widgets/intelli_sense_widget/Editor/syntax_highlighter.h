#ifndef SYNTAX_HIGHLIGHTER_H
#define SYNTAX_HIGHLIGHTER_H

#include <QWidget>

class SyntaxHighlighter : public QWidget
{
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QWidget *parent = nullptr);

signals:
};

#endif // SYNTAX_HIGHLIGHTER_H
