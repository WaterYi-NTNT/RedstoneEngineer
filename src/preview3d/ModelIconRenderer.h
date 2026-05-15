#pragma once
#include "BlockModel.h"
#include "Block.h"
#include <QImage>
#include <QHash>


struct IconPreRot {
    float x = 0.f;
    float y = 0.f;
};

class ModelIconRenderer
{
public:
    struct Pt2 { float x, y; };

    static QImage      render       (const BlockModel &model, int size,
                                     IconPreRot preRot = IconPreRot());
    static IconPreRot  suggestPreRot(BlockType type);

private:
    static Pt2   project(float mx, float my, float mz,
                         float scale, float cx, float cy);
    static float aoFactor(int faceIdx);
    static QImage loadTexture(const QString &path,
                              QHash<QString,QImage> &cache);
    static void  drawFace(class QPainter &p,
                          const ModelElement &elem, int faceIdx,
                          float scale, float cx, float cy,
                          const QHash<QString,QImage> &texCache,
                          IconPreRot preRot);
    static Pt2   projectRotated(float mx, float my, float mz,
                                IconPreRot preRot,
                                float scale, float cx, float cy);
};
