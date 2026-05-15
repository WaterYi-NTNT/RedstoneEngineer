#include "BlockModelLoader.h"
#include "BuiltinModels.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

QString                    BlockModelLoader::s_dataRoot;
QHash<QString,BlockModel>  BlockModelLoader::s_cache;


void BlockModelLoader::setDataPath(const QString &root)
{
    s_dataRoot = root;
    s_cache.clear();
}

void BlockModelLoader::clearCache() { s_cache.clear(); }

BlockModel BlockModelLoader::load(const QString &name)
{
    if (name.isEmpty()) return {};
    if (s_cache.contains(name)) return s_cache.value(name);


    if (BuiltinModels::isBuiltin(name)) {
        BlockModel bm = BuiltinModels::make(name);
        s_cache.insert(name, bm);
        return bm;
    }

    RawModel raw = loadRaw(name);


    for (int depth = 0; depth < 16 && !raw.parent.isEmpty(); ++depth) {
        QString parentName = raw.parent;
        raw.parent.clear();


        if (parentName.startsWith("minecraft:builtin/") ||
            parentName.startsWith("builtin/"))
            break;


        if (parentName.contains(':'))
            parentName = parentName.section(':', 1);

        RawModel parentRaw = loadRaw(parentName);
        raw = mergeWithParent(raw, parentRaw);


        if (raw.hasElements) break;
    }


    resolveTextures(raw);

    BlockModel bm;
    bm.ambientOcclusion = raw.ambientOcclusion;
    bm.elements         = raw.elements;
    bm.isValid          = !raw.elements.isEmpty();

    s_cache.insert(name, bm);
    return bm;
}


BlockModelLoader::RawModel BlockModelLoader::loadRaw(const QString &name)
{
    RawModel raw;
    if (name.isEmpty()) return raw;

    QString path = s_dataRoot + "/models/" + name + ".json";

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return raw;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return raw;

    QJsonObject root = doc.object();

    if (root.contains("parent"))
        raw.parent = root["parent"].toString();

    raw.ambientOcclusion = root["ambientocclusion"].toBool(true);

    if (root.contains("textures")) {
        QJsonObject texObj = root["textures"].toObject();
        for (auto it = texObj.begin(); it != texObj.end(); ++it)
            raw.textures.insert(it.key(), it.value().toString());
    }

    if (root.contains("elements")) {
        raw.hasElements = true;
        for (auto &&ev : root["elements"].toArray())
            raw.elements.append(parseElement(ev.toObject(), raw.textures));
    }

    return raw;
}


BlockModelLoader::RawModel
BlockModelLoader::mergeWithParent(const RawModel &child, const RawModel &parent)
{
    RawModel merged;
    merged.parent           = parent.parent;
    merged.ambientOcclusion = child.ambientOcclusion;

    merged.textures = parent.textures;
    for (auto it = child.textures.begin(); it != child.textures.end(); ++it)
        merged.textures.insert(it.key(), it.value());

    if (child.hasElements) {
        merged.elements    = child.elements;
        merged.hasElements = true;
    } else {
        merged.elements    = parent.elements;
        merged.hasElements = parent.hasElements;
    }

    return merged;
}


void BlockModelLoader::resolveTextures(RawModel &m)
{
    for (int pass = 0; pass < 8; ++pass) {
        bool changed = false;
        for (auto it = m.textures.begin(); it != m.textures.end(); ++it) {
            if (it.value().startsWith('#')) {
                QString key = it.value().mid(1);
                if (m.textures.contains(key)) {
                    it.value() = m.textures.value(key);
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }

    for (auto &elem : m.elements) {
        for (auto &face : elem.faces) {
            if (!face.present) continue;
            QString &tp = face.texturePath;

            if (tp.startsWith('#')) {
                tp = m.textures.value(tp.mid(1), tp);
            }

            if (tp.startsWith('#')) {
                tp = m.textures.value(tp.mid(1), tp);
            }

            if (tp.contains(':'))
                tp = tp.section(':', 1);

            if (!tp.isEmpty()
                && !tp.startsWith('/')
                && !tp.contains(":/")
                && !tp.startsWith('#'))
            {
                tp = s_dataRoot + "/textures/" + tp + ".png";
            }
        }
    }
}


ModelElement BlockModelLoader::parseElement(const QJsonObject         &obj,
                                             const QHash<QString,QString> &texMap)
{
    ModelElement e;

    auto readVec3 = [](const QJsonArray &a, float *out) {
        if (a.size() >= 3) {
            out[0] = (float)a[0].toDouble();
            out[1] = (float)a[1].toDouble();
            out[2] = (float)a[2].toDouble();
        }
    };

    readVec3(obj["from"].toArray(), e.from);
    readVec3(obj["to"].toArray(),   e.to);

    if (obj.contains("rotation")) {
        QJsonObject rot = obj["rotation"].toObject();
        readVec3(rot["origin"].toArray(), e.rotOrigin);
        QString axis = rot["axis"].toString("y").toLower();
        e.rotAxis  = (axis == "x") ? 0 : (axis == "z") ? 2 : 1;
        e.rotAngle = (float)rot["angle"].toDouble(0.0);
        e.rescale  = rot["rescale"].toBool(false);
    }

    static const char *FACE_KEYS[6] = {
        "down","up","north","south","west","east"
    };
    if (obj.contains("faces")) {
        QJsonObject faces = obj["faces"].toObject();
        for (int i = 0; i < 6; ++i) {
            if (faces.contains(FACE_KEYS[i])) {
                e.faces[i] = parseFace(faces[FACE_KEYS[i]].toObject(), texMap);
                e.faces[i].present = true;
            }
        }
    }

    return e;
}


ModelFace BlockModelLoader::parseFace(const QJsonObject         &obj,
                                       const QHash<QString,QString> &texMap)
{
    ModelFace f;
    f.present = true;

    QString texVar = obj["texture"].toString();
    if (texVar.startsWith('#')) {
        f.texturePath = texMap.value(texVar.mid(1), texVar);
    } else {
        f.texturePath = texVar;
    }

    if (obj.contains("uv")) {
        QJsonArray uv = obj["uv"].toArray();
        if (uv.size() == 4)
            for (int i = 0; i < 4; ++i)
                f.uv[i] = (float)uv[i].toDouble();
    }

    f.rotation  = obj["rotation"].toInt(0);
    f.tintindex = obj["tintindex"].toInt(-1);
    f.cullface  = obj.contains("cullface");

    return f;
}


QString BlockModelLoader::resolveTexturePath(const QString             &nameOrRef,
                                              const QHash<QString,QString> &texMap)
{
    QString v = nameOrRef;
    if (v.startsWith('#')) v = texMap.value(v.mid(1), v);
    if (v.contains(':'))   v = v.section(':', 1);
    if (!v.isEmpty() && !v.startsWith('/') && !v.contains(":/"))
        v = s_dataRoot + "/textures/" + v + ".png";
    return v;
}
