#pragma once

#include "core/Block.h"
#include "core/VoxelWorld.h"

#include <QString>
#include <QVector>

struct BlockChange
{
    int   x, y, z;
    Block before;
    Block after;

    bool isNoop() const
    {
        return before.type   == after.type
            && before.facing == after.facing
            && before.flags  == after.flags
            && before.power  == after.power;
    }
};

class EditorCommand
{
public:
    virtual ~EditorCommand() = default;
    virtual void    undo(VoxelWorld &w) = 0;
    virtual void    redo(VoxelWorld &w) = 0;
    virtual bool    isEmpty()     const { return false; }
    virtual QString description() const { return {}; }

protected:

    static void apply(VoxelWorld &w, const BlockChange &c, bool reverse);
};

class PlaceCommand final : public EditorCommand
{
    BlockChange m_c;
public:
    explicit PlaceCommand(BlockChange c) : m_c(std::move(c)) {}

    void    undo(VoxelWorld &w) override { apply(w, m_c, true);  }
    void    redo(VoxelWorld &w) override { apply(w, m_c, false); }
    bool    isEmpty()     const override { return m_c.isNoop(); }
    QString description() const override { return QStringLiteral("单格操作"); }
};

class BatchCommand final : public EditorCommand
{
    QVector<BlockChange> m_changes;
    QString              m_desc;
public:
    explicit BatchCommand(QString desc = {}) : m_desc(std::move(desc)) {}

    void add(BlockChange c)
    {
        if (!c.isNoop())
            m_changes.append(std::move(c));
    }

    bool    isEmpty()     const override { return m_changes.isEmpty(); }
    QString description() const override { return m_desc; }

    void undo(VoxelWorld &w) override
    {
        for (int i = m_changes.size() - 1; i >= 0; --i)
            apply(w, m_changes[i], true);
    }
    void redo(VoxelWorld &w) override
    {
        for (const auto &c : m_changes)
            apply(w, c, false);
    }
};
