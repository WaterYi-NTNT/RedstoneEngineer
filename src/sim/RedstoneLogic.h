#pragma once
#include "core/Block.h"
#include "core/VoxelWorld.h"
#include <cstdint>

class RedstoneLogic
{
public:

    static Block evaluate(const Block &self,
                          const VoxelCoord &pos,
                          const VoxelWorld &world,
                          uint64_t tick);

    static uint8_t getDustOutput(const Block &self,
                                 const VoxelCoord &pos,
                                 const VoxelWorld &world);

    static void applyActuator(Block &self, int inputPower);


    static Block evalTorch     (const Block &self, const VoxelCoord &pos, const VoxelWorld &w);
    static Block evalRepeater  (const Block &self, const VoxelCoord &pos, const VoxelWorld &w, uint64_t tick);
    static Block evalComparator(const Block &self, const VoxelCoord &pos, const VoxelWorld &w);
    static Block evalObserver  (const Block &self, const VoxelCoord &pos, const VoxelWorld &w, uint64_t tick);


    static VoxelCoord facingOffset  (BlockFacing f);
    static BlockFacing opposite     (BlockFacing f);
    static VoxelCoord repeaterInputPos(const VoxelCoord &pos, BlockFacing f);
    static int        getOutputPower(const Block &b);


    static bool isRepeaterLocked(const VoxelCoord &pos,
                                  BlockFacing       facing,
                                  const VoxelWorld &world);


    static std::pair<VoxelCoord, VoxelCoord> sideOffsets(BlockFacing facing);
};
