#include "sim/SimEngine.h"
#include "sim/RedstoneLogic.h"
#include "sim/SimFlags.h"
#include <QTimer>
#include <queue>
#include <algorithm>
#include <cstring>
#include <climits>

const VoxelCoord SimEngine::s_neighbors6[6] = {
    {0, 0, -1},
    {1, 0, 0},
    {0, 0, 1},
    {-1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
};

const VoxelCoord SimEngine::s_horiz4[4] = {
    {0, 0, -1},
    {1, 0, 0},
    {0, 0, 1},
    {-1, 0, 0},
};

SimEngine::SimEngine(VoxelWorld *world, QObject *parent)
    : QObject(parent), m_world(world), m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &SimEngine::onTimer);
}

void SimEngine::start(int intervalMs) { m_timer->start(intervalMs); }
void SimEngine::stop() { m_timer->stop(); }
void SimEngine::onTimer() { stepOnce(); }

void SimEngine::reset()
{
    m_scheduler.clear();
    m_writeBuffer.clear();
    m_changedCoords.clear();
    m_pendingRepeaterOutput.clear();
    m_pendingTorchOutput.clear();
    m_observerSnapshot.clear();
    m_pistonPrevLit.clear();
    m_tick = 0;

    for (auto &[coord, block] : m_world->allBlocks())
    {
        Block b = block;
        b.power = 0;

        b.flags &= ~(SimFlags::ACTIVE |
                     SimFlags::LIT |
                     SimFlags::STRONG_POWERED |
                     SimFlags::WEAK_POWERED |
                     SimFlags::SCHEDULED |
                     SimFlags::LOCKED);
        writeBlock(coord, b);
    }
    flushWriteBuffer();
    takeObserverSnapshots();
    emit tickFinished(m_changedCoords);
}

void SimEngine::notifyBlockChanged(int, int, int) {}

void SimEngine::toggleSource(int x, int y, int z)
{
    Block *b = m_world->getBlockMutable(x, y, z);
    if (!b)
        return;
    if (b->flags & SimFlags::ACTIVE)
    {
        b->flags &= ~SimFlags::ACTIVE;
        b->power = 0;
    }
    else
    {
        b->flags |= SimFlags::ACTIVE;
        b->power = 15;
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

    updateObservers();
    propagateDust();
    propagatePoweredRails();
    scheduleLogicUpdates();

    constexpr int MAX_LOOP = 64;
    for (int i = 0; i < MAX_LOOP && m_scheduler.hasDue(m_tick); ++i)
    {
        auto due = m_scheduler.pollDue(m_tick);
        applyDueEvents(due);
        propagateDust();
        propagatePoweredRails();
        scheduleLogicUpdates();
    }

    updateAllActuators(true);
    executePistonMoves();
    flushWriteBuffer();
    takeObserverSnapshots();
    emit tickFinished(m_changedCoords);
}

void SimEngine::refreshStatic()
{
    m_changedCoords.clear();
    propagateDust();
    propagatePoweredRails();
    updateRepeaterLocks();
    updateAllActuators(false);
    flushWriteBuffer();
    emit tickFinished(m_changedCoords);
}

void SimEngine::applyDueEvents(const std::vector<VoxelCoord> &due)
{
    for (const VoxelCoord &c : due)
    {
        Block *b = m_world->getBlockMutable(c.x, c.y, c.z);
        if (!b)
            continue;

        if (b->type == BlockType::StoneButton || b->type == BlockType::WoodButton)
        {
            b->flags &= ~SimFlags::ACTIVE;
            b->power = 0;
            continue;
        }

        if (b->type == BlockType::Repeater)
        {
            auto it = m_pendingRepeaterOutput.find(c);
            if (it != m_pendingRepeaterOutput.end())
            {
                Block next = *b;
                if (it->second > 0)
                {
                    next.flags |= SimFlags::ACTIVE;
                    next.power = 15;
                }
                else
                {
                    next.flags &= ~SimFlags::ACTIVE;
                    next.power = 0;
                }
                writeBlock(c, next);
                m_pendingRepeaterOutput.erase(it);
            }
            continue;
        }

        if (b->type == BlockType::RedstoneTorch)
        {
            auto it = m_pendingTorchOutput.find(c);
            if (it != m_pendingTorchOutput.end())
            {
                Block next = *b;
                if (it->second > 0)
                {
                    next.flags |= SimFlags::ACTIVE;
                    next.power = 15;
                }
                else
                {
                    next.flags &= ~SimFlags::ACTIVE;
                    next.power = 0;
                }
                writeBlock(c, next);
                m_pendingTorchOutput.erase(it);
            }
            continue;
        }

        if (b->type == BlockType::Observer)
        {
            Block next = *b;
            next.flags &= ~SimFlags::ACTIVE;
            next.power = 0;
            writeBlock(c, next);
            continue;
        }
    }
}

int SimEngine::getReceivedSignal(const VoxelCoord &pos) const
{
    int maxSig = 0;

    Block self = getEffectiveBlock(pos);

    if (self.flags & SimFlags::STRONG_POWERED)
        return 15;

    if (self.flags & SimFlags::WEAK_POWERED)
        maxSig = std::max(maxSig, 15);

    for (const auto &d : s_neighbors6)
    {
        VoxelCoord nc{pos.x + d.x, pos.y + d.y, pos.z + d.z};
        Block nb = getEffectiveBlock(nc);

        switch (nb.type)
        {

        case BlockType::RedstoneWire:
            if (nb.power > 0)
                maxSig = std::max(maxSig, (int)nb.power);
            break;

        case BlockType::RedstoneTorch:
            if ((nb.flags & SimFlags::ACTIVE) && d.x == 0 && d.y == -1 && d.z == 0)
            {
                return 15;
            }
            break;

        case BlockType::Repeater:
            if ((nb.flags & SimFlags::ACTIVE) && nb.power > 0)
            {
                VoxelCoord fo = RedstoneLogic::facingOffset(nb.facing);
                if (fo.x == d.x && fo.y == d.y && fo.z == d.z)
                    return 15;
            }
            break;

        case BlockType::Comparator:
            if (nb.power > 0)
            {
                VoxelCoord fo = RedstoneLogic::facingOffset(nb.facing);
                if (fo.x == d.x && fo.y == d.y && fo.z == d.z)
                    maxSig = std::max(maxSig, (int)nb.power);
            }
            break;

        case BlockType::Lever:
        case BlockType::StoneButton:
        case BlockType::WoodButton:
        case BlockType::StonePressurePlate:
        case BlockType::WoodPressurePlate:
        case BlockType::LightWeightedPressurePlate:
        case BlockType::HeavyWeightedPressurePlate:
            if (nb.flags & SimFlags::ACTIVE)
            {

                VoxelCoord attachOff = RedstoneLogic::facingOffset(
                    RedstoneLogic::opposite(nb.facing));
                if (attachOff.x == d.x && attachOff.y == d.y && attachOff.z == d.z)
                    return 15;
            }
            break;

        case BlockType::RedstoneBlock:
            return 15;

        default:
            break;
        }
    }

    return maxSig;
}

void SimEngine::updateObservers()
{
    for (const auto &[coord, block] : m_world->allBlocks())
    {
        if (block.type != BlockType::Observer)
            continue;

        VoxelCoord backOff = RedstoneLogic::facingOffset(
            RedstoneLogic::opposite(block.facing));
        VoxelCoord obsPos{coord.x + backOff.x,
                          coord.y + backOff.y,
                          coord.z + backOff.z};

        Block currentObserved = m_world->getBlock(obsPos.x, obsPos.y, obsPos.z);

        auto snapshotIt = m_observerSnapshot.find(coord);
        if (snapshotIt == m_observerSnapshot.end())
        {
            m_observerSnapshot[coord] = currentObserved;
            continue;
        }

        bool changed = (memcmp(&currentObserved, &snapshotIt->second,
                               sizeof(Block)) != 0);
        if (!changed)
            continue;

        Block current = getEffectiveBlock(coord);
        if (current.flags & SimFlags::ACTIVE)
            continue;

        Block next = current;
        next.flags |= SimFlags::ACTIVE;
        next.power = 15;
        writeBlock(coord, next);
        m_scheduler.schedule(coord, m_tick + 1);
    }
}

void SimEngine::takeObserverSnapshots()
{
    for (const auto &[coord, block] : m_world->allBlocks())
    {
        if (block.type != BlockType::Observer)
            continue;

        VoxelCoord backOff = RedstoneLogic::facingOffset(
            RedstoneLogic::opposite(block.facing));
        VoxelCoord obsPos{coord.x + backOff.x,
                          coord.y + backOff.y,
                          coord.z + backOff.z};

        m_observerSnapshot[coord] =
            m_world->getBlock(obsPos.x, obsPos.y, obsPos.z);
    }

    auto it = m_observerSnapshot.begin();
    while (it != m_observerSnapshot.end())
    {
        const Block &b = m_world->getBlock(
            it->first.x, it->first.y, it->first.z);
        if (b.type != BlockType::Observer)
            it = m_observerSnapshot.erase(it);
        else
            ++it;
    }
}

void SimEngine::updateRepeaterLocks()
{
    for (const auto &[coord, block] : m_world->allBlocks())
    {
        const Block current = getEffectiveBlock(coord);
        if (current.type != BlockType::Repeater)
            continue;

        auto [sideA, sideB] = RedstoneLogic::sideOffsets(current.facing);

        bool locked = false;
        {
            VoxelCoord posA{coord.x + sideA.x, coord.y, coord.z + sideA.z};
            Block nbA = getEffectiveBlock(posA);
            if (nbA.type == BlockType::Repeater && (nbA.flags & SimFlags::ACTIVE))
            {
                VoxelCoord out = RedstoneLogic::facingOffset(nbA.facing);
                if (out.x == -sideA.x && out.z == -sideA.z)
                    locked = true;
            }
        }

        if (!locked)
        {
            VoxelCoord posB{coord.x + sideB.x, coord.y, coord.z + sideB.z};
            Block nbB = getEffectiveBlock(posB);
            if (nbB.type == BlockType::Repeater && (nbB.flags & SimFlags::ACTIVE))
            {
                VoxelCoord out = RedstoneLogic::facingOffset(nbB.facing);
                if (out.x == -sideB.x && out.z == -sideB.z)
                    locked = true;
            }
        }

        bool wasLocked = (current.flags & SimFlags::LOCKED) != 0;
        if (locked == wasLocked)
            continue;

        Block next = current;
        if (locked)
            next.flags |= SimFlags::LOCKED;
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
            if (current.flags & SimFlags::LOCKED)
                break;

            VoxelCoord inputPos = RedstoneLogic::repeaterInputPos(
                coord, current.facing);
            Block inputBlock = getEffectiveBlock(inputPos);
            bool shouldActive = (inputBlock.power > 0);
            bool isActive = (current.flags & SimFlags::ACTIVE) != 0;

            uint8_t targetPower = shouldActive ? 15u : 0u;
            auto it = m_pendingRepeaterOutput.find(coord);

            if (it != m_pendingRepeaterOutput.end() && it->second == targetPower)
                break;

            if (shouldActive == isActive && it == m_pendingRepeaterOutput.end())
                break;

            m_pendingRepeaterOutput[coord] = targetPower;

            int delay = SimFlags::getRepeaterDelay(current.flags) + 1;
            uint64_t fireTick = m_tick + static_cast<uint64_t>(delay - 1);
            m_scheduler.schedule(coord, fireTick);
            break;
        }

        case BlockType::RedstoneTorch:
        {

            VoxelCoord attachedOff = RedstoneLogic::facingOffset(
                RedstoneLogic::opposite(current.facing));
            VoxelCoord attachedPos{
                coord.x + attachedOff.x,
                coord.y + attachedOff.y,
                coord.z + attachedOff.z};

            bool hostPowered = (getReceivedSignal(attachedPos) > 0);

            bool shouldActive = !hostPowered;
            bool isActive = (current.flags & SimFlags::ACTIVE) != 0;
            if (shouldActive == isActive)
                break;

            uint8_t targetActive = shouldActive ? 1u : 0u;
            auto it = m_pendingTorchOutput.find(coord);
            if (it != m_pendingTorchOutput.end() && it->second == targetActive)
                break;

            m_pendingTorchOutput[coord] = targetActive;
            m_scheduler.schedule(coord, m_tick + 1);
            break;
        }

        case BlockType::Comparator:
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

void SimEngine::tryPropagateVerticalDust(
    const VoxelCoord &cur,
    uint8_t curPow,
    const VoxelCoord &horiz,
    std::queue<std::pair<VoxelCoord, uint8_t>> &q)
{
    if (curPow == 0)
        return;
    const uint8_t newPow = curPow - 1;

    {
        VoxelCoord above{cur.x, cur.y + 1, cur.z};
        VoxelCoord target{cur.x + horiz.x, cur.y + 1, cur.z + horiz.z};
        Block aboveB = getEffectiveBlock(above);
        if (RedstoneLogic::isTransparent(aboveB.type))
        {
            Block tb = getEffectiveBlock(target);
            if (tb.type == BlockType::RedstoneWire && newPow > tb.power)
            {
                tb.power = newPow;
                m_writeBuffer[target] = tb;
                q.push({target, newPow});
            }
        }
    }

    {
        VoxelCoord side{cur.x + horiz.x, cur.y, cur.z + horiz.z};
        VoxelCoord target{cur.x + horiz.x, cur.y - 1, cur.z + horiz.z};
        Block sideB = getEffectiveBlock(side);
        if (RedstoneLogic::isTransparent(sideB.type))
        {
            Block tb = getEffectiveBlock(target);
            if (tb.type == BlockType::RedstoneWire && newPow > tb.power)
            {
                tb.power = newPow;
                m_writeBuffer[target] = tb;
                q.push({target, newPow});
            }
        }
    }
}

void SimEngine::propagateDust()
{

    for (auto &[coord, block] : m_world->allBlocks())
    {
        if (block.type == BlockType::RedstoneWire)
        {
            Block b = block;
            b.power = 0;
            m_writeBuffer[coord] = b;
        }
        else if (!RedstoneLogic::isTransparent(block.type) && block.type != BlockType::Air)
        {

            const uint16_t poweredMask =
                SimFlags::STRONG_POWERED | SimFlags::WEAK_POWERED;
            if (block.flags & poweredMask)
            {
                Block b = block;
                b.flags &= ~poweredMask;
                m_writeBuffer[coord] = b;
            }
        }
    }

    using Entry = std::pair<VoxelCoord, uint8_t>;
    std::queue<Entry> q;

    auto seedFromBlock = [&](const VoxelCoord &sc, const Block &sb)
    {
        uint8_t out = RedstoneLogic::getDustOutput(sb, sc, *m_world);
        if (out == 0)
            return;

        for (const auto &d : s_neighbors6)
        {
            VoxelCoord nc{sc.x + d.x, sc.y + d.y, sc.z + d.z};
            Block nb = getEffectiveBlock(nc);
            if (nb.type != BlockType::RedstoneWire)
                continue;

            if (sb.type == BlockType::Repeater || sb.type == BlockType::Comparator)
            {
                VoxelCoord fo = RedstoneLogic::facingOffset(sb.facing);
                if (d.x != fo.x || d.y != fo.y || d.z != fo.z)
                    continue;
            }

            if (sb.type == BlockType::RedstoneTorch)
            {
                VoxelCoord attachedOff = RedstoneLogic::facingOffset(
                    RedstoneLogic::opposite(sb.facing));
                if (d.x == attachedOff.x && d.y == attachedOff.y && d.z == attachedOff.z)
                    continue;
            }

            uint8_t newPow = out > 0 ? out - 1 : 0u;
            if (newPow > nb.power)
            {
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
        auto [cur, pow] = q.front();
        q.pop();
        Block curB = getEffectiveBlock(cur);
        if (curB.power != pow)
            continue;
        if (pow == 0)
            continue;

        {
            VoxelCoord below{cur.x, cur.y - 1, cur.z};
            Block belowB = getEffectiveBlock(below);
            if (!RedstoneLogic::isTransparent(belowB.type) && belowB.type != BlockType::Air)
            {
                if (!(belowB.flags & SimFlags::WEAK_POWERED))
                {
                    belowB.flags |= SimFlags::WEAK_POWERED;
                    m_writeBuffer[below] = belowB;
                }
            }
        }

        for (const auto &d : s_neighbors6)
        {
            VoxelCoord nc{cur.x + d.x, cur.y + d.y, cur.z + d.z};
            Block nb = getEffectiveBlock(nc);
            if (nb.type != BlockType::RedstoneWire)
                continue;
            uint8_t newPow = pow - 1;
            if (newPow > nb.power)
            {
                nb.power = newPow;
                m_writeBuffer[nc] = nb;
                q.push({nc, newPow});
            }
        }

        for (const auto &h : s_horiz4)
            tryPropagateVerticalDust(cur, pow, h, q);
    }
}

void SimEngine::propagatePoweredRails()
{
    constexpr int CHAIN_MAX = 8;

    CoordMap<int> dist;
    std::queue<VoxelCoord> bfsQ;

    for (const auto &[coord, block] : m_world->allBlocks())
    {
        if (block.type != BlockType::PoweredRail)
            continue;
        dist[coord] = INT_MAX;

        bool directPower = false;
        for (const auto &d : s_neighbors6)
        {
            VoxelCoord nc{coord.x + d.x, coord.y + d.y, coord.z + d.z};
            Block nb = getEffectiveBlock(nc);
            if (nb.type != BlockType::PoweredRail && RedstoneLogic::canActivateRail(nb))
            {
                directPower = true;
                break;
            }
        }

        if (directPower)
        {
            dist[coord] = 0;
            bfsQ.push(coord);
        }
    }

    while (!bfsQ.empty())
    {
        VoxelCoord cur = bfsQ.front();
        bfsQ.pop();
        int curDist = dist[cur];
        if (curDist >= CHAIN_MAX)
            continue;

        for (const auto &d : s_horiz4)
        {
            VoxelCoord nc{cur.x + d.x, cur.y, cur.z + d.z};

            auto it = dist.find(nc);
            if (it == dist.end())
                continue;
            if (it->second <= curDist + 1)
                continue;
            if (!RedstoneLogic::areRailsConnected(cur, nc))
                continue;

            it->second = curDist + 1;
            bfsQ.push(nc);
        }
    }

    for (const auto &[coord, d] : dist)
    {
        Block current = getEffectiveBlock(coord);
        bool shouldLit = (d <= CHAIN_MAX);
        bool wasLit = (current.flags & SimFlags::LIT) != 0;
        if (shouldLit == wasLit)
            continue;

        Block next = current;
        if (shouldLit)
            next.flags |= SimFlags::LIT;
        else
            next.flags &= ~SimFlags::LIT;
        writeBlock(coord, next);
    }
}

void SimEngine::updateAllActuators(bool includePistons)
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
        {
            int inp = computeBlockInput(coord);
            Block newB = current;
            RedstoneLogic::applyActuator(newB, inp);
            if (memcmp(&newB, &current, sizeof(Block)) != 0)
                writeBlock(coord, newB);
            break;
        }

        case BlockType::Piston:
        case BlockType::StickyPiston:
        {
            if (!includePistons)
                break;
            int inp = computeBlockInput(coord);
            Block newB = current;
            RedstoneLogic::applyActuator(newB, inp);
            if (memcmp(&newB, &current, sizeof(Block)) != 0)
                writeBlock(coord, newB);
            break;
        }

        default:
            break;
        }
    }
}

bool SimEngine::isPistonMovable(const Block &block) const
{
    if (block.isEmpty())
        return false;
    if (block.type == BlockType::PistonHead)
        return false;
    if (block.type == BlockType::Piston || block.type == BlockType::StickyPiston)
    {
        if (block.flags & SimFlags::LIT)
            return false;
    }
    return true;
}

bool SimEngine::tryPushChain(const VoxelCoord &pistonPos,
                             const VoxelCoord &dir)
{
    static constexpr int MAX_PUSH = 12;

    std::vector<VoxelCoord> chain;
    VoxelCoord cur{
        pistonPos.x + dir.x,
        pistonPos.y + dir.y,
        pistonPos.z + dir.z};

    for (int i = 0; i < MAX_PUSH; ++i)
    {
        Block b = getEffectiveBlock(cur);
        if (b.isEmpty())
            break;
        if (!isPistonMovable(b))
            return false;
        chain.push_back(cur);
        cur = {cur.x + dir.x, cur.y + dir.y, cur.z + dir.z};
    }

    if (!getEffectiveBlock(cur).isEmpty())
        return false;

    for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i)
    {
        const VoxelCoord &from = chain[i];
        VoxelCoord to{from.x + dir.x, from.y + dir.y, from.z + dir.z};
        writeBlock(to, getEffectiveBlock(from));
        writeBlock(from, Block::air());
    }
    return true;
}

void SimEngine::tryPullBlock(const VoxelCoord &pistonPos,
                             const VoxelCoord &dir)
{
    VoxelCoord armPos{pistonPos.x + dir.x,
                      pistonPos.y + dir.y,
                      pistonPos.z + dir.z};
    VoxelCoord pullFrom{pistonPos.x + dir.x * 2,
                        pistonPos.y + dir.y * 2,
                        pistonPos.z + dir.z * 2};

    Block pullBlock = getEffectiveBlock(pullFrom);
    if (!isPistonMovable(pullBlock))
        return;

    writeBlock(armPos, pullBlock);
    writeBlock(pullFrom, Block::air());
}

void SimEngine::executePistonMoves()
{
    for (const auto &[coord, block] : m_world->allBlocks())
    {
        const Block current = getEffectiveBlock(coord);
        if (current.type != BlockType::Piston && current.type != BlockType::StickyPiston)
            continue;

        const bool nowLit = (current.flags & SimFlags::LIT) != 0;
        const bool isSticky = (current.type == BlockType::StickyPiston);

        auto it = m_pistonPrevLit.find(coord);
        const bool prevLit = (it != m_pistonPrevLit.end()) ? it->second : false;

        if (nowLit == prevLit)
        {
            m_pistonPrevLit[coord] = nowLit;
            continue;
        }

        const VoxelCoord dir = RedstoneLogic::facingOffset(current.facing);
        const VoxelCoord armPos{coord.x + dir.x,
                                coord.y + dir.y,
                                coord.z + dir.z};

        if (nowLit && !prevLit)
        {
            bool ok = tryPushChain(coord, dir);
            if (ok || getEffectiveBlock(armPos).isEmpty())
            {
                Block head;
                head.type = BlockType::PistonHead;
                head.facing = current.facing;
                if (isSticky)
                    head.flags |= SimFlags::ACTIVE;
                writeBlock(armPos, head);
            }
        }
        else if (!nowLit && prevLit)
        {
            writeBlock(armPos, Block::air());
            if (isSticky)
                tryPullBlock(coord, dir);
        }

        m_pistonPrevLit[coord] = nowLit;
    }

    auto it = m_pistonPrevLit.begin();
    while (it != m_pistonPrevLit.end())
    {
        const Block &b = m_world->getBlock(
            it->first.x, it->first.y, it->first.z);
        if (b.type != BlockType::Piston && b.type != BlockType::StickyPiston)
            it = m_pistonPrevLit.erase(it);
        else
            ++it;
    }
}

Block SimEngine::getEffectiveBlock(const VoxelCoord &c) const
{
    auto it = m_writeBuffer.find(c);
    if (it != m_writeBuffer.end())
        return it->second;
    return m_world->getBlock(c.x, c.y, c.z);
}

int SimEngine::computeBlockInput(const VoxelCoord &c) const
{
    int maxPow = 0;
    for (const auto &d : s_neighbors6)
    {
        Block nb = getEffectiveBlock({c.x + d.x, c.y + d.y, c.z + d.z});
        maxPow = std::max(maxPow, static_cast<int>(nb.power));
    }
    return maxPow;
}

void SimEngine::writeBlock(VoxelCoord c, Block b)
{
    m_writeBuffer[c] = b;
}

void SimEngine::flushWriteBuffer()
{
    for (auto &[coord, block] : m_writeBuffer)
    {
        Block old = m_world->getBlock(coord.x, coord.y, coord.z);
        if (memcmp(&block, &old, sizeof(Block)) != 0)
        {
            m_world->setBlock(coord.x, coord.y, coord.z, block);
            m_changedCoords.append(coord);
        }
    }
    m_writeBuffer.clear();
}