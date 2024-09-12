#include "entitymanager.h"
#include "entity.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QElapsedTimer>

EntityManager::EntityManager() {}

EntityManager::~EntityManager()
{
    qDeleteAll(m_entities);
    m_entities.clear();
}

void EntityManager::loadEntitiesFromDirectory(const QString &path)
{
    QElapsedTimer timer;
    timer.start();

    qDebug() << "Iniciando carregamento de entidades do diretório:" << path;
    qDebug() << "Caminho absoluto:" << QDir(path).absolutePath();

    if (path.isEmpty()) {
        qWarning() << "Caminho do diretório vazio";
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "Diretório não encontrado:" << path;
        return;
    }

    // Limpar entidades existentes antes de carregar novas
    qDebug() << "Limpando entidades existentes...";
    qDeleteAll(m_entities);
    m_entities.clear();

    QStringList filters;
    filters << "*.ent";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

    if (fileList.isEmpty()) {
        qWarning() << "Nenhum arquivo .ent encontrado no diretório:" << path;
        return;
    }

    qDebug() << "Encontrados" << fileList.size() << "arquivos .ent no diretório";

    int successfullyLoaded = 0;
    for (const QFileInfo &fileInfo : fileList)
    {
        QString name = fileInfo.baseName();
        QString filePath = fileInfo.filePath();

        qDebug() << "Processando arquivo:" << filePath;

        if (name.isEmpty()) {
            qWarning() << "Nome de arquivo inválido:" << filePath;
            continue;
        }

        if (!m_entities.contains(name))
        {
            try {
                qDebug() << "Criando nova entidade:" << name;
                Entity *entity = new Entity(name, filePath);
                // Removemos a verificação de pixmap nulo
                m_entities[name] = entity;
                successfullyLoaded++;
                qDebug() << "Entidade carregada com sucesso:" << name
                         << "- Tamanho do pixmap:" << entity->getPixmap().size()
                         << "- Número de definições de sprite:" << entity->getSpriteDefinitions().size()
                         << "- É invisível:" << entity->isInvisible();
            } catch (const std::exception& e) {
                qWarning() << "Erro ao criar entidade:" << name << "-" << e.what();
            }
        }
        else
        {
            qWarning() << "Entidade duplicada encontrada:" << name;
        }
    }

    qDebug() << "Total de entidades carregadas com sucesso:" << successfullyLoaded << "de" << fileList.size() << "arquivos";

    if (m_entities.isEmpty()) {
        qWarning() << "Nenhuma entidade foi carregada com sucesso. Verifique o conteúdo dos arquivos .ent e os logs acima para mais detalhes.";
    } else {
        qDebug() << "Entidades carregadas:";
        for (const QString &name : m_entities.keys()) {
            qDebug() << "  -" << name;
        }
    }

    qDebug() << "Tempo total de carregamento:" << timer.elapsed() << "ms";
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
