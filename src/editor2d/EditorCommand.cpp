#include "EditorCommand.h"

void EditorCommand::apply(VoxelWorld &w, const BlockChange &c, bool reverse)
{
    const Block &target = reverse ? c.before : c.after;

    if (target.isEmpty())
        w.clearBlock(c.x, c.y, c.z);
    else
        w.setBlock(c.x, c.y, c.z, target);
}
