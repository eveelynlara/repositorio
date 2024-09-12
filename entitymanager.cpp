#include "entitymanager.h"
#include "entity.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>

EntityManager::EntityManager() {}

EntityManager::~EntityManager()
{
    qDeleteAll(m_entities);
    m_entities.clear();
}

void EntityManager::loadEntitiesFromDirectory(const QString &path)
{
    if (path.isEmpty()) {
        qWarning() << "Caminho do diretório vazio";
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "Diretório não encontrado:" << path;
        return;
    }

    QStringList filters;
    filters << "*.ent";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

    if (fileList.isEmpty()) {
        qWarning() << "Nenhum arquivo .ent encontrado no diretório:" << path;
        return;
    }

    for (const QFileInfo &fileInfo : fileList)
    {
        QString name = fileInfo.baseName();
        QString filePath = fileInfo.filePath();
        
        if (name.isEmpty()) {
            qWarning() << "Nome de arquivo inválido:" << filePath;
            continue;
        }

        if (!m_entities.contains(name))
        {
            try {
                Entity *entity = new Entity(name, filePath);
                if (entity->getPixmap().isNull() || entity->getSpriteDefinitions().isEmpty()) {
                    qWarning() << "Falha ao carregar entidade:" << name;
                    delete entity;
                } else {
                    m_entities[name] = entity;
                    qDebug() << "Entidade carregada com sucesso:" << name;
                }
            } catch (const std::exception& e) {
                qWarning() << "Erro ao criar entidade:" << name << "-" << e.what();
            }
        }
        else
        {
            qWarning() << "Entidade duplicada encontrada:" << name;
        }
    }

    qDebug() << "Total de entidades carregadas:" << m_entities.size();
}

Entity* EntityManager::getEntityByName(const QString &name) const
{
    if (name.isEmpty()) {
        qWarning() << "Nome de entidade vazio";
        return nullptr;
    }

    auto it = m_entities.find(name);
    if (it != m_entities.end()) {
        return it.value();
    } else {
        qWarning() << "Entidade não encontrada:" << name;
        return nullptr;
    }
}

QVector<Entity*> EntityManager::getAllEntities() const
{
    return m_entities.values().toVector();
}