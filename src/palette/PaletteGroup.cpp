#include "PaletteGroup.h"
#include "PaletteItem.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>

PaletteGroup::PaletteGroup(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 2, 0, 6);
    m_mainLayout->setSpacing(2);


    m_titleBtn = new QPushButton("▾  " + title, this);
    m_titleBtn->setFlat(true);
    m_titleBtn->setCheckable(false);
    m_titleBtn->setStyleSheet(R"(
        QPushButton {
            color: #E06C3A;
            font-weight: bold;
            font-size: 11px;
            text-align: left;
            padding: 3px 6px;
            border: none;
            background: transparent;
        }
        QPushButton:hover {
            background: rgba(224, 108, 58, 0.15);
            border-radius: 3px;
        }
    )");
    connect(m_titleBtn, &QPushButton::clicked, this, &PaletteGroup::toggleCollapse);
    m_mainLayout->addWidget(m_titleBtn);


    m_container  = new QWidget(this);
    m_gridLayout = new QGridLayout(m_container);
    m_gridLayout->setContentsMargins(4, 2, 4, 2);
    m_gridLayout->setSpacing(2);
    m_mainLayout->addWidget(m_container);
}

void PaletteGroup::addItem(PaletteItem *item)
{
    m_items.append(item);
    item->setParent(m_container);
}

void PaletteGroup::relayout(int containerWidth)
{
    if (m_collapsed) return;


    for (auto *item : m_items)
        m_gridLayout->removeWidget(item);


    const int availWidth = qMax(containerWidth - 8, PaletteItem::ITEM_SIZE);
    const int cols       = qMax(1, availWidth / PaletteItem::ITEM_SIZE);

    for (int i = 0; i < m_items.size(); ++i) {
        int row = i / cols;
        int col = i % cols;
        m_gridLayout->addWidget(m_items[i], row, col, Qt::AlignLeft | Qt::AlignTop);
    }


    for (int c = 0; c < cols; ++c)
        m_gridLayout->setColumnStretch(c, 0);
    m_gridLayout->setColumnStretch(cols, 1);
}

void PaletteGroup::clearSelection()
{
    for (auto *item : m_items)
        item->setSelected(false);
}

void PaletteGroup::toggleCollapse()
{
    m_collapsed = !m_collapsed;
    m_container->setVisible(!m_collapsed);


    QString title = m_titleBtn->text();
    if (m_collapsed)
        title.replace("▾", "▸");
    else
        title.replace("▸", "▾");
    m_titleBtn->setText(title);
}
