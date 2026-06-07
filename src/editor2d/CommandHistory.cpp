#include "CommandHistory.h"

CommandHistory::CommandHistory(VoxelWorld *world, QObject *parent)
    : QObject(parent)
    , m_world(world)
{}

void CommandHistory::push(std::unique_ptr<EditorCommand> cmd)
{
    if (!cmd || cmd->isEmpty())
        return;

    m_redoStack.clear();

    m_undoStack.push_back(std::move(cmd));

    if (static_cast<int>(m_undoStack.size()) > MAX_STEPS)
        m_undoStack.pop_front();

    emit historyChanged(canUndo(), canRedo());
}

void CommandHistory::undo()
{
    if (m_undoStack.empty()) return;

    auto &cmd = m_undoStack.back();
    cmd->undo(*m_world);
    m_redoStack.push_back(std::move(cmd));
    m_undoStack.pop_back();

    emit historyChanged(canUndo(), canRedo());
}

void CommandHistory::redo()
{
    if (m_redoStack.empty()) return;

    auto &cmd = m_redoStack.back();
    cmd->redo(*m_world);
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.pop_back();

    emit historyChanged(canUndo(), canRedo());
}

void CommandHistory::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit historyChanged(false, false);
}
