#include "tree_node.h"

TreeNode::TreeNode(int value, QObject* parent)
    : QObject(parent)
    , m_value(value)
{}

TreeNode::~TreeNode()
{
    if (m_parent)
    {
        if (m_parent->m_left == this)
        {
            m_parent->m_left = nullptr;
        }
        else if (m_parent->m_right == this)
        {
            m_parent->m_right = nullptr;
        }
    }
}

void TreeNode::setLeft(TreeNode* left)
{
    if (m_left != left)
    {
        TreeNode* oldLeft = m_left;
        m_left = left;

        if (m_left)
        {
            m_left->setParent(this);
        }

        emit leftChanged(oldLeft, m_left);
    }
}

void TreeNode::setRight(TreeNode* right)
{
    if (m_right != right)
    {
        TreeNode* oldRight = m_right;
        m_right = right;

        if (m_right)
        {
            m_right->setParent(this);
        }

        emit rightChanged(oldRight, m_right);
    }
}

void TreeNode::setParent(TreeNode* parent)
{
    if (m_parent != parent)
    {
        TreeNode* oldParent = m_parent;
        m_parent = parent;
        emit parentChanged(oldParent, m_parent);
    }
}
