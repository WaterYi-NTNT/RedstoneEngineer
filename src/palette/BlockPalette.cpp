#include "BlockPalette.h"
#include "PaletteGroup.h"
#include "PaletteItem.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QToolButton>
#include <QLabel>
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


    setupTopBar();


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


void BlockPalette::setupTopBar()
{
    QWidget *bar = new QWidget(this);
    bar->setFixedHeight(36);
    bar->setStyleSheet("background: #1E1E1E; border-bottom: 1px solid #3C3C3C;");

    QHBoxLayout *hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 4, 6, 4);
    hl->setSpacing(4);

    m_btnPaint = new QToolButton(bar);
    m_btnPaint->setText("✏ 画笔");
    m_btnPaint->setCheckable(true);
    m_btnPaint->setChecked(true);
    m_btnPaint->setFixedHeight(26);
    m_btnPaint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_btnSelect = new QToolButton(bar);
    m_btnSelect->setText("⬚ 选择");
    m_btnSelect->setCheckable(true);
    m_btnSelect->setChecked(false);
    m_btnSelect->setFixedHeight(26);
    m_btnSelect->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const QString btnStyle = R"(
        QToolButton {
            color: #AAAAAA;
            background: #2D2D2D;
            border: 1px solid #444444;
            border-radius: 3px;
            font-size: 11px;
            padding: 2px 4px;
        }
        QToolButton:checked {
            color: #FFFFFF;
            background: #264F78;
            border-color: #3A6FA8;
        }
        QToolButton:hover:!checked { background: #3C3C3C; }
    )";
    m_btnPaint ->setStyleSheet(btnStyle);
    m_btnSelect->setStyleSheet(btnStyle);

    connect(m_btnPaint, &QToolButton::clicked, this, [this]() {
        m_btnPaint ->setChecked(true);
        m_btnSelect->setChecked(false);
        emit editModeChanged(false);
    });
    connect(m_btnSelect, &QToolButton::clicked, this, [this]() {
        m_btnSelect->setChecked(true);
        m_btnPaint ->setChecked(false);
        emit editModeChanged(true);
    });

    hl->addWidget(m_btnPaint);
    hl->addWidget(m_btnSelect);


    static_cast<QVBoxLayout*>(layout())->addWidget(bar);
}


void BlockPalette::setupGroups()
{

    struct GroupDef { BlockGroup group; const char *title; };
    static const GroupDef DEFS[] = {
        { BlockGroup::SignalSource, "信号源" },
        { BlockGroup::Logic,        "逻辑元件" },
        { BlockGroup::Actuator,     "执行器" },
        { BlockGroup::Structure,    "结构" },
        { BlockGroup::Other,        "其他" },
    };

    for (const auto &def : DEFS) {
        auto *grp = new PaletteGroup(QString::fromUtf8(def.title), this);

        for (int i = 1; i < static_cast<int>(BlockType::_COUNT); ++i) {
            BlockType type = static_cast<BlockType>(i);
            const BlockMeta &meta = getBlockMeta(type);
            if (meta.group != def.group) continue;

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
