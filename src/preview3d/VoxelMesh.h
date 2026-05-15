#pragma once
#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <QString>
#include <QHash>

#include "core/BlockModel.h"
#include "core/VoxelWorld.h"

struct VoxelVertex {
    float x, y, z;
    float u, v;
    float nx, ny, nz;
    float ao;
};

struct MeshBatch {
    QString              texturePath;
    QVector<VoxelVertex> vertices;
    QVector3D            tint{1.f, 1.f, 1.f};
};

class VoxelMesh
{
public:
    VoxelMesh() = default;
    void rebuild(const VoxelWorld &world);
    const QVector<MeshBatch> &batches() const { return m_batches; }
    bool isEmpty() const { return m_batches.isEmpty(); }

private:
    QVector<MeshBatch> m_batches;

    void appendElement(QHash<QString,int> &batchMap,
                       const ModelElement &elem,
                       const QVector3D    &blockOffset,
                       float rotXdeg, float rotYdeg,
                       const QVector3D    &tint = {1,1,1});

    void appendFace(int batchIdx, const ModelElement &elem,
                    int faceIdx, const QVector3D &offset,
                    float rotXdeg, float rotYdeg);


    static QVector3D dustTint(uint8_t power);

    void appendRedstone(QHash<QString,int> &batchMap,
                        const VoxelWorld   &world,
                        int x, int y, int z,
                        const QVector3D    &offset,
                        uint8_t power);

    void appendFlatQuad(QHash<QString,int> &batchMap,
                        const QString      &texPath,
                        float x0, float z0,
                        float x1, float z1,
                        float y,
                        const float        uv[4][2],
                        const QVector3D    &tint);

    static bool    isRedstoneConnectable(const VoxelWorld &w, int x, int y, int z);
    static QString stairShape(const VoxelWorld &w, int x, int y, int z, BlockFacing f);
    static QString railShape (const VoxelWorld &w, int x, int y, int z, BlockFacing f);

    static QVector3D rotateY(const QVector3D &v, const QVector3D &pivot, float deg);
    static QVector3D rotateX(const QVector3D &v, const QVector3D &pivot, float deg);
};
