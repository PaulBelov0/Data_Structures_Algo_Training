#include "binary_tree_visualization.h"

BinaryTreeVisualization::BinaryTreeVisualization(QWidget* parent)
    : VisualizerBase(parent)
{
    setupView();

    // Настройка фона сцены
    m_scene->setBackgroundBrush(QBrush(QColor(80, 80, 80)));
}

BinaryTreeVisualization::~BinaryTreeVisualization()
{
    clearAllGraphics();
}

void BinaryTreeVisualization::setStructure(QObject* structure)
{
    if (auto* tree = qobject_cast<BinaryTree*>(structure))
    {
        setTree(tree);
    }
}

void BinaryTreeVisualization::clear()
{
    clearAllGraphics();
    m_tree = nullptr;
    m_scene->clear();
}

void BinaryTreeVisualization::updateVisualization()
{
    if (!m_tree) return;

    rebuildVisualization();
    updateNodePositions();
    updateEdges();
    fitTreeToView();

    emit visualizationUpdated();
}

void BinaryTreeVisualization::setTree(BinaryTree* tree)
{
    if (m_tree == tree) return;

    if (m_tree)
    {
        disconnect(m_tree, nullptr, this, nullptr);
    }

    m_tree = tree;

    if (m_tree)
    {
        connect(m_tree, &BinaryTree::nodeInserted,
                this, &BinaryTreeVisualization::onNodeInserted);
        connect(m_tree, &BinaryTree::nodeRemoved,
                this, &BinaryTreeVisualization::onNodeRemoved);
        connect(m_tree, &BinaryTree::structureChanged,
                this, &BinaryTreeVisualization::onStructureChanged);
        connect(m_tree, &BinaryTree::treeCleared,
                this, &BinaryTreeVisualization::onTreeCleared);

        updateVisualization();
    }
    else
    {
        clear();
    }
}

void BinaryTreeVisualization::highlightNode(TreeNode* node, const QColor& color)
{
    if (GraphicsNode* gNode = findGraphicsNode(node))
    {
        gNode->setBaseColor(color);
        gNode->setHighlighted(true);
    }
}

void BinaryTreeVisualization::clearHighlights()
{
    for (GraphicsNode* gNode : m_nodeMap)
    {
        gNode->setHighlighted(false);
        gNode->setActive(false);
        gNode->setVisited(false);
        gNode->setSelected(false);
        // Возвращаем стандартные цвета
        gNode->setBaseColor(QColor(70, 130, 200));
        gNode->setBorderColor(QColor(30, 60, 100));
    }

    for (auto it = m_edgeMap.begin(); it != m_edgeMap.end(); ++it)
    {
        GraphicsEdge* edge = it.value();
        edge->setHighlighted(false);

        // Возвращаем исходные цвета ребер
        TreeNode* parent = it.key().first;
        TreeNode* child = it.key().second;

        if (parent->left() == child) {
            edge->setColor(QColor(70, 130, 180));
        } else {
            edge->setColor(QColor(60, 179, 113));
        }
        edge->setWidth(3.0);
    }
}

void BinaryTreeVisualization::markNodeAsVisited(TreeNode* node)
{
    if (GraphicsNode* gNode = findGraphicsNode(node))
    {
        gNode->setVisited(true);
    }
}

void BinaryTreeVisualization::markNodeAsCurrent(TreeNode* node)
{
    if (GraphicsNode* gNode = findGraphicsNode(node))
    {
        gNode->setActive(true);
    }
}

void BinaryTreeVisualization::setNodeSpacing(qreal horizontal, qreal vertical)
{
    m_horizontalSpacing = horizontal;
    m_verticalSpacing = vertical;
    updateNodePositions();
}

void BinaryTreeVisualization::setNodeRadius(qreal radius)
{
    m_nodeRadius = radius;
    for (GraphicsNode* gNode : m_nodeMap)
    {
        gNode->setRadius(radius);
    }
    updateVisualization();
}

void BinaryTreeVisualization::setShowValues(bool show)
{
    m_showValues = show;
    for (GraphicsNode* gNode : m_nodeMap)
    {
        gNode->setTextVisible(show);
    }
}

void BinaryTreeVisualization::startOperation(const QString& name)
{
    Q_UNUSED(name);
}

void BinaryTreeVisualization::finishOperation(const QString& name)
{
    Q_UNUSED(name);
}

void BinaryTreeVisualization::onNodeInserted(TreeNode* node)
{
    if (!node) return;

    GraphicsNode* gNode = createGraphicsNode(node);
    m_scene->addItem(gNode);

    if (node->parent())
    {
        createEdge(node->parent(), node);
    }

    updateNodePositions();
    updateEdges();
}

void BinaryTreeVisualization::onNodeRemoved(TreeNode* node)
{
    if (!node) return;

    removeGraphicsNode(node);

    updateNodePositions();
    updateEdges();
}

void BinaryTreeVisualization::onStructureChanged()
{
    rebuildVisualization();
    updateNodePositions();
    updateEdges();
    fitTreeToView();
}

void BinaryTreeVisualization::onTreeCleared()
{
    clearAllGraphics();
    m_scene->clear();
}

void BinaryTreeVisualization::resetZoom()
{
    m_view->resetTransform();
    fitTreeToView();
}

void BinaryTreeVisualization::zoomIn()
{
    m_view->scale(1.2, 1.2);
}

void BinaryTreeVisualization::zoomOut()
{
    m_view->scale(0.8, 0.8);
}

void BinaryTreeVisualization::resizeEvent(QResizeEvent* event)
{
    VisualizerBase::resizeEvent(event);
    fitTreeToView();
}

GraphicsNode* BinaryTreeVisualization::createGraphicsNode(TreeNode* node)
{
    GraphicsNode* gNode = new GraphicsNode(node->value());

    // Устанавливаем размер и цвета
    gNode->setRadius(m_nodeRadius);
    gNode->setTextVisible(m_showValues);
    gNode->setBrush(QBrush(QColor(70, 130, 200)));  // Синий
    gNode->setPen(QPen(QColor(30, 60, 100), 2));    // Темно-синяя граница
    gNode->setTextColor(Qt::white);

    // НЕ добавляем на сцену здесь!
    // m_scene->addItem(gNode);  // УДАЛИТЕ эту строку если есть

    m_nodeMap[node] = gNode;
    return gNode;
}

GraphicsEdge* BinaryTreeVisualization::createEdge(TreeNode* parent, TreeNode* child)
{
    GraphicsNode* parentNode = findGraphicsNode(parent);
    GraphicsNode* childNode = findGraphicsNode(child);

    if (!parentNode || !childNode) return nullptr;

    GraphicsEdge* edge = new GraphicsEdge(parentNode, childNode);
    m_scene->addItem(edge);

    // Цвета ребер
    if (parent->left() == child)
    {
        edge->setColor(QColor(70, 130, 180)); // Синий для левых ребер
    }
    else
    {
        edge->setColor(QColor(60, 179, 113)); // Зеленый для правых ребер
    }

    // Установка толщины и стиля
    edge->setWidth(3);
    // edge->setStyle(Qt::SolidLine);

    m_edgeMap[{parent, child}] = edge;
    return edge;
}

void BinaryTreeVisualization::removeGraphicsNode(TreeNode* node)
{
    if (GraphicsNode* gNode = m_nodeMap.take(node))
    {
        auto it = m_edgeMap.begin();
        while (it != m_edgeMap.end())
        {
            if (it.key().first == node || it.key().second == node)
            {
                m_scene->removeItem(it.value());
                delete it.value();
                it = m_edgeMap.erase(it);
            }
            else
            {
                ++it;
            }
        }

        m_scene->removeItem(gNode);
        delete gNode;
    }
}

void BinaryTreeVisualization::removeEdge(TreeNode* parent, TreeNode* child)
{
    if (GraphicsEdge* edge = m_edgeMap.take({parent, child}))
    {
        m_scene->removeItem(edge);
        delete edge;
    }
}

void BinaryTreeVisualization::clearAllGraphics()
{
    for (GraphicsEdge* edge : m_edgeMap)
    {
        m_scene->removeItem(edge);
        delete edge;
    }
    m_edgeMap.clear();

    for (GraphicsNode* gNode : m_nodeMap)
    {
        m_scene->removeItem(gNode);
        delete gNode;
    }
    m_nodeMap.clear();
}

QMap<TreeNode*, QPointF> BinaryTreeVisualization::calculateNodePositions() const
{
    QMap<TreeNode*, QPointF> positions;

    if (!m_tree || !m_tree->root())
    {
        return positions;
    }

    int treeWidth = calculateSubtreeWidth(m_tree->root());

    qreal startX = (treeWidth - 1) * m_horizontalSpacing / 2.0;

    calculatePositionsRecursive(m_tree->root(), startX, 0, positions);

    return positions;
}

int BinaryTreeVisualization::calculateSubtreeWidth(TreeNode* node) const
{
    if (!node) return 0;

    int leftWidth = calculateSubtreeWidth(node->left());
    int rightWidth = calculateSubtreeWidth(node->right());

    return leftWidth + rightWidth + 1;
}

void BinaryTreeVisualization::calculatePositionsRecursive(TreeNode* node, qreal x, qreal y,
                                                          QMap<TreeNode*, QPointF>& positions) const
{
    if (!node) return;

    positions[node] = QPointF(x, y);

    int leftWidth = calculateSubtreeWidth(node->left());

    if (node->left())
    {
        qreal leftX = x - (leftWidth + 1) * m_horizontalSpacing / 2.0;
        calculatePositionsRecursive(node->left(), leftX, y + m_verticalSpacing, positions);
    }

    if (node->right())
    {
        qreal rightX = x + (leftWidth + 1) * m_horizontalSpacing / 2.0;
        calculatePositionsRecursive(node->right(), rightX, y + m_verticalSpacing, positions);
    }
}

void BinaryTreeVisualization::updateNodePositions()
{
    auto positions = calculateNodePositions();

    qDebug() << "=== UPDATE NODE POSITIONS ===";
    qDebug() << "Calculated" << positions.size() << "positions";

    for (auto it = positions.begin(); it != positions.end(); ++it)
    {
        TreeNode* treeNode = it.key();
        QPointF position = it.value();

        if (GraphicsNode* gNode = m_nodeMap.value(treeNode))
        {
            qDebug() << "Setting node" << treeNode->value()
            << "from" << gNode->pos() << "to" << position;

            gNode->setPos(position);

            // Debug: точка в позиции узла

        }
        else
        {
            qDebug() << "ERROR: No graphics node for tree node" << treeNode->value();
        }
    }
}

void BinaryTreeVisualization::updateEdges()
{
    for (GraphicsEdge* edge : m_edgeMap)
    {
        edge->updatePosition();
    }
}

GraphicsNode* BinaryTreeVisualization::findGraphicsNode(TreeNode* node) const
{
    return m_nodeMap.value(node, nullptr);
}

void BinaryTreeVisualization::rebuildVisualization()
{
    clearAllGraphics();

    if (!m_tree || !m_tree->root()) return;

    qDebug() << "=== REBUILD VISUALIZATION ===";
    qDebug() << "m_nodeRadius =" << m_nodeRadius;

    // 1. Создаем ВСЕ узлы (пока без позиций и НЕ добавляем на сцену!)
    std::function<void(TreeNode*)> createNodes = [&](TreeNode* node)
    {
        if (!node) return;

        GraphicsNode* gNode = createGraphicsNode(node); // Только создаем
        // НЕ добавляем на сцену здесь!
        // m_scene->addItem(gNode); <-- УБРАТЬ

        qDebug() << "Node" << node->value() << "created (not on scene yet)";
        qDebug() << "  Initial pos:" << gNode->pos();
        qDebug() << "  Initial rect:" << gNode->rect();

        createNodes(node->left());
        createNodes(node->right());
    };

    createNodes(m_tree->root());

    // 2. Устанавливаем позиции ВСЕХ узлов
    updateNodePositions();

    // 3. ТЕПЕРЬ добавляем все узлы на сцену
    for (GraphicsNode* gNode : m_nodeMap) {
        m_scene->addItem(gNode);
        qDebug() << "Node added to scene at:" << gNode->pos();
    }

    // 4. Создаем ребра (после установки позиций и добавления на сцену!)
    std::function<void(TreeNode*)> createEdges = [&](TreeNode* node)
    {
        if (!node) return;

        if (node->left())
        {
            createEdge(node, node->left());
            createEdges(node->left());
        }

        if (node->right())
        {
            createEdge(node, node->right());
            createEdges(node->right());
        }
    };

    createEdges(m_tree->root());

    updateEdges();

    fitTreeToView();
}

void BinaryTreeVisualization::fitTreeToView()
{
    qDebug() << "=== FIT TO VIEW START ===";
    qDebug() << "Scene items count:" << m_scene->items().size();

    if (m_scene->items().isEmpty()) {
        qDebug() << "ERROR: Scene is empty!";
        return;
    }

    // Перечислим ВСЕ элементы на сцене
    qDebug() << "All scene items:";
    for (auto item : m_scene->items()) {
        qDebug() << "  Item:" << item
                 << "Type:" << item->type()
                 << "Pos:" << item->pos()
                 << "Bounding rect:" << item->boundingRect();
    }

    QRectF itemsRect = m_scene->itemsBoundingRect();
    qDebug() << "Items bounding rect:" << itemsRect;

    if (itemsRect.isEmpty()) {
        qDebug() << "ERROR: Empty bounding rect!";

        // Принудительно создадим тестовый прямоугольник
        QGraphicsRectItem* testRect = m_scene->addRect(QRectF(-100, -100, 200, 200),
                                                       QPen(Qt::red), QBrush(Qt::NoBrush));
        itemsRect = m_scene->itemsBoundingRect();
        qDebug() << "Test rect added. New bounding rect:" << itemsRect;

        // Удалим через 3 секунды
        QTimer::singleShot(3000, [testRect]() {
            if (testRect && testRect->scene()) {
                testRect->scene()->removeItem(testRect);
                delete testRect;
            }
        });
    }

    itemsRect.adjust(-50, -50, 50, 50);
    qDebug() << "Adjusted rect for fitting:" << itemsRect;
    qDebug() << "View size:" << m_view->size();
    qDebug() << "Viewport size:" << m_view->viewport()->size();

    // Сбросим трансформацию
    m_view->resetTransform();
    qDebug() << "Transform after reset:" << m_view->transform();

    // fitInView
    m_view->fitInView(itemsRect, Qt::KeepAspectRatio);
    qDebug() << "Transform after fitInView:" << m_view->transform();

    // Принудительно обновим
    m_view->update();
    m_view->viewport()->update();

    // Проверим результат
    QTimer::singleShot(0, [this, itemsRect]() {
        qDebug() << "=== AFTER EVENT LOOP ===";
        qDebug() << "Final transform:" << m_view->transform();
        qDebug() << "View scene rect:" << m_view->sceneRect();
    });
}
