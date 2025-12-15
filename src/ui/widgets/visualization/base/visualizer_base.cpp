#include "visualizer_base.h"
#include <QVBoxLayout>
#include <QScrollBar>

VisualizerBase::VisualizerBase(QWidget* parent)
    : QWidget(parent)
    , m_view(new QGraphicsView(this))
    , m_scene(new QGraphicsScene(this))
{
    setupView();
    setupScene();

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    setLayout(layout);
}

VisualizerBase::~VisualizerBase()
{
}

void VisualizerBase::setupView()
{
    m_view->setScene(m_scene);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setInteractive(true);

    // Включаем прием событий виджета
    m_view->setAttribute(Qt::WA_AcceptTouchEvents);
    m_view->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);

    // Оптимизация для большой сцены
    m_view->setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing |
                                 QGraphicsView::DontSavePainterState);

    // Настройка прокрутки
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    qDebug() << "View size:" << m_view->size();
    qDebug() << "View viewport size:" << m_view->viewport()->size();
    qDebug() << "Scene rect:" << m_scene->sceneRect();
}

void VisualizerBase::setupScene()
{
    m_scene->setBackgroundBrush(QColor(240, 240, 240));
}

void VisualizerBase::fitToView()
{
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void VisualizerBase::setBackgroundColor(const QColor& color)
{
    m_scene->setBackgroundBrush(color);
}

void VisualizerBase::setAnimationEnabled(bool enabled)
{
    m_animationEnabled = enabled;
}

void VisualizerBase::setAnimationDuration(int ms)
{
    m_animationDuration = qMax(0, ms);
}

void VisualizerBase::wheelEvent(QWheelEvent* event)
{
    // Простой зум без привязки к Ctrl
    qreal zoomFactor = 1.0;
    const qreal angle = event->angleDelta().y();

    if (angle > 0) {
        zoomFactor = 1.15; // Увеличение
    } else if (angle < 0) {
        zoomFactor = 0.85; // Уменьшение
    }

    qreal newZoom = m_zoomFactor * zoomFactor;
    if (newZoom < MIN_ZOOM || newZoom > MAX_ZOOM) {
        return;
    }

    m_zoomFactor = newZoom;
    m_view->scale(zoomFactor, zoomFactor);
    event->accept();
}

void VisualizerBase::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && event->modifiers() & Qt::ShiftModifier)) {
        // Начало панорамирования
        m_panning = true;
        m_lastPanPos = event->pos();
        m_view->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void VisualizerBase::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        // Панорамирование
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();

        QScrollBar* hBar = m_view->horizontalScrollBar();
        QScrollBar* vBar = m_view->verticalScrollBar();

        hBar->setValue(hBar->value() - delta.x());
        vBar->setValue(vBar->value() - delta.y());

        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void VisualizerBase::mouseReleaseEvent(QMouseEvent* event)
{
    if ((event->button() == Qt::MiddleButton ||
         event->button() == Qt::LeftButton) && m_panning) {
        // Конец панорамирования
        m_panning = false;
        m_view->setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}
