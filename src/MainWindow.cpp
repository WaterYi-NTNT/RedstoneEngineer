#include "MainWindow.h"
#include "editor2d/GridView.h"
#include "editor2d/GridScene.h"
#include "preview3d/VoxelRenderer.h"
#include "palette/BlockPalette.h"
#include "core/Block.h"
#include "core/VoxelWorld.h"
#include "sim/SimFlags.h"

#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QSplitter>
#include <QToolBar>
#include <QSpinBox>
#include <QAction>
#include <QActionGroup>
#include <QVector>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName("MainWindow");
    setWindowTitle("RedstoneEngineer  v0.1");
    setMinimumSize(1280, 720);
    resize(1440, 860);

    applyDarkTheme();
    setupWorld();
    setupLayout();
    setupToolBar();
    setupModeToolBar();
    setupSimToolBar();
    setupStatusBar();
    setupSim();
}


void MainWindow::setupWorld()
{
    m_world = new VoxelWorld();
}


void MainWindow::setupSim()
{
    m_simEngine = new SimEngine(m_world, this);

    connect(m_gridScene, &GridScene::sourceInteracted,
            this, &MainWindow::onSourceInteracted);

    connect(m_simEngine, &SimEngine::tickFinished,
            this, &MainWindow::onTickFinished);
    connect(m_simEngine, &SimEngine::tickFinished,
            m_gridScene, &GridScene::markSimChanged);


    connect(m_actSimRun, &QAction::triggered, this, [this]() {

        const int tps      = m_simSpeedBox->value();
        const int interval = 1000 / tps;
        m_simEngine->start(interval);
        updateSimActions(true);
        if (m_statusSimLabel)
            m_statusSimLabel->setText(
                QString("● 运行中  %1 t/s").arg(tps));
    });


    connect(m_actSimPause, &QAction::triggered, this, [this]() {
        m_simEngine->stop();
        updateSimActions(false);
        if (m_statusSimLabel) m_statusSimLabel->setText("⏸ 已暂停");
    });


    connect(m_actSimStep, &QAction::triggered, this, [this]() {
        m_simEngine->stop();
        updateSimActions(false);
        m_simEngine->stepOnce();
        if (m_statusSimLabel)
            m_statusSimLabel->setText(
                QString("⏭ 单步  Tick %1").arg(m_simEngine->currentTick()));
    });


    connect(m_actSimReset, &QAction::triggered, this, [this]() {
        m_simEngine->stop();
        m_simEngine->reset();
        updateSimActions(false);
        if (m_statusSimLabel) m_statusSimLabel->setText("■ 已重置");
        if (m_simTickLabel)   m_simTickLabel->setText("Tick: 0");
        if (m_gridScene)      m_gridScene->update();
        if (m_voxelRenderer)  m_voxelRenderer->update();
    });


    connect(m_simSpeedBox, &QSpinBox::valueChanged, this, [this](int tps) {
        if (m_simEngine->isRunning()) {
            m_simEngine->start(1000 / tps);
            if (m_statusSimLabel)
                m_statusSimLabel->setText(
                    QString("● 运行中  %1 t/s").arg(tps));
        }
    });

    updateSimActions(false);
}


void MainWindow::onTickFinished(const QVector<VoxelCoord> &changed)
{

    if (m_simTickLabel)
        m_simTickLabel->setText(
            QString("Tick: %1").arg(m_simEngine->currentTick()));


    if (!changed.isEmpty()) {
        if (m_gridScene)     m_gridScene->update();
        if (m_voxelRenderer) m_voxelRenderer->markDirty();
    }
}


void MainWindow::onSourceInteracted(int x, int y, int z)
{
    if (!m_simEngine || !m_world) return;
    Block *b = m_world->getBlockMutable(x, y, z);
    if (!b) return;

    switch (b->type) {
    case BlockType::Lever:
        m_simEngine->toggleSource(x, y, z);
        break;
    case BlockType::StoneButton:
    case BlockType::WoodButton:
        if (!(b->flags & SimFlags::ACTIVE)) {
            m_simEngine->toggleSource(x, y, z);
            m_simEngine->scheduleSourceOff(x, y, z, 2);
        }
        break;
    case BlockType::StonePressurePlate:
    case BlockType::WoodPressurePlate:
    case BlockType::LightWeightedPressurePlate:
    case BlockType::HeavyWeightedPressurePlate:
        m_simEngine->toggleSource(x, y, z);
        break;
    default:
        break;
    }

    if (m_simEngine->isRunning()) return;


    m_simEngine->refreshStatic();
}


void MainWindow::updateSimActions(bool running)
{
    if (m_actSimRun)   m_actSimRun  ->setEnabled(!running);
    if (m_actSimPause) m_actSimPause->setEnabled( running);
    if (m_actSimStep)  m_actSimStep ->setEnabled(!running);
}


void MainWindow::updateModeActions(GridScene::EditMode mode)
{
    if (m_actModePaint)    m_actModePaint   ->setChecked(mode == GridScene::EditMode::Paint);
    if (m_actModeSelect)   m_actModeSelect  ->setChecked(mode == GridScene::EditMode::Select);
    if (m_actModeInteract) m_actModeInteract->setChecked(mode == GridScene::EditMode::Interact);

    if (!m_modeHintLabel) return;
    switch (mode) {
    case GridScene::EditMode::Paint:
        m_modeHintLabel->setText("  ✏  左键放置  右键擦除  R旋转画笔");    break;
    case GridScene::EditMode::Select:
        m_modeHintLabel->setText("  🖱  左键选中方块  R旋转选中方块");       break;
    case GridScene::EditMode::Interact:
        m_modeHintLabel->setText("  ⚡  左键触发信号源  右键循环中继器延迟"); break;
    }
}


void MainWindow::applyDarkTheme()
{
    const QColor clrBackground  (0x1E, 0x1E, 0x1E);
    const QColor clrSurface     (0x25, 0x25, 0x26);
    const QColor clrTextPrimary (0xCC, 0xCC, 0xCC);
    const QColor clrTextDisabled(0x66, 0x66, 0x66);
    const QColor clrHighlight   (0x26, 0x4F, 0x78);

    QPalette p;
    p.setColor(QPalette::Window,          clrBackground);
    p.setColor(QPalette::WindowText,      clrTextPrimary);
    p.setColor(QPalette::Base,            clrSurface);
    p.setColor(QPalette::AlternateBase,   clrBackground);
    p.setColor(QPalette::Text,            clrTextPrimary);
    p.setColor(QPalette::BrightText,      Qt::white);
    p.setColor(QPalette::ButtonText,      clrTextPrimary);
    p.setColor(QPalette::Disabled, QPalette::Text,       clrTextDisabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, clrTextDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, clrTextDisabled);
    p.setColor(QPalette::Button,          clrSurface);
    p.setColor(QPalette::Highlight,       clrHighlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase,     clrSurface);
    p.setColor(QPalette::ToolTipText,     clrTextPrimary);
    p.setColor(QPalette::Link,            QColor(0xE0, 0x6C, 0x3A));
    qApp->setPalette(p);

    qApp->setStyleSheet(R"(
        QMainWindow { background-color: #1E1E1E; }
        QMenuBar {
            background-color: #252526; color: #CCCCCC;
            border-bottom: 1px solid #3C3C3C; padding: 2px 4px;
        }
        QMenuBar::item:selected { background-color: #264F78; border-radius: 3px; }
        QMenu {
            background-color: #252526; color: #CCCCCC;
            border: 1px solid #3C3C3C;
        }
        QMenu::item:selected { background-color: #264F78; }
        QToolBar {
            background-color: #2D2D2D; border-bottom: 1px solid #3C3C3C;
            spacing: 4px; padding: 2px 6px;
        }
        QToolButton {
            color: #CCCCCC; background-color: transparent;
            border: 1px solid transparent; border-radius: 3px; padding: 3px 8px;
        }
        QToolButton:hover    { background-color: #3C3C3C; border-color: #555555; }
        QToolButton:pressed  { background-color: #264F78; }
        QToolButton:checked  { background-color: #264F78; border-color: #4A7EAA; }
        QToolButton:disabled { color: #555555; }
        QSpinBox {
            background-color: #3C3C3C; color: #CCCCCC;
            border: 1px solid #555555; border-radius: 3px;
            padding: 2px 4px; min-width: 50px;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #4A4A4A; border: none; width: 16px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #E06C3A;
        }
        QSplitter::handle       { background-color: #3C3C3C; width:2px; height:2px; }
        QSplitter::handle:hover { background-color: #E06C3A; }
        QStatusBar { background-color: #007ACC; color: #FFFFFF; font-size: 12px; }
        QStatusBar QLabel { color: #FFFFFF; padding: 0 8px; }
        QToolTip {
            background-color: #252526; color: #CCCCCC;
            border: 1px solid #E06C3A; padding: 4px; font-size: 12px;
        }
    )");
}


QFrame *MainWindow::makeSeparator()
{
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: rgba(255,255,255,0.3);");
    return sep;
}


void MainWindow::setupLayout()
{

    QMenuBar *mb = new QMenuBar(this);
    QMenu *mFile = mb->addMenu("文件(&F)");
    mFile->addAction("新建(&N)");
    mFile->addAction("打开(&O)");
    mFile->addSeparator();
    mFile->addAction("退出(&Q)", qApp, &QApplication::quit);

    QMenu *mView = mb->addMenu("视图(&V)");
    QAction *actResetCam = mView->addAction("重置 3D 视角 (F)");
    actResetCam->setShortcut(QKeySequence(Qt::Key_F));

    mb->addMenu("帮助(&H)")->addAction("关于");
    setMenuBar(mb);


    m_palette = new BlockPalette(this);

    connect(m_palette, &BlockPalette::blockSelected,
            this, [this](BlockType type) {
        if (m_gridScene) m_gridScene->setCurrentBlockType(type);
        if (m_brushLabel && m_gridScene) {
            const auto &meta    = getBlockMeta(type);
            const char *fnames[]= {"北","东","南","西","上","下"};
            m_brushLabel->setText(
                QString("  画笔：%1  朝向：%2")
                    .arg(QString::fromUtf8(meta.displayName))
                    .arg(fnames[static_cast<int>(m_gridScene->currentFacing())]));
        }
    });

    connect(m_palette, &BlockPalette::editModeChanged,
            this, [this](bool isSelect) {
        if (m_gridScene)
            m_gridScene->setEditMode(isSelect ? GridScene::EditMode::Select
                                              : GridScene::EditMode::Paint);
    });


    m_gridScene = new GridScene(m_world, this);
    m_gridView  = new GridView(this);
    m_gridView->setScene(m_gridScene);
    m_gridView->centerOn(0, 0);

    connect(m_gridScene, &GridScene::layerChanged,
            this, &MainWindow::onLayerChanged);
    connect(m_gridScene, &GridScene::blockModified,
            this, &MainWindow::onBlockModified);
    connect(m_gridView,  &GridView::gridCoordHovered,
            this, &MainWindow::onGridCoordHovered);
    connect(m_gridView,  &GridView::rotateRequested,
            m_gridScene, &GridScene::rotateCurrent);

    connect(m_gridScene, &GridScene::facingChanged,
            this, [this](BlockFacing f) {
        if (!m_brushLabel || !m_gridScene) return;
        const char *fnames[] = {"北","东","南","西","上","下"};
        const auto &meta = getBlockMeta(m_gridScene->currentBlockType());
        m_brushLabel->setText(
            QString("  画笔：%1  朝向：%2")
                .arg(QString::fromUtf8(meta.displayName))
                .arg(fnames[static_cast<int>(f)]));
    });

    connect(m_gridScene, &GridScene::selectionChanged,
            this, [this](int x, int , int z, const Block &b) {
        if (!m_statusCoordLabel) return;
        const auto &meta     = getBlockMeta(b.type);
        const char *fnames[] = {"北","东","南","西","上","下"};
        m_statusCoordLabel->setText(
            QString("选中  X=%1 Z=%2  %3  朝向:%4")
                .arg(x).arg(z)
                .arg(QString::fromUtf8(meta.displayName))
                .arg(fnames[static_cast<int>(b.facing)]));
    });


    m_voxelRenderer = new VoxelRenderer(m_world, this);
    m_voxelRenderer->setMinimumWidth(300);

    connect(actResetCam, &QAction::triggered,
            m_voxelRenderer, &VoxelRenderer::resetCamera);

    if (m_world && m_voxelRenderer) {
        m_world->setChangeCallback([this](int x, int y, int z, const Block &block) {
            Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(z); Q_UNUSED(block);
            if (m_voxelRenderer) m_voxelRenderer->markDirty();
        });
    }


    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_palette);
    m_splitter->addWidget(m_gridView);
    m_splitter->addWidget(m_voxelRenderer);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 6);
    m_splitter->setStretchFactor(2, 4);
    m_splitter->setSizes({220, 750, 450});
    m_splitter->setChildrenCollapsible(false);
    setCentralWidget(m_splitter);
}


void MainWindow::setupToolBar()
{
    m_editorToolBar = addToolBar("编辑器工具栏");
    m_editorToolBar->setMovable(false);
    m_editorToolBar->setFloatable(false);
    m_editorToolBar->setObjectName("editorToolBar");

    m_editorToolBar->addWidget(new QLabel("  层级 Y：", this));

    auto *btnDown = new QAction("▼", this);
    auto *btnUp   = new QAction("▲", this);
    btnDown->setToolTip("降低一层 (Y-1)");
    btnUp  ->setToolTip("升高一层 (Y+1)");

    m_layerSpinBox = new QSpinBox(this);
    m_layerSpinBox->setRange(VoxelWorld::LAYER_MIN, VoxelWorld::LAYER_MAX);
    m_layerSpinBox->setValue(0);

    connect(m_layerSpinBox, &QSpinBox::valueChanged, this, [this](int v) {
        if (m_gridScene) m_gridScene->setCurrentLayer(v);
    });
    connect(btnDown, &QAction::triggered, this,
            [this]() { m_layerSpinBox->setValue(m_layerSpinBox->value() - 1); });
    connect(btnUp,   &QAction::triggered, this,
            [this]() { m_layerSpinBox->setValue(m_layerSpinBox->value() + 1); });

    m_editorToolBar->addAction(btnDown);
    m_editorToolBar->addWidget(m_layerSpinBox);
    m_editorToolBar->addAction(btnUp);
    m_editorToolBar->addSeparator();

    auto *btnClear = new QAction("🗑  清空当前层", this);
    connect(btnClear, &QAction::triggered, this, [this]() {
        if (!m_world || !m_gridScene) return;
        const int y = m_gridScene->currentLayer();
        QVector<VoxelCoord> toErase;
        for (const auto &[coord, block] : m_world->allBlocks())
            if (coord.y == y) toErase.append(coord);
        for (const auto &c : toErase)
            m_world->clearBlock(c.x, c.y, c.z);
        m_gridScene->refresh();
    });
    m_editorToolBar->addAction(btnClear);
    m_editorToolBar->addSeparator();

    m_brushLabel = new QLabel("  画笔：石头  朝向：北", this);
    m_brushLabel->setStyleSheet("color: #AAAAAA; font-size: 12px;");
    m_editorToolBar->addWidget(m_brushLabel);
}


void MainWindow::setupModeToolBar()
{
    m_modeToolBar = addToolBar("编辑模式");
    m_modeToolBar->setMovable(false);
    m_modeToolBar->setFloatable(false);
    m_modeToolBar->setObjectName("modeToolBar");

    auto *label = new QLabel("  模式：", this);
    label->setStyleSheet("color:#AAAAAA; font-size:12px;");
    m_modeToolBar->addWidget(label);

    auto *group = new QActionGroup(this);
    group->setExclusive(true);

    m_actModePaint = new QAction("✏  绘制", this);
    m_actModePaint->setToolTip("绘制模式：左键放置，右键擦除  快捷键 Q");
    m_actModePaint->setCheckable(true);
    m_actModePaint->setChecked(true);
    group->addAction(m_actModePaint);
    connect(m_actModePaint, &QAction::triggered, this, [this]() {
        if (m_gridScene) m_gridScene->setEditMode(GridScene::EditMode::Paint);
    });
    m_modeToolBar->addAction(m_actModePaint);

    m_actModeSelect = new QAction("🖱  选择", this);
    m_actModeSelect->setToolTip("选择模式：左键选中，R旋转  快捷键 E");
    m_actModeSelect->setCheckable(true);
    group->addAction(m_actModeSelect);
    connect(m_actModeSelect, &QAction::triggered, this, [this]() {
        if (m_gridScene) m_gridScene->setEditMode(GridScene::EditMode::Select);
    });
    m_modeToolBar->addAction(m_actModeSelect);

    m_actModeInteract = new QAction("⚡  交互", this);
    m_actModeInteract->setToolTip(
        "交互模式：左键触发拉杆/按钮，右键循环中继器延迟  快捷键 R");
    m_actModeInteract->setCheckable(true);
    group->addAction(m_actModeInteract);
    connect(m_actModeInteract, &QAction::triggered, this, [this]() {
        if (m_gridScene) m_gridScene->setEditMode(GridScene::EditMode::Interact);
    });
    m_modeToolBar->addAction(m_actModeInteract);

    m_modeToolBar->addSeparator();

    m_modeHintLabel = new QLabel("  ✏  左键放置  右键擦除  R旋转画笔", this);
    m_modeHintLabel->setStyleSheet("color:#666688; font-size:11px;");
    m_modeToolBar->addWidget(m_modeHintLabel);

    connect(m_gridScene, &GridScene::editModeChanged,
            this, &MainWindow::updateModeActions);
}


void MainWindow::setupSimToolBar()
{
    m_simToolBar = addToolBar("仿真工具栏");
    m_simToolBar->setMovable(false);
    m_simToolBar->setFloatable(false);
    m_simToolBar->setObjectName("simToolBar");

    auto *simLabel = new QLabel("  ⚡ 仿真：", this);
    simLabel->setStyleSheet("color:#E06C3A; font-weight:bold; font-size:12px;");
    m_simToolBar->addWidget(simLabel);

    m_actSimRun = new QAction("▶  运行", this);
    m_actSimRun->setToolTip("开始仿真  Space");
    m_actSimRun->setShortcut(QKeySequence(Qt::Key_Space));
    m_simToolBar->addAction(m_actSimRun);

    m_actSimPause = new QAction("⏸  暂停", this);
    m_actSimPause->setToolTip("暂停仿真  Space");
    m_simToolBar->addAction(m_actSimPause);

    m_actSimStep = new QAction("⏭  单步", this);
    m_actSimStep->setToolTip("手动前进一个 tick  Tab");
    m_actSimStep->setShortcut(QKeySequence(Qt::Key_Tab));
    m_simToolBar->addAction(m_actSimStep);

    m_simToolBar->addSeparator();

    m_actSimReset = new QAction("⏹  重置", this);
    m_actSimReset->setToolTip("停止并清除所有仿真状态");
    m_simToolBar->addAction(m_actSimReset);

    m_simToolBar->addSeparator();


    m_simToolBar->addWidget(new QLabel("  速率：", this));

    m_simSpeedBox = new QSpinBox(this);
    m_simSpeedBox->setRange(1, 20);
    m_simSpeedBox->setValue(4);
    m_simSpeedBox->setSuffix(" t/s");
    m_simSpeedBox->setToolTip("仿真速率（1~20 tick/s）");
    m_simToolBar->addWidget(m_simSpeedBox);

    m_simToolBar->addSeparator();


    m_simTickLabel = new QLabel("  Tick: 0", this);
    m_simTickLabel->setStyleSheet(
        "color:#9CDCFE; font-family:monospace; font-size:12px; min-width:100px;");
    m_simToolBar->addWidget(m_simTickLabel);

    m_simToolBar->addSeparator();

    auto *simHint = new QLabel(
        "  Space 运行/暂停  Tab 单步  Q绘制  E选择  R交互", this);
    simHint->setStyleSheet("color:#666688; font-size:11px;");
    m_simToolBar->addWidget(simHint);
}


void MainWindow::setupStatusBar()
{
    QStatusBar *bar = statusBar();

    m_statusLayerLabel = new QLabel("Layer  Y = 0", this);
    m_statusLayerLabel->setMinimumWidth(120);
    bar->addWidget(m_statusLayerLabel);
    bar->addWidget(makeSeparator());

    m_statusCoordLabel = new QLabel("X = --   Z = --", this);
    m_statusCoordLabel->setMinimumWidth(260);
    bar->addWidget(m_statusCoordLabel);
    bar->addWidget(makeSeparator());

    m_statusBlockLabel = new QLabel("方块数：0", this);
    m_statusBlockLabel->setMinimumWidth(100);
    bar->addWidget(m_statusBlockLabel);
    bar->addWidget(makeSeparator());

    m_statusSimLabel = new QLabel("■ 仿真未启动", this);
    m_statusSimLabel->setMinimumWidth(160);
    bar->addWidget(m_statusSimLabel);

    bar->addPermanentWidget(new QLabel(
        "RedstoneEngineer v0.1  |  Qt " QT_VERSION_STR "  ", this));
}


void MainWindow::onLayerChanged(int y)
{
    if (m_statusLayerLabel)
        m_statusLayerLabel->setText(QString("Layer  Y = %1").arg(y));
    if (m_layerSpinBox && m_layerSpinBox->value() != y) {
        m_layerSpinBox->blockSignals(true);
        m_layerSpinBox->setValue(y);
        m_layerSpinBox->blockSignals(false);
    }
}

void MainWindow::onGridCoordHovered(int x, int z)
{
    if (m_gridScene && m_gridScene->editMode() == GridScene::EditMode::Paint)
        if (m_statusCoordLabel)
            m_statusCoordLabel->setText(
                QString("X = %1   Z = %2").arg(x).arg(z));
}

void MainWindow::onBlockModified(int x, int y, int z)
{
    if (m_statusBlockLabel && m_world)
        m_statusBlockLabel->setText(
            QString("方块数：%1").arg(m_world->blockCount()));
    if (m_simEngine)
        m_simEngine->notifyBlockChanged(x, y, z);
}
