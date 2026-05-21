#include "sim/RedstoneLogic.h"
#include "sim/SimFlags.h"
#include <algorithm>
#include <cstring>
#include <cmath>

VoxelCoord RedstoneLogic::facingOffset(BlockFacing f)
{
    switch (f) {
    case BlockFacing::North: return { 0, 0,-1};
    case BlockFacing::South: return { 0, 0, 1};
    case BlockFacing::East:  return { 1, 0, 0};
    case BlockFacing::West:  return {-1, 0, 0};
    case BlockFacing::Up:    return { 0, 1, 0};
    case BlockFacing::Down:  return { 0,-1, 0};
    }
    return {};
}

BlockFacing RedstoneLogic::opposite(BlockFacing f)
{
    switch (f) {
    case BlockFacing::North: return BlockFacing::South;
    case BlockFacing::South: return BlockFacing::North;
    case BlockFacing::East:  return BlockFacing::West;
    case BlockFacing::West:  return BlockFacing::East;
    case BlockFacing::Up:    return BlockFacing::Down;
    case BlockFacing::Down:  return BlockFacing::Up;
    }
    return BlockFacing::North;
}

VoxelCoord RedstoneLogic::repeaterInputPos(const VoxelCoord &pos, BlockFacing f)
{
    VoxelCoord back = facingOffset(opposite(f));
    return {pos.x + back.x, pos.y + back.y, pos.z + back.z};
}

std::pair<VoxelCoord,VoxelCoord> RedstoneLogic::sideOffsets(BlockFacing facing)
{
    switch (facing) {
    case BlockFacing::North:
    case BlockFacing::South:
        return { VoxelCoord{1,0,0}, VoxelCoord{-1,0,0} };
    case BlockFacing::East:
    case BlockFacing::West:
        return { VoxelCoord{0,0,1}, VoxelCoord{0,0,-1} };
    default:
        return { VoxelCoord{1,0,0}, VoxelCoord{-1,0,0} };
    }
}

int RedstoneLogic::getOutputPower(const Block &b)
{
    return static_cast<int>(b.power);
}

uint8_t RedstoneLogic::getDustOutput(const Block &self,
                                     const VoxelCoord &,
                                     const VoxelWorld &)
{
    switch (self.type) {
    case BlockType::Lever:
    case BlockType::StoneButton:
    case BlockType::WoodButton:
    case BlockType::StonePressurePlate:
    case BlockType::WoodPressurePlate:
    case BlockType::LightWeightedPressurePlate:
    case BlockType::HeavyWeightedPressurePlate:
        return (self.flags & SimFlags::ACTIVE) ? 15u : 0u;
    case BlockType::RedstoneBlock:
        return 15u;
    case BlockType::RedstoneTorch:
        return (self.flags & SimFlags::ACTIVE) ? 15u : 0u;
    case BlockType::Repeater:
    case BlockType::Comparator:
        return self.power;
    case BlockType::RedstoneWire:
        return 0u;
    default:
        return 0u;
    }
}

Block RedstoneLogic::evaluate(const Block &self,
                               const VoxelCoord &pos,
                               const VoxelWorld &world,
                               uint64_t tick)
{
    switch (self.type) {
    case BlockType::RedstoneTorch: return evalTorch     (self, pos, world);
    case BlockType::Repeater:      return evalRepeater  (self, pos, world, tick);
    case BlockType::Comparator:    return evalComparator(self, pos, world);
    case BlockType::Observer:      return evalObserver  (self, pos, world, tick);
    default:                       return self;
    }
}

Block RedstoneLogic::evalTorch(const Block &self,
                                const VoxelCoord &pos,
                                const VoxelWorld &world)
{
    VoxelCoord attachedOffset = facingOffset(opposite(self.facing));
    VoxelCoord attachedPos{
        pos.x + attachedOffset.x,
        pos.y + attachedOffset.y,
        pos.z + attachedOffset.z
    };
    Block host = world.getBlock(attachedPos.x,
                                attachedPos.y,
                                attachedPos.z);

    bool hostPowered = (host.power > 0)
                    || (host.flags & SimFlags::STRONG_POWERED);

    if (!hostPowered) {
        const VoxelCoord neighbors6[6] = {
            { 0, 0,-1}, { 1, 0, 0}, { 0, 0, 1},
            {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0},
        };
        for (const auto &d : neighbors6) {
            VoxelCoord np{
                attachedPos.x + d.x,
                attachedPos.y + d.y,
                attachedPos.z + d.z
            };
            Block nb = world.getBlock(np.x, np.y, np.z);
            if (nb.type == BlockType::RedstoneWire && nb.power > 0) {
                hostPowered = true;
                break;
            }
        }
    }

    Block next = self;
    if (hostPowered) {
        next.flags &= ~SimFlags::ACTIVE;
        next.power  = 0;
    } else {
        next.flags |=  SimFlags::ACTIVE;
        next.power  = 15;
    }
    return next;
}

Block RedstoneLogic::evalRepeater(const Block &self,
                                   const VoxelCoord &pos,
                                   const VoxelWorld &world,
                                   uint64_t)
{
    if (self.flags & SimFlags::LOCKED)
        return self;

    VoxelCoord inputPos = repeaterInputPos(pos, self.facing);
    Block input = world.getBlock(inputPos.x, inputPos.y, inputPos.z);

    bool shouldActive = (input.power > 0);
    bool isActive     = (self.flags & SimFlags::ACTIVE) != 0;

    if (shouldActive == isActive)
        return self;

    Block next = self;
    if (shouldActive) {
        next.flags |=  SimFlags::ACTIVE;
        next.power  = 15;
    } else {
        next.flags &= ~SimFlags::ACTIVE;
        next.power  = 0;
    }
    return next;
}

Block RedstoneLogic::evalComparator(const Block &self,
                                     const VoxelCoord &pos,
                                     const VoxelWorld &world)
{
    VoxelCoord inputPos = repeaterInputPos(pos, self.facing);
    Block backBlock = world.getBlock(inputPos.x, inputPos.y, inputPos.z);
    int backPow = backBlock.power;

    VoxelCoord fwdOff = facingOffset(self.facing);
    auto rotCW = [](VoxelCoord d) -> VoxelCoord { return {d.z, d.y, -d.x}; };
    VoxelCoord leftOff  = rotCW(fwdOff);
    VoxelCoord rightOff = {-leftOff.x, leftOff.y, -leftOff.z};

    Block leftB  = world.getBlock(pos.x + leftOff.x,  pos.y, pos.z + leftOff.z);
    Block rightB = world.getBlock(pos.x + rightOff.x, pos.y, pos.z + rightOff.z);
    int sidePow  = std::max(leftB.power, rightB.power);

    int outPow = 0;
    if (SimFlags::isSubtractMode(self.flags)) {
        outPow = std::max(backPow - sidePow, 0);
    } else {
        outPow = (backPow >= sidePow) ? backPow : 0;
    }

    Block next = self;
    next.power = static_cast<uint8_t>(std::clamp(outPow, 0, 15));
    if (next.power > 0) next.flags |=  SimFlags::ACTIVE;
    else                next.flags &= ~SimFlags::ACTIVE;
    return next;
}

Block RedstoneLogic::evalObserver(const Block &self,
                                   const VoxelCoord &pos,
                                   const VoxelWorld &world,
                                   uint64_t)
{
    VoxelCoord backOffset = facingOffset(opposite(self.facing));
    VoxelCoord backPos    = { pos.x + backOffset.x,
                              pos.y + backOffset.y,
                              pos.z + backOffset.z };
    const Block &observed = world.getBlock(backPos.x, backPos.y, backPos.z);
    const bool changed = (observed.flags & SimFlags::SCHEDULED) != 0;

    Block next = self;
    if (changed) {
        next.flags |=  SimFlags::ACTIVE;
        next.power  = 15;
    } else {
        next.flags &= ~SimFlags::ACTIVE;
        next.power  = 0;
    }
    return next;
}

void RedstoneLogic::applyActuator(Block &self, int inputPower)
{
    bool powered = (inputPower > 0);
    switch (self.type) {
    case BlockType::RedstoneLamp:
    case BlockType::IronDoor:
    case BlockType::IronTrapdoor:
    case BlockType::FenceGate:
    case BlockType::Piston:
    case BlockType::StickyPiston:
        if (powered) self.flags |=  SimFlags::LIT;
        else         self.flags &= ~SimFlags::LIT;
        break;
    default:
        break;
    }
}

bool RedstoneLogic::isRepeaterLocked(const VoxelCoord &pos,
                                      BlockFacing facing,
                                      const VoxelWorld &world)
{
    auto [sideA, sideB] = sideOffsets(facing);

    {
        VoxelCoord checkPos{ pos.x + sideA.x, pos.y, pos.z + sideA.z };
        const Block &nb = world.getBlock(checkPos.x, checkPos.y, checkPos.z);
        if (nb.type == BlockType::Repeater && (nb.flags & SimFlags::ACTIVE)) {
            VoxelCoord out = facingOffset(nb.facing);
            if (out.x == -sideA.x && out.z == -sideA.z)
                return true;
        }
    }

    {
        VoxelCoord checkPos{ pos.x + sideB.x, pos.y, pos.z + sideB.z };
        const Block &nb = world.getBlock(checkPos.x, checkPos.y, checkPos.z);
        if (nb.type == BlockType::Repeater && (nb.flags & SimFlags::ACTIVE)) {
            VoxelCoord out = facingOffset(nb.facing);
            if (out.x == -sideB.x && out.z == -sideB.z)
                return true;
        }
    }

    return false;
}

bool RedstoneLogic::isTransparent(BlockType t)
{
    switch (t) {
    case BlockType::Air:
    case BlockType::RedstoneWire:
    case BlockType::RedstoneTorch:
    case BlockType::Repeater:
    case BlockType::Comparator:
    case BlockType::Lever:
    case BlockType::StoneButton:
    case BlockType::WoodButton:
    case BlockType::StonePressurePlate:
    case BlockType::WoodPressurePlate:
    case BlockType::LightWeightedPressurePlate:
    case BlockType::HeavyWeightedPressurePlate:
    case BlockType::PoweredRail:
    case BlockType::Glass:
        return true;
    default:
        return false;
    }
}

bool RedstoneLogic::canActivateRail(const Block &b)
{
    switch (b.type) {
    case BlockType::Lever:
    case BlockType::StoneButton:
    case BlockType::WoodButton:
    case BlockType::StonePressurePlate:
    case BlockType::WoodPressurePlate:
    case BlockType::LightWeightedPressurePlate:
    case BlockType::HeavyWeightedPressurePlate:
        return (b.flags & SimFlags::ACTIVE) != 0;
    case BlockType::RedstoneBlock:
        return true;
    case BlockType::RedstoneTorch:
    case BlockType::Repeater:
    case BlockType::Comparator:
        return (b.flags & SimFlags::ACTIVE) != 0 || b.power > 0;
    case BlockType::RedstoneWire:
        return b.power > 0;
    case BlockType::PoweredRail:
        return (b.flags & SimFlags::LIT) != 0;
    default:
        return false;
    }
}

bool RedstoneLogic::areRailsConnected(const VoxelCoord &a,
                                       const VoxelCoord &b)
{
    if (a.y != b.y) return false;
    const int dx = std::abs(a.x - b.x);
    const int dz = std::abs(a.z - b.z);
    return (dx + dz == 1);
}