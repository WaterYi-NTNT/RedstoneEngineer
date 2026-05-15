#include "BlockPalette.h"
#include "PaletteGroup.h"
#include "PaletteItem.h"

#include <QVBoxLayout>
#include <QScrollArea>
#include <QResizeEvent>

BlockPalette::BlockPalette(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(PALETTE_WIDTH);
    setObjectName("BlockPalette");
    setStyleSheet("QWidget#BlockPalette { background-color: #252526; }");

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(R"(
        QScrollArea        { background: #252526; border: none; }
        QScrollBar:vertical{
            background: #2D2D2D; width: 6px; margin: 0;
        }
        QScrollBar::handle:vertical{
            background: #555555; border-radius: 3px; min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");

    QWidget *content = new QWidget();
    content->setStyleSheet("background: #252526;");
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(0, 4, 0, 8);
    m_contentLayout->setSpacing(0);
    m_contentLayout->addStretch(1);

    scroll->setWidget(content);
    rootLayout->addWidget(scroll, 1);

    setupGroups();
}

void BlockPalette::setupGroups()
{
    struct GroupDef { BlockGroup group; const char *title; };
    static const GroupDef DEFS[] = {
        { BlockGroup::SignalSource, "信号源"   },
        { BlockGroup::Logic,        "逻辑元件" },
        { BlockGroup::Actuator,     "执行器"   },
        { BlockGroup::Structure,    "结构"     },
        { BlockGroup::Other,        "其他"     },
    };

    for (const auto &def : DEFS) {
        auto *grp = new PaletteGroup(QString::fromUtf8(def.title), this);

        for (int i = 1; i < static_cast<int>(BlockType::_COUNT); ++i) {
            BlockType type = static_cast<BlockType>(i);
            const BlockMeta &meta = getBlockMeta(type);
            if (meta.group != def.group) continue;

            
            if (type == BlockType::PistonHead) continue;

            auto *item = new PaletteItem(type, this);
            connect(item, &PaletteItem::clicked, this,
                    [this, item](BlockType t) { onItemClicked(t, item); });
            grp->addItem(item);
        }

        m_groups.append(grp);
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, grp);
    }

    relayoutAll();

    
    for (auto *grp : m_groups) {
        for (auto *item : grp->items()) {
            if (item->blockType() == BlockType::Stone) {
                item->setSelected(true);
                m_selectedItem = item;
                m_selectedType = BlockType::Stone;
            }
        }
    }
}

void BlockPalette::onItemClicked(BlockType type, PaletteItem *item)
{
    if (m_selectedItem && m_selectedItem != item)
        m_selectedItem->setSelected(false);

    item->setSelected(true);
    m_selectedItem = item;
    m_selectedType = type;

    emit blockSelected(type);
}

void BlockPalette::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutAll();
}

void BlockPalette::relayoutAll()
{
    const int w = width();
    for (auto *grp : m_groups)
        grp->relayout(w);
}