#pragma once
#include <QWidget>
#include <QVector>
#include "Block.h"

class QVBoxLayout;
class QScrollArea;
class QToolButton;
class PaletteGroup;
class PaletteItem;

class BlockPalette : public QWidget
{
    Q_OBJECT
public:
    explicit BlockPalette(QWidget *parent = nullptr);


    static constexpr int PALETTE_WIDTH = 280;

    BlockType selectedType() const { return m_selectedType; }

signals:
    void blockSelected(BlockType type);
    void editModeChanged(bool isSelectMode);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupTopBar();
    void setupGroups();
    void onItemClicked(BlockType type, PaletteItem *item);
    void relayoutAll();

    QVBoxLayout           *m_contentLayout = nullptr;
    QVector<PaletteGroup*> m_groups;

    BlockType    m_selectedType = BlockType::Stone;
    PaletteItem *m_selectedItem = nullptr;

    QToolButton *m_btnPaint  = nullptr;
    QToolButton *m_btnSelect = nullptr;
};
