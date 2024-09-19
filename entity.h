#ifndef ENTITY_H
#define ENTITY_H

#include <QString>
#include <QPixmap>
#include <QVector>
#include <QRectF>
#include <QSizeF>
#include <QXmlStreamReader>

// Adicione este enum no início do arquivo, fora da classe
enum class EntityType {
    Horizontal,
    Vertical,
    Layerable,
    Invisible
};

class Entity
{
public:
    Entity(const QString &name, const QString &filePath);

    QString getName() const { return m_name; }
    QPixmap getPixmap() const { return m_pixmap; }
    QVector<QRectF> getSpriteDefinitions() const { return m_spriteDefinitions; }
    int getSelectedTileIndex() const { return m_selectedTileIndex; }
    void setSelectedTileIndex(int index) { m_selectedTileIndex = index; }
    bool hasSprite() const { return m_hasSprite; }
    QSizeF getCollisionSize() const { return m_collisionSize; }
    bool isInvisible() const { return m_isInvisible; }
    bool hasOnlyCollision() const;

    // Adicione estes novos métodos
    EntityType getType() const { return m_type; }
    QSizeF getCurrentSize() const {
        if (m_isInvisible) {
            return m_collisionSize;
        }
        if (m_spriteDefinitions.isEmpty()) {
            return m_pixmap.size();
        }
        QSizeF tileSize = m_spriteDefinitions[m_selectedTileIndex].size();
        if (m_type == EntityType::Vertical) {
            return QSizeF(tileSize.width(), tileSize.width());
        }
        return tileSize;
    }

private:
    // Adicione este novo membro
    EntityType m_type;

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
