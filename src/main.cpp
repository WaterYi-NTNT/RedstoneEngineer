#include <QApplication>
#include "MainWindow.h"
#include "BlockModelLoader.h"
#include "BlockStateLoader.h"

int main(int argc, char *argv[])
{
    BlockModelLoader::clearCache();
    QApplication app(argc, argv);
    app.setApplicationName("RedstoneEngineer");

    const QString DATA_ROOT = "E:/Code/RedstoneEngineer/data/1.21.1";
    BlockModelLoader::setDataPath(DATA_ROOT);
    BlockStateLoader::setDataPath(DATA_ROOT);

    MainWindow w;
    w.setWindowTitle("RedstoneEngineer v0.1");
    w.resize(1280, 800);
    w.show();

    return app.exec();
}
