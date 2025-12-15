#ifndef JAVA_LSP_CLIENT_H
#define JAVA_LSP_CLIENT_H

#include <QObject>

class Java_LSP_client : public QObject
{
    Q_OBJECT
public:
    explicit Java_LSP_client(QObject *parent = nullptr);

signals:
};

#endif // JAVA_LSP_CLIENT_H
