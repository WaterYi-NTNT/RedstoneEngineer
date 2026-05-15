#include "PaletteItem.h"
#include "BlockStateLoader.h"
#include "BlockModelLoader.h"
#include "ModelIconRenderer.h"

#include <QPainter>
#include <QMouseEvent>

PaletteItem::PaletteItem(BlockType type, QWidget *parent)
    : QWidget(parent), m_type(type)
{
    setFixedSize(ITEM_WIDTH, ITEM_HEIGHT);
    setToolTip(QString::fromUtf8(getBlockMeta(type).displayName));
    buildIcon();
}

void PaletteItem::buildIcon()
{
    BlockStateResult bsr = BlockStateLoader::getResult(m_type);
    if(bsr.modelName.isEmpty()) return;

    BlockModel bm = BlockModelLoader::load(bsr.modelName);

    IconPreRot preRot = ModelIconRenderer::suggestPreRot(m_type);
    m_icon = ModelIconRenderer::render(bm, ICON_SIZE, preRot);
}

void PaletteItem::setSelected(bool on){ m_selected=on; update(); }
void PaletteItem::mousePressEvent(QMouseEvent *e)
{ if(e->button()==Qt::LeftButton) emit clicked(m_type); }
void PaletteItem::enterEvent(QEnterEvent *){ m_hovered=true;  update(); }
void PaletteItem::leaveEvent(QEvent *)     { m_hovered=false; update(); }

void PaletteItem::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect rc = rect().adjusted(1,1,-1,-1);
    if(m_selected)
        p.fillRect(rc, QColor(60,120,200,180));
    else if(m_hovered)
        p.fillRect(rc, QColor(255,255,255,30));

    p.setPen(m_selected ? QColor(100,160,255,160) : QColor(70,70,70,100));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rc);

    const int TEXT_H = 16;
    const int ICON_Y = (ITEM_HEIGHT - TEXT_H - ICON_SIZE) / 2;
    const int ICON_X = (ITEM_WIDTH  - ICON_SIZE) / 2;

    p.setClipRect(rc);
    if(!m_icon.isNull()){
        p.drawImage(QRect(ICON_X, qMax(ICON_Y,2), ICON_SIZE, ICON_SIZE), m_icon);
    } else {
        p.setPen(QColor(80,80,80));
        p.setBrush(QColor(50,50,50));
        p.drawRect(ICON_X, 2, ICON_SIZE, ICON_SIZE);
        p.setPen(QColor(140,140,140));
        QFont fq; fq.setPixelSize(16); p.setFont(fq);
        p.drawText(QRect(ICON_X,2,ICON_SIZE,ICON_SIZE),Qt::AlignCenter,"?");
    }
    p.setClipping(false);

    QString name = QString::fromUtf8(getBlockMeta(m_type).displayName);
    if(name.length()>5) name=name.left(4)+"…";
    QFont f; f.setPixelSize(10); p.setFont(f);
    p.setPen(m_selected ? Qt::white : QColor(180,180,180));
    p.drawText(QRect(0,ITEM_HEIGHT-TEXT_H,ITEM_WIDTH,TEXT_H),
               Qt::AlignCenter, name);
}
