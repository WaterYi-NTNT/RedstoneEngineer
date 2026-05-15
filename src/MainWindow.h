#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QLabel>
#include <QSpinBox>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>

#include "core/VoxelWorld.h"
#include "core/Block.h"
#include "palette/BlockPalette.h"
#include "sim/SimEngine.h"
#include "editor2d/GridScene.h"

class GridView;
class VoxelRenderer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLayerChanged    (int y);
    void onGridCoordHovered(int x, int z);
    void onBlockModified   (int x, int y, int z);
    void onTickFinished    (const QVector<VoxelCoord> &changed);
    void onSourceInteracted(int x, int y, int z);
    void updateModeActions (GridScene::EditMode mode);

private:
    
    VoxelWorld *m_world     = nullptr;
    SimEngine  *m_simEngine = nullptr;

    
    BlockPalette  *m_palette       = nullptr;
    GridView      *m_gridView      = nullptr;
    GridScene     *m_gridScene     = nullptr;
    VoxelRenderer *m_voxelRenderer = nullptr;
    QSplitter     *m_splitter      = nullptr;

    
    QToolBar *m_editorToolBar = nullptr;
    QSpinBox *m_layerSpinBox  = nullptr;
    QLabel   *m_brushLabel    = nullptr;

    
    QToolBar *m_modeToolBar     = nullptr;
    QAction  *m_actModePaint    = nullptr;
    QAction  *m_actModeSelect   = nullptr;
    QAction  *m_actModeInteract = nullptr;
    QLabel   *m_modeHintLabel   = nullptr;

    
    QToolBar *m_simToolBar   = nullptr;
    QAction  *m_actSimRun    = nullptr;
    QAction  *m_actSimPause  = nullptr;
    QAction  *m_actSimStep   = nullptr;
    QAction  *m_actSimReset  = nullptr;
    QLabel   *m_simTickLabel = nullptr;
    QSpinBox *m_simSpeedBox  = nullptr;

    
    QLabel *m_statusLayerLabel = nullptr;
    QLabel *m_statusCoordLabel = nullptr;
    QLabel *m_statusBlockLabel = nullptr;
    QLabel *m_statusSimLabel   = nullptr;

    
    QString m_currentFile;          
    bool    m_modified = false;     

    bool saveToFile  (const QString &path);
    bool loadFromFile(const QString &path);
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void setCurrentFile(const QString &path);
    void setModified(bool modified);
    bool confirmDiscard();          

    
    void setupWorld();
    void setupLayout();
    void setupToolBar();
    void setupModeToolBar();
    void setupSimToolBar();
    void setupStatusBar();
    void setupSim();
    void applyDarkTheme();
    void updateSimActions(bool running);

    QFrame *makeSeparator();
};