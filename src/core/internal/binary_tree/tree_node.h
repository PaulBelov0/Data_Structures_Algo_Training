// TreeNode.h (только узел)
#ifndef TREENODE_H
#define TREENODE_H

#include <QObject>
#include <QPointer>

class TreeNode : public QObject
{
    Q_OBJECT

public:
    explicit TreeNode(int value, QObject* parent = nullptr);
    ~TreeNode() override;

    int value() const { return m_value; }
    TreeNode* left() const { return m_left; }
    TreeNode* right() const { return m_right; }
    TreeNode* parent() const { return m_parent; }

    void setLeft(TreeNode* left);
    void setRight(TreeNode* right);
    void setParent(TreeNode* parent);

    // Вспомогательные
    bool isLeaf() const { return !m_left && !m_right; }
    bool hasLeft() const { return m_left != nullptr; }
    bool hasRight() const { return m_right != nullptr; }

signals:
    void leftChanged(TreeNode* oldLeft, TreeNode* newLeft);
    void rightChanged(TreeNode* oldRight, TreeNode* newRight);
    void parentChanged(TreeNode* oldParent, TreeNode* newParent);

    void nodeSelected(bool selected);
    void nodeHighlighted(bool highlighted);

private:
    friend class BinaryTree;

    const int m_value;
    QPointer<TreeNode> m_left = nullptr;
    QPointer<TreeNode> m_right = nullptr;
    QPointer<TreeNode> m_parent = nullptr;

    Q_DISABLE_COPY(TreeNode)
};

#endif // TREENODE_H
