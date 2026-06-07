#pragma once
#include <QWidget>
#include <QVector>

class QVBoxLayout;
class QGridLayout;
class QLabel;
class QPushButton;
class PaletteItem;

class PaletteGroup : public QWidget
{
    Q_OBJECT
public:
    explicit PaletteGroup(const QString &title, QWidget *parent = nullptr);

    void addItem(PaletteItem *item);
    void relayout(int containerWidth);
    void clearSelection();

    const QVector<PaletteItem*> &items() const { return m_items; }

private slots:
    void toggleCollapse();

private:
    QVBoxLayout          *m_mainLayout  = nullptr;
    QPushButton          *m_titleBtn    = nullptr;
    QWidget              *m_container   = nullptr;
    QGridLayout          *m_gridLayout  = nullptr;
    QVector<PaletteItem*> m_items;
    bool                  m_collapsed   = false;
};
