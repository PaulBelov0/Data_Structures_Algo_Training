#ifndef VISUALIZER_BASE_H
#define VISUALIZER_BASE_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPointer>
#include <QMouseEvent>

class VisualizerBase : public QWidget
{
    Q_OBJECT

public:
    explicit VisualizerBase(QWidget* parent = nullptr);
    virtual ~VisualizerBase();

    virtual void setStructure(QObject* structure) = 0;
    virtual void clear() = 0;
    virtual void updateVisualization() = 0;

    void fitToView();
    void setBackgroundColor(const QColor& color);
    void setAnimationEnabled(bool enabled);
    void setAnimationDuration(int ms);

    bool isAnimationRunning() const { return false; };        // MUST BE OVERRIDED IN INHERITES
    QGraphicsScene* scene() const { return m_scene; }
    QGraphicsView* view() const { return m_view; }

signals:
    void visualizationReady();
    void animationStarted();
    void animationFinished();

protected:
    QPointer<QGraphicsView> m_view;
    QPointer<QGraphicsScene> m_scene;
    bool m_animationEnabled = true;
    int m_animationDuration = 500;

    bool m_panning = false;
    QPoint m_lastPanPos;
    qreal m_zoomFactor = 1.0;
    const qreal MIN_ZOOM = 0.1;
    const qreal MAX_ZOOM = 10.0;
    const qreal ZOOM_STEP = 0.001;

    //Scene&view setup
    virtual void setupScene();
    virtual void setupView();

    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    Q_DISABLE_COPY(VisualizerBase)
};

#endif // VISUALIZER_BASE_H
