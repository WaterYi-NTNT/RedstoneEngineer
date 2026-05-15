#pragma once
#include <QWidget>
#include <QImage>
#include "Block.h"

class PaletteItem : public QWidget
{
    Q_OBJECT
public:
    explicit PaletteItem(BlockType type, QWidget *parent = nullptr);

    BlockType blockType() const { return m_type; }
    void      setSelected(bool on);

    static constexpr int ICON_SIZE   = 42;
    static constexpr int ITEM_WIDTH  = 64;
    static constexpr int ITEM_HEIGHT = 72;
    static constexpr int ITEM_SIZE   = ITEM_WIDTH;

signals:
    void clicked(BlockType type);

protected:
    void mousePressEvent(QMouseEvent *) override;
    void paintEvent     (QPaintEvent *) override;
    void enterEvent     (QEnterEvent *) override;
    void leaveEvent     (QEvent *)      override;

private:
    void buildIcon();
    BlockType m_type;
    QImage    m_icon;
    bool      m_selected = false;
    bool      m_hovered  = false;
};
