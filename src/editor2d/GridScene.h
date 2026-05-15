#pragma once
#include "core/VoxelWorld.h"
#include "core/Block.h"
#include "core/BlockModel.h"
#include "sim/SimFlags.h"

#include <QGraphicsScene>
#include <QTimer>
#include <QPixmap>
#include <QVector>
#include <QHash>
#include <QPoint>
#include <QColor>
#include <QVector3D>
#include <QVector2D>

class GridScene : public QGraphicsScene
{
    Q_OBJECT
public:
    static constexpr int CELL_SIZE = 32;

    // ── 编辑模式 ──────────────────────────────────────────
    enum class EditMode { Paint, Select, Interact };

    explicit GridScene(VoxelWorld *world, QObject *parent = nullptr);

    // ── 层控制 ────────────────────────────────────────────
    void setCurrentLayer(int y);
    int  currentLayer()  const { return m_currentLayer; }

    // ── 画笔状态（MainWindow 使用的名称） ─────────────────
    void        setPaintType   (BlockType t)   { m_paintType   = t; }
    void        setPaintFacing (BlockFacing f)  { m_paintFacing = f; }
    BlockType   paintType()    const            { return m_paintType; }
    BlockFacing paintFacing()  const            { return m_paintFacing; }

    // MainWindow 也用这组别名
    void        setCurrentBlockType(BlockType t)    { setPaintType(t); }
    BlockType   currentBlockType()  const           { return paintType(); }
    BlockFacing currentFacing()     const           { return paintFacing(); }

    // ── 编辑模式 ──────────────────────────────────────────
    void     setEditMode(EditMode mode);
    EditMode editMode()  const { return m_editMode; }

    // ── 其他控制 ──────────────────────────────────────────
    void rotateCurrent();
    void refresh();
    void markSimChanged(const QVector<VoxelCoord> &changed);

    // ── 坐标转换（GridView 需要访问） ─────────────────────
    QPoint  sceneToGrid(const QPointF &p) const;
    QPointF gridToScene(int gx, int gz)   const;

    // ── 静态工具 ──────────────────────────────────────────
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
    void selectionChanged(int x, int y, int z, const Block &b);
    void sourceInteracted(int x, int y, int z);

protected:
    void keyPressEvent     (QKeyEvent                *event) override;
    void mousePressEvent   (QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent    (QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent (QGraphicsSceneMouseEvent *event) override;
    void drawBackground    (QPainter *painter, const QRectF &rect) override;
    void drawForeground    (QPainter *painter, const QRectF &rect) override;

private:
    // ── 绘制 ──────────────────────────────────────────────
    void drawBlock             (QPainter *painter, int gx, int gz,
                                const Block &block,
                                double opacity = 1.0) const;
    void drawModelTopView      (QPainter *painter, const QRectF &cell,
                                const BlockModel &model,
                                float rotXdeg, float rotYdeg) const;
    void drawSimOverlay        (QPainter *painter, const QRectF &cell,
                                const Block &block) const;
    void drawFlashOverlay      (QPainter *painter, const QRectF &cell,
                                int gx, int gz) const;
    void drawRepeaterDelay     (QPainter *painter, const QRectF &cell,
                                const Block &block) const;
    void drawHopperIndicator   (QPainter *painter, const QRectF &cell,
                                BlockFacing facing) const;
    void drawSelectionHighlight(QPainter *painter, int gx, int gz) const;
    void drawTinted            (QPainter *painter, const QRectF &rect,
                                const QPixmap &pix,
                                const QColor &tint) const;

    // ── 纹理工具 ──────────────────────────────────────────
    struct TexInfo {
        QString key;
        double  rotateDeg = 0.0;
        bool    showArrow = false;
    };
    TexInfo getTopViewTex   (const Block &block) const;
    QString getStateTexPath (const Block &block, int faceIndex) const;

    // ── 红石粉连接 ────────────────────────────────────────
    struct DustConn { bool n, s, e, w; };
    DustConn getDustConnections(int gx, int gz) const;
    bool     canConnectDust   (int gx, int gz) const;

    // ── 交互工具 ──────────────────────────────────────────
    void paintCell    (const QPointF &scenePos, bool erase);
    void selectCell   (const QPointF &scenePos);
    void interactCell (const QPointF &scenePos);
    void adjustCell   (const QPointF &scenePos);
    bool isInteractableSource(int gx, int gz) const;
    bool isRepeater          (int gx, int gz) const;
    void cycleRepeaterDelay  (int gx, int gz);

    // ── 成员变量 ──────────────────────────────────────────
    VoxelWorld *m_world        = nullptr;
    int         m_currentLayer = 0;

    BlockType   m_paintType    = BlockType::Stone;
    BlockFacing m_paintFacing  = BlockFacing::North;
    EditMode    m_editMode     = EditMode::Paint;

    bool m_isDrawing    = false;
    bool m_isErasing    = false;
    bool m_hasSelection = false;
    int  m_selX = 0, m_selZ = 0;

    // ── Alt+V 预览下层 ────────────────────────────────────
    bool m_previewBelowLayer = false;

    // ── 闪烁（QHash 替代 QMap，QPoint 在 Qt6 无 operator<）─
    QTimer             *m_flashTimer = nullptr;
    QHash<QPoint, int>  m_flashFrames;
    static constexpr int FLASH_FRAMES_INIT = 6;
};