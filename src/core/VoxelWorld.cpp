#include "VoxelWorld.h"

Block VoxelWorld::getBlock(int x, int y, int z) const
{
    if (!inBounds(x, y, z)) return Block::air();
    auto it = m_blocks.find({x, y, z});
    return (it != m_blocks.end()) ? it->second : Block::air();
}

void VoxelWorld::setBlock(int x, int y, int z, const Block &block)
{
    if (!inBounds(x, y, z)) return;
    if (block.isEmpty())
        m_blocks.erase({x, y, z});
    else
        m_blocks[{x, y, z}] = block;

    if (m_onChange) m_onChange(x, y, z, block);
}

void VoxelWorld::clearBlock(int x, int y, int z)
{
    m_blocks.erase({x, y, z});
    if (m_onChange) m_onChange(x, y, z, Block::air());
}

bool VoxelWorld::hasBlock(int x, int y, int z) const
{
    if (!inBounds(x, y, z)) return false;
    auto it = m_blocks.find({x, y, z});
    return (it != m_blocks.end()) && !it->second.isEmpty();
}

Block *VoxelWorld::getBlockMutable(int x, int y, int z)
{
    if (!inBounds(x, y, z)) return nullptr;
    auto it = m_blocks.find({x, y, z});
    if (it == m_blocks.end()) return nullptr;
    return &it->second;
}

void VoxelWorld::notifyChange(int x, int y, int z, const Block &block)
{
    if (m_onChange) m_onChange(x, y, z, block);
}

void VoxelWorld::clearAll()
{
    m_blocks.clear();
}

bool VoxelWorld::inBounds(int x, int y, int z) const
{
    return x >= WORLD_MIN && x <= WORLD_MAX
        && y >= LAYER_MIN && y <= LAYER_MAX
        && z >= WORLD_MIN && z <= WORLD_MAX;
}
