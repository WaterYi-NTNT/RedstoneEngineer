#pragma once
#include <cstdint>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include "core/Block.h"

struct ScheduledEvent
{
    VoxelCoord coord;
    uint64_t   fireTick;
};

struct EventKey
{
    VoxelCoord coord;
    uint64_t   fireTick;
    bool operator==(const EventKey &o) const
    {
        return coord == o.coord && fireTick == o.fireTick;
    }
};

struct EventKeyHash
{
    size_t operator()(const EventKey &k) const noexcept
    {
        size_t h = VoxelCoordHash{}(k.coord);
        h ^= std::hash<uint64_t>{}(k.fireTick) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

class TickScheduler
{
public:
    void schedule(VoxelCoord coord, uint64_t fireTick)
    {
        EventKey key{coord, fireTick};
        if (m_seen.contains(key)) return;
        m_seen.insert(key);
        m_queue.push_back({coord, fireTick});
        std::push_heap(m_queue.begin(), m_queue.end(),
            [](const ScheduledEvent &a, const ScheduledEvent &b)
            { return a.fireTick > b.fireTick; });
    }

    std::vector<VoxelCoord> pollDue(uint64_t currentTick)
    {
        std::vector<VoxelCoord> due;
        while (!m_queue.empty() && m_queue.front().fireTick <= currentTick) {
            ScheduledEvent e = m_queue.front();
            std::pop_heap(m_queue.begin(), m_queue.end(),
                [](const ScheduledEvent &a, const ScheduledEvent &b)
                { return a.fireTick > b.fireTick; });
            m_queue.pop_back();
            due.push_back(e.coord);
            m_seen.erase({e.coord, e.fireTick});
        }
        return due;
    }

    bool hasDue(uint64_t currentTick) const
    {
        return !m_queue.empty() && m_queue.front().fireTick <= currentTick;
    }

    void clear() { m_queue.clear(); m_seen.clear(); }
    bool empty() const { return m_queue.empty(); }

private:
    std::vector<ScheduledEvent> m_queue;
    std::unordered_set<EventKey, EventKeyHash> m_seen;
};
