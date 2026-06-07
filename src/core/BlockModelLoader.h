#pragma once
#include "BlockModel.h"
#include <QString>
#include <QHash>
#include <QJsonObject>

class BlockModelLoader
{
public:
    static void        setDataPath(const QString &root);
    static BlockModel  load(const QString &name);
    static QString     resolveTexturePath(const QString &nameOrRef,
                                          const QHash<QString,QString> &texMap);
    static void        clearCache();

private:
    struct RawModel {
        QString              parent;
        QHash<QString,QString> textures;
        QVector<ModelElement> elements;
        bool hasElements = false;
        bool ambientOcclusion = true;
    };

    static RawModel   loadRaw(const QString &name);
    static RawModel   mergeWithParent(const RawModel &child, const RawModel &parent);
    static void       resolveTextures(RawModel &m);
    static ModelElement parseElement(const QJsonObject &obj,
                                     const QHash<QString,QString> &texMap);
    static ModelFace    parseFace(const QJsonObject &obj,
                                  const QHash<QString,QString> &texMap);

    static QString                    s_dataRoot;
    static QHash<QString, BlockModel> s_cache;
};
