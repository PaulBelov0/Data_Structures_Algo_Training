#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QDebug>

#include "widgets/visualization/binary_tree_visualization.h"
#include "../core/generators/binary_tree_generator.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private:
    BinaryTreeType m_binTreeType = BinaryTreeType::Random;
};
#endif // MAIN_WINDOW_H
