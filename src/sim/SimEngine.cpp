#include "sim/SimEngine.h"
#include "sim/RedstoneLogic.h"
#include "sim/SimFlags.h"
#include <QTimer>
#include <queue>
#include <algorithm>
#include <cstring>
#include <climits>

// ── 静态常量 ──────────────────────────────────────────────
const VoxelCoord SimEngine::s_neighbors6[6] = {
    { 0, 0,-1}, { 1, 0, 0}, { 0, 0, 1},
    {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0},
};

const VoxelCoord SimEngine::s_horiz4[4] = {
    { 0, 0,-1}, { 1, 0, 0}, { 0, 0, 1}, {-1, 0, 0},
};

// ══════════════════════════════════════════════════════════
//  构造 / 生命周期
// ══════════════════════════════════════════════════════════

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
    m_observerSnapshot.clear();
    m_pistonPrevLit.clear();
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
    takeObserverSnapshots();
    emit tickFinished(m_changedCoords);
}

void SimEngine::notifyBlockChanged(int, int, int) {}

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

// ══════════════════════════════════════════════════════════
//  主循环
// ══════════════════════════════════════════════════════════

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

// ══════════════════════════════════════════════════════════
//  到期事件处理
// ══════════════════════════════════════════════════════════

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

        if (b->type == BlockType::Observer)
        {
            Block next = *b;
            next.flags &= ~SimFlags::ACTIVE;
            next.power  = 0;
            writeBlock(c, next);
            continue;
        }
    }
}

// ══════════════════════════════════════════════════════════
//  观察者
// ══════════════════════════════════════════════════════════

void SimEngine::updateObservers()
{
    for (const auto &[coord, block] : m_world->allBlocks())
    {
        if (block.type != BlockType::Observer) continue;

        VoxelCoord backOff = RedstoneLogic::facingOffset(
                                 RedstoneLogic::opposite(block.facing));
        VoxelCoord obsPos{ coord.x + backOff.x,
                           coord.y + backOff.y,
                           coord.z + backOff.z };

        Block currentObserved = m_world->getBlock(obsPos.x, obsPos.y, obsPos.z);

        auto snapshotIt = m_observerSnapshot.find(coord);
        if (snapshotIt == m_observerSnapshot.end()) {
            m_observerSnapshot[coord] = currentObserved;
            continue;
        }

        bool changed = (memcmp(&currentObserved, &snapshotIt->second,
                                sizeof(Block)) != 0);
        if (!changed) continue;

        Block current = getEffectiveBlock(coord);
        if (current.flags & SimFlags::ACTIVE) continue;

        Block next = current;
        next.flags |=  SimFlags::ACTIVE;
        next.power  = 15;
        writeBlock(coord, next);
        m_scheduler.schedule(coord, m_tick + 1);
    }
}

void SimEngine::takeObserverSnapshots()
{
    for (const auto &[coord, block] : m_world->allBlocks())
    {
        if (block.type != BlockType::Observer) continue;

        VoxelCoord backOff = RedstoneLogic::facingOffset(
                                 RedstoneLogic::opposite(block.facing));
        VoxelCoord obsPos{ coord.x + backOff.x,
                           coord.y + backOff.y,
                           coord.z + backOff.z };

        m_observerSnapshot[coord] =
            m_world->getBlock(obsPos.x, obsPos.y, obsPos.z);
    }

    auto it = m_observerSnapshot.begin();
    while (it != m_observerSnapshot.end()) {
        const Block &b = m_world->getBlock(
            it->first.x, it->first.y, it->first.z);
        if (b.type != BlockType::Observer)
            it = m_observerSnapshot.erase(it);
        else
            ++it;
    }
}

// ══════════════════════════════════════════════════════════
//  中继器锁定
// ══════════════════════════════════════════════════════════

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
            if (nbA.type == BlockType::Repeater && (nbA.flags & SimFlags::ACTIVE)) {
                VoxelCoord out = RedstoneLogic::facingOffset(nbA.facing);
                if (out.x == -sideA.x && out.z == -sideA.z)
                    locked = true;
            }
        }

        if (!locked) {
            VoxelCoord posB{ coord.x + sideB.x, coord.y, coord.z + sideB.z };
            Block nbB = getEffectiveBlock(posB);
            if (nbB.type == BlockType::Repeater && (nbB.flags & SimFlags::ACTIVE)) {
                VoxelCoord out = RedstoneLogic::facingOffset(nbB.facing);
                if (out.x == -sideB.x && out.z == -sideB.z)
                    locked = true;
            }
        }

        bool wasLocked = (current.flags & SimFlags::LOCKED) != 0;
        if (locked == wasLocked) continue;

        Block next = current;
        if (locked) next.flags |=  SimFlags::LOCKED;
        else        next.flags &= ~SimFlags::LOCKED;
        writeBlock(coord, next);
    }
}

// ══════════════════════════════════════════════════════════
//  逻辑更新调度
// ══════════════════════════════════════════════════════════

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

// ══════════════════════════════════════════════════════════
//  P3：红石粉传播
// ══════════════════════════════════════════════════════════

void SimEngine::tryPropagateVerticalDust(
    const VoxelCoord &cur,
    uint8_t curPow,
    const VoxelCoord &horiz,
    std::queue<std::pair<VoxelCoord,uint8_t>> &q)
{
    if (curPow == 0) return;
    const uint8_t newPow = curPow - 1;

    // ── 上坡：cur 上方透明 → 传到 (cur+horiz, y+1) ──────
    {
        VoxelCoord above { cur.x,           cur.y + 1, cur.z           };
        VoxelCoord target{ cur.x + horiz.x, cur.y + 1, cur.z + horiz.z };
        Block aboveB = getEffectiveBlock(above);
        if (RedstoneLogic::isTransparent(aboveB.type)) {
            Block tb = getEffectiveBlock(target);
            if (tb.type == BlockType::RedstoneWire && newPow > tb.power) {
                tb.power = newPow;
                m_writeBuffer[target] = tb;
                q.push({target, newPow});
            }
        }
    }

    // ── 下坡：水平相邻格透明 → 传到 (cur+horiz, y-1) ────
    {
        VoxelCoord side  { cur.x + horiz.x, cur.y,     cur.z + horiz.z };
        VoxelCoord target{ cur.x + horiz.x, cur.y - 1, cur.z + horiz.z };
        Block sideB = getEffectiveBlock(side);
        if (RedstoneLogic::isTransparent(sideB.type)) {
            Block tb = getEffectiveBlock(target);
            if (tb.type == BlockType::RedstoneWire && newPow > tb.power) {
                tb.power = newPow;
                m_writeBuffer[target] = tb;
                q.push({target, newPow});
            }
        }
    }
}

void SimEngine::propagateDust()
{
    // ── 先将所有粉清零 ────────────────────────────────────
    for (auto &[coord, block] : m_world->allBlocks()) {
        if (block.type == BlockType::RedstoneWire) {
            Block b = block;
            b.power = 0;
            m_writeBuffer[coord] = b;
        }
    }

    using Entry = std::pair<VoxelCoord, uint8_t>;
    std::queue<Entry> q;

    // ── 以所有非粉方块为种子播种 ─────────────────────────
    auto seedFromBlock = [&](const VoxelCoord &sc, const Block &sb)
    {
        uint8_t out = RedstoneLogic::getDustOutput(sb, sc, *m_world);
        if (out == 0) return;

        for (const auto &d : s_neighbors6) {
            VoxelCoord nc{ sc.x+d.x, sc.y+d.y, sc.z+d.z };
            Block nb = getEffectiveBlock(nc);
            if (nb.type != BlockType::RedstoneWire) continue;

            // 中继器/比较器只向 facing 方向输出
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

    // ── BFS 展开：同层 + 竖向爬坡 ────────────────────────
    while (!q.empty())
    {
        auto [cur, pow] = q.front(); q.pop();
        Block curB = getEffectiveBlock(cur);
        if (curB.power != pow) continue;
        if (pow == 0)          continue;

        // 同层 6 邻居（原有逻辑）
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

        // P3：竖向爬坡（4水平方向各检测上坡/下坡）
        for (const auto &h : s_horiz4)
            tryPropagateVerticalDust(cur, pow, h, q);
    }
}

// ══════════════════════════════════════════════════════════
//  P4：充能铁轨激活与链式传播
// ══════════════════════════════════════════════════════════

void SimEngine::propagatePoweredRails()
{
    constexpr int CHAIN_MAX = 8;

    // ── Step1：收集所有充能铁轨，标记直接有源者 ──────────
    CoordMap<int> dist;
    std::queue<VoxelCoord> bfsQ;

    for (const auto &[coord, block] : m_world->allBlocks())
    {
        if (block.type != BlockType::PoweredRail) continue;
        dist[coord] = INT_MAX;   // 默认未到达

        // 检查 6 邻居是否存在直接激活源
        bool directPower = false;
        for (const auto &d : s_neighbors6) {
            VoxelCoord nc{ coord.x+d.x, coord.y+d.y, coord.z+d.z };
            Block nb = getEffectiveBlock(nc);
            // 排除另一个充能铁轨（避免互相喂电），由链式 BFS 处理
            if (nb.type != BlockType::PoweredRail
             && RedstoneLogic::canActivateRail(nb)) {
                directPower = true;
                break;
            }
        }

        if (directPower) {
            dist[coord] = 0;
            bfsQ.push(coord);
        }
    }

    // ── Step2：BFS 链式传播，最多 CHAIN_MAX 格 ───────────
    while (!bfsQ.empty())
    {
        VoxelCoord cur = bfsQ.front(); bfsQ.pop();
        int curDist = dist[cur];
        if (curDist >= CHAIN_MAX) continue;

        for (const auto &d : s_horiz4)   // 链式只走水平方向
        {
            VoxelCoord nc{ cur.x+d.x, cur.y, cur.z+d.z };

            auto it = dist.find(nc);
            if (it == dist.end()) continue;           // 不是充能铁轨
            if (it->second <= curDist + 1) continue;  // 已有更短路径
            if (!RedstoneLogic::areRailsConnected(cur, nc)) continue;

            it->second = curDist + 1;
            bfsQ.push(nc);
        }
    }

    // ── Step3：写入激活状态 ───────────────────────────────
    for (const auto &[coord, d] : dist)
    {
        Block current = getEffectiveBlock(coord);
        bool shouldLit = (d <= CHAIN_MAX);
        bool wasLit    = (current.flags & SimFlags::LIT) != 0;
        if (shouldLit == wasLit) continue;

        Block next = current;
        if (shouldLit) next.flags |=  SimFlags::LIT;
        else           next.flags &= ~SimFlags::LIT;
        writeBlock(coord, next);
    }
}

// ══════════════════════════════════════════════════════════
//  执行器更新
// ══════════════════════════════════════════════════════════

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
            int   inp  = computeBlockInput(coord);
            Block newB = current;
            RedstoneLogic::applyActuator(newB, inp);
            if (memcmp(&newB, &current, sizeof(Block)) != 0)
                writeBlock(coord, newB);
            break;
        }

        case BlockType::Piston:
        case BlockType::StickyPiston:
        {
            if (!includePistons) break;
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

// ══════════════════════════════════════════════════════════
//  活塞移动
// ══════════════════════════════════════════════════════════

bool SimEngine::isPistonMovable(const Block &block) const
{
    if (block.isEmpty()) return false;
    if (block.type == BlockType::PistonHead) return false;
    if (block.type == BlockType::Piston
     || block.type == BlockType::StickyPiston)
    {
        if (block.flags & SimFlags::LIT) return false;
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
        pistonPos.z + dir.z
    };

    for (int i = 0; i < MAX_PUSH; ++i) {
        Block b = getEffectiveBlock(cur);
        if (b.isEmpty()) break;
        if (!isPistonMovable(b)) return false;
        chain.push_back(cur);
        cur = { cur.x + dir.x, cur.y + dir.y, cur.z + dir.z };
    }

    if (!getEffectiveBlock(cur).isEmpty()) return false;

    for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i) {
        const VoxelCoord &from = chain[i];
        VoxelCoord to{ from.x + dir.x, from.y + dir.y, from.z + dir.z };
        writeBlock(to,   getEffectiveBlock(from));
        writeBlock(from, Block::air());
    }
    return true;
}

void SimEngine::tryPullBlock(const VoxelCoord &pistonPos,
                              const VoxelCoord &dir)
{
    VoxelCoord armPos  { pistonPos.x + dir.x,
                         pistonPos.y + dir.y,
                         pistonPos.z + dir.z };
    VoxelCoord pullFrom{ pistonPos.x + dir.x * 2,
                         pistonPos.y + dir.y * 2,
                         pistonPos.z + dir.z * 2 };

    Block pullBlock = getEffectiveBlock(pullFrom);
    if (!isPistonMovable(pullBlock)) return;

    writeBlock(armPos,   pullBlock);
    writeBlock(pullFrom, Block::air());
}

void SimEngine::executePistonMoves()
{
    for (const auto &[coord, block] : m_world->allBlocks())
    {
        const Block current = getEffectiveBlock(coord);
        if (current.type != BlockType::Piston
         && current.type != BlockType::StickyPiston) continue;

        const bool nowLit   = (current.flags & SimFlags::LIT) != 0;
        const bool isSticky = (current.type == BlockType::StickyPiston);

        auto it = m_pistonPrevLit.find(coord);
        const bool prevLit = (it != m_pistonPrevLit.end()) ? it->second : false;

        if (nowLit == prevLit) {
            m_pistonPrevLit[coord] = nowLit;
            continue;
        }

        const VoxelCoord dir    = RedstoneLogic::facingOffset(current.facing);
        const VoxelCoord armPos { coord.x + dir.x,
                                  coord.y + dir.y,
                                  coord.z + dir.z };

        if (nowLit && !prevLit) {
            bool ok = tryPushChain(coord, dir);
            if (ok || getEffectiveBlock(armPos).isEmpty()) {
                Block head;
                head.type   = BlockType::PistonHead;
                head.facing = current.facing;
                if (isSticky) head.flags |= SimFlags::ACTIVE;
                writeBlock(armPos, head);
            }
        } else if (!nowLit && prevLit) {
            writeBlock(armPos, Block::air());
            if (isSticky)
                tryPullBlock(coord, dir);
        }

        m_pistonPrevLit[coord] = nowLit;
    }

    // 清理已删除的活塞记录
    auto it = m_pistonPrevLit.begin();
    while (it != m_pistonPrevLit.end()) {
        const Block &b = m_world->getBlock(
            it->first.x, it->first.y, it->first.z);
        if (b.type != BlockType::Piston
         && b.type != BlockType::StickyPiston)
            it = m_pistonPrevLit.erase(it);
        else
            ++it;
    }
}

// ══════════════════════════════════════════════════════════
//  读写工具
// ══════════════════════════════════════════════════════════

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

void SimEngine::writeBlock(VoxelCoord c, Block b)
{
    m_writeBuffer[c] = b;
}

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