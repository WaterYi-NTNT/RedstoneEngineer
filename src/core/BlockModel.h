#pragma once
#include <QString>
#include <QVector>
#include <QHash>

enum FaceIndex { FACE_DOWN=0, FACE_UP, FACE_NORTH, FACE_SOUTH, FACE_WEST, FACE_EAST };

struct ModelFace {
    bool    present     = false;
    float   uv[4]       = {0,0,16,16};
    QString texturePath;
    int     rotation    = 0;
    int     tintindex   = -1;
    bool    cullface    = false;
};

struct ModelElement {
    float from[3]       = {0,0,0};
    float to[3]         = {16,16,16};
    ModelFace faces[6];

    float rotOrigin[3]  = {8,8,8};
    int   rotAxis       = 1;
    float rotAngle      = 0.f;
    bool  rescale       = false;
};

struct BlockModel {
    QVector<ModelElement> elements;
    bool ambientOcclusion = true;
    bool isValid          = false;
};
