#include "io/LitematicExporter.h"
#include "core/Block.h"
#include "sim/SimFlags.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QString>
#include <QVector>
#include <climits>
#include <cstring>
#include <zlib.h>

namespace {

struct NbtWriter
{
    QByteArray buf;

    void u8 (uint8_t  v) { buf += static_cast<char>(v); }
    void i16(int16_t  v) { u8(v >> 8); u8(v); }
    void i32(int32_t  v) { u8(v>>24); u8(v>>16); u8(v>>8); u8(v); }
    void i64(int64_t  v) { i32(int32_t(v >> 32)); i32(int32_t(v)); }

    void nbtStr(const QString &s) {
        QByteArray utf8 = s.toUtf8();
        i16(static_cast<int16_t>(utf8.size()));
        buf += utf8;
    }

    void tagInt   (const QString &n, int32_t v)    { u8(3);  nbtStr(n); i32(v); }
    void tagLong  (const QString &n, int64_t v)    { u8(4);  nbtStr(n); i64(v); }
    void tagString(const QString &n, const QString &v) { u8(8); nbtStr(n); nbtStr(v); }

    void beginCompound(const QString &n) { u8(10); nbtStr(n); }
    void endCompound()                   { u8(0); }

    void beginCompoundList(const QString &n, int32_t count) {
        u8(9); nbtStr(n); u8(10); i32(count);
    }

    void emptyList(const QString &n) {
        u8(9); nbtStr(n); u8(10); i32(0);
    }

    void tagLongArray(const QString &n, const QVector<int64_t> &v) {
        u8(12); nbtStr(n); i32(v.size());
        for (int64_t x : v) i64(x);
    }
};

QByteArray gzipCompress(const QByteArray &input)
{
    z_stream zs{};

    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED,
                     15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return {};

    zs.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
    zs.avail_in = static_cast<uInt>(input.size());

    QByteArray output;
    char chunk[32768];
    int ret;
    do {
        zs.next_out  = reinterpret_cast<Bytef*>(chunk);
        zs.avail_out = sizeof(chunk);
        ret = deflate(&zs, Z_FINISH);
        output.append(chunk, static_cast<int>(sizeof(chunk) - zs.avail_out));
    } while (ret == Z_OK);

    deflateEnd(&zs);
    return (ret == Z_STREAM_END) ? output : QByteArray{};
}

struct McState {
    QString            name;
    QMap<QString,QString> props;
};

static QString facingStr(BlockFacing f)
{
    switch(f){
    case BlockFacing::North: return "north";
    case BlockFacing::East:  return "east";
    case BlockFacing::South: return "south";
    case BlockFacing::West:  return "west";
    case BlockFacing::Up:    return "up";
    case BlockFacing::Down:  return "down";
    }
    return "north";
}

static QString faceStr(BlockFacing f)
{
    if (f == BlockFacing::Up)   return "floor";
    if (f == BlockFacing::Down) return "ceiling";
    return "wall";
}

static QString facingForFaceBlock(BlockFacing f)
{
    if (f == BlockFacing::Up || f == BlockFacing::Down) return "north";
    return facingStr(f);
}

static McState toMcState(const Block &block)
{
    McState s;
    const bool active   = (block.flags & SimFlags::ACTIVE)  != 0;
    const bool lit      = (block.flags & SimFlags::LIT)     != 0;
    const bool locked   = (block.flags & SimFlags::LOCKED)  != 0;
    const bool extended = (block.flags & SimFlags::LIT)     != 0;
    const QString f     = facingStr(block.facing);

    switch (block.type)
    {

    case BlockType::Air:
        s.name = "minecraft:air";
        break;

    case BlockType::Lever:
        s.name = "minecraft:lever";
        s.props["face"]    = faceStr(block.facing);
        s.props["facing"]  = facingForFaceBlock(block.facing);
        s.props["powered"] = active ? "true" : "false";
        break;

    case BlockType::StoneButton:
        s.name = "minecraft:stone_button";
        s.props["face"]    = faceStr(block.facing);
        s.props["facing"]  = facingForFaceBlock(block.facing);
        s.props["powered"] = active ? "true" : "false";
        break;

    case BlockType::WoodButton:
        s.name = "minecraft:oak_button";
        s.props["face"]    = faceStr(block.facing);
        s.props["facing"]  = facingForFaceBlock(block.facing);
        s.props["powered"] = active ? "true" : "false";
        break;

    case BlockType::StonePressurePlate:
        s.name = "minecraft:stone_pressure_plate";
        s.props["powered"] = active ? "true" : "false";
        break;

    case BlockType::WoodPressurePlate:
        s.name = "minecraft:oak_pressure_plate";
        s.props["powered"] = active ? "true" : "false";
        break;

    case BlockType::LightWeightedPressurePlate:
        s.name = "minecraft:light_weighted_pressure_plate";
        s.props["power"] = QString::number(block.power);
        break;

    case BlockType::HeavyWeightedPressurePlate:
        s.name = "minecraft:heavy_weighted_pressure_plate";
        s.props["power"] = QString::number(block.power);
        break;

    case BlockType::DaylightDetector:
        s.name = "minecraft:daylight_detector";
        s.props["inverted"] = "false";
        s.props["power"]    = "0";
        break;

    case BlockType::TargetBlock:
        s.name = "minecraft:target";
        s.props["power"] = "0";
        break;

    case BlockType::SculkSensor:
        s.name = "minecraft:sculk_sensor";
        s.props["power"]             = "0";
        s.props["sculk_sensor_phase"]= "inactive";
        s.props["waterlogged"]       = "false";
        break;

    case BlockType::CalibratedSculkSensor:
        s.name = "minecraft:calibrated_sculk_sensor";
        s.props["facing"]            = f;
        s.props["output_signal"]     = "0";
        s.props["sculk_sensor_phase"]= "inactive";
        s.props["waterlogged"]       = "false";
        break;

    case BlockType::TripwireHook:
        s.name = "minecraft:tripwire_hook";
        s.props["attached"] = "false";
        s.props["facing"]   = f;
        s.props["powered"]  = "false";
        break;

    case BlockType::Tripwire:
        s.name = "minecraft:tripwire";
        s.props["attached"] = "false";
        s.props["disarmed"] = "false";
        s.props["east"]     = "false";
        s.props["north"]    = "false";
        s.props["powered"]  = "false";
        s.props["south"]    = "false";
        s.props["west"]     = "false";
        break;

    case BlockType::TrappedChest:
        s.name = "minecraft:trapped_chest";
        s.props["facing"]     = f;
        s.props["type"]       = "single";
        s.props["waterlogged"]= "false";
        break;

    case BlockType::Lectern:
        s.name = "minecraft:lectern";
        s.props["facing"]   = f;
        s.props["has_book"] = "false";
        s.props["powered"]  = "false";
        break;

    case BlockType::RedstoneWire:
        s.name = "minecraft:redstone_wire";
        s.props["east"]  = "none";
        s.props["north"] = "none";
        s.props["power"] = QString::number(block.power);
        s.props["south"] = "none";
        s.props["west"]  = "none";
        break;

    case BlockType::RedstoneTorch:
        if (block.facing == BlockFacing::North || block.facing == BlockFacing::East ||
            block.facing == BlockFacing::South || block.facing == BlockFacing::West) {

            s.name = "minecraft:redstone_wall_torch";
            s.props["facing"] = f;
            s.props["lit"]    = active ? "true" : "false";
        } else {

            s.name = "minecraft:redstone_torch";
            s.props["lit"] = active ? "true" : "false";
        }
        break;

    case BlockType::RedstoneBlock:
        s.name = "minecraft:redstone_block";
        break;

    case BlockType::Repeater: {
        const int delay = SimFlags::getRepeaterDelay(block.flags) + 1;
        s.name = "minecraft:repeater";
        s.props["delay"]   = QString::number(delay);
        s.props["facing"]  = f;
        s.props["locked"]  = locked ? "true" : "false";
        s.props["powered"] = active ? "true" : "false";
        break;
    }

    case BlockType::Comparator: {
        const bool sub = SimFlags::isSubtractMode(block.flags);
        s.name = "minecraft:comparator";
        s.props["facing"]  = f;
        s.props["mode"]    = sub ? "subtract" : "compare";
        s.props["powered"] = active ? "true" : "false";
        break;
    }

    case BlockType::Observer:
        s.name = "minecraft:observer";
        s.props["facing"]  = f;
        s.props["powered"] = active ? "true" : "false";
        break;

    case BlockType::Piston:
        s.name = "minecraft:piston";
        s.props["extended"] = extended ? "true" : "false";
        s.props["facing"]   = f;
        break;

    case BlockType::StickyPiston:
        s.name = "minecraft:sticky_piston";
        s.props["extended"] = extended ? "true" : "false";
        s.props["facing"]   = f;
        break;

    case BlockType::PistonHead: {
        const bool sticky = (block.flags & SimFlags::ACTIVE) != 0;
        s.name = "minecraft:piston_head";
        s.props["facing"] = f;
        s.props["short"]  = "false";
        s.props["type"]   = sticky ? "sticky" : "normal";
        break;
    }

    case BlockType::Dropper:
        s.name = "minecraft:dropper";
        s.props["facing"]    = f;
        s.props["triggered"] = "false";
        break;

    case BlockType::Dispenser:
        s.name = "minecraft:dispenser";
        s.props["facing"]    = f;
        s.props["triggered"] = "false";
        break;

    case BlockType::Hopper:
        s.name = "minecraft:hopper";
        s.props["enabled"] = "true";

        s.props["facing"]  = (block.facing == BlockFacing::Up) ? "down" : f;
        break;

    case BlockType::TNT:
        s.name = "minecraft:tnt";
        s.props["unstable"] = "false";
        break;

    case BlockType::RedstoneLamp:
        s.name = "minecraft:redstone_lamp";
        s.props["lit"] = lit ? "true" : "false";
        break;

    case BlockType::IronDoor:
        s.name = "minecraft:iron_door";
        s.props["facing"]  = f;
        s.props["half"]    = "lower";
        s.props["hinge"]   = "left";
        s.props["open"]    = "false";
        s.props["powered"] = active ? "true" : "false";
        break;

    case BlockType::IronTrapdoor:
        s.name = "minecraft:iron_trapdoor";
        s.props["facing"]     = f;
        s.props["half"]       = "bottom";
        s.props["open"]       = "false";
        s.props["powered"]    = active ? "true" : "false";
        s.props["waterlogged"]= "false";
        break;

    case BlockType::FenceGate:
        s.name = "minecraft:oak_fence_gate";
        s.props["facing"]  = f;
        s.props["in_wall"] = "false";
        s.props["open"]    = "false";
        s.props["powered"] = active ? "true" : "false";
        break;

    case BlockType::NoteBlock:
        s.name = "minecraft:note_block";
        s.props["instrument"] = "harp";
        s.props["note"]       = "0";
        s.props["powered"]    = active ? "true" : "false";
        break;

    case BlockType::PoweredRail:
        s.name = "minecraft:powered_rail";
        s.props["powered"]    = active ? "true" : "false";
        s.props["shape"]      = "north_south";
        s.props["waterlogged"]= "false";
        break;

    case BlockType::Stone:
        s.name = "minecraft:stone";
        break;

    case BlockType::Glass:
        s.name = "minecraft:glass";
        break;

    case BlockType::SlabTop:
        s.name = "minecraft:stone_slab";
        s.props["type"]       = "top";
        s.props["waterlogged"]= "false";
        break;

    case BlockType::SlabBottom:
        s.name = "minecraft:stone_slab";
        s.props["type"]       = "bottom";
        s.props["waterlogged"]= "false";
        break;

    case BlockType::Stair:
        s.name = "minecraft:stone_stairs";
        s.props["facing"]     = f;
        s.props["half"]       = "bottom";
        s.props["shape"]      = "straight";
        s.props["waterlogged"]= "false";
        break;

    default:

        s.name = "minecraft:stone";
        break;
    }

    return s;
}

static int requiredBits(int paletteSize)
{
    if (paletteSize <= 1) return 2;
    int bits = 0, n = paletteSize - 1;
    while (n > 0) { bits++; n >>= 1; }
    return std::max(2, bits);
}

static QVector<int64_t> packBlockStates(const QVector<int> &indices, int paletteSize)
{
    const int bpe       = requiredBits(paletteSize);
    const int total     = indices.size();
    const int64_t totalBits = static_cast<int64_t>(total) * bpe;
    const int longCount = static_cast<int>((totalBits + 63) / 64);

    QVector<int64_t> longs(longCount, 0);

    for (int i = 0; i < total; ++i) {
        const int64_t bitPos   = static_cast<int64_t>(i) * bpe;
        const int     longIdx  = static_cast<int>(bitPos / 64);
        const int     bitOff   = static_cast<int>(bitPos % 64);
        const int64_t val      = static_cast<int64_t>(indices[i]);

        longs[longIdx] |= (val << bitOff);

        if (bitOff + bpe > 64 && longIdx + 1 < longCount) {
            const int bitsInFirst = 64 - bitOff;
            longs[longIdx + 1] |= (val >> bitsInFirst);
        }
    }
    return longs;
}

static QByteArray buildNbt(const VoxelWorld &world, const QString &schematicName)
{

    int minX = INT_MAX, maxX = INT_MIN;
    int minY = INT_MAX, maxY = INT_MIN;
    int minZ = INT_MAX, maxZ = INT_MIN;

    for (const auto &[coord, block] : world.allBlocks()) {
        if (block.isEmpty()) continue;
        minX = qMin(minX, coord.x); maxX = qMax(maxX, coord.x);
        minY = qMin(minY, coord.y); maxY = qMax(maxY, coord.y);
        minZ = qMin(minZ, coord.z); maxZ = qMax(maxZ, coord.z);
    }

    if (minX == INT_MAX) {

        minX = maxX = minY = maxY = minZ = maxZ = 0;
    }

    const int sizeX = maxX - minX + 1;
    const int sizeY = maxY - minY + 1;
    const int sizeZ = maxZ - minZ + 1;
    const int totalVol = sizeX * sizeY * sizeZ;

    QVector<McState>      palette;
    QMap<QString, int>    paletteLookup;

    auto paletteKey = [](const McState &s) -> QString {
        if (s.props.isEmpty()) return s.name;
        QString p;
        for (auto it = s.props.begin(); it != s.props.end(); ++it) {
            if (!p.isEmpty()) p += ',';
            p += it.key() + '=' + it.value();
        }
        return s.name + '[' + p + ']';
    };

    McState airState;
    airState.name = "minecraft:air";
    palette.append(airState);
    paletteLookup["minecraft:air"] = 0;

    auto getOrAddPalette = [&](const McState &ms) -> int {
        const QString key = paletteKey(ms);
        auto it = paletteLookup.find(key);
        if (it != paletteLookup.end()) return it.value();
        const int idx = palette.size();
        palette.append(ms);
        paletteLookup[key] = idx;
        return idx;
    };

    QVector<int> blockIndices(totalVol, 0);

    int nonAirCount = 0;
    for (const auto &[coord, block] : world.allBlocks()) {
        if (block.isEmpty()) continue;
        const int lx = coord.x - minX;
        const int ly = coord.y - minY;
        const int lz = coord.z - minZ;
        if (lx < 0 || lx >= sizeX || ly < 0 || ly >= sizeY || lz < 0 || lz >= sizeZ)
            continue;
        const int idx = (ly * sizeZ + lz) * sizeX + lx;
        blockIndices[idx] = getOrAddPalette(toMcState(block));
        ++nonAirCount;
    }

    const QVector<int64_t> longArray = packBlockStates(blockIndices, palette.size());

    const int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
    NbtWriter nbt;

    nbt.beginCompound(QString());

    nbt.tagInt("MinecraftDataVersion", 3955);
    nbt.tagInt("Version", 6);

    nbt.beginCompound("Metadata");
      nbt.tagString("Name",        schematicName);
      nbt.tagString("Author",      "RedstoneEngineer");
      nbt.tagString("Description", "");
      nbt.tagLong("TimeCreated",   nowMs);
      nbt.tagLong("TimeModified",  nowMs);
      nbt.tagInt ("RegionCount",   1);
      nbt.tagInt ("TotalBlocks",   nonAirCount);
      nbt.tagInt ("TotalVolume",   totalVol);
      nbt.beginCompound("EnclosingSize");
        nbt.tagInt("x", sizeX);
        nbt.tagInt("y", sizeY);
        nbt.tagInt("z", sizeZ);
      nbt.endCompound();
    nbt.endCompound();

    nbt.beginCompound("Regions");
    nbt.beginCompound("Region");

      nbt.beginCompound("Position");
        nbt.tagInt("x", 0); nbt.tagInt("y", 0); nbt.tagInt("z", 0);
      nbt.endCompound();

      nbt.beginCompound("Size");
        nbt.tagInt("x", sizeX);
        nbt.tagInt("y", sizeY);
        nbt.tagInt("z", sizeZ);
      nbt.endCompound();

      nbt.beginCompoundList("BlockStatePalette", palette.size());
      for (const McState &ms : palette) {

          nbt.tagString("Name", ms.name);
          if (!ms.props.isEmpty()) {
              nbt.beginCompound("Properties");
              for (auto it = ms.props.begin(); it != ms.props.end(); ++it)
                  nbt.tagString(it.key(), it.value());
              nbt.endCompound();
          }
          nbt.endCompound();
      }

      nbt.tagLongArray("BlockStates", longArray);

      nbt.emptyList("Entities");
      nbt.emptyList("TileEntities");
      nbt.emptyList("PendingBlockTicks");
      nbt.emptyList("PendingFluidTicks");

    nbt.endCompound();
    nbt.endCompound();

    nbt.endCompound();

    return nbt.buf;
}

}

LitematicExporter::Result
LitematicExporter::exportToFile(const VoxelWorld &world,
                                const QString    &filePath,
                                const QString    &name)
{
    Result result;

    const QString schName = name.isEmpty()
                          ? QFileInfo(filePath).baseName()
                          : name;

    const QByteArray rawNbt = buildNbt(world, schName);
    if (rawNbt.isEmpty()) {
        result.errorMessage = "NBT 构建失败（内部错误）";
        return result;
    }

    const QByteArray compressed = gzipCompress(rawNbt);
    if (compressed.isEmpty()) {
        result.errorMessage = "GZIP 压缩失败";
        return result;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        result.errorMessage = QString("无法写入文件：%1").arg(filePath);
        return result;
    }
    file.write(compressed);
    file.close();

    result.success = true;
    return result;
}
