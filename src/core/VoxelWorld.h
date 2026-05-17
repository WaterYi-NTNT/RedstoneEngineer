#pragma once

#include <unordered_map>
#include <functional>
#include "Block.h"

class VoxelWorld
{
public:
    static constexpr int WORLD_MIN = -512;
    static constexpr int WORLD_MAX =  512;
    static constexpr int LAYER_MIN =    0;
    static constexpr int LAYER_MAX =   15;

    using BlockMap       = std::unordered_map<VoxelCoord, Block, VoxelCoordHash>;
    using ChangeCallback = std::function<void(int x, int y, int z, const Block &)>;

    VoxelWorld() = default;

    Block  getBlock       (int x, int y, int z) const;
    void   setBlock       (int x, int y, int z, const Block &block);
    void   clearBlock     (int x, int y, int z);
    bool   hasBlock       (int x, int y, int z) const;

    Block *getBlockMutable(int x, int y, int z);

    void   notifyChange   (int x, int y, int z, const Block &block);

    void   clearAll();
    int    blockCount() const { return static_cast<int>(m_blocks.size()); }

    const BlockMap &allBlocks() const { return m_blocks; }

    void setChangeCallback(ChangeCallback cb) { m_onChange = std::move(cb); }

private:
    BlockMap       m_blocks;
    ChangeCallback m_onChange;

    bool inBounds(int x, int y, int z) const;
};
