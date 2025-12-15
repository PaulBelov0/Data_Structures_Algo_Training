#include "main_window.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setMinimumSize(1200, 800);
    QWidget* layer = new QWidget(this);
    setCentralWidget(layer);

    QVBoxLayout* layout = new QVBoxLayout(layer);

    QComboBox* dataStructSelector = new QComboBox(layer);
    dataStructSelector->addItems({"Binary tree"});
    layout->addWidget(dataStructSelector);

    BinaryTreeVisualization* binTreeVis = new BinaryTreeVisualization(this);
    layout->addWidget(binTreeVis, 1);

    QPushButton* generateBtn = new QPushButton("Generate", layer);
    layout->addWidget(generateBtn);

    connect(generateBtn, &QPushButton::clicked, [this, binTreeVis]{

        BinaryTreeGenerator* binTreeGen = new BinaryTreeGenerator(this);
        BinaryTree* tree = binTreeGen->generateTree(BinaryTreeType::Random, 25, false);
        binTreeVis->setTree(tree);
        qDebug() << "Tree Is Empty: " << binTreeVis->tree()->isEmpty();

        binTreeVis->updateGeometry();
        qDebug() << "BinTreeVis size after update:" << binTreeVis->size();
    });

}
