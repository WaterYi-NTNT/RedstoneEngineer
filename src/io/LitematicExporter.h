#pragma once
#include "core/VoxelWorld.h"
#include <QString>

class LitematicExporter
{
public:
    struct Result {
        bool    success = false;
        QString errorMessage;
    };

    static Result exportToFile(const VoxelWorld &world,
                               const QString    &filePath,
                               const QString    &name = {});
};
