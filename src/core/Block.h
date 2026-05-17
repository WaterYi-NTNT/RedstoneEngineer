#pragma once
#include <cstdint>
#include <functional>

struct VoxelCoord
{
    int x = 0, y = 0, z = 0;

    bool operator==(const VoxelCoord &o) const
    {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct VoxelCoordHash
{
    size_t operator()(const VoxelCoord &c) const noexcept
    {
        size_t h = std::hash<int>{}(c.x);
        h ^= std::hash<int>{}(c.y) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(c.z) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

enum class BlockType : uint8_t
{
    Air = 0,

    Lever,
    StoneButton,
    WoodButton,
    StonePressurePlate,
    WoodPressurePlate,
    LightWeightedPressurePlate,
    HeavyWeightedPressurePlate,
    DaylightDetector,
    TargetBlock,
    SculkSensor,
    CalibratedSculkSensor,
    TripwireHook,
    Tripwire,
    TrappedChest,
    Lectern,

    RedstoneWire,
    RedstoneTorch,
    RedstoneBlock,
    Repeater,
    Comparator,
    Observer,

    Piston,
    StickyPiston,
    PistonHead,
    Dropper,
    Dispenser,
    Hopper,
    TNT,
    RedstoneLamp,
    IronDoor,
    IronTrapdoor,
    FenceGate,
    NoteBlock,
    PoweredRail,

    Stone,
    Glass,
    SlabTop,
    SlabBottom,
    Stair,

    Other,

    _COUNT
};

enum class BlockFacing : uint8_t
{
    North = 0,
    East,
    South,
    West,
    Up,
    Down,
};

enum class BlockGroup : uint8_t
{
    SignalSource = 0,
    Logic,
    Actuator,
    Structure,
    Other,
};

struct BlockMeta
{
    const char *enumKey;
    const char *displayName;
    BlockGroup  group;
    bool        hasDirection;
    bool        conductsSolid;
    bool        canFaceVertically;
};

inline const BlockMeta &getBlockMeta(BlockType type)
{

    static const BlockMeta TABLE[] = {

        { "other",                         "空气",           BlockGroup::Other,        false, false, false },

        { "lever",                         "拉杆",           BlockGroup::SignalSource,  true,  false, true  },
        { "stone_button",                  "石头按钮",       BlockGroup::SignalSource,  true,  false, true  },
        { "wood_button",                   "木头按钮",       BlockGroup::SignalSource,  true,  false, true  },
        { "stone_pressure_plate",          "石质压力板",     BlockGroup::SignalSource,  false, false, false },
        { "wood_pressure_plate",           "木质压力板",     BlockGroup::SignalSource,  false, false, false },
        { "light_weighted_pressure_plate", "金质压力板",     BlockGroup::SignalSource,  false, false, false },
        { "heavy_weighted_pressure_plate", "铁质压力板",     BlockGroup::SignalSource,  false, false, false },
        { "daylight_detector",             "阳光传感器",     BlockGroup::SignalSource,  false, false, false },
        { "target_block",                  "目标方块",       BlockGroup::SignalSource,  false, false, false },
        { "sculk_sensor",                  "幽匿传感器",     BlockGroup::SignalSource,  false, false, false },
        { "calibrated_sculk_sensor",       "校准幽匿传感器", BlockGroup::SignalSource,  true,  false, false },
        { "tripwire_hook",                 "绊线钩",         BlockGroup::SignalSource,  true,  false, false },
        { "tripwire",                      "绊线",           BlockGroup::SignalSource,  false, false, false },
        { "trapped_chest",                 "陷阱箱",         BlockGroup::SignalSource,  true,  false, false },
        { "lectern",                       "讲台",           BlockGroup::SignalSource,  true,  false, false },

        { "redstone_dust_dot",             "红石粉",         BlockGroup::Logic,         false, false, false },
        { "redstone_torch",                "红石火把",       BlockGroup::Logic,         true,  false, true  },
        { "redstone_block",                "红石块",         BlockGroup::Logic,         false, true,  false },
        { "repeater_top",                  "中继器",         BlockGroup::Logic,         true,  false, false },
        { "comparator_top",                "比较器",         BlockGroup::Logic,         true,  false, false },
        { "observer_top",                  "侦测器",         BlockGroup::Logic,         true,  false, true  },

        { "piston_side",                   "活塞",           BlockGroup::Actuator,      true,  true,  true  },
        { "piston_side",                   "粘性活塞",       BlockGroup::Actuator,      true,  true,  true  },
        { "piston_top_normal",             "活塞臂",         BlockGroup::Actuator,      true,  false, true  },
        { "dropper_front",                 "投掷器",         BlockGroup::Actuator,      true,  true,  true  },
        { "dispenser_front",               "发射器",         BlockGroup::Actuator,      true,  true,  true  },
        { "hopper_side",                   "漏斗",           BlockGroup::Actuator,      true,  true,  true  },
        { "tnt",                           "TNT",            BlockGroup::Actuator,      false, false, false },
        { "redstone_lamp",                 "红石灯",         BlockGroup::Actuator,      false, true,  false },
        { "iron_door_top",                 "铁门",           BlockGroup::Actuator,      true,  false, false },
        { "iron_trapdoor",                 "铁活板门",       BlockGroup::Actuator,      true,  false, false },
        { "fence_gate",                    "栅栏门",         BlockGroup::Actuator,      true,  false, false },
        { "note_block",                    "音符盒",         BlockGroup::Actuator,      false, true,  false },
        { "powered_rail",                  "充能铁轨",       BlockGroup::Actuator,      true,  false, false },

        { "stone",                         "石头",           BlockGroup::Structure,     false, true,  false },
        { "glass",                         "玻璃",           BlockGroup::Structure,     false, false, false },
        { "slab_top",                      "台阶（上半）",   BlockGroup::Structure,     false, false, false },
        { "slab_bottom",                   "台阶（下半）",   BlockGroup::Structure,     false, false, false },
        { "stair",                         "楼梯",           BlockGroup::Structure,     true,  false, false },

        { "other",                         "其他方块",       BlockGroup::Other,         false, true,  false },
    };

    const auto idx = static_cast<size_t>(type);
    if (idx >= static_cast<size_t>(BlockType::_COUNT))
        return TABLE[static_cast<size_t>(BlockType::Other)];
    return TABLE[idx];
}

struct Block
{
    BlockType   type    = BlockType::Air;
    BlockFacing facing  = BlockFacing::North;
    uint8_t     power   = 0;
    uint8_t     flags   = 0;

    bool isEmpty()   const { return type == BlockType::Air; }
    bool isPowered() const { return power > 0; }

    static Block air() { return Block{}; }
    static Block make(BlockType t, BlockFacing f = BlockFacing::North)
    {
        Block b; b.type = t; b.facing = f; return b;
    }

    bool operator==(const Block &o) const
    {
        return type == o.type && facing == o.facing
            && power == o.power && flags == o.flags;
    }
    bool operator!=(const Block &o) const { return !(*this == o); }
};
