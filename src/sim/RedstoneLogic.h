#pragma once
#include "core/VoxelWorld.h"
#include "core/Block.h"
#include <cstdint>
#include <utility>

class RedstoneLogic
{
public:

    static VoxelCoord facingOffset(BlockFacing f);
    static BlockFacing opposite(BlockFacing f);
    static VoxelCoord repeaterInputPos(const VoxelCoord &pos, BlockFacing f);
    static std::pair<VoxelCoord,VoxelCoord> sideOffsets(BlockFacing facing);

    static int     getOutputPower(const Block &b);
    static uint8_t getDustOutput (const Block &self,
                                  const VoxelCoord &pos,
                                  const VoxelWorld &world);

    static Block evaluate     (const Block &self, const VoxelCoord &pos,
                               const VoxelWorld &world, uint64_t tick);
    static void  applyActuator(Block &self, int inputPower);

    static bool isRepeaterLocked(const VoxelCoord &pos,
                                 BlockFacing facing,
                                 const VoxelWorld &world);

    static bool isTransparent(BlockType t);

    static bool canActivateRail   (const Block &b);
    static bool areRailsConnected (const VoxelCoord &a, const VoxelCoord &b);

private:
    static Block evalTorch      (const Block &self, const VoxelCoord &pos,
                                 const VoxelWorld &world);
    static Block evalRepeater   (const Block &self, const VoxelCoord &pos,
                                 const VoxelWorld &world, uint64_t tick);
    static Block evalComparator (const Block &self, const VoxelCoord &pos,
                                 const VoxelWorld &world);
    static Block evalObserver   (const Block &self, const VoxelCoord &pos,
                                 const VoxelWorld &world, uint64_t tick);
};
