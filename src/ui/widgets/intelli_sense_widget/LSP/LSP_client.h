#ifndef LSP_CLIENT_H
#define LSP_CLIENT_H

#include <QObject>

class LSPClient : public QObject
{
    Q_OBJECT
public:
    explicit LSPClient(QObject *parent = nullptr);

signals:
};

#endif // LSP_CLIENT_H
