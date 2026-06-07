#pragma once
#include "core/VoxelWorld.h"
#include "core/Block.h"
#include "sim/TickScheduler.h"
#include <QObject>
#include <QTimer>
#include <QVector>
#include <vector>
#include <queue>
#include <unordered_map>
#include <climits>

struct VoxelCoordEqual {
    bool operator()(const VoxelCoord &a, const VoxelCoord &b) const noexcept {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};

template<typename V>
using CoordMap = std::unordered_map<VoxelCoord, V, VoxelCoordHash, VoxelCoordEqual>;

class SimEngine : public QObject
{
    Q_OBJECT
public:
    explicit SimEngine(VoxelWorld *world, QObject *parent = nullptr);

    void start(int intervalMs);
    void stop();
    void reset();
    void stepOnce();
    void refreshStatic();
    void notifyBlockChanged(int x, int y, int z);
    void toggleSource(int x, int y, int z);
    void scheduleSourceOff(int x, int y, int z, int delayTicks);

    uint64_t currentTick() const { return m_tick; }
    bool     isRunning()   const { return m_timer->isActive(); }

signals:
    void tickFinished(const QVector<VoxelCoord> &changed);

private slots:
    void onTimer();

private:

    void propagateDust();
    void propagatePoweredRails();
    void scheduleLogicUpdates();
    void updateRepeaterLocks();
    void updateObservers();
    void takeObserverSnapshots();
    void applyDueEvents(const std::vector<VoxelCoord> &due);
    void updateAllActuators(bool includePistons);
    void executePistonMoves();

    void tryPropagateVerticalDust(
        const VoxelCoord &cur,
        uint8_t curPow,
        const VoxelCoord &horiz,
        std::queue<std::pair<VoxelCoord,uint8_t>> &q);

    bool isPistonMovable(const Block &block) const;
    bool tryPushChain   (const VoxelCoord &pistonPos, const VoxelCoord &dir);
    void tryPullBlock   (const VoxelCoord &pistonPos, const VoxelCoord &dir);

    Block getEffectiveBlock(const VoxelCoord &c) const;
    int getReceivedSignal(const VoxelCoord &pos) const;
    int   computeBlockInput(const VoxelCoord &c) const;
    void  writeBlock(VoxelCoord c, Block b);
    void  flushWriteBuffer();

    VoxelWorld *m_world  = nullptr;
    QTimer     *m_timer  = nullptr;
    uint64_t    m_tick   = 0;

    TickScheduler m_scheduler;

    CoordMap<Block>    m_writeBuffer;
    CoordMap<uint8_t>  m_pendingRepeaterOutput;
    CoordMap<uint8_t> m_pendingTorchOutput;
    CoordMap<Block>    m_observerSnapshot;
    CoordMap<bool>     m_pistonPrevLit;

    QVector<VoxelCoord> m_changedCoords;

    static const VoxelCoord s_neighbors6[6];
    static const VoxelCoord s_horiz4[4];
};
