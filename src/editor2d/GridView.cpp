#include "GridView.h"
#include "GridScene.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>

GridView::GridView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing,           false);
    setRenderHint(QPainter::SmoothPixmapTransform,  false);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy  (Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor        (QGraphicsView::NoAnchor);
    setDragMode            (QGraphicsView::NoDrag);

    setFrameShape          (QFrame::NoFrame);
    setSceneRect           (-2000, -2000, 4000, 4000);
    setFocusPolicy         (Qt::StrongFocus);
}

void GridView::setZoomRange(double minZoom, double maxZoom)
{
    m_minZoom = minZoom;
    m_maxZoom = maxZoom;
}

void GridView::wheelEvent(QWheelEvent *event)
{
    const double delta = event->angleDelta().y();
    if (qFuzzyIsNull(delta)) return;
    const double factor = (delta > 0) ? ZOOM_STEP : (1.0 / ZOOM_STEP);
    applyZoom(factor, event->position().toPoint());
    event->accept();
}

void GridView::applyZoom(double factor, QPoint anchor)
{
    const double newZoom = m_currentZoom * factor;
    if (newZoom < m_minZoom || newZoom > m_maxZoom) return;

    const QPointF before = mapToScene(anchor);
    scale(factor, factor);
    const QPointF after  = mapToScene(anchor);
    translate(after.x() - before.x(), after.y() - before.y());

    m_currentZoom = newZoom;
    emit zoomChanged(m_currentZoom);
}

void GridView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
    {
        m_isPanning   = true;
        m_panStartPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void GridView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning)
    {
        const QPoint delta = event->pos() - m_panStartPos;
        m_panStartPos = event->pos();
        translate(delta.x() / transform().m11(),
                  delta.y() / transform().m22());
        event->accept();
        return;
    }

    if (auto *gs = qobject_cast<GridScene *>(scene()))
    {
        const QPointF sp = mapToScene(event->pos());
        const QPoint  gp = gs->sceneToGrid(sp);
        emit gridCoordHovered(gp.x(), gp.y());
    }

    QGraphicsView::mouseMoveEvent(event);
}

void GridView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
    {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void GridView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_R && !event->isAutoRepeat())
    {
        emit rotateRequested();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}
