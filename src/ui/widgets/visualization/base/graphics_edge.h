#ifndef GRAPHICS_EDGE_H
#define GRAPHICS_EDGE_H

#include <QGraphicsLineItem>
#include <QPointer>
#include <QPointF>
#include <QEvent>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QApplication>
#include <QGraphicsScene>
#include <cmath>

class GraphicsEdge : public QGraphicsLineItem
{
public:
    explicit GraphicsEdge(QGraphicsItem* startItem, QGraphicsItem* endItem,
                          QGraphicsItem* parent = nullptr);

    void setColor(const QColor& color);
    void setWidth(qreal width);
    void setDashed(bool dashed);
    void setHighlighted(bool highlighted);

    void updatePosition();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

private:
    QGraphicsItem* m_startItem = nullptr;
    QGraphicsItem* m_endItem = nullptr;
    QColor m_color = QColor(0, 0, 0);
    qreal m_width = 2.0;
    bool m_dashed = false;
    bool m_highlighted = false;

    Q_DISABLE_COPY(GraphicsEdge)
};

#endif // GRAPHICS_EDGE_H
