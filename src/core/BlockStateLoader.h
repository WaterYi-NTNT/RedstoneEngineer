#pragma once
#include "Block.h"
#include <QString>
#include <QHash>

struct BlockStateResult {
    QString modelName;
    float   rotX = 0.f;
    float   rotY = 0.f;
    bool    isValid() const { return !modelName.isEmpty(); }
};

struct BlockStateQuery {
    BlockFacing facing       = BlockFacing::North;

    bool powered   = false;
    bool lit       = false;
    bool locked    = false;
    bool extended  = false;
    bool open      = false;

    int  repeaterDelay = 1;

    QString shape;
    QString face;
    QString mode;
    QString half;
    QString hinge;

    bool matchPowered   = false;
    bool matchLit       = false;
    bool matchLocked    = false;
    bool matchExtended  = false;
    bool matchOpen      = false;
    bool matchDelay     = false;
    bool matchShape     = false;
    bool matchFace      = false;
    bool matchMode      = false;
    bool matchHalf      = false;
    bool matchHinge     = false;
};

class BlockStateLoader
{
public:
    static void setDataPath(const QString &root);

    static BlockStateResult getResult(BlockType   type,
                                      BlockFacing facing = BlockFacing::North);

    static BlockStateResult getResultWithQuery(const QString        &bsId,
                                               const BlockStateQuery &query);

    static BlockStateResult getResultWithShape(const QString &bsId,
                                               BlockFacing    facing,
                                               const QString &shape);

    static QString getModelName(BlockType   type,
                                BlockFacing facing = BlockFacing::North)
    {
        return getResult(type, facing).modelName;
    }

private:

    static QHash<QString,QString> parseKey(const QString &key);

    static BlockStateResult fromJsonWithQuery(const QString        &bsId,
                                              const BlockStateQuery &query);

    static BlockStateResult fromJson         (const QString &bsId,
                                              BlockFacing    facing,
                                              const QString &shape = QString());
    static BlockStateResult fromJsonWithExtra(const QString &bsId,
                                              BlockFacing    facing,
                                              const QString &extraCondition);

    static BlockStateResult staticResult(BlockType type);
    static QString          facingStr   (BlockFacing f);

    static QString                          s_dataRoot;
    static QHash<QString, BlockStateResult> s_cache;
};
