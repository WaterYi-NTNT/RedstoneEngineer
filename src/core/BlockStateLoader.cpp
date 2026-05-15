#include "core/BlockStateLoader.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

QString                          BlockStateLoader::s_dataRoot;
QHash<QString, BlockStateResult> BlockStateLoader::s_cache;

void BlockStateLoader::setDataPath(const QString &root)
{
    s_dataRoot = root;
    s_cache.clear();
}

QString BlockStateLoader::facingStr(BlockFacing f)
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

QHash<QString,QString> BlockStateLoader::parseKey(const QString &key)
{
    QHash<QString,QString> props;
    if(key.isEmpty()) return props;

    const QStringList parts = key.split(',', Qt::SkipEmptyParts);
    for(const QString &part : parts){
        const int eq = part.indexOf('=');
        if(eq < 0) continue;
        props.insert(part.left(eq).trimmed(),
                     part.mid(eq+1).trimmed());
    }
    return props;
}

BlockStateResult BlockStateLoader::staticResult(BlockType type)
{
    BlockStateResult r;
    switch(type){
    case BlockType::Air:                         break;
    case BlockType::Lever:                       r.modelName="block/lever"; break;
    case BlockType::StoneButton:                 r.modelName="block/stone_button"; break;
    case BlockType::WoodButton:                  r.modelName="block/oak_button"; break;
    case BlockType::StonePressurePlate:          r.modelName="block/stone_pressure_plate"; break;
    case BlockType::WoodPressurePlate:           r.modelName="block/oak_pressure_plate"; break;
    case BlockType::LightWeightedPressurePlate:  r.modelName="block/light_weighted_pressure_plate"; break;
    case BlockType::HeavyWeightedPressurePlate:  r.modelName="block/heavy_weighted_pressure_plate"; break;
    case BlockType::DaylightDetector:            r.modelName="block/daylight_detector"; break;
    case BlockType::TargetBlock:                 r.modelName="block/target"; break;
    case BlockType::SculkSensor:                 r.modelName="block/sculk_sensor"; break;
    case BlockType::CalibratedSculkSensor:       r.modelName="block/calibrated_sculk_sensor"; break;
    case BlockType::TripwireHook:                r.modelName="block/tripwire_hook"; break;
    case BlockType::Tripwire:                    r.modelName="item/string"; break;
    case BlockType::TrappedChest:                r.modelName="@builtin/trapped_chest"; break;
    case BlockType::Lectern:                     r.modelName="block/lectern"; break;
    case BlockType::RedstoneWire:                r.modelName="block/redstone_dust_dot"; break;
    case BlockType::RedstoneTorch:               r.modelName="block/redstone_torch"; break;
    case BlockType::RedstoneBlock:               r.modelName="block/redstone_block"; break;
    case BlockType::Repeater:                    r.modelName="block/repeater"; break;
    case BlockType::Comparator:                  r.modelName="block/comparator"; break;
    case BlockType::Observer:                    r.modelName="block/observer"; break;
    case BlockType::Piston:                      r.modelName="block/piston"; break;
    case BlockType::StickyPiston:                r.modelName="block/sticky_piston"; break;
    case BlockType::Dropper:                     r.modelName="block/dropper"; break;
    case BlockType::Dispenser:                   r.modelName="block/dispenser"; break;
    case BlockType::Hopper:                      r.modelName="block/hopper"; break;
    case BlockType::TNT:                         r.modelName="block/tnt"; break;
    case BlockType::RedstoneLamp:                r.modelName="block/redstone_lamp"; break;
    case BlockType::IronDoor:                    r.modelName="block/iron_door_bottom"; break;
    case BlockType::IronTrapdoor:                r.modelName="block/iron_trapdoor"; break;
    case BlockType::FenceGate:                   r.modelName="block/oak_fence_gate"; break;
    case BlockType::NoteBlock:                   r.modelName="block/note_block"; break;
    case BlockType::PoweredRail:                 r.modelName="block/powered_rail"; break;
    case BlockType::Stone:                       r.modelName="block/stone"; break;
    case BlockType::Glass:                       r.modelName="block/glass"; break;
    case BlockType::SlabTop:                     r.modelName="block/stone_slab_top"; break;
    case BlockType::SlabBottom:                  r.modelName="block/stone_slab"; break;
    case BlockType::Stair:                       r.modelName="block/stone_stairs"; break;
    default:                                     r.modelName="block/stone"; break;
    }
    return r;
}

static const char* blockstateId(BlockType type)
{
    switch(type){
    case BlockType::Lever:                 return "lever";
    case BlockType::StoneButton:           return "stone_button";
    case BlockType::WoodButton:            return "oak_button";
    case BlockType::TripwireHook:          return "tripwire_hook";
    case BlockType::CalibratedSculkSensor: return "calibrated_sculk_sensor";
    case BlockType::Lectern:               return "lectern";
    case BlockType::Repeater:              return "repeater";
    case BlockType::Comparator:            return "comparator";
    case BlockType::Observer:              return "observer";
    case BlockType::Piston:                return "piston";
    case BlockType::StickyPiston:          return "sticky_piston";
    case BlockType::Dropper:               return "dropper";
    case BlockType::Dispenser:             return "dispenser";
    case BlockType::Hopper:                return "hopper";
    case BlockType::IronDoor:              return "iron_door";
    case BlockType::IronTrapdoor:          return "iron_trapdoor";
    case BlockType::FenceGate:             return "oak_fence_gate";
    case BlockType::PoweredRail:           return "powered_rail";
    case BlockType::Stair:                 return "stone_stairs";
    case BlockType::RedstoneTorch:         return "redstone_torch";
    case BlockType::RedstoneLamp:          return "redstone_lamp";
    case BlockType::StonePressurePlate:    return "stone_pressure_plate";
    case BlockType::WoodPressurePlate:     return "oak_pressure_plate";
    case BlockType::LightWeightedPressurePlate: return "light_weighted_pressure_plate";
    case BlockType::HeavyWeightedPressurePlate: return "heavy_weighted_pressure_plate";
    default:                               return nullptr;
    }
}

BlockStateResult BlockStateLoader::fromJsonWithQuery(const QString        &bsId,
                                                      const BlockStateQuery &query)
{

    QString cacheKey = QString("%1|f=%2|pw=%3%4|lit=%5%6|lk=%7%8|ex=%9%10"
                               "|op=%11%12|dl=%13%14|sh=%15%16|fc=%17%18"
                               "|md=%19%20|hl=%21%22|hg=%23%24")
        .arg(bsId)
        .arg(facingStr(query.facing))
        .arg(query.powered   ? 1:0).arg(query.matchPowered   ? "!" : "")
        .arg(query.lit       ? 1:0).arg(query.matchLit       ? "!" : "")
        .arg(query.locked    ? 1:0).arg(query.matchLocked    ? "!" : "")
        .arg(query.extended  ? 1:0).arg(query.matchExtended  ? "!" : "")
        .arg(query.open      ? 1:0).arg(query.matchOpen      ? "!" : "")
        .arg(query.repeaterDelay).arg(query.matchDelay       ? "!" : "")
        .arg(query.shape).arg(query.matchShape               ? "!" : "")
        .arg(query.face ).arg(query.matchFace                ? "!" : "")
        .arg(query.mode ).arg(query.matchMode                ? "!" : "")
        .arg(query.half ).arg(query.matchHalf                ? "!" : "")
        .arg(query.hinge).arg(query.matchHinge               ? "!" : "");

    if(s_cache.contains(cacheKey)) return s_cache.value(cacheKey);

    BlockStateResult result;
    if(s_dataRoot.isEmpty()) return result;

    QFile f(s_dataRoot + "/blockstates/" + bsId + ".json");
    if(!f.open(QIODevice::ReadOnly)) return result;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if(err.error != QJsonParseError::NoError || !doc.isObject()) return result;

    QJsonObject root = doc.object();
    if(!root.contains("variants")) return result;

    QJsonObject variants = root["variants"].toObject();
    const QString fs = facingStr(query.facing);

    int bestScore = -9999;

    for(auto it = variants.begin(); it != variants.end(); ++it)
    {
        const QString &key = it.key();
        const QHash<QString,QString> props = parseKey(key);

        if(!key.isEmpty()){
            if(props.contains("facing") && props["facing"] != fs)
                continue;
        }

        if(query.matchPowered  && props.contains("powered")
            && props["powered"]  != (query.powered  ? "true":"false")) continue;
        if(query.matchLit      && props.contains("lit")
            && props["lit"]      != (query.lit      ? "true":"false")) continue;
        if(query.matchLocked   && props.contains("locked")
            && props["locked"]   != (query.locked   ? "true":"false")) continue;
        if(query.matchExtended && props.contains("extended")
            && props["extended"] != (query.extended ? "true":"false")) continue;
        if(query.matchOpen     && props.contains("open")
            && props["open"]     != (query.open     ? "true":"false")) continue;
        if(query.matchDelay    && props.contains("delay")
            && props["delay"]    != QString::number(query.repeaterDelay)) continue;
        if(query.matchShape    && props.contains("shape")
            && props["shape"]    != query.shape) continue;
        if(query.matchFace     && props.contains("face")
            && props["face"]     != query.face)  continue;
        if(query.matchMode     && props.contains("mode")
            && props["mode"]     != query.mode)  continue;
        if(query.matchHalf     && props.contains("half")
            && props["half"]     != query.half)  continue;
        if(query.matchHinge    && props.contains("hinge")
            && props["hinge"]    != query.hinge) continue;

        int score = 0;

        auto scoreAttr = [&](const QString &attr, const QString &expected){
            if(!props.contains(attr)) return;
            score += (props[attr] == expected) ? +2 : -1;
        };

        scoreAttr("powered",  query.powered  ? "true":"false");
        scoreAttr("lit",      query.lit      ? "true":"false");
        scoreAttr("locked",   query.locked   ? "true":"false");
        scoreAttr("extended", query.extended ? "true":"false");
        scoreAttr("open",     query.open     ? "true":"false");

        if(props.contains("delay"))
            score += (props["delay"] == QString::number(query.repeaterDelay)) ? +3 : -2;

        if(!query.shape.isEmpty() && props.contains("shape"))
            score += (props["shape"] == query.shape) ? +3 : -2;

        if(!query.face.isEmpty() && props.contains("face"))
            score += (props["face"] == query.face) ? +3 : -2;

        if(!query.mode.isEmpty() && props.contains("mode"))
            score += (props["mode"] == query.mode) ? +3 : -2;

        if(!query.half.isEmpty() && props.contains("half"))
            score += (props["half"] == query.half) ? +2 : -1;

        if(!query.hinge.isEmpty() && props.contains("hinge"))
            score += (props["hinge"] == query.hinge) ? +2 : -1;

        score -= key.count(',');

        if(score <= bestScore) continue;

        QJsonObject entry = it.value().isArray()
            ? it.value().toArray().first().toObject()
            : it.value().toObject();

        QString model = entry["model"].toString();
        if(model.isEmpty()) continue;
        if(model.contains(':')) model = model.section(':', 1);

        result.modelName = model;
        result.rotX = (float)entry["x"].toDouble(0.0);
        result.rotY = (float)entry["y"].toDouble(0.0);
        bestScore = score;
    }

    if(result.isValid())
        s_cache.insert(cacheKey, result);

    return result;
}

BlockStateResult BlockStateLoader::fromJson(const QString &bsId,
                                             BlockFacing    facing,
                                             const QString &shape)
{
    BlockStateQuery q;
    q.facing = facing;
    if(!shape.isEmpty()){
        q.shape      = shape;
        q.matchShape = true;
    }
    return fromJsonWithQuery(bsId, q);
}

BlockStateResult BlockStateLoader::fromJsonWithExtra(const QString &bsId,
                                                      BlockFacing    facing,
                                                      const QString &extraCondition)
{
    BlockStateQuery q;
    q.facing = facing;

    const int eq = extraCondition.indexOf('=');
    if(eq > 0){
        const QString attr = extraCondition.left(eq).trimmed();
        const QString val  = extraCondition.mid(eq+1).trimmed();
        if(attr == "face"){
            q.face      = val;
            q.matchFace = true;
        }
    }
    return fromJsonWithQuery(bsId, q);
}

BlockStateResult BlockStateLoader::getResultWithQuery(const QString        &bsId,
                                                       const BlockStateQuery &query)
{
    BlockStateResult r = fromJsonWithQuery(bsId, query);
    if(r.isValid()) return r;

    BlockStateQuery fallback;
    fallback.facing = query.facing;
    return fromJsonWithQuery(bsId, fallback);
}

BlockStateResult BlockStateLoader::getResult(BlockType type, BlockFacing facing)
{
    if(type == BlockType::Air) return {};

    switch(type){
    case BlockType::TrappedChest: {
        BlockStateResult r = staticResult(type);
        switch(facing){
        case BlockFacing::East:  r.rotY =  90.f; break;
        case BlockFacing::South: r.rotY = 180.f; break;
        case BlockFacing::West:  r.rotY = 270.f; break;
        default:                 r.rotY =   0.f; break;
        }
        return r;
    }

    case BlockType::Lever: {
        BlockStateQuery q;
        q.matchFace = true;
        switch(facing){
        case BlockFacing::Up:
            q.face = "floor";   q.facing = BlockFacing::North; break;
        case BlockFacing::Down:
            q.face = "ceiling"; q.facing = BlockFacing::North; break;
        default:
            q.face = "wall";    q.facing = facing; break;
        }
        BlockStateResult r = fromJsonWithQuery("lever", q);
        if(r.isValid()) return r;
        return staticResult(type);
    }

    case BlockType::StoneButton:
    case BlockType::WoodButton: {
        BlockStateQuery q;
        q.matchFace = true;
        switch(facing){
        case BlockFacing::Up:
            q.face = "floor";   q.facing = BlockFacing::North; break;
        case BlockFacing::Down:
            q.face = "ceiling"; q.facing = BlockFacing::North; break;
        default:
            q.face = "wall";    q.facing = facing; break;
        }
        QString bsName = (type == BlockType::StoneButton) ? "stone_button" : "oak_button";
        BlockStateResult r = fromJsonWithQuery(bsName, q);
        if(r.isValid()) return r;
        return staticResult(type);
    }

    default: break;
    }

    const char *bsId = blockstateId(type);
    if(bsId){
        BlockStateQuery q;
        q.facing = facing;
        BlockStateResult r = fromJsonWithQuery(QString::fromLatin1(bsId), q);
        if(r.isValid()) return r;
    }
    return staticResult(type);
}

BlockStateResult BlockStateLoader::getResultWithShape(const QString &bsId,
                                                       BlockFacing    facing,
                                                       const QString &shape)
{
    return fromJson(bsId, facing, shape);
}
