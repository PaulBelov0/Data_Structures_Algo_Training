// BinaryTree.cpp
#include "binary_tree.h"

#include <QDebug>
#include <algorithm>

BinaryTree::BinaryTree(QObject* parent) : QObject(parent)
{
}

BinaryTree::~BinaryTree()
{
    clear();
}

void BinaryTree::insert(int value)
{
    emit operationStarted(QString("Вставка значения %1").arg(value));

    if (m_root == nullptr) {
        m_root = new TreeNode(value, this);
        m_size++;
        emit nodeInserted(m_root);
        emit structureChanged();
        emit operationFinished("Вставка завершена");
        return;
    }

    TreeNode* newNode = insertRecursive(m_root, value);
    if (newNode) {
        m_size++;
        emit nodeInserted(newNode);
        emit structureChanged();
    }

    emit operationFinished("Вставка завершена");
}

TreeNode* BinaryTree::insertRecursive(TreeNode* node, int value, TreeNode* parent)
{
    if (!node) {
        TreeNode* newNode = new TreeNode(value, this);
        newNode->setParent(parent);
        return newNode;
    }

    // Для учебных целей - выделяем сравнение
    emit comparisonMade(node, nullptr);

    if (value < node->value()) {
        TreeNode* leftChild = insertRecursive(node->left(), value, node);
        if (leftChild && !node->left()) {
            node->setLeft(leftChild);
        }
    } else {
        // Дубликаты идут в правое поддерево (простейшая политика)
        TreeNode* rightChild = insertRecursive(node->right(), value, node);
        if (rightChild && !node->right()) {
            node->setRight(rightChild);
        }
    }

    return node;
}

void BinaryTree::remove(int value)
{
    emit operationStarted(QString("Удаление значения %1").arg(value));

    TreeNode* nodeToRemove = find(value);
    if (!nodeToRemove) {
        emit operationFinished("Значение не найдено");
        return;
    }

    m_root = removeRecursive(m_root, value);
    emit nodeRemoved(nodeToRemove);
    emit structureChanged();

    emit operationFinished("Удаление завершено");
}

TreeNode* BinaryTree::removeRecursive(TreeNode* node, int value)
{
    if (!node) {
        return nullptr;
    }

    emit comparisonMade(node, nullptr);

    if (value < node->value()) {
        node->setLeft(removeRecursive(node->left(), value));
    } else if (value > node->value()) {
        node->setRight(removeRecursive(node->right(), value));
    } else {
        // Нашли узел для удаления
        m_size--;

        if (!node->hasLeft() && !node->hasRight()) {
            // Лист
            node->deleteLater();
            return nullptr;
        } else if (!node->hasLeft()) {
            // Только правый ребенок
            TreeNode* rightChild = node->right();
            if (rightChild) {
                rightChild->setParent(node->parent());
            }
            node->deleteLater();
            return rightChild;
        } else if (!node->hasRight()) {
            // Только левый ребенок
            TreeNode* leftChild = node->left();
            if (leftChild) {
                leftChild->setParent(node->parent());
            }
            node->deleteLater();
            return leftChild;
        } else {
            // Два ребенка
            TreeNode* minNode = findMin(node->right());
            // Копируем значение (нужно сделать m_value не const в TreeNode)
            const_cast<int&>(node->m_value) = minNode->value();
            // Удаляем минимальный узел
            node->setRight(removeRecursive(node->right(), minNode->value()));
        }
    }

    return node;
}

TreeNode* BinaryTree::find(int value) const
{
    TreeNode* current = m_root;

    while (current) {
        emit const_cast<BinaryTree*>(this)->comparisonMade(current, nullptr);

        if (value == current->value()) {
            return current;
        } else if (value < current->value()) {
            current = current->left();
        } else {
            current = current->right();
        }
    }

    return nullptr;
}

TreeNode* BinaryTree::findMin(TreeNode* node) const
{
    if (!node) return nullptr;

    while (node->left()) {
        node = node->left();
    }

    return node;
}

void BinaryTree::clear()
{
    deleteSubtree(m_root);
    m_root = nullptr;
    m_size = 0;
    emit treeCleared();
    emit structureChanged();
}

void BinaryTree::deleteSubtree(TreeNode* node)
{
    if (!node) return;

    deleteSubtree(node->left());
    deleteSubtree(node->right());
    node->deleteLater();
}

void BinaryTree::buildFromValues(const QVector<int>& values)
{
    clear();

    emit operationStarted("Построение дерева из списка значений");

    for (int value : values) {
        insert(value);
    }

    emit operationFinished("Дерево построено");
}

// === Методы для алгоритмов балансировки ===

void BinaryTree::setRoot(TreeNode* newRoot)
{
    if (m_root == newRoot) return;

    // Отсоединяем старый корень
    if (m_root) {
        m_root->setParent(nullptr);
    }

    m_root = newRoot;
    if (m_root) {
        m_root->setParent(nullptr);
    }

    emit structureChanged();
}

void BinaryTree::rotateLeft(TreeNode* node)
{
    if (!node || !node->right()) return;

    emit operationStarted("Поворот влево");

    TreeNode* pivot = node->right();
    TreeNode* parent = node->parent();

    // Перенаправляем связи
    node->setRight(pivot->left());
    if (pivot->left()) {
        pivot->left()->setParent(node);
    }

    pivot->setLeft(node);
    node->setParent(pivot);

    pivot->setParent(parent);

    // Обновляем ссылку родителя
    if (parent) {
        if (parent->left() == node) {
            parent->setLeft(pivot);
        } else {
            parent->setRight(pivot);
        }
    }

    // Если поворачивали корень
    if (node == m_root) {
        m_root = pivot;
    }

    emit structureChanged();
    emit operationFinished("Поворот завершен");
}

void BinaryTree::rotateRight(TreeNode* node)
{
    if (!node || !node->left()) return;

    emit operationStarted("Поворот вправо");

    TreeNode* pivot = node->left();
    TreeNode* parent = node->parent();

    // Перенаправляем связи
    node->setLeft(pivot->right());
    if (pivot->right()) {
        pivot->right()->setParent(node);
    }

    pivot->setRight(node);
    node->setParent(pivot);

    pivot->setParent(parent);

    // Обновляем ссылку родителя
    if (parent) {
        if (parent->left() == node) {
            parent->setLeft(pivot);
        } else {
            parent->setRight(pivot);
        }
    }

    // Если поворачивали корень
    if (node == m_root) {
        m_root = pivot;
    }

    emit structureChanged();
    emit operationFinished("Поворот завершен");
}

void BinaryTree::swapNodes(TreeNode* node1, TreeNode* node2)
{
    if (!node1 || !node2 || node1 == node2) return;

    emit operationStarted("Обмен значений узлов");

    // Меняем значения (требует изменения TreeNode)
    std::swap(const_cast<int&>(node1->m_value), const_cast<int&>(node2->m_value));

    emit structureChanged();
    emit operationFinished("Обмен завершен");
}

void BinaryTree::updateParentLink(TreeNode* node, TreeNode* newChild)
{
    if (!node || !node->parent()) return;

    TreeNode* parent = node->parent();
    if (parent->left() == node) {
        parent->setLeft(newChild);
    } else {
        parent->setRight(newChild);
    }
}

// === Слоты для визуальной обратной связи ===

void BinaryTree::highlightNode(TreeNode* node, bool highlight)
{
    if (node) {
        emit nodeHighlighted(node, highlight);
    }
}

void BinaryTree::markNodeAsVisited(TreeNode* node)
{
    if (node) {
        emit nodeVisited(node);
    }
}

void BinaryTree::markNodeAsCurrent(TreeNode* node)
{
    if (node) {
        emit nodeCurrent(node);
    }
}

void BinaryTree::markComparison(TreeNode* node1, TreeNode* node2)
{
    emit comparisonMade(node1, node2);
}
