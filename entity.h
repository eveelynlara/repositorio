#ifndef ENTITY_H
#define ENTITY_H

#include <QString>
#include <QPixmap>
#include <QVector>
#include <QRectF>
#include <QSizeF>
#include <QXmlStreamReader>

class Entity
{
public:
    Entity(const QString &name, const QString &filePath);

    QString getName() const { return m_name; }
    QPixmap getPixmap() const { return m_pixmap; }
    QVector<QRectF> getSpriteDefinitions() const { return m_spriteDefinitions; }
    int getSelectedTileIndex() const { return m_selectedTileIndex; }
    void setSelectedTileIndex(int index) { m_selectedTileIndex = index; }
    bool isInvisible() const { return m_isInvisible; }
    bool hasSprite() const { return m_hasSprite; }
    QSizeF getCollisionSize() const { return m_collisionSize; }

private:
    QString m_name;
    QPixmap m_pixmap;
    QVector<QRectF> m_spriteDefinitions;
    int m_selectedTileIndex;
    bool m_isInvisible;
    bool m_hasSprite;
    QSizeF m_collisionSize;

    void loadEntityDefinition(const QString &filePath);
    bool loadImage(const QString &imageName, const QString &entityPath);
    void loadCollisionInfo(QXmlStreamReader &xml);
    void loadCustomSpriteDefinitions(const QString &xmlPath);
};

#endif // ENTITY_H
