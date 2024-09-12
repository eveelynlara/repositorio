#ifndef ENTITY_H
#define ENTITY_H

#include <QString>
#include <QPixmap>
#include <QVector>
#include <QRectF>

class Entity
{
public:
    Entity(const QString &name, const QString &filePath);

    QString getName() const { return m_name; }
    QPixmap getPixmap() const { return m_pixmap; }
    QVector<QRectF> getSpriteDefinitions() const { return m_spriteDefinitions; }
    int getSelectedTileIndex() const { return m_selectedTileIndex; }
    void setSelectedTileIndex(int index) { m_selectedTileIndex = index; }

private:
    QString m_name;
    QPixmap m_pixmap;
    QVector<QRectF> m_spriteDefinitions;
    int m_selectedTileIndex;

    void loadSpriteDefinitions(const QString &filePath);
};

#endif // ENTITY_H