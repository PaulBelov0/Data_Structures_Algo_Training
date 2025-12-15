#ifndef GRAPHICS_NODE_H
#define GRAPHICS_NODE_H

#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>

class GraphicsNode : public QGraphicsEllipseItem
{
public:
    explicit GraphicsNode(int value, QGraphicsItem* parent = nullptr);
    ~GraphicsNode();

    int value() const { return m_value; }

    void setBaseColor(const QColor& color);
    void setTextColor(const QColor& color);
    void setBorderColor(const QColor& color);
    void setSelected(bool selected);
    void setHighlighted(bool highlighted);
    void setVisited(bool visited);
    void setActive(bool active);

    void setRadius(qreal radius);
    qreal radius() const { return m_radius; }

    void setTextVisible(bool visible);
    void setFont(const QFont& font);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    void updateAppearance();
private:
    int m_value;
    QGraphicsTextItem* m_textItem;
    QColor m_baseColor = QColor(70, 130, 200);  // Синий по умолчанию
    QColor m_textColor = Qt::white;             // Белый текст
    QColor m_borderColor = QColor(30, 60, 100);

    bool m_selected = false;
    bool m_highlighted = false;
    bool m_visited = false;
    bool m_active = false;
    qreal m_radius = 25.0;

    QColor calculateCurrentColor() const;

    void updateTextPosition();

    Q_DISABLE_COPY(GraphicsNode)
};

#endif // GRAPHICS_NODE_H
