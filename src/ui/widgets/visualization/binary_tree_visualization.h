#ifndef BINARY_TREE_VISUALIZATION_H
#define BINARY_TREE_VISUALIZATION_H

#include <QMap>
#include <QTimer>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QScrollBar>
#include <QDebug>

#include "../../../core/internal/binary_tree/binary_tree.h"
#include "../../../core/internal/binary_tree/tree_node.h"
#include "base/visualizer_base.h"
#include "base/graphics_node.h"
#include "base/graphics_edge.h"

class BinaryTreeVisualization : public VisualizerBase
{
    Q_OBJECT

public:
    explicit BinaryTreeVisualization(QWidget* parent = nullptr);
    ~BinaryTreeVisualization();

    void setStructure(QObject* structure) override;
    void clear() override;
    void updateVisualization() override;

    void setTree(BinaryTree* tree);
    BinaryTree* tree() const { return m_tree; }

    void highlightNode(TreeNode* node, const QColor& color = Qt::yellow);
    void clearHighlights();
    void markNodeAsVisited(TreeNode* node);
    void markNodeAsCurrent(TreeNode* node);

    void setNodeSpacing(qreal horizontal, qreal vertical);
    void setNodeRadius(qreal radius);
    void setShowValues(bool show);

    void startOperation(const QString& name);
    void finishOperation(const QString& name);

public slots:
    void onNodeInserted(TreeNode* node);
    void onNodeRemoved(TreeNode* node);
    void onStructureChanged();
    void onTreeCleared();

    void resetZoom();
    void zoomIn();
    void zoomOut();

signals:
    void visualizationUpdated();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    BinaryTree* m_tree = nullptr;

    QMap<TreeNode*, GraphicsNode*> m_nodeMap;
    QMap<QPair<TreeNode*, TreeNode*>, GraphicsEdge*> m_edgeMap;

    qreal m_nodeRadius = 20.0;
    qreal m_horizontalSpacing = 80.0;
    qreal m_verticalSpacing = 100.0;
    bool m_showValues = true;

    GraphicsNode* createGraphicsNode(TreeNode* node);
    GraphicsEdge* createEdge(TreeNode* parent, TreeNode* child);
    void removeGraphicsNode(TreeNode* node);
    void removeEdge(TreeNode* parent, TreeNode* child);
    void clearAllGraphics();

    QMap<TreeNode*, QPointF> calculateNodePositions() const;
    void updateNodePositions();
    void updateEdges();

    GraphicsNode* findGraphicsNode(TreeNode* node) const;
    void rebuildVisualization();
    void fitTreeToView();

    int calculateSubtreeWidth(TreeNode* node) const;
    void calculatePositionsRecursive(TreeNode* node, qreal x, qreal y,
                                     QMap<TreeNode*, QPointF>& positions) const;

    Q_DISABLE_COPY(BinaryTreeVisualization)
};


#endif // BINARY_TREE_VISUALIZATION_H
