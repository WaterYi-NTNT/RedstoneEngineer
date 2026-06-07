#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include "MainWindow.h"
#include "BlockModelLoader.h"
#include "BlockStateLoader.h"

int main(int argc, char *argv[])
{
    BlockModelLoader::clearCache();
    QApplication app(argc, argv);
    app.setApplicationName("RedstoneEngineer");

    const QStringList candidates = {
        QApplication::applicationDirPath() + "/data/1.21.1",
        QApplication::applicationDirPath() + "/data",
#ifdef QT_DEBUG
        "E:/Code/RedstoneEngineer/data/1.21.1",
#endif
    };

    QString dataRoot;
    for (const QString &path : candidates) {
        if (QDir(path).exists()) {
            dataRoot = path;
            break;
        }
    }

    if (dataRoot.isEmpty()) {
        QMessageBox::critical(
            nullptr,
            "数据目录缺失",
            "找不到 Minecraft 数据目录（data/1.21.1）。\n"
            "请将 data 文件夹放在程序同级目录下。");
        return 1;
    }

    BlockModelLoader::setDataPath(dataRoot);
    BlockStateLoader::setDataPath(dataRoot);

    MainWindow w;
    w.show();

    return app.exec();
}
