#include "main_window.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setMinimumSize(1200, 800);
    QWidget* layer = new QWidget(this);
    setCentralWidget(layer);

    QGridLayout* layout = new QGridLayout(layer);

    QWidget* topMenu = new QWidget(layer);
    QGridLayout* topMenuLayout = new QGridLayout(topMenu);

    QComboBox* dataStructSelector = new QComboBox(layer);
    dataStructSelector->addItems({"Binary tree"});
    dataStructSelector->setStyleSheet(Styles::grey_combobox());
    topMenuLayout->addWidget(dataStructSelector, 0, 0);

    QPushButton* generateBtn = new QPushButton("Generate", layer);
    generateBtn->setStyleSheet(Styles::cyan_push_button());
    topMenuLayout->addWidget(generateBtn, 0, 1);

    QPushButton* runBtn = new QPushButton("Run Applicaton", layer);
    runBtn->setStyleSheet(Styles::green_push_button());
    topMenuLayout->addWidget(runBtn, 0, 2);

    QWidget* sliderContainer = new QWidget(this);
    QHBoxLayout* sliderLayout = new QHBoxLayout(sliderContainer);

    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setRange(5, 100);
    slider->setValue(50);

    QSpinBox* spinBox = new QSpinBox();
    spinBox->setRange(5, 100);
    spinBox->setValue(50);

    connect(slider, &QSlider::valueChanged, spinBox, &QSpinBox::setValue);
    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            slider, &QSlider::setValue);

    sliderLayout->addWidget(slider);
    sliderLayout->addWidget(spinBox);

    topMenuLayout->addWidget(sliderContainer, 1, 0, 1, 3);

    layout->addWidget(topMenu, 0, 1, 1, 1);

    BinaryTreeVisualization* binTreeVis = new BinaryTreeVisualization(this);
    layout->addWidget(binTreeVis, 1, 1, 6, 1);


    CodeEditorWidget* codeEditor = new CodeEditorWidget(this);
    codeEditor->installEventFilter(this);
    layout->addWidget(codeEditor, 0, 0, 7, 1);


    connect(generateBtn, &QPushButton::clicked, [this, binTreeVis, slider]{

        BinaryTreeGenerator* binTreeGen = new BinaryTreeGenerator(this);
        BinaryTree* tree = binTreeGen->generateTree(BinaryTreeType::Random, slider->value(), false);
        binTreeVis->setTree(tree);
        qDebug() << "Tree Is Empty: " << binTreeVis->tree()->isEmpty();

        binTreeVis->updateGeometry();
        qDebug() << "BinTreeVis size after update:" << binTreeVis->size();
    });

}
