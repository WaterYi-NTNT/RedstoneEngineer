#pragma once

#include "EditorCommand.h"

#include <QObject>
#include <deque>
#include <memory>

class CommandHistory : public QObject
{
    Q_OBJECT

    static constexpr int MAX_STEPS = 64;

    VoxelWorld *m_world;

    std::deque<std::unique_ptr<EditorCommand>> m_undoStack;
    std::deque<std::unique_ptr<EditorCommand>> m_redoStack;

public:
    explicit CommandHistory(VoxelWorld *world, QObject *parent = nullptr);

    void push(std::unique_ptr<EditorCommand> cmd);

    void undo();
    void redo();

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    void clear();

signals:

    void historyChanged(bool canUndo, bool canRedo);
};
