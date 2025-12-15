#include "ui/main_window.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qDebug() << "Available styles:" << QStyleFactory::keys();

    // a.setStyle(QStyleFactory::create("windowsvista"));
    MainWindow w;
    w.show();
    return a.exec();
}
