#include "binary_tree_generator.h"



BinaryTreeGenerator::BinaryTreeGenerator(QObject* parent)
    : QObject(parent)
{
}

BinaryTree* BinaryTreeGenerator::generateTree(BinaryTreeType type, int nodeCount, bool allowDuplicates)
{
    BinaryTree* tree = nullptr;

    switch (type) {
    case Random:
        tree = generateRandomTree(nodeCount, allowDuplicates);
        break;
    case LeftHeavy:
        tree = generateLeftHeavyTree(nodeCount, allowDuplicates);
        break;
    case RightHeavy:
        tree = generateRightHeavyTree(nodeCount, allowDuplicates);
        break;
    }

    if (tree) {
        emit treeGenerated(tree);
    }

    return tree;
}

BinaryTree* BinaryTreeGenerator::generateRandomTree(int nodeCount, bool allowDuplicates)
{
    QVector<int> values = generateValues(nodeCount, allowDuplicates);
    std::shuffle(values.begin(), values.end(), *QRandomGenerator::global());

    BinaryTree* tree = new BinaryTree(this);

    for (int value : values)
    {
        tree->insert(value);
    }

    return tree;
}

BinaryTree* BinaryTreeGenerator::generateLeftHeavyTree(int nodeCount, bool allowDuplicates)
{
    auto values = generateValues(nodeCount, allowDuplicates);
    std::sort(values.begin(), values.end(), std::greater<int>());

    BinaryTree* tree = new BinaryTree(this);
    for (int value : values) {
        tree->insert(value);
    }

    return tree;
}

BinaryTree* BinaryTreeGenerator::generateRightHeavyTree(int nodeCount, bool allowDuplicates)
{
    auto values = generateValues(nodeCount, allowDuplicates);
    std::sort(values.begin(), values.end());

    BinaryTree* tree = new BinaryTree(this);
    for (int value : values) {
        tree->insert(value);
    }

    return tree;
}

QVector<int> BinaryTreeGenerator::generateValues(int count, bool allowDuplicates) const
{
    QVector<int> values;
    values.reserve(count);

    if (allowDuplicates)
    {
        for (int i = 0; i < count; ++i)
        {
            values.append(randomInt(count - 1));
        }
    }
    else
    {
        QSet<int> used;
        while (values.size() < count)
        {
            int value = randomInt(count * 2);

            if (!used.contains(value))
            {
                used.insert(value);
                values.append(value);
            }
        }
    }

    return values;
}

int BinaryTreeGenerator::randomInt(int max) const
{
    return QRandomGenerator::global()->bounded(max + 1);
}
