#include "sim/SimEngine.h"
#include "sim/RedstoneLogic.h"
#include "sim/SimFlags.h"
#include <QTimer>
#include <queue>
#include <algorithm>
#include <cstring>

const VoxelCoord SimEngine::s_neighbors6[6] = {
    {  0,  0, -1 },
    {  1,  0,  0 },
    {  0,  0,  1 },
    { -1,  0,  0 },
    {  0,  1,  0 },
    {  0, -1,  0 },
};

SimEngine::SimEngine(VoxelWorld *world, QObject *parent)
    : QObject(parent)
    , m_world(world)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &SimEngine::onTimer);
}

void SimEngine::start(int intervalMs) { m_timer->start(intervalMs); }
void SimEngine::stop()                { m_timer->stop(); }
void SimEngine::onTimer()             { stepOnce(); }


void SimEngine::reset()
{
    m_scheduler.clear();
    m_writeBuffer.clear();
    m_changedCoords.clear();
    m_pendingRepeaterOutput.clear();
    m_tick = 0;

    for (auto &[coord, block] : m_world->allBlocks()) {
        Block b = block;
        b.power  = 0;
        b.flags &= ~(SimFlags::ACTIVE | SimFlags::LIT |
                     SimFlags::STRONG_POWERED | SimFlags::SCHEDULED |
                     SimFlags::LOCKED);
        writeBlock(coord, b);
    }
    flushWriteBuffer();
    emit tickFinished(m_changedCoords);
}


void SimEngine::notifyBlockChanged(int , int , int ) {}

void SimEngine::toggleSource(int x, int y, int z)
{
    Block *b = m_world->getBlockMutable(x, y, z);
    if (!b) return;
    if (b->flags & SimFlags::ACTIVE) {
        b->flags &= ~SimFlags::ACTIVE;
        b->power  = 0;
    } else {
        b->flags |=  SimFlags::ACTIVE;
        b->power  = 15;
    }
}

void SimEngine::scheduleSourceOff(int x, int y, int z, int delayTicks)
{
    m_scheduler.schedule({x, y, z}, m_tick + delayTicks);
}


void SimEngine::stepOnce()
{
    ++m_tick;
    m_changedCoords.clear();

    propagateDust();
    scheduleLogicUpdates();

    constexpr int MAX_LOOP = 64;
    for (int i = 0; i < MAX_LOOP && m_scheduler.hasDue(m_tick); ++i)
    {
        auto due = m_scheduler.pollDue(m_tick);
        applyDueEvents(due);

        propagateDust();
        scheduleLogicUpdates();
    }

    updateAllActuators();
    flushWriteBuffer();
    emit tickFinished(m_changedCoords);
}


void SimEngine::refreshStatic()
{
    m_changedCoords.clear();
    propagateDust();
    updateRepeaterLocks();
    updateAllActuators();
    flushWriteBuffer();
    emit tickFinished(m_changedCoords);
}


void SimEngine::applyDueEvents(const std::vector<VoxelCoord> &due)
{
    for (const VoxelCoord &c : due)
    {
        Block *b = m_world->getBlockMutable(c.x, c.y, c.z);
        if (!b) continue;

        if (b->type == BlockType::StoneButton
         || b->type == BlockType::WoodButton)
        {
            b->flags &= ~SimFlags::ACTIVE;
            b->power  = 0;
            continue;
        }

        if (b->type == BlockType::Repeater)
        {
            auto it = m_pendingRepeaterOutput.find(c);
            if (it != m_pendingRepeaterOutput.end())
            {
                Block next = *b;
                if (it->second > 0) {
                    next.flags |=  SimFlags::ACTIVE;
                    next.power  = 15;
                } else {
                    next.flags &= ~SimFlags::ACTIVE;
                    next.power  = 0;
                }
                writeBlock(c, next);
                m_pendingRepeaterOutput.erase(it);
            }
            continue;
        }
    }
}


void SimEngine::updateRepeaterLocks()
{
    for (const auto &[coord, block] : m_world->allBlocks())
    {
        const Block current = getEffectiveBlock(coord);
        if (current.type != BlockType::Repeater) continue;


        auto [sideA, sideB] = RedstoneLogic::sideOffsets(current.facing);


        bool locked = false;
        {
            VoxelCoord posA{ coord.x + sideA.x, coord.y, coord.z + sideA.z };
            Block nbA = getEffectiveBlock(posA);
            if (nbA.type == BlockType::Repeater
             && (nbA.flags & SimFlags::ACTIVE))
            {
                VoxelCoord out = RedstoneLogic::facingOffset(nbA.facing);
                if (out.x == -sideA.x && out.z == -sideA.z)
                    locked = true;
            }
        }


        if (!locked)
        {
            VoxelCoord posB{ coord.x + sideB.x, coord.y, coord.z + sideB.z };
            Block nbB = getEffectiveBlock(posB);
            if (nbB.type == BlockType::Repeater
             && (nbB.flags & SimFlags::ACTIVE))
            {
                VoxelCoord out = RedstoneLogic::facingOffset(nbB.facing);
                if (out.x == -sideB.x && out.z == -sideB.z)
                    locked = true;
            }
        }


        bool wasLocked = (current.flags & SimFlags::LOCKED) != 0;
        if (locked == wasLocked) continue;

        Block next = current;
        if (locked)
            next.flags |=  SimFlags::LOCKED;
        else
            next.flags &= ~SimFlags::LOCKED;
        writeBlock(coord, next);
    }
}


void SimEngine::scheduleLogicUpdates()
{

    updateRepeaterLocks();

    for (auto &[coord, block] : m_world->allBlocks())
    {
        const Block current = getEffectiveBlock(coord);

        switch (current.type)
        {
        case BlockType::Repeater:
        {

            if (current.flags & SimFlags::LOCKED) break;

            VoxelCoord inputPos   = RedstoneLogic::repeaterInputPos(
                                        coord, current.facing);
            Block      inputBlock = getEffectiveBlock(inputPos);
            bool shouldActive     = (inputBlock.power > 0);
            bool isActive         = (current.flags & SimFlags::ACTIVE) != 0;

            uint8_t targetPower = shouldActive ? 15u : 0u;
            auto it = m_pendingRepeaterOutput.find(coord);

            if (it != m_pendingRepeaterOutput.end()
             && it->second == targetPower) break;

            if (shouldActive == isActive
             && it == m_pendingRepeaterOutput.end()) break;

            m_pendingRepeaterOutput[coord] = targetPower;

            int delay         = SimFlags::getRepeaterDelay(current.flags) + 1;
            uint64_t fireTick = m_tick + static_cast<uint64_t>(delay - 1);
            m_scheduler.schedule(coord, fireTick);
            break;
        }

        case BlockType::RedstoneTorch:
        case BlockType::Comparator:
        case BlockType::Observer:
        {
            Block newB = RedstoneLogic::evaluate(
                             current, coord, *m_world, m_tick);
            if (memcmp(&newB, &current, sizeof(Block)) != 0)
                writeBlock(coord, newB);
            break;
        }

        default:
            break;
        }
    }
}


void SimEngine::propagateDust()
{
    for (auto &[coord, block] : m_world->allBlocks()) {
        if (block.type == BlockType::RedstoneWire) {
            Block b = block;
            b.power = 0;
            m_writeBuffer[coord] = b;
        }
    }

    using Entry = std::pair<VoxelCoord, uint8_t>;
    std::queue<Entry> q;

    auto seedFromBlock = [&](const VoxelCoord &sc, const Block &sb)
    {
        uint8_t out = RedstoneLogic::getDustOutput(sb, sc, *m_world);
        if (out == 0) return;

        for (const auto &d : s_neighbors6) {
            VoxelCoord nc{ sc.x+d.x, sc.y+d.y, sc.z+d.z };
            Block nb = getEffectiveBlock(nc);
            if (nb.type != BlockType::RedstoneWire) continue;

            if (sb.type == BlockType::Repeater
             || sb.type == BlockType::Comparator)
            {
                VoxelCoord fo = RedstoneLogic::facingOffset(sb.facing);
                if (d.x != fo.x || d.y != fo.y || d.z != fo.z) continue;
            }

            uint8_t newPow = out > 0 ? out - 1 : 0u;
            if (newPow > nb.power) {
                nb.power = newPow;
                m_writeBuffer[nc] = nb;
                q.push({nc, newPow});
            }
        }
    };

    for (auto &[coord, block] : m_world->allBlocks())
        seedFromBlock(coord, getEffectiveBlock(coord));

    while (!q.empty())
    {
        auto [cur, pow] = q.front(); q.pop();
        Block curB = getEffectiveBlock(cur);
        if (curB.power != pow) continue;
        if (pow == 0)          continue;

        for (const auto &d : s_neighbors6) {
            VoxelCoord nc{ cur.x+d.x, cur.y+d.y, cur.z+d.z };
            Block nb = getEffectiveBlock(nc);
            if (nb.type != BlockType::RedstoneWire) continue;
            uint8_t newPow = pow - 1;
            if (newPow > nb.power) {
                nb.power = newPow;
                m_writeBuffer[nc] = nb;
                q.push({nc, newPow});
            }
        }
    }
}


void SimEngine::updateAllActuators()
{
    for (auto &[coord, block] : m_world->allBlocks())
    {
        const Block current = getEffectiveBlock(coord);
        switch (current.type)
        {
        case BlockType::RedstoneLamp:
        case BlockType::IronDoor:
        case BlockType::IronTrapdoor:
        case BlockType::FenceGate:
        case BlockType::Piston:
        case BlockType::StickyPiston:
        {
            int   inp  = computeBlockInput(coord);
            Block newB = current;
            RedstoneLogic::applyActuator(newB, inp);
            if (memcmp(&newB, &current, sizeof(Block)) != 0)
                writeBlock(coord, newB);
            break;
        }
        default: break;
        }
    }
}


Block SimEngine::getEffectiveBlock(const VoxelCoord &c) const
{
    auto it = m_writeBuffer.find(c);
    if (it != m_writeBuffer.end()) return it->second;
    return m_world->getBlock(c.x, c.y, c.z);
}

int SimEngine::computeBlockInput(const VoxelCoord &c) const
{
    int maxPow = 0;
    for (const auto &d : s_neighbors6) {
        Block nb = getEffectiveBlock({c.x+d.x, c.y+d.y, c.z+d.z});
        maxPow = std::max(maxPow, static_cast<int>(nb.power));
    }
    return maxPow;
}

void SimEngine::writeBlock(VoxelCoord c, Block b) { m_writeBuffer[c] = b; }

void SimEngine::flushWriteBuffer()
{
    for (auto &[coord, block] : m_writeBuffer) {
        Block old = m_world->getBlock(coord.x, coord.y, coord.z);
        if (memcmp(&block, &old, sizeof(Block)) != 0) {
            m_world->setBlock(coord.x, coord.y, coord.z, block);
            m_changedCoords.append(coord);
        }
    }
    m_writeBuffer.clear();
}
