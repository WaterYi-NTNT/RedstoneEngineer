#pragma once
#include "core/VoxelWorld.h"
#include "core/Block.h"
#include "core/BlockModel.h"
#include "sim/SimFlags.h"
#include "editor2d/EditorCommand.h"
#include "editor2d/CommandHistory.h"

#include <QGraphicsScene>
#include <QTimer>
#include <QPixmap>
#include <QVector>
#include <QHash>
#include <QPoint>
#include <QRect>
#include <QColor>
#include <QVector3D>
#include <QVector2D>

class GridScene : public QGraphicsScene
{
    Q_OBJECT
public:
    static constexpr int CELL_SIZE = 32;

    enum class EditMode { Paint, Select, Interact };

    explicit GridScene(VoxelWorld *world, QObject *parent = nullptr);

    void setCurrentLayer(int y);
    int  currentLayer()  const { return m_currentLayer; }

    void        setPaintType   (BlockType t)    { m_paintType   = t; }
    void        setPaintFacing (BlockFacing f)  { m_paintFacing = f; }
    BlockType   paintType()    const            { return m_paintType; }
    BlockFacing paintFacing()  const            { return m_paintFacing; }

    void        setCurrentBlockType(BlockType t)    { setPaintType(t); }
    BlockType   currentBlockType()  const           { return paintType(); }
    BlockFacing currentFacing()     const           { return paintFacing(); }

    void     setEditMode(EditMode mode);
    EditMode editMode()  const { return m_editMode; }

    void rotateCurrent();
    void refresh();
    void markSimChanged(const QVector<VoxelCoord> &changed);

    QPoint  sceneToGrid(const QPointF &p) const;
    QPointF gridToScene(int gx, int gz)   const;

    void setCommandHistory(CommandHistory *h) { m_history = h; }

    void copySelection();
    void pasteAtHover();

    QRect selectionRect() const { return m_hasSelection ? m_selRect : QRect(); }
    bool  hasSelection()  const { return m_hasSelection; }

    QPoint hoverGrid() const { return m_hoverGrid; }

    static QColor      dustColor        (uint8_t power);
    static QColor      sourceActiveColor();
    static QColor      modeOverlayColor (EditMode mode);
    static QColor      fallbackColor    (BlockType type);
    static double      facingToAngle    (BlockFacing f);
    static BlockFacing rotateFacingCW   (BlockFacing f);
    static QPixmap     loadPixmap       (const QString &key,
                                         int size = CELL_SIZE);

signals:
    void layerChanged    (int y);
    void editModeChanged (EditMode mode);
    void facingChanged   (BlockFacing f);
    void blockModified   (int x, int y, int z);
    void selectionChanged(const QRect &selRect, int layer);
    void sourceInteracted(int x, int y, int z);

protected:
    void keyPressEvent     (QKeyEvent                *event) override;
    void mousePressEvent   (QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent    (QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent (QGraphicsSceneMouseEvent *event) override;
    void drawBackground    (QPainter *painter, const QRectF &rect) override;
    void drawForeground    (QPainter *painter, const QRectF &rect) override;

private:

    void drawBlock           (QPainter *painter, int gx, int gz,
                              const Block &block,
                              double opacity = 1.0) const;
    void drawModelTopView    (QPainter *painter, const QRectF &cell,
                              const BlockModel &model,
                              float rotXdeg, float rotYdeg) const;
    void drawSimOverlay      (QPainter *painter, const QRectF &cell,
                              const Block &block) const;
    void drawFlashOverlay    (QPainter *painter, const QRectF &cell,
                              int gx, int gz) const;
    void drawRepeaterDelay   (QPainter *painter, const QRectF &cell,
                              const Block &block) const;
    void drawHopperIndicator (QPainter *painter, const QRectF &cell,
                              BlockFacing facing) const;
    void drawSelectionRect   (QPainter *painter,
                              const QRect &selRect,
                              bool isDragging) const;

    void drawMovePreview     (QPainter *painter) const;
    void drawTinted          (QPainter *painter, const QRectF &rect,
                              const QPixmap &pix,
                              const QColor &tint) const;

    struct TexInfo {
        QString key;
        double  rotateDeg = 0.0;
        bool    showArrow = false;
    };
    TexInfo getTopViewTex   (const Block &block) const;
    QString getStateTexPath (const Block &block, int faceIndex) const;

    struct DustConn { bool n, s, e, w; };
    DustConn getDustConnections(int gx, int gz) const;
    bool     canConnectDust   (int gx, int gz) const;

    void paintCell    (const QPointF &scenePos, bool erase);
    void interactCell (const QPointF &scenePos);
    void adjustCell   (const QPointF &scenePos);
    bool isInteractableSource(int gx, int gz) const;
    bool isRepeater          (int gx, int gz) const;
    void cycleRepeaterDelay  (int gx, int gz);

    void startMoveSelection (const QPoint &gridAnchor);
    void commitMoveSelection();
    void abortMoveSelection ();

    static QRect makeSelRect(const QPoint &a, const QPoint &b);

    CommandHistory *m_history      = nullptr;
    BatchCommand   *m_pendingBatch = nullptr;

    void beginBatch (const QString &desc);
    void commitBatch();
    void abortBatch ();

    struct ClipEntry { int dx, dz; Block block; };
    QVector<ClipEntry> m_clipboard;
    bool               m_hasClip = false;

    VoxelWorld *m_world        = nullptr;
    int         m_currentLayer = 0;

    BlockType   m_paintType    = BlockType::Stone;
    BlockFacing m_paintFacing  = BlockFacing::North;
    EditMode    m_editMode     = EditMode::Paint;

    bool m_isDrawing    = false;
    bool m_isErasing    = false;
    bool m_previewBelowLayer = false;

    bool   m_hasSelection     = false;
    QRect  m_selRect;
    QPoint m_selAnchor;
    bool   m_isDraggingSelect = false;

    bool              m_isDraggingMove = false;
    QPoint            m_moveDragAnchor;
    QPoint            m_moveOffset;
    QVector<ClipEntry> m_moveSnapshot;

    QPoint m_hoverGrid { 0, 0 };

    QTimer             *m_flashTimer = nullptr;
    QHash<QPoint, int>  m_flashFrames;
    static constexpr int FLASH_FRAMES_INIT = 6;
};
