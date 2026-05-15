#pragma once
#include <unordered_map>
#include <cstdint>
#include <QObject>
#include <QTimer>
#include <QVector>
#include "core/VoxelWorld.h"
#include "sim/TickScheduler.h"

class SimEngine : public QObject
{
    Q_OBJECT

public:
    explicit SimEngine(VoxelWorld *world, QObject *parent = nullptr);

    void start(int intervalMs = 250);
    void stop();
    void stepOnce();
    void refreshStatic();
    void reset();

    void notifyBlockChanged(int x, int y, int z);
    void toggleSource      (int x, int y, int z);
    void scheduleSourceOff (int x, int y, int z, int delayTicks = 2);

    uint64_t currentTick()      const { return m_tick; }
    bool     isRunning()        const { return m_timer->isActive(); }
    int      lastChangedCount() const { return m_changedCoords.size(); }

signals:
    void tickFinished(QVector<VoxelCoord> changedCoords);

private slots:
    void onTimer();

private:
    void applyDueEvents      (const std::vector<VoxelCoord> &due);
    void scheduleLogicUpdates();
    void propagateDust();
    void updateAllActuators();
    void flushWriteBuffer();


    void updateRepeaterLocks();

    int   computeBlockInput(const VoxelCoord &c) const;
    void  writeBlock       (VoxelCoord c, Block b);
    Block getEffectiveBlock(const VoxelCoord &c) const;

    static const VoxelCoord s_neighbors6[6];

    VoxelWorld   *m_world;
    TickScheduler m_scheduler;
    QTimer       *m_timer;
    uint64_t      m_tick = 0;

    std::unordered_map<VoxelCoord, Block, VoxelCoordHash>   m_writeBuffer;
    QVector<VoxelCoord>                                     m_changedCoords;
    std::unordered_map<VoxelCoord, uint8_t, VoxelCoordHash> m_pendingRepeaterOutput;
};
