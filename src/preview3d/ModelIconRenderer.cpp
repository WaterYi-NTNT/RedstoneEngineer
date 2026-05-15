#include "ModelIconRenderer.h"
#include <QPainter>
#include <QPolygonF>
#include <QImage>
#include <QTransform>
#include <QtMath>
#include <algorithm>


IconPreRot ModelIconRenderer::suggestPreRot(BlockType type)
{
    switch(type){

    case BlockType::Piston:
    case BlockType::StickyPiston:
        return {90.f, 0.f};

    case BlockType::Dropper:
    case BlockType::Dispenser:
    case BlockType::Observer:
        return {0.f, 180.f};

    case BlockType::Repeater:
    case BlockType::Comparator:
        return {0.f, 180.f};

    case BlockType::Stair:
        return {0.f, 90.f};

    case BlockType::TrappedChest:
        return {0.f, 180.f};


    default:
        return {0.f, 0.f};
    }
}


ModelIconRenderer::Pt2
ModelIconRenderer::project(float mx, float my, float mz,
                            float scale, float cx, float cy)
{
    const float S  = scale / 16.f;
    const float px =  (mx - mz) * S * 0.866f;
    const float py = -(my * S)  + (mx + mz) * S * 0.5f;
    return { cx + px, cy + py };
}


ModelIconRenderer::Pt2
ModelIconRenderer::projectRotated(float mx, float my, float mz,
                                   IconPreRot preRot,
                                   float scale, float cx, float cy)
{
    float x = mx - 8.f;
    float y = my - 8.f;
    float z = mz - 8.f;


    if(!qFuzzyIsNull(preRot.x)){
        float rad = qDegreesToRadians(preRot.x);
        float c = qCos(rad), s = qSin(rad);
        float ny =  y*c - z*s;
        float nz =  y*s + z*c;
        y = ny; z = nz;
    }


    if(!qFuzzyIsNull(preRot.y)){
        float rad = qDegreesToRadians(preRot.y);
        float c = qCos(rad), s = qSin(rad);
        float nx =  x*c + z*s;
        float nz = -x*s + z*c;
        x = nx; z = nz;
    }

    return project(x + 8.f, y + 8.f, z + 8.f, scale, cx, cy);
}


static void applyPreRot(float &x, float &y, float &z, IconPreRot preRot)
{
    x -= 8.f; y -= 8.f; z -= 8.f;

    if(!qFuzzyIsNull(preRot.x)){
        float rad = qDegreesToRadians(preRot.x);
        float c = qCos(rad), s = qSin(rad);
        float ny = y*c - z*s, nz = y*s + z*c;
        y = ny; z = nz;
    }
    if(!qFuzzyIsNull(preRot.y)){
        float rad = qDegreesToRadians(preRot.y);
        float c = qCos(rad), s = qSin(rad);
        float nx = x*c + z*s, nz = -x*s + z*c;
        x = nx; z = nz;
    }

    x += 8.f; y += 8.f; z += 8.f;
}

float ModelIconRenderer::aoFactor(int fi)
{
    switch(fi){
    case FACE_UP:    return 1.00f;
    case FACE_SOUTH:
    case FACE_EAST:  return 0.72f;
    case FACE_NORTH:
    case FACE_WEST:  return 0.58f;
    case FACE_DOWN:  return 0.40f;
    default:         return 0.65f;
    }
}

QImage ModelIconRenderer::loadTexture(const QString &path,
                                       QHash<QString,QImage> &cache)
{
    if(cache.contains(path)) return cache.value(path);
    QImage img; img.load(path);
    if(img.isNull()) img = QImage(16,16,QImage::Format_ARGB32);
    else             img = img.convertToFormat(QImage::Format_ARGB32);
    cache.insert(path, img);
    return img;
}


void ModelIconRenderer::drawFace(QPainter &p,
                                  const ModelElement &elem, int fi,
                                  float scale, float cx, float cy,
                                  const QHash<QString,QImage> &texCache,
                                  IconPreRot preRot)
{
    const ModelFace &face = elem.faces[fi];
    if(!face.present) return;

    float x0=elem.from[0], y0=elem.from[1], z0=elem.from[2];
    float x1=elem.to[0],   y1=elem.to[1],   z1=elem.to[2];

    struct V3{ float x,y,z; };
    V3 corners[4];
    switch(fi){
    case FACE_UP:
        corners[0]={x0,y1,z0}; corners[1]={x1,y1,z0};
        corners[2]={x1,y1,z1}; corners[3]={x0,y1,z1}; break;
    case FACE_DOWN:
        corners[0]={x0,y0,z1}; corners[1]={x1,y0,z1};
        corners[2]={x1,y0,z0}; corners[3]={x0,y0,z0}; break;
    case FACE_NORTH:
        corners[0]={x0,y1,z0}; corners[1]={x1,y1,z0};
        corners[2]={x1,y0,z0}; corners[3]={x0,y0,z0}; break;
    case FACE_SOUTH:
        corners[0]={x1,y1,z1}; corners[1]={x0,y1,z1};
        corners[2]={x0,y0,z1}; corners[3]={x1,y0,z1}; break;
    case FACE_WEST:
        corners[0]={x0,y1,z1}; corners[1]={x0,y1,z0};
        corners[2]={x0,y0,z0}; corners[3]={x0,y0,z1}; break;
    case FACE_EAST:
        corners[0]={x1,y1,z0}; corners[1]={x1,y1,z1};
        corners[2]={x1,y0,z1}; corners[3]={x1,y0,z0}; break;
    default: return;
    }

    QPolygonF poly(4);
    for(int i=0;i<4;++i){
        Pt2 s = projectRotated(corners[i].x, corners[i].y, corners[i].z,
                               preRot, scale, cx, cy);
        poly[i] = QPointF(s.x, s.y);
    }

    float ao = aoFactor(fi);
    const QImage &tex = texCache.value(face.texturePath);

    if(!tex.isNull()){
        float u0f = face.uv[0]/16.f * tex.width();
        float v0f = face.uv[1]/16.f * tex.height();
        float u1f = face.uv[2]/16.f * tex.width();
        float v1f = face.uv[3]/16.f * tex.height();
        int iw = qMax(1,(int)(u1f-u0f));
        int ih = qMax(1,(int)(v1f-v0f));
        QImage subTex = tex.copy((int)u0f,(int)v0f,iw,ih);
        if(subTex.isNull()) subTex = tex;

        float W = subTex.width();
        float H = subTex.height();


        QPolygonF srcRect;
        srcRect << QPointF(0, 0)
                << QPointF(W, 0)
                << QPointF(W, H)
                << QPointF(0, H);


        int faceRot = (face.rotation / 90) % 4;
        if(faceRot < 0) faceRot += 4;
        if(faceRot != 0){

            QPolygonF rotSrc;
            for(int i = 0; i < 4; ++i)
                rotSrc << srcRect[(i + faceRot) % 4];
            srcRect = rotSrc;
        }


        if(!qFuzzyIsNull(preRot.x) && (fi == FACE_EAST || fi == FACE_WEST)){


            int steps = (int)qRound(preRot.x / 90.f) % 4;
            if(steps < 0) steps += 4;

            steps = (4 - steps + 1) % 4;
            if(steps != 0){
                QPolygonF rotSrc;
                for(int i = 0; i < 4; ++i)
                    rotSrc << srcRect[(i + steps) % 4];
                srcRect = rotSrc;
            }
        }

        QTransform t;
        bool ok = QTransform::quadToQuad(srcRect, poly, t);
        if(ok){
            p.save();
            p.setTransform(t, true);
            p.setOpacity(ao);
            p.drawImage(0, 0, subTex);
            p.setOpacity(1.0);
            p.restore();
        }
    } else {
        p.setBrush(QColor(80,60,50,(int)(ao*200)));
        p.setPen(Qt::NoPen);
        p.drawPolygon(poly);
    }

    p.setPen(QPen(QColor(0,0,0,30), 0.4));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(poly);
}


QImage ModelIconRenderer::render(const BlockModel &model, int size, IconPreRot preRot)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    if(!model.isValid || model.elements.isEmpty()) return img;

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing,          true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QHash<QString,QImage> texCache;
    for(const auto &elem : model.elements)
        for(const auto &face : elem.faces)
            if(face.present && !face.texturePath.isEmpty())
                if(!texCache.contains(face.texturePath))
                    texCache.insert(face.texturePath,
                                    loadTexture(face.texturePath, texCache));


    float minX=16,minY=16,minZ=16, maxX=0,maxY=0,maxZ=0;
    for(const auto &elem : model.elements){
        minX=qMin(minX,elem.from[0]); maxX=qMax(maxX,elem.to[0]);
        minY=qMin(minY,elem.from[1]); maxY=qMax(maxY,elem.to[1]);
        minZ=qMin(minZ,elem.from[2]); maxZ=qMax(maxZ,elem.to[2]);
    }
    float spanX=maxX-minX, spanY=maxY-minY, spanZ=maxZ-minZ;
    float projW=(spanX+spanZ)*0.866f;
    float projH= spanY+(spanX+spanZ)*0.5f;
    float span = qMax(projW, projH);
    if(qFuzzyIsNull(span)) span = 16.f;
    const float scale = (float)size * 0.80f / span * 16.f;


    float mcx=(minX+maxX)/2.f, mcy=(minY+maxY)/2.f, mcz=(minZ+maxZ)/2.f;
    applyPreRot(mcx, mcy, mcz, preRot);
    Pt2 centerProj = project(mcx, mcy, mcz, scale, 0, 0);
    float cx = (float)size*0.5f - centerProj.x;
    float cy = (float)size*0.5f - centerProj.y;


    struct FaceEntry {
        const ModelElement *elem;
        int                 fi;
        float               depth;
    };
    QVector<FaceEntry> faceList;
    faceList.reserve(model.elements.size() * 6);

    for(const auto &elem : model.elements){
        for(int fi = 0; fi < 6; ++fi){
            if(!elem.faces[fi].present) continue;


            float fcx = (elem.from[0]+elem.to[0])/2.f;
            float fcy = (elem.from[1]+elem.to[1])/2.f;
            float fcz = (elem.from[2]+elem.to[2])/2.f;


            switch(fi){
            case FACE_UP:    fcy = elem.to[1];   break;
            case FACE_DOWN:  fcy = elem.from[1];  break;
            case FACE_NORTH: fcz = elem.from[2];  break;
            case FACE_SOUTH: fcz = elem.to[2];    break;
            case FACE_WEST:  fcx = elem.from[0];  break;
            case FACE_EAST:  fcx = elem.to[0];    break;
            }


            applyPreRot(fcx, fcy, fcz, preRot);


            float depth = fcx + fcz - fcy * 0.f;

            faceList.append({&elem, fi, depth});
        }
    }


    std::sort(faceList.begin(), faceList.end(),
              [](const FaceEntry &a, const FaceEntry &b){
                  return a.depth < b.depth;
              });


    for(const auto &fe : faceList)
        drawFace(p, *fe.elem, fe.fi, scale, cx, cy, texCache, preRot);

    p.end();
    return img;
}
