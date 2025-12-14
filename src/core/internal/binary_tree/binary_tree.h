// BinaryTree.h
#ifndef BINARYTREE_H
#define BINARYTREE_H

#include <QObject>
#include "tree_node.h"

class BinaryTree : public QObject
{
    Q_OBJECT

public:
    explicit BinaryTree(QObject* parent = nullptr);
    ~BinaryTree() override;

    void insert(int value);
    void remove(int value);
    bool contains(int value) const;
    TreeNode* find(int value) const;
    TreeNode* root() const { return m_root; }

    void balance();

    int size() const { return m_size; }
    bool isEmpty() const { return m_size == 0; }
    int height() const;
    bool isBalanced() const;

    void clear();

    QString toJson() const;
    bool fromJson(const QString& json);

    void selectNode(TreeNode* node);
    void clearSelection();
    TreeNode* selectedNode() const { return m_selectedNode; }

signals:
    void nodeInserted(TreeNode* node);
    void nodeRemoved(TreeNode* node);
    void nodeFound(TreeNode* node);
    void treeCleared();

    void structureChanged();
    void treeRebalanced();

    void selectionChanged(TreeNode* oldNode, TreeNode* newNode);

    void operationStarted(const QString& description);
    void operationProgress(int current, int total);
    void operationFinished(const QString& description);

private:
    TreeNode* m_root = nullptr;
    TreeNode* m_selectedNode = nullptr;
    int m_size = 0;

    TreeNode* insertInternal(TreeNode* node, int value);
    TreeNode* removeInternal(TreeNode* node, int value);
    TreeNode* findMin(TreeNode* node) const;
    TreeNode* findInternal(TreeNode* node, int value) const;

    TreeNode* rotateLeft(TreeNode* node);
    TreeNode* rotateRight(TreeNode* node);
    TreeNode* rotateLeftRight(TreeNode* node);
    TreeNode* rotateRightLeft(TreeNode* node);
    TreeNode* balanceNode(TreeNode* node);
    int getBalanceFactor(TreeNode* node) const;

    int heightInternal(TreeNode* node) const;
    bool isBalancedInternal(TreeNode* node) const;
    void clearInternal(TreeNode* node);

    void updateSize();
    void emitStructureChange();

    Q_DISABLE_COPY(BinaryTree)
};

#endif // BINARYTREE_H
