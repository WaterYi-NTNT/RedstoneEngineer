#include "editor2d/GridScene.h"
#include "core/BlockModelLoader.h"
#include "core/BlockModel.h"
#include "core/BlockStateLoader.h"
#include "sim/SimFlags.h"
#include "sim/RedstoneLogic.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QFont>
#include <QtMath>
#include <QDir>
#include <climits>
#include <cmath>
#include <memory>
#include <unordered_map>

GridScene::GridScene(VoxelWorld *world, QObject *parent)
    : QGraphicsScene(parent)
    , m_world(world)
{
    const int halfPx = VoxelWorld::WORLD_MAX * CELL_SIZE;
    setSceneRect(-halfPx, -halfPx, halfPx * 2, halfPx * 2);
    setBackgroundBrush(QColor(0x1A, 0x1A, 0x2E));

    m_flashTimer = new QTimer(this);
    m_flashTimer->setInterval(100);
    connect(m_flashTimer, &QTimer::timeout, this, [this]()
    {
        if (m_flashFrames.isEmpty()) { m_flashTimer->stop(); return; }
        QList<QPoint> expired;
        for (auto it = m_flashFrames.begin(); it != m_flashFrames.end(); ++it)
            if (--it.value() <= 0) expired.append(it.key());
        for (const QPoint &p : expired) m_flashFrames.remove(p);
        QList<QPoint> all = m_flashFrames.keys();
        all.append(expired);
        for (const QPoint &p : all)
            update(QRectF(gridToScene(p.x(), p.y()), QSizeF(CELL_SIZE, CELL_SIZE)));
    });
}

void GridScene::markSimChanged(const QVector<VoxelCoord> &changed)
{
    bool any = false;
    for (const VoxelCoord &c : changed) {
        if (c.y != m_currentLayer) continue;
        m_flashFrames[QPoint(c.x, c.z)] = FLASH_FRAMES_INIT;
        any = true;
    }
    if (any && !m_flashTimer->isActive())
        m_flashTimer->start();
}

QPoint GridScene::sceneToGrid(const QPointF &p) const
{
    return QPoint(static_cast<int>(std::floor(p.x() / CELL_SIZE)),
                  static_cast<int>(std::floor(p.y() / CELL_SIZE)));
}
QPointF GridScene::gridToScene(int gx, int gz) const
{
    return QPointF(gx * CELL_SIZE, gz * CELL_SIZE);
}

QRect GridScene::makeSelRect(const QPoint &a, const QPoint &b)
{
    const int x1 = qMin(a.x(), b.x()), z1 = qMin(a.y(), b.y());
    const int x2 = qMax(a.x(), b.x()), z2 = qMax(a.y(), b.y());
    return QRect(x1, z1, x2 - x1 + 1, z2 - z1 + 1);
}

void GridScene::setCurrentLayer(int y)
{
    if (y < VoxelWorld::LAYER_MIN || y > VoxelWorld::LAYER_MAX) return;
    m_currentLayer     = y;
    m_hasSelection     = false;
    m_isDraggingSelect = false;
    abortMoveSelection();
    m_flashFrames.clear();
    update();
    emit layerChanged(y);
}

void GridScene::refresh() { update(); }

void GridScene::setEditMode(EditMode mode)
{
    m_editMode         = mode;
    m_hasSelection     = false;
    m_isDraggingSelect = false;
    m_isDrawing        = false;
    m_isErasing        = false;
    abortMoveSelection();
    for (auto *v : views()) v->unsetCursor();
    update();
    emit editModeChanged(mode);
}

void GridScene::beginBatch(const QString &desc)
{
    abortBatch();
    if (m_history) m_pendingBatch = new BatchCommand(desc);
}

void GridScene::commitBatch()
{
    if (!m_history || !m_pendingBatch) { abortBatch(); return; }
    m_history->push(std::unique_ptr<BatchCommand>(m_pendingBatch));
    m_pendingBatch = nullptr;
}

void GridScene::abortBatch()
{
    delete m_pendingBatch;
    m_pendingBatch = nullptr;
}

void GridScene::startMoveSelection(const QPoint &gridAnchor)
{
    if (!m_world || !m_hasSelection) return;

    m_moveSnapshot.clear();
    const int ox = m_selRect.left();
    const int oz = m_selRect.top();
    for (int gx = m_selRect.left(); gx <= m_selRect.right(); ++gx) {
        for (int gz = m_selRect.top(); gz <= m_selRect.bottom(); ++gz) {
            if (!m_world->hasBlock(gx, m_currentLayer, gz)) continue;
            m_moveSnapshot.append({gx - ox, gz - oz,
                                   m_world->getBlock(gx, m_currentLayer, gz)});
        }
    }

    if (m_moveSnapshot.isEmpty()) return;

    m_moveDragAnchor = gridAnchor;
    m_moveOffset     = {0, 0};
    m_isDraggingMove = true;
    update();
}

void GridScene::commitMoveSelection()
{
    if (!m_isDraggingMove || m_moveSnapshot.isEmpty()) {
        abortMoveSelection();
        return;
    }

    if (m_moveOffset.isNull()) {
        abortMoveSelection();
        return;
    }

    const int ox = m_selRect.left();
    const int oz = m_selRect.top();
    const int dx = m_moveOffset.x();
    const int dz = m_moveOffset.y();

    QHash<QPoint, Block> srcMap;
    for (const auto &e : m_moveSnapshot)
        srcMap[{ox + e.dx, oz + e.dz}] = e.block;

    auto batch = std::make_unique<BatchCommand>(QStringLiteral("移动"));

    for (const auto &e : m_moveSnapshot)
        batch->add({ox + e.dx, m_currentLayer, oz + e.dz, e.block, Block{}});

    for (const auto &e : m_moveSnapshot) {
        const int tx = ox + e.dx + dx;
        const int tz = oz + e.dz + dz;
        const QPoint dstPt{tx, tz};

        Block beforeAtDst;
        if (srcMap.contains(dstPt))
            beforeAtDst = srcMap[dstPt];
        else
            beforeAtDst = m_world->hasBlock(tx, m_currentLayer, tz)
                        ? m_world->getBlock(tx, m_currentLayer, tz)
                        : Block{};

        batch->add({tx, m_currentLayer, tz, beforeAtDst, e.block});
    }

    batch->redo(*m_world);
    if (m_history) m_history->push(std::move(batch));

    m_selRect        = m_selRect.translated(dx, dz);
    m_isDraggingMove = false;
    m_moveOffset     = {0, 0};
    m_moveSnapshot.clear();

    update();
    emit selectionChanged(m_selRect, m_currentLayer);
    if (!m_selRect.isEmpty())
        emit blockModified(m_selRect.left(), m_currentLayer, m_selRect.top());
}

void GridScene::abortMoveSelection()
{
    m_isDraggingMove = false;
    m_moveOffset     = {0, 0};
    m_moveSnapshot.clear();
}

void GridScene::copySelection()
{
    if (!m_world || !m_hasSelection) return;
    m_clipboard.clear();
    m_hasClip = false;

    const int ox = m_selRect.left();
    const int oz = m_selRect.top();
    for (int gx = m_selRect.left(); gx <= m_selRect.right(); ++gx) {
        for (int gz = m_selRect.top(); gz <= m_selRect.bottom(); ++gz) {
            if (!m_world->hasBlock(gx, m_currentLayer, gz)) continue;
            m_clipboard.append({gx - ox, gz - oz,
                                m_world->getBlock(gx, m_currentLayer, gz)});
        }
    }
    if (!m_clipboard.isEmpty()) m_hasClip = true;
}

void GridScene::pasteAtHover()
{
    if (!m_world || !m_hasClip || m_clipboard.isEmpty()) return;

    const int ox = m_hoverGrid.x();
    const int oz = m_hoverGrid.y();
    auto batch = std::make_unique<BatchCommand>(QStringLiteral("粘贴"));

    for (const ClipEntry &e : m_clipboard) {
        const int tx = ox + e.dx, tz = oz + e.dz;
        const Block before = m_world->hasBlock(tx, m_currentLayer, tz)
                           ? m_world->getBlock(tx, m_currentLayer, tz) : Block{};
        m_world->setBlock(tx, m_currentLayer, tz, e.block);
        const Block after = m_world->hasBlock(tx, m_currentLayer, tz)
                          ? m_world->getBlock(tx, m_currentLayer, tz) : Block{};
        batch->add({tx, m_currentLayer, tz, before, after});
        for (int ddx = -1; ddx <= 1; ++ddx)
        for (int ddz = -1; ddz <= 1; ++ddz)
            update(QRectF(gridToScene(tx+ddx, tz+ddz), QSizeF(CELL_SIZE, CELL_SIZE)));
        emit blockModified(tx, m_currentLayer, tz);
    }

    if (m_history) m_history->push(std::move(batch));

    if (!m_clipboard.isEmpty()) {
        int maxDx = 0, maxDz = 0;
        for (const auto &e : m_clipboard) {
            maxDx = qMax(maxDx, e.dx);
            maxDz = qMax(maxDz, e.dz);
        }
        m_selRect      = QRect(ox, oz, maxDx + 1, maxDz + 1);
        m_hasSelection = true;
        emit selectionChanged(m_selRect, m_currentLayer);
        update();
    }
}

void GridScene::keyPressEvent(QKeyEvent *event)
{
    const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;

    if (event->key() == Qt::Key_Delete
     && m_editMode == EditMode::Select
     && m_hasSelection
     && m_world)
    {
        auto batch = std::make_unique<BatchCommand>(QStringLiteral("删除选区"));

        for (int gx = m_selRect.left(); gx <= m_selRect.right(); ++gx) {
            for (int gz = m_selRect.top(); gz <= m_selRect.bottom(); ++gz) {
                if (!m_world->hasBlock(gx, m_currentLayer, gz)) continue;
                const Block before = m_world->getBlock(gx, m_currentLayer, gz);
                batch->add({gx, m_currentLayer, gz, before, Block{}});
                m_flashFrames.remove(QPoint(gx, gz));
            }
        }

        if (!batch->isEmpty()) {
            batch->redo(*m_world);
            if (m_history) m_history->push(std::move(batch));
            emit blockModified(m_selRect.left(), m_currentLayer, m_selRect.top());
        }

        m_hasSelection = false;
        emit selectionChanged(QRect(), m_currentLayer);
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape)
    {
        if (m_isDraggingMove) {
            abortMoveSelection();
            update();
            event->accept();
            return;
        }
        if (m_hasSelection || m_isDraggingSelect) {
            m_hasSelection     = false;
            m_isDraggingSelect = false;
            emit selectionChanged(QRect(), m_currentLayer);
            update();
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_A && ctrl) {
        if (!m_world) { event->accept(); return; }
        int minX = INT_MAX, maxX = INT_MIN;
        int minZ = INT_MAX, maxZ = INT_MIN;
        for (const auto &[coord, block] : m_world->allBlocks()) {
            if (coord.y != m_currentLayer || block.isEmpty()) continue;
            minX = qMin(minX, coord.x); maxX = qMax(maxX, coord.x);
            minZ = qMin(minZ, coord.z); maxZ = qMax(maxZ, coord.z);
        }
        if (minX == INT_MAX) { event->accept(); return; }
        m_selRect          = QRect(minX, minZ, maxX - minX + 1, maxZ - minZ + 1);
        m_hasSelection     = true;
        m_isDraggingSelect = false;
        abortMoveSelection();
        if (m_editMode != EditMode::Select) setEditMode(EditMode::Select);
        emit selectionChanged(m_selRect, m_currentLayer);
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_C && ctrl) {
        copySelection();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_V && ctrl) {
        pasteAtHover();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_V
     && !(event->modifiers() & Qt::AltModifier)
     && !ctrl)
    {
        m_previewBelowLayer = !m_previewBelowLayer;
        update();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Q: setEditMode(EditMode::Paint);    return;
    case Qt::Key_E: setEditMode(EditMode::Select);   return;
    case Qt::Key_T: setEditMode(EditMode::Interact); return;
    case Qt::Key_R: rotateCurrent();                 return;
    default: break;
    }

    QGraphicsScene::keyPressEvent(event);
}

void GridScene::rotateCurrent()
{
    if (m_editMode == EditMode::Paint) {
        m_paintFacing = rotateFacingCW(m_paintFacing);
        if (m_paintType == BlockType::Hopper && m_paintFacing == BlockFacing::Up)
            m_paintFacing = BlockFacing::Down;
        const auto &meta = getBlockMeta(m_paintType);
        if (!meta.canFaceVertically &&
            (m_paintFacing == BlockFacing::Up || m_paintFacing == BlockFacing::Down))
            m_paintFacing = BlockFacing::North;
        emit facingChanged(m_paintFacing);
        return;
    }

    if (m_editMode == EditMode::Select) {
        if (!m_hasSelection || !m_world) return;
        if (m_selRect.width() != 1 || m_selRect.height() != 1) return;

        const int gx = m_selRect.left(), gz = m_selRect.top();
        Block *b = m_world->getBlockMutable(gx, m_currentLayer, gz);
        if (!b || b->isEmpty()) return;
        const auto &meta = getBlockMeta(b->type);
        if (!meta.hasDirection) return;

        const Block blockBefore = *b;
        b->facing = rotateFacingCW(b->facing);
        if (b->type == BlockType::Hopper && b->facing == BlockFacing::Up)
            b->facing = BlockFacing::Down;
        if (!meta.canFaceVertically &&
            (b->facing == BlockFacing::Up || b->facing == BlockFacing::Down))
            b->facing = BlockFacing::North;

        if (m_history)
            m_history->push(std::make_unique<PlaceCommand>(
                BlockChange{gx, m_currentLayer, gz, blockBefore, *b}));

        m_world->notifyChange(gx, m_currentLayer, gz, *b);
        for (int ddx = -1; ddx <= 1; ++ddx)
        for (int ddz = -1; ddz <= 1; ++ddz)
            update(QRectF(gridToScene(gx+ddx, gz+ddz), QSizeF(CELL_SIZE, CELL_SIZE)));
        emit blockModified(gx, m_currentLayer, gz);
        emit selectionChanged(m_selRect, m_currentLayer);
    }
}

bool GridScene::isInteractableSource(int gx, int gz) const
{
    if (!m_world || !m_world->hasBlock(gx, m_currentLayer, gz)) return false;
    const Block &b = m_world->getBlock(gx, m_currentLayer, gz);
    switch (b.type) {
    case BlockType::Lever:
    case BlockType::StoneButton:
    case BlockType::WoodButton:
    case BlockType::StonePressurePlate:
    case BlockType::WoodPressurePlate:
    case BlockType::LightWeightedPressurePlate:
    case BlockType::HeavyWeightedPressurePlate:
        return true;
    default: return false;
    }
}

bool GridScene::isRepeater(int gx, int gz) const
{
    if (!m_world || !m_world->hasBlock(gx, m_currentLayer, gz)) return false;
    return m_world->getBlock(gx, m_currentLayer, gz).type == BlockType::Repeater;
}

void GridScene::cycleRepeaterDelay(int gx, int gz)
{
    if (!m_world) return;
    Block *b = m_world->getBlockMutable(gx, m_currentLayer, gz);
    if (!b || b->type != BlockType::Repeater) return;

    uint8_t cur  = SimFlags::getRepeaterDelay(b->flags);
    uint8_t next = static_cast<uint8_t>((cur + 1) % 4);
    b->flags     = SimFlags::setRepeaterDelay(b->flags, next);
    m_world->notifyChange(gx, m_currentLayer, gz, *b);

    for (int dx = -1; dx <= 1; ++dx)
    for (int dz = -1; dz <= 1; ++dz)
        update(QRectF(gridToScene(gx+dx, gz+dz), QSizeF(CELL_SIZE, CELL_SIZE)));
    emit blockModified(gx, m_currentLayer, gz);
}

void GridScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    switch (m_editMode)
    {
    case EditMode::Paint:
        if (event->button() == Qt::LeftButton) {
            m_isDrawing = true; m_isErasing = false;
            beginBatch(QStringLiteral("绘制"));
            paintCell(event->scenePos(), false);
        } else if (event->button() == Qt::RightButton) {
            m_isErasing = true; m_isDrawing = false;
            beginBatch(QStringLiteral("擦除"));
            paintCell(event->scenePos(), true);
        }
        break;

    case EditMode::Select:
        if (event->button() == Qt::LeftButton) {
            const QPoint g = sceneToGrid(event->scenePos());
            if (m_hasSelection && !m_isDraggingMove && m_selRect.contains(g)) {
                startMoveSelection(g);
            } else {
                if (m_isDraggingMove) commitMoveSelection();
                m_selAnchor        = g;
                m_selRect          = QRect(g, QSize(1, 1));
                m_hasSelection     = true;
                m_isDraggingSelect = true;
                update();
            }
        } else if (event->button() == Qt::RightButton) {

            if (!m_world) break;
            const QPoint g = sceneToGrid(event->scenePos());
            if (!m_hasSelection || !m_selRect.contains(g)) break;
            const int gx = g.x(), gz = g.y();
            if (!m_world->hasBlock(gx, m_currentLayer, gz)) break;

            const Block before = m_world->getBlock(gx, m_currentLayer, gz);
            m_world->clearBlock(gx, m_currentLayer, gz);
            m_flashFrames.remove(QPoint(gx, gz));

            if (m_history)
                m_history->push(std::make_unique<PlaceCommand>(
                    BlockChange{gx, m_currentLayer, gz, before, Block{}}));

            for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz)
                update(QRectF(gridToScene(gx+dx, gz+dz), QSizeF(CELL_SIZE, CELL_SIZE)));
            emit blockModified(gx, m_currentLayer, gz);
        }
        break;

    case EditMode::Interact:
        if (event->button() == Qt::LeftButton)
            interactCell(event->scenePos());
        else if (event->button() == Qt::RightButton)
            adjustCell(event->scenePos());
        break;
    }

    QGraphicsScene::mousePressEvent(event);
}

void GridScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    m_hoverGrid = sceneToGrid(event->scenePos());

    if (m_editMode == EditMode::Paint) {
        if      (m_isDrawing) paintCell(event->scenePos(), false);
        else if (m_isErasing) paintCell(event->scenePos(), true);
    }

    if (m_editMode == EditMode::Select && m_isDraggingSelect) {
        const QPoint cur    = sceneToGrid(event->scenePos());
        const QRect newRect = makeSelRect(m_selAnchor, cur);
        if (newRect != m_selRect) { m_selRect = newRect; update(); }
    }

    if (m_editMode == EditMode::Select && m_isDraggingMove) {
        const QPoint cur    = sceneToGrid(event->scenePos());
        const QPoint newOff = cur - m_moveDragAnchor;
        if (newOff != m_moveOffset) { m_moveOffset = newOff; update(); }
    }

    if (m_editMode == EditMode::Select) {
        Qt::CursorShape shape;
        if (m_isDraggingMove)
            shape = Qt::ClosedHandCursor;
        else if (m_isDraggingSelect)
            shape = Qt::CrossCursor;
        else if (m_hasSelection && m_selRect.contains(m_hoverGrid))
            shape = Qt::SizeAllCursor;
        else
            shape = Qt::CrossCursor;
        for (auto *v : views()) v->setCursor(shape);
    } else {
        for (auto *v : views()) v->unsetCursor();
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void GridScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isDrawing || m_isErasing) commitBatch();
    m_isDrawing = m_isErasing = false;

    if (m_editMode == EditMode::Select
     && m_isDraggingSelect
     && event->button() == Qt::LeftButton)
    {
        m_isDraggingSelect = false;
        emit selectionChanged(m_selRect, m_currentLayer);
        update();
    }

    if (m_editMode == EditMode::Select
     && m_isDraggingMove
     && event->button() == Qt::LeftButton)
    {
        commitMoveSelection();
    }

    QGraphicsScene::mouseReleaseEvent(event);
}

void GridScene::paintCell(const QPointF &scenePos, bool erase)
{
    if (!m_world) return;
    const QPoint g = sceneToGrid(scenePos);
    const int gx = g.x(), gz = g.y();

    const Block before = m_world->hasBlock(gx, m_currentLayer, gz)
                       ? m_world->getBlock(gx, m_currentLayer, gz) : Block{};

    if (erase) {
        if (!m_world->hasBlock(gx, m_currentLayer, gz)) return;
        m_world->clearBlock(gx, m_currentLayer, gz);
        m_flashFrames.remove(QPoint(gx, gz));
    } else {
        if (m_world->hasBlock(gx, m_currentLayer, gz)) return;
        m_world->setBlock(gx, m_currentLayer, gz,
                          Block::make(m_paintType, m_paintFacing));
    }

    if (m_history && m_pendingBatch) {
        const Block after = m_world->hasBlock(gx, m_currentLayer, gz)
                          ? m_world->getBlock(gx, m_currentLayer, gz) : Block{};
        m_pendingBatch->add({gx, m_currentLayer, gz, before, after});
    }

    for (int dx = -1; dx <= 1; ++dx)
    for (int dz = -1; dz <= 1; ++dz)
        update(QRectF(gridToScene(gx+dx, gz+dz), QSizeF(CELL_SIZE, CELL_SIZE)));
    emit blockModified(gx, m_currentLayer, gz);
}

void GridScene::interactCell(const QPointF &scenePos)
{
    if (!m_world) return;
    const QPoint g = sceneToGrid(scenePos);
    const int gx = g.x(), gz = g.y();
    if (!isInteractableSource(gx, gz)) return;
    emit sourceInteracted(gx, m_currentLayer, gz);
    for (int dx = -1; dx <= 1; ++dx)
    for (int dz = -1; dz <= 1; ++dz)
        update(QRectF(gridToScene(gx+dx, gz+dz), QSizeF(CELL_SIZE, CELL_SIZE)));
}

void GridScene::adjustCell(const QPointF &scenePos)
{
    if (!m_world) return;
    const QPoint g = sceneToGrid(scenePos);
    const int gx = g.x(), gz = g.y();
    if (!m_world->hasBlock(gx, m_currentLayer, gz)) return;
    Block *b = m_world->getBlockMutable(gx, m_currentLayer, gz);
    if (!b) return;

    const Block before = *b;

    switch (b->type) {
    case BlockType::Repeater:
        cycleRepeaterDelay(gx, gz);
        break;
    case BlockType::Comparator:
        b->flags = SimFlags::toggleComparatorMode(b->flags);
        m_world->notifyChange(gx, m_currentLayer, gz, *b);
        for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            update(QRectF(gridToScene(gx+dx, gz+dz), QSizeF(CELL_SIZE, CELL_SIZE)));
        emit blockModified(gx, m_currentLayer, gz);
        break;
    default:
        return;
    }

    if (m_history) {
        const Block after = m_world->hasBlock(gx, m_currentLayer, gz)
                          ? m_world->getBlock(gx, m_currentLayer, gz) : Block{};
        m_history->push(std::make_unique<PlaceCommand>(
            BlockChange{gx, m_currentLayer, gz, before, after}));
    }
}

void GridScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);
    const int l = static_cast<int>(std::floor(rect.left()  / CELL_SIZE)) * CELL_SIZE;
    const int t = static_cast<int>(std::floor(rect.top()   / CELL_SIZE)) * CELL_SIZE;
    const int r = static_cast<int>(std::ceil (rect.right() / CELL_SIZE)) * CELL_SIZE;
    const int b = static_cast<int>(std::ceil (rect.bottom()/ CELL_SIZE)) * CELL_SIZE;

    painter->setPen(QPen(QColor(255,255,255,30), 0.5));
    for (int x = l; x <= r; x += CELL_SIZE) painter->drawLine(x,t,x,b);
    for (int z = t; z <= b; z += CELL_SIZE) painter->drawLine(l,z,r,z);

    const int CHUNK = CELL_SIZE * 8;
    const int lc = static_cast<int>(std::floor(rect.left()  / CHUNK)) * CHUNK;
    const int tc = static_cast<int>(std::floor(rect.top()   / CHUNK)) * CHUNK;
    const int rc = static_cast<int>(std::ceil (rect.right() / CHUNK)) * CHUNK;
    const int bc = static_cast<int>(std::ceil (rect.bottom()/ CHUNK)) * CHUNK;
    painter->setPen(QPen(QColor(255,255,255,50), 0.8));
    for (int x = lc; x <= rc; x += CHUNK) painter->drawLine(x,t,x,b);
    for (int z = tc; z <= bc; z += CHUNK) painter->drawLine(l,z,r,z);

    painter->setPen(QPen(QColor(0x80,0x80,0xC0), 1.5));
    painter->drawLine(0,t,0,b);
    painter->drawLine(l,0,r,0);

    if (m_previewBelowLayer) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0x44,0xAA,0xFF,30));
        painter->drawRect(rect);
        painter->setPen(QColor(0x44,0xAA,0xFF,180));
        painter->setFont(QFont("Arial", 8, QFont::Bold));
        painter->drawText(QRectF(rect.left()+4, rect.top()+4, 200, 16),
            Qt::AlignLeft|Qt::AlignTop,
            QString("▼ Layer %1 Preview").arg(m_currentLayer - 1));
        painter->restore();
    }
}

void GridScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    if (!m_world) return;

    const int gxMin = static_cast<int>(std::floor(rect.left()  / CELL_SIZE)) - 1;
    const int gzMin = static_cast<int>(std::floor(rect.top()   / CELL_SIZE)) - 1;
    const int gxMax = static_cast<int>(std::ceil (rect.right() / CELL_SIZE)) + 1;
    const int gzMax = static_cast<int>(std::ceil (rect.bottom()/ CELL_SIZE)) + 1;

    if (m_previewBelowLayer && m_currentLayer - 1 >= VoxelWorld::LAYER_MIN) {
        const int belowLayer = m_currentLayer - 1;
        painter->save();
        painter->setOpacity(0.75);
        for (const auto &[coord, block] : m_world->allBlocks()) {
            if (coord.y != belowLayer || block.isEmpty()) continue;
            if (coord.x < gxMin || coord.x > gxMax) continue;
            if (coord.z < gzMin || coord.z > gzMax) continue;
            drawBlock(painter, coord.x, coord.z, block, 0.75);
        }
        painter->restore();
    }

    painter->setOpacity(1.0);
    for (const auto &[coord, block] : m_world->allBlocks()) {
        if (coord.y != m_currentLayer || block.isEmpty()) continue;
        if (coord.x < gxMin || coord.x > gxMax) continue;
        if (coord.z < gzMin || coord.z > gzMax) continue;
        drawBlock(painter, coord.x, coord.z, block, 1.0);
    }

    if (m_isDraggingMove)
        drawMovePreview(painter);

    if (m_hasSelection && m_editMode == EditMode::Select) {
        if (m_isDraggingMove)
            drawSelectionRect(painter, m_selRect.translated(m_moveOffset), false);
        else
            drawSelectionRect(painter, m_selRect, m_isDraggingSelect);
    }
}

void GridScene::drawMovePreview(QPainter *painter) const
{
    if (m_moveSnapshot.isEmpty()) return;

    const int ox = m_selRect.left();
    const int oz = m_selRect.top();
    const int dx = m_moveOffset.x();
    const int dz = m_moveOffset.y();

    painter->save();

    for (const auto &e : m_moveSnapshot) {
        const int sx = ox + e.dx;
        const int sz = oz + e.dz;
        const QRectF cell(gridToScene(sx, sz), QSizeF(CELL_SIZE, CELL_SIZE));

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0x1A, 0x1A, 0x2E, 160));
        painter->drawRect(cell);

        painter->setPen(QPen(QColor(0xFF, 0x90, 0x50, 60), 1.0));
        const int step = 6;
        for (int s = -CELL_SIZE; s < CELL_SIZE * 2; s += step) {
            painter->drawLine(
                QPointF(cell.left() + s,             cell.top()),
                QPointF(cell.left() + s + CELL_SIZE, cell.bottom()));
        }

        painter->setPen(QPen(QColor(0xFF, 0x90, 0x50, 120), 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(cell.adjusted(0.5, 0.5, -0.5, -0.5));
    }

    painter->setOpacity(0.75);
    for (const auto &e : m_moveSnapshot)
        drawBlock(painter, ox + e.dx + dx, oz + e.dz + dz, e.block, 0.75);
    painter->setOpacity(1.0);

    for (const auto &e : m_moveSnapshot) {
        const QRectF cell(gridToScene(ox + e.dx + dx, oz + e.dz + dz),
                          QSizeF(CELL_SIZE, CELL_SIZE));
        painter->setPen(QPen(QColor(0x44, 0xFF, 0x88, 180), 1.2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(cell.adjusted(0.5, 0.5, -0.5, -0.5));
    }

    if (!m_moveOffset.isNull()) {
        const QRect movedSel = m_selRect.translated(dx, dz);
        const QPointF center = gridToScene(
            movedSel.left() + movedSel.width()  / 2,
            movedSel.top()  + movedSel.height() / 2);

        const QString lbl = QString("Δ(%1, %2)")
            .arg(dx > 0 ? QString("+%1").arg(dx) : QString::number(dx))
            .arg(dz > 0 ? QString("+%1").arg(dz) : QString::number(dz));

        painter->setFont(QFont("Arial", 8, QFont::Bold));
        QFontMetrics fm(painter->font());
        const QRectF tb = fm.boundingRect(lbl);
        const QRectF bg(center.x()-tb.width()/2-4, center.y()-tb.height()/2-2,
                        tb.width()+8, tb.height()+4);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0x1A, 0x1A, 0x2E, 200));
        painter->drawRoundedRect(bg, 3, 3);
        painter->setPen(QColor(0x44, 0xFF, 0x88));
        painter->drawText(center, lbl);
    }

    painter->restore();
}

void GridScene::drawSelectionRect(QPainter *painter,
                                  const QRect &selRect,
                                  bool isDragging) const
{
    const QPointF tl = gridToScene(selRect.left(), selRect.top());
    const QPointF br = gridToScene(selRect.left() + selRect.width(),
                                   selRect.top()  + selRect.height());
    const QRectF sr(tl, br);

    painter->save();

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0xE0, 0x6C, 0x3A, isDragging ? 25 : 40));
    painter->drawRect(sr);

    QPen borderPen(QColor(0xE0, 0x6C, 0x3A), 2.0);
    if (isDragging) borderPen.setStyle(Qt::DashLine);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(sr.adjusted(0.5, 0.5, -0.5, -0.5));

    if (!isDragging) {
        painter->setPen(QPen(QColor(0xFF, 0x90, 0x50), 2.5));
        const int cs = 6;
        const QRectF r = sr.adjusted(1,1,-1,-1);
        painter->drawLine(r.topLeft(),     r.topLeft()     + QPointF( cs,  0));
        painter->drawLine(r.topLeft(),     r.topLeft()     + QPointF(  0, cs));
        painter->drawLine(r.topRight(),    r.topRight()    + QPointF(-cs,  0));
        painter->drawLine(r.topRight(),    r.topRight()    + QPointF(  0, cs));
        painter->drawLine(r.bottomLeft(),  r.bottomLeft()  + QPointF( cs,  0));
        painter->drawLine(r.bottomLeft(),  r.bottomLeft()  + QPointF(  0,-cs));
        painter->drawLine(r.bottomRight(), r.bottomRight() + QPointF(-cs,  0));
        painter->drawLine(r.bottomRight(), r.bottomRight() + QPointF(  0,-cs));
    }

    if (selRect.width() > 1 || selRect.height() > 1) {
        const QString label = QString("%1 × %2")
                              .arg(selRect.width()).arg(selRect.height());
        painter->setFont(QFont("Arial", 8, QFont::Bold));
        QFontMetrics fm(painter->font());
        const QRectF tb = fm.boundingRect(label);
        const QPointF lpos(sr.center().x(),
                           sr.height() >= 20 ? sr.center().y() : sr.bottom() + 12);
        const QRectF bg(lpos.x()-tb.width()/2-3, lpos.y()-tb.height()/2-2,
                        tb.width()+6, tb.height()+4);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0x1A, 0x1A, 0x2E, 180));
        painter->drawRoundedRect(bg, 3, 3);
        painter->setPen(QColor(0xFF, 0xCC, 0x88));
        painter->drawText(lpos, label);
    }

    painter->restore();
}

static QString texPathFromModel(const BlockModel &model, int preferredFace)
{
    for (const auto &elem : model.elements) {
        const ModelFace &f = elem.faces[preferredFace];
        if (f.present && !f.texturePath.isEmpty()) return f.texturePath;
    }
    for (const auto &elem : model.elements)
        for (const auto &f : elem.faces)
            if (f.present && !f.texturePath.isEmpty()) return f.texturePath;
    return {};
}

QString GridScene::getStateTexPath(const Block &block, int faceIndex) const
{
    BlockStateResult bsr;
    switch (block.type)
    {
    case BlockType::Repeater: {
        const bool active = (block.flags & SimFlags::ACTIVE) != 0;
        const bool locked = (block.flags & SimFlags::LOCKED) != 0;
        const int  delay  = SimFlags::getRepeaterDelay(block.flags) + 1;
        BlockStateQuery q;
        q.facing=block.facing; q.powered=active; q.matchPowered=true;
        q.locked=locked; q.matchLocked=true; q.repeaterDelay=delay; q.matchDelay=true;
        bsr = BlockStateLoader::getResultWithQuery("repeater", q);
        if (!bsr.isValid()) bsr = BlockStateLoader::getResult(block.type, block.facing);
        break;
    }
    case BlockType::RedstoneTorch: {
        const bool lit    = (block.flags & SimFlags::ACTIVE) != 0;
        const bool isWall = (block.facing!=BlockFacing::Up && block.facing!=BlockFacing::Down);
        const QString bsName = isWall ? QStringLiteral("redstone_wall_torch")
                                      : QStringLiteral("redstone_torch");
        BlockStateQuery q; q.facing=block.facing; q.lit=lit; q.matchLit=true;
        bsr = BlockStateLoader::getResultWithQuery(bsName, q);
        if (!bsr.isValid()) bsr = BlockStateLoader::getResult(block.type, block.facing);
        break;
    }
    case BlockType::Comparator: {
        const bool active   = (block.flags & SimFlags::ACTIVE) != 0;
        const bool subtract = SimFlags::isSubtractMode(block.flags);
        BlockStateQuery q; q.facing=block.facing;
        q.powered=active; q.matchPowered=true;
        q.mode=subtract?"subtract":"compare"; q.matchMode=true;
        bsr = BlockStateLoader::getResultWithQuery("comparator", q);
        if (!bsr.isValid()) bsr = BlockStateLoader::getResult(block.type, block.facing);
        break;
    }
    case BlockType::Piston:
    case BlockType::StickyPiston: {
        const bool extended = (block.flags & SimFlags::LIT) != 0;
        const QString bsId  = (block.type==BlockType::StickyPiston)
                              ? QStringLiteral("sticky_piston")
                              : QStringLiteral("piston");
        BlockStateQuery q; q.facing=block.facing; q.extended=extended; q.matchExtended=true;
        bsr = BlockStateLoader::getResultWithQuery(bsId, q);
        if (!bsr.isValid()) bsr = BlockStateLoader::getResult(block.type, block.facing);
        break;
    }
    case BlockType::PistonHead: {
        const bool sticky = (block.flags & SimFlags::ACTIVE) != 0;
        const QString modelName = sticky ? QStringLiteral("block/piston_head_sticky")
                                         : QStringLiteral("block/piston_head");
        BlockModel model = BlockModelLoader::load(modelName);
        if (!model.isValid || model.elements.isEmpty()) return QString();
        return texPathFromModel(model, faceIndex);
    }
    default: return QString();
    }
    if (!bsr.isValid()) return QString();
    BlockModel model = BlockModelLoader::load(bsr.modelName);
    if (!model.isValid || model.elements.isEmpty()) return QString();
    return texPathFromModel(model, faceIndex);
}

GridScene::TexInfo GridScene::getTopViewTex(const Block &block) const
{
    const BlockFacing f  = block.facing;
    const bool horiz  = (f==BlockFacing::North||f==BlockFacing::South
                      || f==BlockFacing::East ||f==BlockFacing::West);
    const bool facingUp = (f==BlockFacing::Up);
    const bool facingDn = (f==BlockFacing::Down);
    const double rot    = horiz ? facingToAngle(f) : 0.0;

    switch (block.type)
    {
    case BlockType::Repeater: {
        const QString path = getStateTexPath(block, FACE_UP);
        if (!path.isEmpty()) return {path, rot, true};
        return {QStringLiteral("repeater_top"), rot, true};
    }
    case BlockType::RedstoneTorch: {
        const bool isWall = (block.facing!=BlockFacing::Up && block.facing!=BlockFacing::Down);
        const double r2 = isWall ? facingToAngle(block.facing) : 0.0;
        const QString path = getStateTexPath(block, FACE_NORTH);
        if (!path.isEmpty()) return {path, r2, false};
        const auto &meta = getBlockMeta(block.type);
        return {QString(meta.enumKey), r2, false};
    }
    case BlockType::Comparator: {
        const QString path = getStateTexPath(block, FACE_UP);
        if (!path.isEmpty()) return {path, rot, true};
        return {QStringLiteral("comparator_top"), rot, true};
    }
    case BlockType::Piston:
    case BlockType::StickyPiston: {
        if (facingUp) {
            const QString path = getStateTexPath(block, FACE_UP);
            if (!path.isEmpty()) return {path, 0.0, false};
            return {block.type==BlockType::StickyPiston
                    ? QStringLiteral("piston_top_sticky")
                    : QStringLiteral("piston_top_normal"), 0.0, false};
        }
        if (facingDn) return {QStringLiteral("piston_bottom"), 0.0, false};
        {
            const QString path = getStateTexPath(block, FACE_UP);
            if (!path.isEmpty()) return {path, rot, true};
            return {QStringLiteral("piston_side"), rot, true};
        }
    }
    case BlockType::PistonHead: {
        const bool sticky = (block.flags & SimFlags::ACTIVE) != 0;
        return {sticky ? QStringLiteral("piston_top_sticky")
                       : QStringLiteral("piston_top_normal"), rot, false};
    }
    case BlockType::Observer:
        if (facingUp) return {"observer_front",0.0,false};
        if (facingDn) return {"observer_back", 0.0,false};
        return {"observer_top", rot, true};
    case BlockType::Dropper:
        if (!horiz) return {"dropper_front_vertical",0.0,false};
        return {"dropper_front", rot, true};
    case BlockType::Dispenser:
        if (!horiz) return {"dispenser_front_vertical",0.0,false};
        return {"dispenser_front", rot, true};
    case BlockType::Hopper:
        return {"hopper_top", 0.0, false};
    case BlockType::TrappedChest: {
        static QString chestTopKey;
        if (chestTopKey.isEmpty()) {
            BlockModel ref = BlockModelLoader::load("block/stone");
            if (ref.isValid && !ref.elements.isEmpty()) {
                for (const auto &elem : ref.elements) {
                    for (const auto &face : elem.faces) {
                        if (face.present && !face.texturePath.isEmpty()) {
                            QString tp = face.texturePath;
                            int i = tp.indexOf("/textures/");
                            if (i >= 0)
                                chestTopKey = "@crop:" + tp.left(i) +
                                    "/textures/entity/chest/trapped.png|14,19,14,14";
                            break;
                        }
                    }
                    if (!chestTopKey.isEmpty()) break;
                }
            }
        }
        return {chestTopKey, rot, true};
    }
    default: {
        const auto &meta = getBlockMeta(block.type);
        const bool needRot = meta.hasDirection && horiz;
        return {QString(meta.enumKey), needRot ? rot : 0.0, meta.hasDirection && horiz};
    }
    }
}

QColor GridScene::dustColor(uint8_t power)
{
    const float t = static_cast<float>(power) / 15.0f;
    return QColor(static_cast<int>(0x5C + t*(0xFF-0x5C)),
                  static_cast<int>(t*0x50), 0);
}
QColor GridScene::sourceActiveColor() { return QColor(0xC8,0xFF,0x00,220); }
QColor GridScene::modeOverlayColor(EditMode mode)
{
    switch (mode) {
    case EditMode::Paint:    return QColor(0x26,0x8B,0xD2);
    case EditMode::Select:   return QColor(0xE0,0x6C,0x3A);
    case EditMode::Interact: return QColor(0x2A,0xA1,0x98);
    }
    return Qt::white;
}

void GridScene::drawRepeaterDelay(QPainter *painter,
                                  const QRectF &cell, const Block &block) const
{
    if (block.type != BlockType::Repeater) return;
    painter->save();

    const uint8_t delayIndex = SimFlags::getRepeaterDelay(block.flags);
    const uint8_t delayTicks = delayIndex + 1;
    const bool    active     = (block.flags & SimFlags::ACTIVE) != 0;
    constexpr double SQ = 3.5, GAP = 1.5;

    const QPointF cen  = cell.center();
    const bool    isNS = (block.facing==BlockFacing::North||block.facing==BlockFacing::South);
    const double  totalLen = delayTicks*SQ + (delayTicks-1)*GAP;

    QPointF origin; double stepX=0, stepY=0;
    if (isNS) { origin=QPointF(cen.x()-totalLen/2.0,cen.y()-SQ/2.0); stepX=SQ+GAP; }
    else      { origin=QPointF(cen.x()-SQ/2.0,cen.y()-totalLen/2.0); stepY=SQ+GAP; }

    for (uint8_t i = 0; i < delayTicks; ++i) {
        const QRectF sq(origin.x()+i*stepX, origin.y()+i*stepY, SQ, SQ);
        const bool isLast = (i == delayTicks-1);
        if (isLast) painter->setBrush(active?QColor(0xFF,0xCC,0x00):QColor(0xCC,0xCC,0x88));
        else        painter->setBrush(active?QColor(0x88,0x55,0x00):QColor(0x66,0x66,0x44));
        painter->setPen(Qt::NoPen);
        painter->drawRect(sq);
    }
    painter->setPen(QColor(0xFF,0xFF,0xFF,180));
    painter->setFont(QFont("Arial", 5, QFont::Bold));
    painter->drawText(QRectF(cell.right()-13,cell.bottom()-9,12,8),
                      Qt::AlignRight|Qt::AlignBottom,
                      QString("%1t").arg(delayTicks));
    painter->restore();
}

void GridScene::drawModelTopView(QPainter *painter, const QRectF &cell,
                                  const BlockModel &model,
                                  float rotXdeg, float rotYdeg) const
{
    auto rotateY = [](const QVector3D &v, const QVector3D &p, float deg) -> QVector3D {
        if (qFuzzyIsNull(deg)) return v;
        float c=qCos(qDegreesToRadians(deg)), s=qSin(qDegreesToRadians(deg));
        float dx=v.x()-p.x(), dz=v.z()-p.z();
        return {p.x()+dx*c+dz*s, v.y(), p.z()-dx*s+dz*c};
    };
    auto rotateX = [](const QVector3D &v, const QVector3D &p, float deg) -> QVector3D {
        if (qFuzzyIsNull(deg)) return v;
        float c=qCos(qDegreesToRadians(deg)), s=qSin(qDegreesToRadians(deg));
        float dy=v.y()-p.y(), dz=v.z()-p.z();
        return {v.x(), p.y()+dy*c-dz*s, p.z()+dy*s+dz*c};
    };
    auto rotAround = [](const QVector3D &v, const QVector3D &o,
                        int axis, float deg) -> QVector3D {
        if (qFuzzyIsNull(deg)) return v;
        QVector3D d=v-o;
        float c=qCos(qDegreesToRadians(deg)), s=qSin(qDegreesToRadians(deg));
        QVector3D r;
        switch(axis){
        case 0: r={d.x(),d.y()*c-d.z()*s,d.y()*s+d.z()*c}; break;
        case 2: r={d.x()*c-d.y()*s,d.x()*s+d.y()*c,d.z()};  break;
        default:r={d.x()*c+d.z()*s,d.y(),-d.x()*s+d.z()*c}; break;
        }
        return o+r;
    };

    static const QVector3D FACE_NORMALS[6] = {
        {0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{-1,0,0},{1,0,0}
    };
    auto project = [&](const QVector3D &v) -> QPointF {
        return QPointF(cell.left()+v.x()*cell.width(), cell.top()+v.z()*cell.height());
    };

    painter->save();
    painter->setClipRect(cell);

    for (const auto &elem : model.elements) {
        float x0=elem.from[0]/16.f, y0=elem.from[1]/16.f, z0=elem.from[2]/16.f;
        float x1=elem.to[0]/16.f,   y1=elem.to[1]/16.f,   z1=elem.to[2]/16.f;

        for (int fi = 0; fi < 6; ++fi) {
            const ModelFace &face = elem.faces[fi];
            if (!face.present || face.texturePath.isEmpty()) continue;

            QVector3D c[4];
            switch(fi){
            case FACE_UP:    c[0]={x0,y1,z0};c[1]={x1,y1,z0};c[2]={x1,y1,z1};c[3]={x0,y1,z1}; break;
            case FACE_DOWN:  c[0]={x0,y0,z1};c[1]={x1,y0,z1};c[2]={x1,y0,z0};c[3]={x0,y0,z0}; break;
            case FACE_NORTH: c[0]={x0,y1,z0};c[1]={x1,y1,z0};c[2]={x1,y0,z0};c[3]={x0,y0,z0}; break;
            case FACE_SOUTH: c[0]={x1,y1,z1};c[1]={x0,y1,z1};c[2]={x0,y0,z1};c[3]={x1,y0,z1}; break;
            case FACE_WEST:  c[0]={x0,y1,z1};c[1]={x0,y1,z0};c[2]={x0,y0,z0};c[3]={x0,y0,z1}; break;
            case FACE_EAST:  c[0]={x1,y1,z0};c[1]={x1,y1,z1};c[2]={x1,y0,z1};c[3]={x1,y0,z0}; break;
            default: continue;
            }

            if (!qFuzzyIsNull(elem.rotAngle)) {
                QVector3D org(elem.rotOrigin[0]/16.f,elem.rotOrigin[1]/16.f,elem.rotOrigin[2]/16.f);
                for (auto &v:c) v=rotAround(v,org,elem.rotAxis,elem.rotAngle);
            }
            const QVector3D pivot(0.5f,0.5f,0.5f);
            if (!qFuzzyIsNull(rotXdeg)) for (auto &v:c) v=rotateX(v,pivot,rotXdeg);
            if (!qFuzzyIsNull(rotYdeg)) for (auto &v:c) v=rotateY(v,pivot,rotYdeg);

            QVector3D n=FACE_NORMALS[fi];
            if (!qFuzzyIsNull(elem.rotAngle)) n=rotAround(n,{0,0,0},elem.rotAxis,elem.rotAngle);
            if (!qFuzzyIsNull(rotXdeg)) n=rotateX(n,{0,0,0},rotXdeg);
            if (!qFuzzyIsNull(rotYdeg)) n=rotateY(n,{0,0,0},rotYdeg);
            n=n.normalized();
            if (n.y()<0.05f) continue;

            const float u0_mc=face.uv[0]/16.f, v0_mc=face.uv[1]/16.f;
            const float u1_mc=face.uv[2]/16.f, v1_mc=face.uv[3]/16.f;
            QVector2D mc[4]={{u0_mc,v0_mc},{u1_mc,v0_mc},{u1_mc,v1_mc},{u0_mc,v1_mc}};
            const int steps=((face.rotation/90)%4+4)%4;
            QVector2D mc_rot[4];
            for (int i=0;i<4;i++) mc_rot[i]=mc[(i+steps)%4];

            QPointF pts[4];
            for (int i=0;i<4;i++) pts[i]=project(c[i]);

            QPixmap tex=loadPixmap(face.texturePath);
            if (tex.isNull()) continue;
            const float tw=tex.width(), th=tex.height();

            QPolygonF srcQuad, dstQuad;
            for (int i=0;i<4;i++) {
                srcQuad<<QPointF(mc_rot[i].x()*tw, mc_rot[i].y()*th);
                dstQuad<<pts[i];
            }
            QTransform transform;
            if (!QTransform::quadToQuad(srcQuad,dstQuad,transform)) continue;

            painter->save();
            painter->setClipRegion(
                QRegion(dstQuad.toPolygon()).intersected(QRegion(cell.toRect())),
                Qt::ReplaceClip);
            painter->setTransform(transform,true);
            const float brightness=qBound(0.5f,n.y(),1.0f);
            if (brightness<0.99f) painter->setOpacity(0.5+brightness*0.5);
            painter->drawPixmap(0,0,tex);
            painter->restore();
        }
    }
    painter->restore();
}

void GridScene::drawSimOverlay(QPainter *painter,
                               const QRectF &cell, const Block &block) const
{
    painter->save();
    const bool active        = (block.flags & SimFlags::ACTIVE)         != 0;
    const bool lit           = (block.flags & SimFlags::LIT)            != 0;
    const bool strongPowered = (block.flags & SimFlags::STRONG_POWERED) != 0;

    switch (block.type)
    {
    case BlockType::Lever:
    case BlockType::StoneButton:
    case BlockType::WoodButton:
    case BlockType::StonePressurePlate:
    case BlockType::WoodPressurePlate:
    case BlockType::LightWeightedPressurePlate:
    case BlockType::HeavyWeightedPressurePlate:
        if (active) {
            painter->setPen(QPen(sourceActiveColor(), 2.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(cell.adjusted(1,1,-1,-1));
            painter->setBrush(sourceActiveColor());
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPointF(cell.right()-5,cell.top()+5), 3, 3);
        } else {
            painter->setPen(QPen(QColor(0x88,0x88,0x88,120), 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(cell.adjusted(2,2,-2,-2));
        }
        break;
    case BlockType::RedstoneLamp:
        if (lit) {
            painter->setBrush(QColor(0xFF,0xEE,0x44,80));
            painter->setPen(Qt::NoPen);
            painter->drawRect(cell);
            painter->setPen(QPen(QColor(0xFF,0xEE,0x44,220), 2.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(cell.adjusted(1,1,-1,-1));
            painter->setBrush(QColor(0xFF,0xFF,0x88));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPointF(cell.right()-5,cell.top()+5), 3, 3);
        }
        break;
    case BlockType::RedstoneTorch:
        if (active) {
            painter->setBrush(QColor(0xFF,0x80,0x00,45));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(cell.center(), CELL_SIZE*0.42, CELL_SIZE*0.42);
        }
        break;
    case BlockType::Repeater:
        if (active) {
            painter->setPen(QPen(QColor(0xFF,0xAA,0x00,200), 2.0));
            const QPointF cen=cell.center(); const double half=CELL_SIZE*0.35;
            switch(block.facing){
            case BlockFacing::North: painter->drawLine(QPointF(cen.x()-4,cen.y()-half),QPointF(cen.x()+4,cen.y()-half)); break;
            case BlockFacing::South: painter->drawLine(QPointF(cen.x()-4,cen.y()+half),QPointF(cen.x()+4,cen.y()+half)); break;
            case BlockFacing::East:  painter->drawLine(QPointF(cen.x()+half,cen.y()-4),QPointF(cen.x()+half,cen.y()+4)); break;
            case BlockFacing::West:  painter->drawLine(QPointF(cen.x()-half,cen.y()-4),QPointF(cen.x()-half,cen.y()+4)); break;
            default: break;
            }
        }
        drawRepeaterDelay(painter, cell, block);
        break;
    case BlockType::Comparator: {
        if (block.power > 0) {
            painter->setBrush(dustColor(block.power));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(cell.center(), 3.0, 3.0);
        }
        const bool subtract = SimFlags::isSubtractMode(block.flags);
        painter->setPen(QColor(0xFF,0xFF,0xFF,180));
        painter->setFont(QFont("Arial", 5, QFont::Bold));
        painter->drawText(QRectF(cell.right()-13,cell.bottom()-9,12,8),
                          Qt::AlignRight|Qt::AlignBottom, subtract?"-":"=");
        break;
    }
    case BlockType::IronDoor:
    case BlockType::IronTrapdoor:
    case BlockType::FenceGate:
        if (lit) {
            painter->setBrush(QColor(0x44,0xFF,0x44,50));
            painter->setPen(Qt::NoPen);
            painter->drawRect(cell);
        }
        break;
    case BlockType::Piston:
    case BlockType::StickyPiston:
        if (lit) {
            painter->setBrush(QColor(0xFF,0x44,0x44,220));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPointF(cell.right()-5,cell.bottom()-5),3,3);
        }
        break;
    case BlockType::PistonHead: {
        const bool sticky=(block.flags&SimFlags::ACTIVE)!=0;
        painter->setPen(QPen(sticky?QColor(0x44,0xAA,0x44,180):QColor(0xAA,0x88,0x44,180),1.2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(cell.adjusted(1,1,-1,-1));
        break;
    }
    default:
        if (strongPowered) {
            painter->setPen(QPen(QColor(0x44,0xAA,0xFF,150),1.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(cell.adjusted(2,2,-2,-2));
        }
        break;
    }
    painter->restore();
}

void GridScene::drawFlashOverlay(QPainter *painter,
                                 const QRectF &cell, int gx, int gz) const
{
    auto it = m_flashFrames.find(QPoint(gx, gz));
    if (it == m_flashFrames.end() || it.value() <= 0) return;
    const int alpha = static_cast<int>(
        static_cast<float>(it.value()) / FLASH_FRAMES_INIT * 120.0f);
    painter->save();
    painter->setBrush(QColor(0xFF,0xFF,0xFF,alpha));
    painter->setPen(Qt::NoPen);
    painter->drawRect(cell);
    painter->restore();
}

void GridScene::drawBlock(QPainter *painter,
                          int gx, int gz,
                          const Block &block,
                          double ) const
{
    const QRectF cell (gridToScene(gx, gz), QSizeF(CELL_SIZE, CELL_SIZE));
    const QRectF inner = cell.adjusted(1, 1, -1, -1);

    painter->save();

    if (block.type == BlockType::RedstoneWire)
    {
        const auto conn  = getDustConnections(gx, gz);
        const int  nConn = (conn.n?1:0)+(conn.s?1:0)+(conn.e?1:0)+(conn.w?1:0);

        painter->fillRect(inner, QColor(0x20,0x05,0x05));
        const QColor  tint   = dustColor(block.power);
        const QPixmap lineNS = loadPixmap("redstone_dust_line0");
        const QPixmap dot    = loadPixmap("redstone_dust_dot");

        const QRectF halfN(inner.left(), inner.top(),
                           inner.width(), inner.height()/2.0);
        const QRectF halfS(inner.left(), inner.top()+inner.height()/2.0,
                           inner.width(), inner.height()/2.0);
        const QRectF halfE(inner.left()+inner.width()/2.0, inner.top(),
                           inner.width()/2.0, inner.height());
        const QRectF halfW(inner.left(), inner.top(),
                           inner.width()/2.0, inner.height());

        auto drawHalf = [&](const QRectF &clipRect, const QPixmap &pix,
                            double rotateDeg = 0.0) {
            if (pix.isNull()) return;
            painter->save();
            painter->setClipRect(clipRect);
            if (!qFuzzyIsNull(rotateDeg)) {
                painter->translate(cell.center());
                painter->rotate(rotateDeg);
                painter->translate(-cell.center());
            }
            drawTinted(painter, inner, pix, tint);
            painter->restore();
        };

        if (nConn == 0) { drawTinted(painter, inner, dot, tint); }
        else {
            if (conn.n) drawHalf(halfN, lineNS);
            if (conn.s) drawHalf(halfS, lineNS);
            if (conn.e) drawHalf(halfE, lineNS, 90.0);
            if (conn.w) drawHalf(halfW, lineNS, 90.0);
            drawTinted(painter, inner, dot, tint);
        }

        {
            const int y = m_currentLayer;
            painter->save();
            painter->setPen(Qt::NoPen);
            struct ArrDef { int dx,dz; QPointF tip,pl,pr; };
            const QPointF cen = cell.center();
            const double aw = 2.0;
            ArrDef dirs[4] = {
                {0,-1,{cen.x(),cell.top()+3.5},{cen.x()-aw,cell.top()+3.5+aw*1.5},{cen.x()+aw,cell.top()+3.5+aw*1.5}},
                {0, 1,{cen.x(),cell.bottom()-3.5},{cen.x()-aw,cell.bottom()-3.5-aw*1.5},{cen.x()+aw,cell.bottom()-3.5-aw*1.5}},
                {1, 0,{cell.right()-3.5,cen.y()},{cell.right()-3.5-aw*1.5,cen.y()-aw},{cell.right()-3.5-aw*1.5,cen.y()+aw}},
                {-1,0,{cell.left()+3.5,cen.y()},{cell.left()+3.5+aw*1.5,cen.y()-aw},{cell.left()+3.5+aw*1.5,cen.y()+aw}},
            };
            for (const auto &d : dirs) {
                Block above = m_world->getBlock(gx,y+1,gz);
                if (RedstoneLogic::isTransparent(above.type)) {
                    Block tb = m_world->getBlock(gx+d.dx,y+1,gz+d.dz);
                    if (tb.type == BlockType::RedstoneWire) {
                        QColor ac = dustColor(tb.power); ac.setAlpha(200);
                        painter->setBrush(ac);
                        QPolygonF tri; tri<<d.tip<<d.pl<<d.pr;
                        painter->drawPolygon(tri);
                    }
                }
                Block side = m_world->getBlock(gx+d.dx,y,gz+d.dz);
                if (RedstoneLogic::isTransparent(side.type)) {
                    Block tb = m_world->getBlock(gx+d.dx,y-1,gz+d.dz);
                    if (tb.type == BlockType::RedstoneWire) {
                        QColor ac = dustColor(tb.power); ac.setAlpha(110);
                        painter->setBrush(ac);
                        QPolygonF tri; tri<<d.tip<<d.pl<<d.pr;
                        painter->drawPolygon(tri);
                    }
                }
            }
            painter->restore();
        }

        if (block.power > 0) {
            painter->save();
            painter->setPen(block.power<=3 ? QColor(0xFF,0xCC,0x00)
                                           : QColor(0xFF,0xFF,0xFF,160));
            painter->setFont(QFont("Arial", 6, QFont::Bold));
            painter->drawText(QRectF(cell.left()+1,cell.top()+1,10,10),
                              Qt::AlignLeft|Qt::AlignTop,
                              QString::number(block.power));
            painter->restore();
        }

        painter->restore();
        painter->save();
        drawFlashOverlay(painter, cell, gx, gz);
        painter->restore();
        return;
    }

    if (block.type == BlockType::Piston || block.type == BlockType::StickyPiston)
    {
        painter->restore();
        const bool extended = (block.flags & SimFlags::LIT) != 0;
        const bool sticky   = (block.type == BlockType::StickyPiston);
        BlockStateQuery q; q.facing=block.facing; q.extended=extended; q.matchExtended=true;
        const QString bsId = sticky ? QStringLiteral("sticky_piston")
                                    : QStringLiteral("piston");
        BlockStateResult bsr = BlockStateLoader::getResultWithQuery(bsId, q);
        if (!bsr.isValid()) bsr = BlockStateLoader::getResult(block.type, block.facing);

        painter->save();
        painter->fillRect(inner, fallbackColor(block.type));
        if (bsr.isValid()) {
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if (model.isValid && !model.elements.isEmpty()) {
                drawModelTopView(painter, inner, model, -bsr.rotX, -bsr.rotY);
                if (extended) {
                    const QString headName = sticky
                        ? QStringLiteral("block/piston_head_sticky")
                        : QStringLiteral("block/piston_head");
                    BlockModel headModel = BlockModelLoader::load(headName);
                    if (headModel.isValid && !headModel.elements.isEmpty()) {
                        float headRotX=0.f, headRotY=0.f;
                        switch(block.facing){
                        case BlockFacing::South: headRotY=180.f; break;
                        case BlockFacing::East:  headRotY= 90.f; break;
                        case BlockFacing::West:  headRotY=270.f; break;
                        case BlockFacing::Up:    headRotX=270.f; break;
                        case BlockFacing::Down:  headRotX= 90.f; break;
                        default: break;
                        }
                        BlockModel shiftedHead = headModel;
                        for (auto &elem : shiftedHead.elements) {
                            elem.from[2] -= 16.f; elem.to[2] -= 16.f;
                        }
                        drawModelTopView(painter, inner, shiftedHead, -headRotX, -headRotY);
                    }
                }
            }
        }
        painter->restore();
        painter->save();
        drawSimOverlay  (painter, cell, block);
        drawFlashOverlay(painter, cell, gx, gz);
        painter->restore();
        return;
    }

    if (block.type == BlockType::PistonHead)
    {
        painter->restore();
        const bool sticky = (block.flags & SimFlags::ACTIVE) != 0;
        const QString modelName = sticky ? QStringLiteral("block/piston_head_sticky")
                                         : QStringLiteral("block/piston_head");
        BlockModel model = BlockModelLoader::load(modelName);
        float rotX=0.f, rotY=0.f;
        switch(block.facing){
        case BlockFacing::South: rotY=180.f; break;
        case BlockFacing::East:  rotY= 90.f; break;
        case BlockFacing::West:  rotY=270.f; break;
        case BlockFacing::Up:    rotX=270.f; break;
        case BlockFacing::Down:  rotX= 90.f; break;
        default: break;
        }
        painter->save();
        if (model.isValid && !model.elements.isEmpty())
            drawModelTopView(painter, inner, model, -rotX, -rotY);
        painter->restore();
        painter->save();
        drawSimOverlay  (painter, cell, block);
        drawFlashOverlay(painter, cell, gx, gz);
        painter->restore();
        return;
    }

    const TexInfo info = getTopViewTex(block);
    const QPixmap tex  = loadPixmap(info.key);

    if (block.type == BlockType::RedstoneLamp) {
        const bool lit = (block.flags & SimFlags::LIT) != 0;
        painter->fillRect(inner, lit ? QColor(0xFF,0xEE,0x44) : QColor(0x80,0x78,0x20));
        if (!tex.isNull()) painter->drawPixmap(inner.toRect(), tex);
    } else {
        if (tex.isNull())
            painter->fillRect(inner, fallbackColor(block.type));
        else {
            if (!qFuzzyIsNull(info.rotateDeg)) {
                painter->translate(cell.center());
                painter->rotate(info.rotateDeg);
                painter->translate(-cell.center());
            }
            painter->drawPixmap(inner.toRect(), tex);
        }
    }

    if (info.showArrow) {
        painter->save();
        painter->setPen(QPen(QColor(255,255,255,170), 1.0));
        const QPointF cen = cell.center();
        const double  r   = CELL_SIZE * 0.28;
        const QPointF tip(cen.x(), cen.y()-r);
        painter->drawLine(cen, tip);
        painter->drawLine(tip, QPointF(tip.x()-2.5, tip.y()+3.5));
        painter->drawLine(tip, QPointF(tip.x()+2.5, tip.y()+3.5));
        painter->restore();
    }

    const auto &meta = getBlockMeta(block.type);
    if (meta.canFaceVertically &&
        (block.facing==BlockFacing::Up || block.facing==BlockFacing::Down)) {
        painter->save();
        painter->setPen(QColor(255,255,255,200));
        painter->setFont(QFont("Arial", 7, QFont::Bold));
        painter->drawText(QRectF(cell.right()-11,cell.top()+1,10,10),
                          Qt::AlignCenter,
                          block.facing==BlockFacing::Up ? "↑" : "↓");
        painter->restore();
    }

    if (block.type == BlockType::Hopper)
        drawHopperIndicator(painter, cell, block.facing);

    painter->restore();
    painter->save();
    drawSimOverlay  (painter, cell, block);
    drawFlashOverlay(painter, cell, gx, gz);
    painter->restore();
}

void GridScene::drawHopperIndicator(QPainter *painter,
                                    const QRectF &cell, BlockFacing facing) const
{
    painter->save();
    const QColor arrowColor(0x44,0xEE,0x44,220);
    painter->setPen(QPen(arrowColor,1.5));
    painter->setBrush(arrowColor);
    const QPointF cen = cell.center();
    if (facing == BlockFacing::Down) {
        const double d = CELL_SIZE*0.12;
        QPolygonF diamond;
        diamond<<QPointF(cen.x(),cen.y()-d)<<QPointF(cen.x()+d,cen.y())
               <<QPointF(cen.x(),cen.y()+d)<<QPointF(cen.x()-d,cen.y());
        painter->drawPolygon(diamond);
    } else {
        const double off=CELL_SIZE*0.5*0.75, aw=CELL_SIZE*0.12, ah=CELL_SIZE*0.14;
        QPolygonF arrow;
        switch(facing){
        case BlockFacing::North: arrow<<QPointF(cen.x(),cen.y()-off-ah)<<QPointF(cen.x()-aw,cen.y()-off)<<QPointF(cen.x()+aw,cen.y()-off); break;
        case BlockFacing::South: arrow<<QPointF(cen.x(),cen.y()+off+ah)<<QPointF(cen.x()-aw,cen.y()+off)<<QPointF(cen.x()+aw,cen.y()+off); break;
        case BlockFacing::East:  arrow<<QPointF(cen.x()+off+ah,cen.y())<<QPointF(cen.x()+off,cen.y()-aw)<<QPointF(cen.x()+off,cen.y()+aw); break;
        case BlockFacing::West:  arrow<<QPointF(cen.x()-off-ah,cen.y())<<QPointF(cen.x()-off,cen.y()-aw)<<QPointF(cen.x()-off,cen.y()+aw); break;
        default: break;
        }
        if (!arrow.isEmpty()) painter->drawPolygon(arrow);
    }
    painter->restore();
}

GridScene::DustConn GridScene::getDustConnections(int gx, int gz) const
{
    DustConn c{false,false,false,false};
    if (!m_world) return c;
    const int y = m_currentLayer;
    c.n=canConnectDust(gx,gz-1); c.s=canConnectDust(gx,gz+1);
    c.e=canConnectDust(gx+1,gz); c.w=canConnectDust(gx-1,gz);
    {
        Block above = m_world->getBlock(gx,y+1,gz);
        if (RedstoneLogic::isTransparent(above.type)) {
            if (!c.n && m_world->getBlock(gx,  y+1,gz-1).type==BlockType::RedstoneWire) c.n=true;
            if (!c.s && m_world->getBlock(gx,  y+1,gz+1).type==BlockType::RedstoneWire) c.s=true;
            if (!c.e && m_world->getBlock(gx+1,y+1,gz  ).type==BlockType::RedstoneWire) c.e=true;
            if (!c.w && m_world->getBlock(gx-1,y+1,gz  ).type==BlockType::RedstoneWire) c.w=true;
        }
    }
    if (!c.n && RedstoneLogic::isTransparent(m_world->getBlock(gx,  y,gz-1).type))
        c.n=(m_world->getBlock(gx,  y-1,gz-1).type==BlockType::RedstoneWire);
    if (!c.s && RedstoneLogic::isTransparent(m_world->getBlock(gx,  y,gz+1).type))
        c.s=(m_world->getBlock(gx,  y-1,gz+1).type==BlockType::RedstoneWire);
    if (!c.e && RedstoneLogic::isTransparent(m_world->getBlock(gx+1,y,gz  ).type))
        c.e=(m_world->getBlock(gx+1,y-1,gz  ).type==BlockType::RedstoneWire);
    if (!c.w && RedstoneLogic::isTransparent(m_world->getBlock(gx-1,y,gz  ).type))
        c.w=(m_world->getBlock(gx-1,y-1,gz  ).type==BlockType::RedstoneWire);
    return c;
}

bool GridScene::canConnectDust(int gx, int gz) const
{
    if (!m_world) return false;
    const Block &b = m_world->getBlock(gx, m_currentLayer, gz);
    if (b.isEmpty()) return false;
    switch(b.type){
    case BlockType::RedstoneWire:    case BlockType::RedstoneTorch:
    case BlockType::RedstoneBlock:   case BlockType::Repeater:
    case BlockType::Comparator:      case BlockType::Observer:
    case BlockType::Lever:           case BlockType::StoneButton:
    case BlockType::WoodButton:      case BlockType::StonePressurePlate:
    case BlockType::WoodPressurePlate:
    case BlockType::LightWeightedPressurePlate:
    case BlockType::HeavyWeightedPressurePlate:
    case BlockType::TargetBlock:     case BlockType::Lectern:
    case BlockType::RedstoneLamp:    case BlockType::Dropper:
    case BlockType::Dispenser:       case BlockType::Hopper:
    case BlockType::TNT:             case BlockType::NoteBlock:
    case BlockType::Stone:           case BlockType::Other:
        return true;
    default: return false;
    }
}

void GridScene::drawTinted(QPainter *painter, const QRectF &rect,
                           const QPixmap &pix, const QColor &tint) const
{
    if (pix.isNull()) return;
    QPixmap tinted(pix.size());
    tinted.fill(Qt::transparent);
    {
        QPainter tp(&tinted);
        tp.drawPixmap(0,0,pix);
        tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tp.fillRect(tinted.rect(), tint);
    }
    painter->drawPixmap(rect.toRect(), tinted);
}

QPixmap GridScene::loadPixmap(const QString &key, int size)
{
    static std::unordered_map<std::string, QPixmap> cache;
    const std::string k = (key+"@"+QString::number(size)).toStdString();
    auto it = cache.find(k);
    if (it != cache.end()) return it->second;

    QPixmap raw;
    if (key.startsWith("@crop:")) {
        QString rest = key.mid(6);
        int sep = rest.lastIndexOf('|');
        if (sep > 0) {
            QStringList nums = rest.mid(sep+1).split(',');
            if (nums.size() == 4) {
                QPixmap full(rest.left(sep));
                if (!full.isNull())
                    raw = full.copy(nums[0].toInt(), nums[1].toInt(),
                                    nums[2].toInt(), nums[3].toInt());
            }
        }
    } else if (QDir::isAbsolutePath(key)) {
        raw.load(key);
    } else {
        raw.load(QString(":/textures/%1.png").arg(key));
    }

    QPixmap scaled;
    if (!raw.isNull())
        scaled = raw.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    cache[k] = scaled;
    return scaled;
}

double GridScene::facingToAngle(BlockFacing f)
{
    switch(f){
    case BlockFacing::North: return   0.0;
    case BlockFacing::East:  return  90.0;
    case BlockFacing::South: return 180.0;
    case BlockFacing::West:  return 270.0;
    default: return 0.0;
    }
}

BlockFacing GridScene::rotateFacingCW(BlockFacing f)
{
    switch(f){
    case BlockFacing::North: return BlockFacing::East;
    case BlockFacing::East:  return BlockFacing::South;
    case BlockFacing::South: return BlockFacing::West;
    case BlockFacing::West:  return BlockFacing::Up;
    case BlockFacing::Up:    return BlockFacing::Down;
    case BlockFacing::Down:  return BlockFacing::North;
    default: return BlockFacing::North;
    }
}

QColor GridScene::fallbackColor(BlockType type)
{
    switch(type){
    case BlockType::RedstoneWire:  return QColor(0xCC,0x30,0x30);
    case BlockType::RedstoneTorch: return QColor(0xFF,0x66,0x00);
    case BlockType::RedstoneBlock: return QColor(0xAA,0x00,0x00);
    case BlockType::Repeater:      return QColor(0xAA,0x88,0x44);
    case BlockType::Comparator:    return QColor(0x88,0x44,0xAA);
    case BlockType::Observer:      return QColor(0x66,0x66,0x88);
    case BlockType::Piston:        return QColor(0x88,0x88,0x88);
    case BlockType::StickyPiston:  return QColor(0x44,0x99,0x66);
    case BlockType::PistonHead:    return QColor(0x66,0x66,0x55);
    case BlockType::Dropper:       return QColor(0x55,0x55,0x88);
    case BlockType::Dispenser:     return QColor(0x66,0x55,0x88);
    case BlockType::Hopper:        return QColor(0x44,0x44,0x66);
    case BlockType::TNT:           return QColor(0xCC,0x44,0x44);
    case BlockType::RedstoneLamp:  return QColor(0x80,0x78,0x20);
    case BlockType::Stone:         return QColor(0x66,0x66,0x66);
    case BlockType::Glass:         return QColor(0x99,0xCC,0xDD);
    case BlockType::Other:         return QColor(0x33,0x33,0x33);
    default:                       return QColor(0x44,0x44,0x55);
    }
}
