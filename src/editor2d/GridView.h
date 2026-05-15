#pragma once

#include <QGraphicsView>
#include <QPoint>

class GridView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit GridView(QWidget *parent = nullptr);

    void setZoomRange(double minZoom, double maxZoom);

signals:
    void gridCoordHovered(int gridX, int gridZ);
    void zoomChanged(double factor);
    void rotateRequested();

protected:
    void wheelEvent      (QWheelEvent  *event) override;
    void mousePressEvent (QMouseEvent  *event) override;
    void mouseMoveEvent  (QMouseEvent  *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent   (QKeyEvent    *event) override;

private:
    bool   m_isPanning   = false;
    QPoint m_panStartPos;

    double m_currentZoom = 1.0;
    double m_minZoom     = 0.1;
    double m_maxZoom     = 8.0;
    static constexpr double ZOOM_STEP = 1.15;

    void applyZoom(double factor, QPoint anchor);
};
