#include "graphics_node.h"
#include <QPainter>
#include <QFontMetrics>

GraphicsNode::GraphicsNode(int value, QGraphicsItem* parent)
    : QGraphicsEllipseItem(parent)
    , m_value(value)
{
    // Создаем текстовый элемент
    m_textItem = new QGraphicsTextItem(QString::number(value), this);

    // Настраиваем эллипс
    setRadius(m_radius);

    updateAppearance();
}

GraphicsNode::~GraphicsNode()
{
    // Текст удалится автоматически как child
}

void GraphicsNode::setBaseColor(const QColor& color)
{
    if (m_baseColor != color) {
        m_baseColor = color;
        updateAppearance();
    }
}

void GraphicsNode::setTextColor(const QColor& color)
{
    if (m_textColor != color) {
        m_textColor = color;
        m_textItem->setDefaultTextColor(color);
        update();
    }
}

void GraphicsNode::setBorderColor(const QColor& color)
{
    if (m_borderColor != color) {
        m_borderColor = color;
        update();
    }
}

void GraphicsNode::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        updateAppearance();
    }
}

void GraphicsNode::setHighlighted(bool highlighted)
{
    if (m_highlighted != highlighted) {
        m_highlighted = highlighted;
        updateAppearance();
    }
}

void GraphicsNode::setVisited(bool visited)
{
    if (m_visited != visited) {
        m_visited = visited;
        updateAppearance();
    }
}

void GraphicsNode::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        updateAppearance();
    }
}

void GraphicsNode::setRadius(qreal radius)
{
    if (radius > 0 && m_radius != radius) {
        m_radius = radius;

        // ВАЖНО: setRect использует ЛЕВЫЙ ВЕРХНИЙ угол и ШИРИНУ/ВЫСОТУ
        // А не центр и радиус!
        setRect(-radius, -radius, radius * 2, radius * 2);

        qDebug() << "GraphicsNode::setRadius(" << radius << ")";
        qDebug() << "  Rect set to:" << rect();
        qDebug() << "  Bounding rect:" << boundingRect();

        updateTextPosition();
        update();
    }
}

void GraphicsNode::setTextVisible(bool visible)
{
    m_textItem->setVisible(visible);
}

void GraphicsNode::setFont(const QFont& font)
{
    m_textItem->setFont(font);
    updateTextPosition();
}

QRectF GraphicsNode::boundingRect() const
{
    // Немного больше для тени/обводки
    qreal margin = 2.0;
    return rect().adjusted(-margin, -margin, margin, margin);
}

void GraphicsNode::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                         QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // Родительская отрисовка
    QGraphicsEllipseItem::paint(painter, option, widget);
}

QColor GraphicsNode::calculateCurrentColor() const
{
    QColor color = m_baseColor;

    if (m_selected) {
        color = color.lighter(150);  // Светлее на 50%
    } else if (m_highlighted) {
        color = color.lighter(130);  // Светлее на 30%
    } else if (m_active) {
        color = QColor(150, 255, 150);  // Светло-зеленый
    } else if (m_visited) {
        color = color.darker(120);  // Темнее на 20%
    }

    return color;
}

void GraphicsNode::updateAppearance()
{

    // 1. Устанавливаем кисть (заливку)
    QBrush brush(calculateCurrentColor());
    setBrush(brush);

    // 2. Устанавливаем перо (границу)
    QPen pen;
    pen.setColor(m_borderColor);
    pen.setWidth(2);

    // Специальные состояния переопределяют цвет границы
    if (m_selected) {
        pen.setColor(Qt::red);
        pen.setWidth(3);
    } else if (m_active) {
        pen.setColor(Qt::green);
        pen.setWidth(3);
    } else if (m_highlighted) {
        pen.setColor(Qt::yellow);
        pen.setWidth(3);
    }

    setPen(pen);
    qDebug() << "Final pen color:" << pen.color();

    // 3. Устанавливаем цвет текста
    // Автоматически выбираем контрастный цвет
    QColor currentFill = calculateCurrentColor();
    QColor textColor = (currentFill.lightness() > 128) ? Qt::black : Qt::white;

    // Но если был задан явный цвет текста, используем его
    if (m_textColor != Qt::white) { // Если не стандартный белый
        textColor = m_textColor;
    }

    m_textItem->setDefaultTextColor(textColor);
    qDebug() << "Text color set to:" << textColor;

    update();
}

void GraphicsNode::updateTextPosition()
{
    if (!m_textItem) return;

    // Получаем bounding rect текста
    QRectF textRect = m_textItem->boundingRect();

    // Центрируем текст относительно узла
    // Вычитаем половину ширины/высоты текста
    qreal x = -textRect.width() / 2.0;
    qreal y = -textRect.height() / 2.0;

    m_textItem->setPos(x, y);

    qDebug() << "Text position updated:";
    qDebug() << "  Text rect:" << textRect;
    qDebug() << "  Text pos:" << m_textItem->pos();
    qDebug() << "  Node center: (0, 0)";
}
