#ifndef CPP_LSP_CLIENT_H
#define CPP_LSP_CLIENT_H

#include <QObject>

class Cpp_LSP_client : public QObject
{
    Q_OBJECT
public:
    explicit Cpp_LSP_client(QObject *parent = nullptr);

signals:
};

#endif // CPP_LSP_CLIENT_H
