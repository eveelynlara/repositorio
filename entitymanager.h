#ifndef ENTITYMANAGER_H
#define ENTITYMANAGER_H

#include <QVector>
#include <QString>
#include <QMap>

class Entity;

class EntityManager
{
public:
    EntityManager();
    ~EntityManager();

    void loadEntitiesFromDirectory(const QString &path);
    Entity* getEntityByName(const QString &name) const;
    QVector<Entity*> getAllEntities() const;

private:
    QMap<QString, Entity*> m_entities;
};

#endif // ENTITYMANAGER_H