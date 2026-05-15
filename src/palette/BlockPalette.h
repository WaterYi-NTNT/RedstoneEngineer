#pragma once
#include "core/Block.h"
#include <QWidget>
#include <QList>

class QVBoxLayout;
class PaletteGroup;
class PaletteItem;

class BlockPalette : public QWidget
{
    Q_OBJECT
public:
    static constexpr int PALETTE_WIDTH = 220;

    explicit BlockPalette(QWidget *parent = nullptr);

    BlockType selectedType() const { return m_selectedType; }

signals:
    void blockSelected(BlockType type);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupGroups();
    void relayoutAll();
    void onItemClicked(BlockType type, PaletteItem *item);

    QVBoxLayout        *m_contentLayout = nullptr;
    QList<PaletteGroup*> m_groups;
    PaletteItem        *m_selectedItem  = nullptr;
    BlockType           m_selectedType  = BlockType::Stone;
};