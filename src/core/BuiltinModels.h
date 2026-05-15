#pragma once
#include "BlockModel.h"
#include <QString>

class BuiltinModels {
public:
    static bool       isBuiltin(const QString &modelName);
    static BlockModel make(const QString &modelName);
};
