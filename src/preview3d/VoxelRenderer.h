#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>
#include <QHash>
#include <QVector3D>

#include "Camera.h"
#include "VoxelMesh.h"
#include "VoxelWorld.h"

class VoxelRenderer : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit VoxelRenderer(QWidget *parent = nullptr);
    explicit VoxelRenderer(VoxelWorld *world, QWidget *parent = nullptr);
    ~VoxelRenderer() override;

    void setWorld(VoxelWorld *world);
    void markDirty();
    Camera &camera(){ return m_camera; }

public slots:
    void resetCamera(){ m_camera.reset(); update(); }

protected:
    void initializeGL()  override;
    void resizeGL(int,int) override;
    void paintGL()       override;
    void mousePressEvent  (QMouseEvent *) override;
    void mouseMoveEvent   (QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent       (QWheelEvent *) override;

private:
    void rebuildMesh();
    void loadTexture(const QString &path);

    VoxelWorld           *m_world    = nullptr;
    bool                  m_dirty    = true;
    Camera                m_camera;
    VoxelMesh             m_mesh;

    QOpenGLShaderProgram *m_prog     = nullptr;
    QOpenGLShaderProgram *m_gridProg = nullptr;

    struct GLBatch {
        QOpenGLVertexArrayObject vao;
        QOpenGLBuffer            vbo{QOpenGLBuffer::VertexBuffer};
        int                      count = 0;
        QString                  texPath;
        QVector3D                tint{1,1,1};
    };
    QVector<GLBatch*>               m_glBatches;
    QHash<QString,QOpenGLTexture*>  m_textures;

    QOpenGLVertexArrayObject m_gridVao;
    QOpenGLBuffer            m_gridVbo{QOpenGLBuffer::VertexBuffer};
    int                      m_gridVertCount=0;

    QPoint           m_lastMouse;
    Qt::MouseButtons m_mouseButtons;
};
