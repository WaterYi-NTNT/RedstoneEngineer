#pragma once

#include <QGraphicsScene>
#include <QPoint>
#include <QColor>
#include <QPixmap>
#include <QHash>
#include <QTimer>
#include <QVector>
#include "core/VoxelWorld.h"
#include "core/Block.h"
#include "sim/SimFlags.h"

class GridScene : public QGraphicsScene
{
    Q_OBJECT

public:
    enum class EditMode {
        Paint,
        Select,
        Interact
    };

    explicit GridScene(VoxelWorld *world, QObject *parent = nullptr);

    static constexpr int CELL_SIZE = 24;

    int  currentLayer() const { return m_currentLayer; }
    void setCurrentLayer(int y);

    void        setCurrentBlockType(BlockType type) { m_paintType = type; }
    BlockType   currentBlockType()            const { return m_paintType; }
    BlockFacing currentFacing()               const { return m_paintFacing; }

    void     setEditMode(EditMode mode);
    EditMode editMode()   const { return m_editMode; }

    void rotateCurrent();

    QPoint  sceneToGrid(const QPointF &scenePos) const;
    QPointF gridToScene(int gx, int gz)          const;

signals:
    void layerChanged     (int y);
    void blockModified    (int x, int y, int z);
    void selectionChanged (int x, int y, int z, const Block &block);
    void facingChanged    (BlockFacing facing);
    void editModeChanged  (EditMode mode);
    void sourceInteracted (int x, int y, int z);

public slots:
    void refresh();
    void markSimChanged(const QVector<VoxelCoord> &changed);

protected:
    void mousePressEvent  (QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent   (QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent    (QKeyEvent *event)               override;
    void drawBackground   (QPainter *painter, const QRectF &rect) override;
    void drawForeground   (QPainter *painter, const QRectF &rect) override;

private:
    VoxelWorld  *m_world        = nullptr;
    int          m_currentLayer = 0;
    BlockType    m_paintType    = BlockType::Stone;
    BlockFacing  m_paintFacing  = BlockFacing::North;
    EditMode     m_editMode     = EditMode::Paint;
    bool         m_isDrawing    = false;
    bool         m_isErasing    = false;

    bool m_hasSelection = false;
    int  m_selX = 0, m_selZ = 0;

    QHash<QPoint, int> m_flashFrames;
    QTimer            *m_flashTimer  = nullptr;
    static constexpr int FLASH_FRAMES_INIT = 6;

    void paintCell (const QPointF &scenePos, bool erase);
    void selectCell(const QPointF &scenePos);
    void interactCell     (const QPointF &scenePos);
    void adjustCell       (const QPointF &scenePos);

    bool isInteractableSource(int gx, int gz) const;
    bool isRepeater          (int gx, int gz) const;
    void cycleRepeaterDelay  (int gx, int gz);

    void drawBlock             (QPainter *painter, int gx, int gz, const Block &block) const;
    void drawHopperIndicator   (QPainter *painter, const QRectF &cell, BlockFacing facing) const;
    void drawSelectionHighlight(QPainter *painter, int gx, int gz) const;
    void drawModeIndicator     (QPainter *painter, const QRectF &cell) const;
    void drawSimOverlay        (QPainter *painter, const QRectF &cell, const Block &block) const;
    void drawFlashOverlay      (QPainter *painter, const QRectF &cell, int gx, int gz) const;
    void drawRepeaterDelay     (QPainter *painter, const QRectF &cell, const Block &block) const;

    struct TexInfo {
        QString key;
        double  rotateDeg = 0.0;
        bool    showArrow = false;
    };
    TexInfo  getTopViewTex(const Block &block) const;


    QString  getStateTexPath(const Block &block, int faceIndex) const;

    struct DustConn { bool n, s, e, w; };
    DustConn getDustConnections(int gx, int gz) const;
    bool     canConnectDust    (int gx, int gz) const;

    void drawTinted(QPainter *painter,
                    const QRectF  &rect,
                    const QPixmap &pix,
                    const QColor  &tint) const;

    static QPixmap     loadPixmap      (const QString &key, int size = CELL_SIZE);
    static double      facingToAngle   (BlockFacing f);
    static BlockFacing rotateFacingCW  (BlockFacing f);
    static QColor      fallbackColor   (BlockType type);
    static QColor      dustColor       (uint8_t power);
    static QColor      sourceActiveColor();
    static QColor      modeOverlayColor(EditMode mode);
};
