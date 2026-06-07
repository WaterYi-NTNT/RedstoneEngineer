#pragma once
#include <cstdint>
#include <vector>
#include "core/Block.h"

struct ScheduledEvent
{
    VoxelCoord coord;
    uint64_t   fireTick;
};

class TickScheduler
{
public:
    void schedule(VoxelCoord coord, uint64_t fireTick)
    {

        for (const auto &e : m_queue)
            if (e.coord == coord && e.fireTick == fireTick) return;
        m_queue.push_back({coord, fireTick});
    }

    std::vector<VoxelCoord> pollDue(uint64_t currentTick)
    {
        std::vector<VoxelCoord> due;
        auto it = m_queue.begin();
        while (it != m_queue.end()) {
            if (it->fireTick <= currentTick) {
                due.push_back(it->coord);
                it = m_queue.erase(it);
            } else {
                ++it;
            }
        }
        return due;
    }

    bool hasDue(uint64_t currentTick) const
    {
        for (const auto &e : m_queue)
            if (e.fireTick <= currentTick) return true;
        return false;
    }

    void clear() { m_queue.clear(); }
    bool empty() const { return m_queue.empty(); }

private:
    std::vector<ScheduledEvent> m_queue;
};
