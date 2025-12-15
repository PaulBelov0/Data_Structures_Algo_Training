#include "graphics_edge.h"
#include <QPainter>
#include <QPen>
#include <cmath>

GraphicsEdge::GraphicsEdge(QGraphicsItem* startItem, QGraphicsItem* endItem,
                           QGraphicsItem* parent)
    : QGraphicsLineItem(parent)
    , m_startItem(startItem)
    , m_endItem(endItem)
{
    // Базовая настройка
    setPen(QPen(m_color, m_width));
    setZValue(-1); // Чтобы рёбра были под нодами

    updatePosition();
}

void GraphicsEdge::setColor(const QColor& color)
{
    if (m_color != color) {
        m_color = color;
        QPen p = pen();
        p.setColor(color);
        setPen(p);
        update();
    }
}

void GraphicsEdge::setWidth(qreal width)
{
    if (width > 0 && m_width != width) {
        m_width = width;
        QPen p = pen();
        p.setWidthF(width);
        setPen(p);
        update();
    }
}

void GraphicsEdge::setDashed(bool dashed)
{
    if (m_dashed != dashed) {
        m_dashed = dashed;
        QPen p = pen();
        p.setStyle(dashed ? Qt::DashLine : Qt::SolidLine);
        setPen(p);
        update();
    }
}

void GraphicsEdge::setHighlighted(bool highlighted)
{
    if (m_highlighted != highlighted) {
        m_highlighted = highlighted;

        QPen p = pen();
        if (highlighted) {
            p.setColor(Qt::red);
            p.setWidthF(m_width * 1.5);
        } else {
            p.setColor(m_color);
            p.setWidthF(m_width);
        }
        setPen(p);
        update();
    }
}

void GraphicsEdge::updatePosition()
{
    if (!m_startItem || !m_endItem) {
        return;
    }

    // Получаем bounding rect нод
    QRectF startRect = m_startItem->boundingRect();
    QRectF endRect = m_endItem->boundingRect();

    // Центры в локальных координатах нод
    QPointF startCenter = startRect.center();
    QPointF endCenter = endRect.center();

    // Переводим в координаты сцены
    QPointF startScenePos = m_startItem->mapToScene(startCenter);
    QPointF endScenePos = m_endItem->mapToScene(endCenter);

    // Переводим в локальные координаты ребра
    startScenePos = mapFromScene(startScenePos);
    endScenePos = mapFromScene(endScenePos);

    // Устанавливаем линию
    setLine(QLineF(startScenePos, endScenePos));
}

void GraphicsEdge::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                         QWidget* widget)
{
    Q_UNUSED(widget);

    // Обновляем позицию
    updatePosition();

    // Настраиваем перо в зависимости от состояния
    QPen edgePen = pen();
    edgePen.setColor(m_color);
    edgePen.setWidthF(m_width);

    if (m_dashed) {
        edgePen.setStyle(Qt::DashLine);
    } else {
        edgePen.setStyle(Qt::SolidLine);
    }

    if (m_highlighted) {
        edgePen.setColor(Qt::red);
        edgePen.setWidthF(m_width * 1.5);
    }

    // Рисуем линию
    painter->setPen(edgePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(line());

    // Если линия очень короткая, рисуем её толще для видимости
    if (line().length() < 5.0) {
        painter->setPen(QPen(edgePen.color(), edgePen.widthF() * 2));
        painter->drawPoint(line().pointAt(0.5));
    }
}
