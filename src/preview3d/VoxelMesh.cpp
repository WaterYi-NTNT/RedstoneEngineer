#include "preview3d/VoxelMesh.h"
#include "core/BlockModel.h"
#include "core/BlockModelLoader.h"
#include "core/BlockStateLoader.h"
#include "sim/SimFlags.h"
#include <QtMath>


static const QVector3D FACE_NORMALS[6] = {
    { 0,-1, 0}, { 0, 1, 0},
    { 0, 0,-1}, { 0, 0, 1},
    {-1, 0, 0}, { 1, 0, 0},
};

static float faceAO(int fi)
{
    switch(fi){
    case FACE_UP:    return 1.00f;
    case FACE_NORTH:
    case FACE_SOUTH: return 0.80f;
    case FACE_EAST:
    case FACE_WEST:  return 0.65f;
    case FACE_DOWN:  return 0.50f;
    default:         return 0.75f;
    }
}


static QVector3D rotAround(const QVector3D &v, const QVector3D &o,
                            int axis, float deg)
{
    if(qFuzzyIsNull(deg)) return v;
    QVector3D d = v - o;
    float c = qCos(qDegreesToRadians(deg)), s = qSin(qDegreesToRadians(deg));
    QVector3D r;
    switch(axis){
    case 0: r = {d.x(), d.y()*c - d.z()*s, d.y()*s + d.z()*c}; break;
    case 2: r = {d.x()*c - d.y()*s, d.x()*s + d.y()*c, d.z()};  break;
    default:r = {d.x()*c + d.z()*s, d.y(), -d.x()*s + d.z()*c}; break;
    }
    return o + r;
}

QVector3D VoxelMesh::rotateY(const QVector3D &v, const QVector3D &p, float deg)
{
    if(qFuzzyIsNull(deg)) return v;
    float c = qCos(qDegreesToRadians(deg)), s = qSin(qDegreesToRadians(deg));
    float dx = v.x()-p.x(), dz = v.z()-p.z();
    return {p.x()+dx*c+dz*s, v.y(), p.z()-dx*s+dz*c};
}

QVector3D VoxelMesh::rotateX(const QVector3D &v, const QVector3D &p, float deg)
{
    if(qFuzzyIsNull(deg)) return v;
    float c = qCos(qDegreesToRadians(deg)), s = qSin(qDegreesToRadians(deg));
    float dy = v.y()-p.y(), dz = v.z()-p.z();
    return {v.x(), p.y()+dy*c-dz*s, p.z()+dy*s+dz*c};
}


QVector3D VoxelMesh::dustTint(uint8_t power)
{
    const float t = static_cast<float>(power) / 15.0f;
    return QVector3D(
        0.36f + t * (1.00f - 0.36f),
        0.00f + t * 0.31f,
        0.00f
    );
}


void VoxelMesh::appendFace(int batchIdx, const ModelElement &elem,
                            int fi, const QVector3D &offset,
                            float rotXdeg, float rotYdeg)
{
    const ModelFace &face = elem.faces[fi];
    if(!face.present) return;

    float x0=elem.from[0]/16.f, y0=elem.from[1]/16.f, z0=elem.from[2]/16.f;
    float x1=elem.to[0]  /16.f, y1=elem.to[1]  /16.f, z1=elem.to[2]  /16.f;

    QVector3D c[4];
    switch(fi){
    case FACE_UP:
        c[0]={x0,y1,z0}; c[1]={x1,y1,z0};
        c[2]={x1,y1,z1}; c[3]={x0,y1,z1}; break;
    case FACE_DOWN:
        c[0]={x0,y0,z1}; c[1]={x1,y0,z1};
        c[2]={x1,y0,z0}; c[3]={x0,y0,z0}; break;
    case FACE_NORTH:
        c[0]={x0,y1,z0}; c[1]={x1,y1,z0};
        c[2]={x1,y0,z0}; c[3]={x0,y0,z0}; break;
    case FACE_SOUTH:
        c[0]={x1,y1,z1}; c[1]={x0,y1,z1};
        c[2]={x0,y0,z1}; c[3]={x1,y0,z1}; break;
    case FACE_WEST:
        c[0]={x0,y1,z1}; c[1]={x0,y1,z0};
        c[2]={x0,y0,z0}; c[3]={x0,y0,z1}; break;
    case FACE_EAST:
        c[0]={x1,y1,z0}; c[1]={x1,y1,z1};
        c[2]={x1,y0,z1}; c[3]={x1,y0,z0}; break;
    default: return;
    }

    if(!qFuzzyIsNull(elem.rotAngle)){
        QVector3D org(elem.rotOrigin[0]/16.f,
                      elem.rotOrigin[1]/16.f,
                      elem.rotOrigin[2]/16.f);
        for(auto &v:c) v = rotAround(v, org, elem.rotAxis, elem.rotAngle);
    }

    const QVector3D pivot(0.5f, 0.5f, 0.5f);
    if(!qFuzzyIsNull(rotXdeg)) for(auto &v:c) v = rotateX(v, pivot, rotXdeg);
    if(!qFuzzyIsNull(rotYdeg)) for(auto &v:c) v = rotateY(v, pivot, rotYdeg);
    for(auto &v:c) v += offset;

    float u0f=face.uv[0]/16.f, v0f=1.f-face.uv[1]/16.f;
    float u1f=face.uv[2]/16.f, v1f=1.f-face.uv[3]/16.f;
    QVector2D uvs[4]={{u0f,v0f},{u1f,v0f},{u1f,v1f},{u0f,v1f}};

    if(face.rotation != 0){
        float rad = qDegreesToRadians(-(float)face.rotation);
        float cosR = qCos(rad), sinR = qSin(rad);
        for(auto &uv:uvs){
            float du=uv.x()-0.5f, dv=uv.y()-0.5f;
            uv = {0.5f+du*cosR-dv*sinR, 0.5f+du*sinR+dv*cosR};
        }
    }

    QVector3D n = FACE_NORMALS[fi];
    if(!qFuzzyIsNull(rotXdeg)) n = rotateX(n, {0,0,0}, rotXdeg);
    if(!qFuzzyIsNull(rotYdeg)) n = rotateY(n, {0,0,0}, rotYdeg);
    n = n.normalized();
    float ao = faceAO(fi);

    const int triIdx[6] = {0,1,2,0,2,3};
    for(int idx:triIdx){
        VoxelVertex vtx;
        vtx.x=c[idx].x(); vtx.y=c[idx].y(); vtx.z=c[idx].z();
        vtx.u=uvs[idx].x(); vtx.v=uvs[idx].y();
        vtx.nx=n.x(); vtx.ny=n.y(); vtx.nz=n.z();
        vtx.ao=ao;
        m_batches[batchIdx].vertices.append(vtx);
    }
}


void VoxelMesh::appendElement(QHash<QString,int> &batchMap,
                               const ModelElement &elem,
                               const QVector3D    &blockOffset,
                               float rotXdeg, float rotYdeg,
                               const QVector3D    &tint)
{
    for(int fi = 0; fi < 6; ++fi){
        if(!elem.faces[fi].present) continue;
        const QString &texPath = elem.faces[fi].texturePath;

        QString batchKey = texPath + QStringLiteral("|%1,%2,%3")
            .arg(tint.x(),0,'f',2)
            .arg(tint.y(),0,'f',2)
            .arg(tint.z(),0,'f',2);

        int idx = batchMap.value(batchKey, -1);
        if(idx == -1){
            m_batches.append(MeshBatch{texPath, {}, tint});
            idx = m_batches.size() - 1;
            batchMap.insert(batchKey, idx);
        }
        appendFace(idx, elem, fi, blockOffset, rotXdeg, rotYdeg);
    }
}


void VoxelMesh::appendFlatQuad(QHash<QString,int> &batchMap,
                                const QString      &texPath,
                                float x0, float z0,
                                float x1, float z1,
                                float y,
                                const float        uv[4][2],
                                const QVector3D    &tint)
{
    QString batchKey = texPath + QStringLiteral("|%1,%2,%3")
        .arg(tint.x(),0,'f',2)
        .arg(tint.y(),0,'f',2)
        .arg(tint.z(),0,'f',2);

    int idx = batchMap.value(batchKey, -1);
    if(idx == -1){
        m_batches.append(MeshBatch{texPath, {}, tint});
        idx = m_batches.size() - 1;
        batchMap.insert(batchKey, idx);
    }

    QVector3D pos[4]={
        {x0,y,z0},{x1,y,z0},{x1,y,z1},{x0,y,z1},
    };
    const int triIdx[6]={0,1,2,0,2,3};
    for(int t:triIdx){
        VoxelVertex vtx;
        vtx.x=pos[t].x(); vtx.y=pos[t].y(); vtx.z=pos[t].z();
        vtx.u=uv[t][0];   vtx.v=uv[t][1];
        vtx.nx=0; vtx.ny=1; vtx.nz=0;
        vtx.ao=1.0f;
        m_batches[idx].vertices.append(vtx);
    }
}


bool VoxelMesh::isRedstoneConnectable(const VoxelWorld &w, int x, int y, int z)
{
    if(!w.hasBlock(x,y,z)) return false;
    const Block &b = w.getBlock(x,y,z);
    switch(b.type){
    case BlockType::RedstoneWire:
    case BlockType::RedstoneTorch:
    case BlockType::Repeater:
    case BlockType::Comparator:
    case BlockType::Observer:
        return true;
    default:
        return getBlockMeta(b.type).conductsSolid;
    }
}


void VoxelMesh::appendRedstone(QHash<QString,int> &batchMap,
                                const VoxelWorld   &world,
                                int x, int y, int z,
                                const QVector3D    &offset,
                                uint8_t power)
{
    static QString dotTexPath;
    static QString lineTexPath;
    if(dotTexPath.isEmpty()){
        auto tryLoad = [&](const QString &modelName) -> bool {
            BlockModel m = BlockModelLoader::load(modelName);
            if(!m.isValid || m.elements.isEmpty()) return false;
            for(const auto &elem : m.elements)
                for(const auto &face : elem.faces)
                    if(face.present && !face.texturePath.isEmpty()){
                        int i = face.texturePath.indexOf("/textures/");
                        if(i >= 0){
                            QString root = face.texturePath.left(i);
                            dotTexPath  = root+"/textures/block/redstone_dust_dot.png";
                            lineTexPath = root+"/textures/block/redstone_dust_line0.png";
                            return true;
                        }
                    }
            return false;
        };
        if(!tryLoad("block/redstone_dust_dot"))
            tryLoad("block/stone");
    }
    if(dotTexPath.isEmpty() || lineTexPath.isEmpty()) return;


    const QVector3D tint = dustTint(power);

    bool N = isRedstoneConnectable(world,x,y,z-1);
    bool S = isRedstoneConnectable(world,x,y,z+1);
    bool E = isRedstoneConnectable(world,x+1,y,z);
    bool W = isRedstoneConnectable(world,x-1,y,z);
    bool anyConnect = N||S||E||W;

    float fx=(float)x, fy=(float)y+0.01f, fz=(float)z;
    float cx=fx+0.5f, cz=fz+0.5f;
    const float hw=0.10f, dh=0.15f;

    static const float UV_NS[4][2]={{0,0},{1,0},{1,1},{0,1}};
    static const float UV_EW[4][2]={{0,1},{0,0},{1,0},{1,1}};
    static const float UV_DOT[4][2]={{0,0},{1,0},{1,1},{0,1}};

    if(!anyConnect){
        appendFlatQuad(batchMap, dotTexPath,
                       cx-dh, cz-dh, cx+dh, cz+dh,
                       fy, UV_DOT, tint);
        return;
    }

    if(N) appendFlatQuad(batchMap, lineTexPath,
                         cx-hw, fz,    cx+hw, cz,    fy, UV_NS, tint);
    if(S) appendFlatQuad(batchMap, lineTexPath,
                         cx-hw, cz,    cx+hw, fz+1.f, fy, UV_NS, tint);
    if(E) appendFlatQuad(batchMap, lineTexPath,
                         cx, cz-hw, fx+1.f, cz+hw,   fy, UV_EW, tint);
    if(W) appendFlatQuad(batchMap, lineTexPath,
                         fx, cz-hw, cx,     cz+hw,   fy, UV_EW, tint);

    appendFlatQuad(batchMap, dotTexPath,
                   cx-hw, cz-hw, cx+hw, cz+hw,
                   fy+0.002f, UV_DOT, tint);
}


static void facingDir(BlockFacing f, int &dx, int &dz)
{
    dx=0; dz=0;
    switch(f){
    case BlockFacing::North: dz=-1; break;
    case BlockFacing::South: dz= 1; break;
    case BlockFacing::East:  dx= 1; break;
    case BlockFacing::West:  dx=-1; break;
    default: break;
    }
}

static BlockFacing rotateCW(BlockFacing f)
{
    switch(f){
    case BlockFacing::North: return BlockFacing::East;
    case BlockFacing::East:  return BlockFacing::South;
    case BlockFacing::South: return BlockFacing::West;
    case BlockFacing::West:  return BlockFacing::North;
    default: return f;
    }
}

static BlockFacing rotateCCW(BlockFacing f)
{
    switch(f){
    case BlockFacing::North: return BlockFacing::West;
    case BlockFacing::West:  return BlockFacing::South;
    case BlockFacing::South: return BlockFacing::East;
    case BlockFacing::East:  return BlockFacing::North;
    default: return f;
    }
}

static BlockFacing oppositeFacing(BlockFacing f)
{
    return rotateCW(rotateCW(f));
}

QString VoxelMesh::stairShape(const VoxelWorld &w,
                               int x, int y, int z, BlockFacing f)
{
    if(f==BlockFacing::Up||f==BlockFacing::Down) return "straight";

    int fdx, fdz;
    facingDir(f, fdx, fdz);

    BlockFacing right = rotateCW(f);
    BlockFacing left  = rotateCCW(f);
    BlockFacing opp   = oppositeFacing(f);

    int bx=x-fdx, bz=z-fdz;
    if(w.hasBlock(bx,y,bz)){
        const Block &back = w.getBlock(bx,y,bz);
        if(back.type==BlockType::Stair){
            BlockFacing bf = back.facing;
            if(bf!=f && bf!=opp){
                if(bf==right) return "inner_right";
                if(bf==left)  return "inner_left";
            }
        }
    }

    int ffx=x+fdx, ffz=z+fdz;
    if(w.hasBlock(ffx,y,ffz)){
        const Block &front = w.getBlock(ffx,y,ffz);
        if(front.type==BlockType::Stair){
            BlockFacing ff = front.facing;
            if(ff!=f && ff!=opp){
                if(ff==right) return "outer_right";
                if(ff==left)  return "outer_left";
            }
        }
    }
    return "straight";
}

QString VoxelMesh::railShape(const VoxelWorld &w,
                              int x, int y, int z, BlockFacing f)
{
    auto isRail=[&](int nx,int nz)->bool{
        if(!w.hasBlock(nx,y,nz)) return false;
        return w.getBlock(nx,y,nz).type==BlockType::PoweredRail;
    };
    bool N=isRail(x,z-1), S=isRail(x,z+1);
    bool E=isRail(x+1,z), W=isRail(x-1,z);

    if(N&&E&&!S&&!W) return "corner_north_east";
    if(N&&W&&!S&&!E) return "corner_north_west";
    if(S&&E&&!N&&!W) return "corner_south_east";
    if(S&&W&&!N&&!E) return "corner_south_west";
    if((N||S)&&!E&&!W) return "north_south";
    if((E||W)&&!N&&!S) return "east_west";

    switch(f){
    case BlockFacing::East:
    case BlockFacing::West: return "east_west";
    default:                return "north_south";
    }
}


void VoxelMesh::rebuild(const VoxelWorld &world)
{
    m_batches.clear();
    m_batches.reserve(128);
    QHash<QString,int> batchMap;

    for(const auto &[coord, blk] : world.allBlocks())
    {
        if(blk.isEmpty()) continue;
        int x=coord.x, y=coord.y, z=coord.z;
        QVector3D offset((float)x, (float)y, (float)z);


        if(blk.type == BlockType::RedstoneWire){
            appendRedstone(batchMap, world, x, y, z, offset, blk.power);
            continue;
        }


        if(blk.type == BlockType::Stair){
            QString shape = stairShape(world, x, y, z, blk.facing);
            BlockStateResult bsr = BlockStateLoader::getResultWithShape(
                "stone_stairs", blk.facing, shape);
            if(!bsr.isValid())
                bsr = BlockStateLoader::getResult(BlockType::Stair, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::PoweredRail){
            QString shape = railShape(world, x, y, z, blk.facing);
            BlockStateResult bsr = BlockStateLoader::getResultWithShape(
                "powered_rail", blk.facing, shape);
            if(!bsr.isValid())
                bsr = BlockStateLoader::getResult(BlockType::PoweredRail, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::Repeater){
            const bool active = (blk.flags & SimFlags::ACTIVE) != 0;
            const bool locked = (blk.flags & SimFlags::LOCKED) != 0;
            const int  delay  = SimFlags::getRepeaterDelay(blk.flags) + 1;

            BlockStateQuery q;
            q.facing       = blk.facing;
            q.powered      = active;  q.matchPowered = true;
            q.locked       = locked;  q.matchLocked  = true;
            q.repeaterDelay= delay;   q.matchDelay   = true;

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery("repeater", q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::Comparator){
            const bool active   = (blk.flags & SimFlags::ACTIVE) != 0;
            const bool subtract = SimFlags::isSubtractMode(blk.flags);

            BlockStateQuery q;
            q.facing      = blk.facing;
            q.powered     = active;             q.matchPowered = true;
            q.mode        = subtract ? "subtract" : "compare";
            q.matchMode   = true;

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery("comparator", q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY + 180.f);
            continue;
        }


        if(blk.type == BlockType::RedstoneTorch){
            const bool lit = (blk.flags & SimFlags::ACTIVE) != 0;

            BlockStateQuery q;
            q.facing   = blk.facing;
            q.lit      = lit; q.matchLit = true;

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery(
                "redstone_torch", q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::RedstoneLamp){
            const bool lit = (blk.flags & SimFlags::LIT) != 0;

            BlockStateQuery q;
            q.lit    = lit; q.matchLit = true;

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery(
                "redstone_lamp", q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::Lever){
            const bool active = (blk.flags & SimFlags::ACTIVE) != 0;

            BlockStateQuery q;
            q.powered    = active; q.matchPowered = true;
            q.matchFace  = true;
            switch(blk.facing){
            case BlockFacing::Up:
                q.face = "floor";   q.facing = BlockFacing::North; break;
            case BlockFacing::Down:
                q.face = "ceiling"; q.facing = BlockFacing::North; break;
            default:
                q.face = "wall";    q.facing = blk.facing; break;
            }

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery("lever", q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::StoneButton
        || blk.type == BlockType::WoodButton){
            const bool active = (blk.flags & SimFlags::ACTIVE) != 0;
            const QString bsName = (blk.type == BlockType::StoneButton)
                                 ? "stone_button" : "oak_button";

            BlockStateQuery q;
            q.powered    = active; q.matchPowered = true;
            q.matchFace  = true;
            switch(blk.facing){
            case BlockFacing::Up:
                q.face = "floor";   q.facing = BlockFacing::North; break;
            case BlockFacing::Down:
                q.face = "ceiling"; q.facing = BlockFacing::North; break;
            default:
                q.face = "wall";    q.facing = blk.facing; break;
            }

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery(bsName, q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::StonePressurePlate
        || blk.type == BlockType::WoodPressurePlate
        || blk.type == BlockType::LightWeightedPressurePlate
        || blk.type == BlockType::HeavyWeightedPressurePlate)
        {
            const bool active = (blk.flags & SimFlags::ACTIVE) != 0;

            QString bsName;
            switch(blk.type){
            case BlockType::StonePressurePlate:
                bsName = "stone_pressure_plate"; break;
            case BlockType::WoodPressurePlate:
                bsName = "oak_pressure_plate"; break;
            case BlockType::LightWeightedPressurePlate:
                bsName = "light_weighted_pressure_plate"; break;
            case BlockType::HeavyWeightedPressurePlate:
                bsName = "heavy_weighted_pressure_plate"; break;
            default: break;
            }
            if(bsName.isEmpty()) goto plain;

            BlockStateQuery q;
            q.facing     = blk.facing;
            q.powered    = active; q.matchPowered = true;

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery(bsName, q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::Piston
        || blk.type == BlockType::StickyPiston){
            const bool extended = (blk.flags & SimFlags::LIT) != 0;
            const char *bsId = (blk.type == BlockType::Piston)
                             ? "piston" : "sticky_piston";

            BlockStateQuery q;
            q.facing      = blk.facing;
            q.extended    = extended; q.matchExtended = true;

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery(
                QString::fromLatin1(bsId), q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::IronDoor){
            const bool open = (blk.flags & SimFlags::LIT) != 0;

            BlockStateQuery q;
            q.facing   = blk.facing;
            q.open     = open; q.matchOpen = true;
            q.half     = "lower"; q.matchHalf = true;

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery("iron_door", q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        if(blk.type == BlockType::IronTrapdoor
        || blk.type == BlockType::FenceGate){
            const bool open = (blk.flags & SimFlags::LIT) != 0;
            const char *bsId = (blk.type == BlockType::IronTrapdoor)
                             ? "iron_trapdoor" : "oak_fence_gate";

            BlockStateQuery q;
            q.facing = blk.facing;
            q.open   = open; q.matchOpen = true;

            BlockStateResult bsr = BlockStateLoader::getResultWithQuery(
                QString::fromLatin1(bsId), q);
            if(!bsr.isValid()) bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
            continue;
        }


        plain:
        {
            BlockStateResult bsr = BlockStateLoader::getResult(blk.type, blk.facing);
            if(!bsr.isValid()) continue;
            BlockModel model = BlockModelLoader::load(bsr.modelName);
            if(!model.isValid || model.elements.isEmpty()) continue;
            for(const auto &elem : model.elements)
                appendElement(batchMap, elem, offset, -bsr.rotX, -bsr.rotY);
        }
    }
}
