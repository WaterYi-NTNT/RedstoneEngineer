#include "BuiltinModels.h"
#include "BlockModelLoader.h"

static QString chestTexturePath(const QString &fileName)
{
    static QString rootPrefix;
    if(rootPrefix.isEmpty()){

        BlockModel ref = BlockModelLoader::load("block/stone");
        if(ref.isValid && !ref.elements.isEmpty()){
            for(const auto &elem : ref.elements){
                for(const auto &face : elem.faces){
                    if(face.present && !face.texturePath.isEmpty()){
                        QString tp = face.texturePath;
                        int i = tp.indexOf("/textures/");
                        if(i >= 0){
                            rootPrefix = tp.left(i);
                        }
                        break;
                    }
                }
                if(!rootPrefix.isEmpty()) break;
            }
        }
    }
    if(rootPrefix.isEmpty()) return QString();
    return rootPrefix + "/textures/entity/chest/" + fileName;
}

static inline void setFacePx(ModelFace &f, const QString &tex,
                             float pxU0, float pxV0,
                             float pxU1, float pxV1,
                             float texSize = 64.f)
{
    f.present     = true;
    f.texturePath = tex;
    f.uv[0] = pxU0 * 16.f / texSize;
    f.uv[1] = pxV0 * 16.f / texSize;
    f.uv[2] = pxU1 * 16.f / texSize;
    f.uv[3] = pxV1 * 16.f / texSize;
    f.rotation = 0;
}

static BlockModel makeChest(const QString &textureFile)
{
    BlockModel m;
    QString tex = chestTexturePath(textureFile);
    if(tex.isEmpty()){
        m.isValid = false;
        return m;
    }

    m.isValid = true;
    m.elements.resize(3);

    {
        auto &e = m.elements[0];
        e.from[0]=1;  e.from[1]=0;  e.from[2]=1;
        e.to[0]  =15; e.to[1]  =10; e.to[2]  =15;
        e.rotAngle = 0; e.rotAxis = 1;
        e.rotOrigin[0]=8; e.rotOrigin[1]=8; e.rotOrigin[2]=8;

        setFacePx(e.faces[FACE_DOWN],    tex, 14, 19, 28, 33);
        setFacePx(e.faces[FACE_UP],  tex, 28, 19, 42, 33);
        setFacePx(e.faces[FACE_SOUTH], tex, 14, 33, 28, 43);
        setFacePx(e.faces[FACE_NORTH], tex, 56, 43, 42, 33);
        setFacePx(e.faces[FACE_WEST],  tex,  0, 33, 14, 43);
        setFacePx(e.faces[FACE_EAST],  tex, 28, 33, 42, 43);

    }

    {
        auto &e = m.elements[1];
        e.from[0]=1;  e.from[1]=10; e.from[2]=1;
        e.to[0]  =15; e.to[1]  =14; e.to[2]  =15;
        e.rotAngle = 0; e.rotAxis = 1;
        e.rotOrigin[0]=8; e.rotOrigin[1]=8; e.rotOrigin[2]=8;

        setFacePx(e.faces[FACE_DOWN],    tex, 14,  0, 28, 14);
        setFacePx(e.faces[FACE_UP],  tex, 28, 0, 42, 14);
        setFacePx(e.faces[FACE_SOUTH], tex, 14, 14, 28, 19);
        setFacePx(e.faces[FACE_NORTH], tex, 56, 19, 42, 14);
        setFacePx(e.faces[FACE_WEST],  tex,  0, 14, 14, 19);
        setFacePx(e.faces[FACE_EAST],  tex, 28, 14, 42, 19);

    }

    {
        auto &e = m.elements[2];
        e.from[0]=7;  e.from[1]=7;  e.from[2]=0;
        e.to[0]  =9;  e.to[1]  =11; e.to[2]  =1;
        e.rotAngle = 0; e.rotAxis = 1;
        e.rotOrigin[0]=8; e.rotOrigin[1]=8; e.rotOrigin[2]=8;

        setFacePx(e.faces[FACE_NORTH], tex, 1, 1, 3, 5);
        setFacePx(e.faces[FACE_SOUTH], tex, 5, 1, 7, 5);
        setFacePx(e.faces[FACE_WEST],  tex, 0, 1, 1, 5);
        setFacePx(e.faces[FACE_EAST],  tex, 3, 1, 4, 5);
        setFacePx(e.faces[FACE_UP],    tex, 1, 0, 3, 1);
        setFacePx(e.faces[FACE_DOWN],  tex, 3, 0, 5, 1);
    }

    return m;
}

bool BuiltinModels::isBuiltin(const QString &modelName)
{
    return modelName.startsWith("@builtin/");
}

BlockModel BuiltinModels::make(const QString &modelName)
{
    if(modelName == "@builtin/chest")          return makeChest("normal.png");
    if(modelName == "@builtin/trapped_chest")  return makeChest("trapped.png");
    if(modelName == "@builtin/ender_chest")    return makeChest("ender.png");

    BlockModel empty;
    empty.isValid = false;
    return empty;
}
