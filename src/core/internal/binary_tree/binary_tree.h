// core/internal/binary_tree/binary_tree.h
#ifndef BINARYTREE_H
#define BINARYTREE_H


#include <QObject>
#include <QVector>
#include <QDebug>

#include "tree_node.h"


class BinaryTree : public QObject
{
    Q_OBJECT

public:
    explicit BinaryTree(QObject* parent = nullptr);
    ~BinaryTree() override;

    // Базовые операции (для пользовательского кода)
    void insert(int value);
    void remove(int value);
    TreeNode* find(int value) const;
    void clear();

    // Для работы с визуализацией и алгоритмами
    TreeNode* root() const { return m_root; }
    bool isEmpty() const { return m_root == nullptr; }
    int size() const { return m_size; }

    // Для учебных целей - прямой доступ к операциям
    // (будут вызываться из пользовательского кода балансировки)
    void setRoot(TreeNode* newRoot);
    void rotateLeft(TreeNode* node);
    void rotateRight(TreeNode* node);
    void swapNodes(TreeNode* node1, TreeNode* node2);

    // Генерация дерева
    void buildFromValues(const QVector<int>& values);

signals:
    // Сигналы для визуализатора
    void nodeInserted(TreeNode* node);
    void nodeRemoved(TreeNode* node);
    void structureChanged();
    void treeCleared();

    // Сигналы для анимаций и подсказок
    void nodeHighlighted(TreeNode* node, bool highlighted);
    void nodeVisited(TreeNode* node);
    void nodeCurrent(TreeNode* node);
    void comparisonMade(TreeNode* node1, TreeNode* node2);
    void operationStarted(const QString& description);
    void operationFinished(const QString& description);

public slots:
    // Слоты для визуальной обратной связи
    void highlightNode(TreeNode* node, bool highlight = true);
    void markNodeAsVisited(TreeNode* node);
    void markNodeAsCurrent(TreeNode* node);
    void markComparison(TreeNode* node1, TreeNode* node2);

private:
    // Внутренние вспомогательные методы
    TreeNode* insertRecursive(TreeNode* node, int value, TreeNode* parent = nullptr);
    TreeNode* removeRecursive(TreeNode* node, int value);
    TreeNode* findMin(TreeNode* node) const;
    void deleteSubtree(TreeNode* node);
    void updateParentLink(TreeNode* node, TreeNode* newChild);

    TreeNode* m_root = nullptr;
    int m_size = 0;

    Q_DISABLE_COPY(BinaryTree)
};
#endif // BINARYTREE_H
