// generators/TreeGenerator.h
#ifndef BINARYTREEGENERATOR_H
#define BINARYTREEGENERATOR_H

#include <QObject>
#include <QRandomGenerator>
#include <QSet>

#include <algorithm>

#include "../internal/binary_tree/binary_tree.h"
#include "../internal/binary_tree/tree_node.h"

enum BinaryTreeType
{
    Random,
    LeftHeavy,
    RightHeavy
};

class BinaryTreeGenerator : public QObject
{
    Q_OBJECT

public:
    explicit BinaryTreeGenerator(QObject* parent = nullptr);

    BinaryTree* generateTree(BinaryTreeType type,
                             int nodeCount,
                             bool allowDuplicates = false);

    BinaryTree* generateRandomTree(int nodeCount, bool allowDuplicates = false);
    BinaryTree* generateLeftHeavyTree(int nodeCount, bool allowDuplicates = false);
    BinaryTree* generateRightHeavyTree(int nodeCount, bool allowDuplicates = false);

signals:
    void treeGenerated(BinaryTree* tree);

private:
    QVector<int> generateValues(int count, bool allowDuplicates) const;
    int randomInt(int max) const;
};


#endif // BINARYTREEGENERATOR_H
