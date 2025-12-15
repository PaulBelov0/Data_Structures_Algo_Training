#include "ui/main_window.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qDebug() << "Available styles:" << QStyleFactory::keys();

    // Установим стиль Fusion (он хорошо поддерживает кастомные виджеты)
    // a.setStyle(QStyleFactory::create("Fusion"));
    MainWindow w;
    w.show();
    return a.exec();
}
