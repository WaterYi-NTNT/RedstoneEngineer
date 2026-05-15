#include "VoxelRenderer.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLTexture>
#include <QImage>


static const char *VERT_SRC = R"glsl(
#version 330 core
layout(location=0) in vec3  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in vec3  aNormal;
layout(location=3) in float aAO;

uniform mat4 uMVP;

out vec2  vUV;
out float vLight;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));
    float ndotl = abs(dot(normalize(aNormal), lightDir));
    vLight = (0.45 + 0.55 * ndotl) * aAO;
    vUV    = aUV;
}
)glsl";

static const char *FRAG_SRC = R"glsl(
#version 330 core
in vec2  vUV;
in float vLight;

uniform sampler2D uTex;
uniform vec3      uTint;   // ✅ 新增：tint 颜色（默认 1,1,1 = 不染色）

out vec4 fragColor;

void main()
{
    vec4 col = texture(uTex, vUV);
    if(col.a < 0.1) discard;
    // ✅ 将贴图颜色乘以 tint
    fragColor = vec4(col.rgb * uTint * vLight, col.a);
}
)glsl";

static const char *GRID_VERT = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main(){ gl_Position = uMVP * vec4(aPos,1.0); }
)glsl";

static const char *GRID_FRAG = R"glsl(
#version 330 core
out vec4 fragColor;
void main(){ fragColor = vec4(0.4,0.4,0.4,0.5); }
)glsl";

static QSurfaceFormat makeFormat()
{
    QSurfaceFormat fmt;
    fmt.setVersion(3,3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    return fmt;
}

VoxelRenderer::VoxelRenderer(QWidget *parent)
    : QOpenGLWidget(parent){ setFocusPolicy(Qt::StrongFocus); setFormat(makeFormat()); }

VoxelRenderer::VoxelRenderer(VoxelWorld *world, QWidget *parent)
    : QOpenGLWidget(parent), m_world(world){ setFocusPolicy(Qt::StrongFocus); setFormat(makeFormat()); }

VoxelRenderer::~VoxelRenderer()
{
    makeCurrent();
    for(auto *b:m_glBatches){ b->vao.destroy(); b->vbo.destroy(); delete b; }
    qDeleteAll(m_textures);
    delete m_prog; delete m_gridProg;
    m_gridVao.destroy(); m_gridVbo.destroy();
    doneCurrent();
}

void VoxelRenderer::setWorld(VoxelWorld *world){ m_world=world; markDirty(); }
void VoxelRenderer::markDirty(){ m_dirty=true; update(); }

void VoxelRenderer::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.18f,0.20f,0.22f,1.0f);

    m_prog=new QOpenGLShaderProgram(this);
    m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex,   VERT_SRC);
    m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, FRAG_SRC);
    m_prog->link();

    m_gridProg=new QOpenGLShaderProgram(this);
    m_gridProg->addShaderFromSourceCode(QOpenGLShader::Vertex,   GRID_VERT);
    m_gridProg->addShaderFromSourceCode(QOpenGLShader::Fragment, GRID_FRAG);
    m_gridProg->link();

    QVector<float> gv;
    const int R=32;
    for(int i=-R;i<=R;++i){
        gv<<(float)i<<0.f<<(float)-R;
        gv<<(float)i<<0.f<<(float) R;
        gv<<(float)-R<<0.f<<(float)i;
        gv<<(float) R<<0.f<<(float)i;
    }
    m_gridVertCount=gv.size()/3;
    m_gridVao.create(); m_gridVao.bind();
    m_gridVbo.create(); m_gridVbo.bind();
    m_gridVbo.allocate(gv.constData(), gv.size()*sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
    m_gridVao.release();
}

void VoxelRenderer::resizeGL(int w, int h){ glViewport(0,0,w,h); }

void VoxelRenderer::paintGL()
{
    if(m_dirty) rebuildMesh();
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    float aspect=(float)width()/(float)(height()?height():1);
    QMatrix4x4 vp=m_camera.projMatrix(aspect)*m_camera.viewMatrix();

    m_gridProg->bind();
    m_gridProg->setUniformValue("uMVP",vp);
    m_gridVao.bind();
    glDrawArrays(GL_LINES,0,m_gridVertCount);
    m_gridVao.release();
    m_gridProg->release();

    m_prog->bind();
    m_prog->setUniformValue("uMVP",vp);
    m_prog->setUniformValue("uTex",0);

    for(auto *batch:m_glBatches){
        if(batch->count==0) continue;

        m_prog->setUniformValue("uTint",
            batch->tint.x(), batch->tint.y(), batch->tint.z());

        QOpenGLTexture *tex=m_textures.value(batch->texPath,nullptr);
        if(tex) tex->bind(0);
        batch->vao.bind();
        glDrawArrays(GL_TRIANGLES,0,batch->count);
        batch->vao.release();
        if(tex) tex->release();
    }

    m_prog->release();
}

void VoxelRenderer::rebuildMesh()
{
    m_dirty=false;
    if(!m_world) return;

    for(auto *b:m_glBatches){ b->vao.destroy(); b->vbo.destroy(); delete b; }
    m_glBatches.clear();

    m_mesh.rebuild(*m_world);

    for(const auto &mb:m_mesh.batches()){
        loadTexture(mb.texturePath);

        auto *gb=new GLBatch();
        gb->texPath=mb.texturePath;
        gb->count=mb.vertices.size();
        gb->tint=mb.tint;

        gb->vao.create(); gb->vao.bind();
        gb->vbo.create(); gb->vbo.bind();
        gb->vbo.allocate(mb.vertices.constData(),
                         mb.vertices.size()*(int)sizeof(VoxelVertex));

        const int stride=sizeof(VoxelVertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,
            reinterpret_cast<void*>(offsetof(VoxelVertex,x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,stride,
            reinterpret_cast<void*>(offsetof(VoxelVertex,u)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,stride,
            reinterpret_cast<void*>(offsetof(VoxelVertex,nx)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,stride,
            reinterpret_cast<void*>(offsetof(VoxelVertex,ao)));

        gb->vao.release();
        m_glBatches.append(gb);
    }
}

void VoxelRenderer::loadTexture(const QString &path)
{
    if(path.isEmpty()||m_textures.contains(path)) return;
    QImage img(path);
    if(img.isNull()) return;
    auto *tex=new QOpenGLTexture(img.flipped(Qt::Vertical));
    tex->setMinificationFilter(QOpenGLTexture::Nearest);
    tex->setMagnificationFilter(QOpenGLTexture::Nearest);
    tex->setWrapMode(QOpenGLTexture::Repeat);
    m_textures.insert(path,tex);
}

void VoxelRenderer::mousePressEvent(QMouseEvent *e)
{ m_lastMouse=e->pos(); m_mouseButtons=e->buttons(); }

void VoxelRenderer::mouseMoveEvent(QMouseEvent *e)
{
    QPoint delta=e->pos()-m_lastMouse;
    m_lastMouse=e->pos();
    if(e->buttons()&Qt::LeftButton)
        m_camera.orbit((float)delta.x(),(float)delta.y());
    if(e->buttons()&Qt::MiddleButton)
        m_camera.pan((float)delta.x(),(float)delta.y());
    update();
}

void VoxelRenderer::mouseReleaseEvent(QMouseEvent *){}

void VoxelRenderer::wheelEvent(QWheelEvent *e)
{ m_camera.zoom(e->angleDelta().y()/120.f); update(); }
