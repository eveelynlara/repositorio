#include "entity.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QPainter>

Entity::Entity(const QString &name, const QString &filePath)
    : m_type(EntityType::Horizontal),
      m_name(name),
      m_selectedTileIndex(0),
      m_isInvisible(false),
      m_hasSprite(true)
{
    qDebug() << "Iniciando carregamento da entidade:" << name;

    if (name.isEmpty() || filePath.isEmpty()) {
        qWarning() << "Nome ou caminho do arquivo vazio para a entidade";
        return;
    }

    loadEntityDefinition(filePath);

    qDebug() << "Carregamento da entidade concluído:" << name;
}

bool Entity::loadImage(const QString &imageName, const QString &entityPath)
{
    QFileInfo fileInfo(entityPath);
    QString imagePath = fileInfo.dir().filePath(imageName);

    qDebug() << "Tentando carregar imagem:" << imagePath;

    if (QFileInfo::exists(imagePath) && m_pixmap.load(imagePath)) {
        qDebug() << "Imagem carregada com sucesso:" << imagePath;
        m_hasSprite = true;
        m_isInvisible = false;
    } else {
        qDebug() << "Arquivo de imagem não encontrado ou falha ao carregar. Criando um pixmap padrão.";
        // Usar o tamanho da colisão se disponível, caso contrário, usar um tamanho padrão
        int width = m_collisionSize.isValid() ? m_collisionSize.width() : 64;
        int height = m_collisionSize.isValid() ? m_collisionSize.height() : 64;
        m_pixmap = QPixmap(width, height);
        m_pixmap.fill(Qt::transparent);
        QPainter painter(&m_pixmap);
        painter.setPen(Qt::red);
        painter.drawRect(0, 0, width - 1, height - 1);
        painter.drawText(m_pixmap.rect(), Qt::AlignCenter, m_name);
        m_isInvisible = true;
        m_hasSprite = false;
    }

    qDebug() << "Pixmap criado com sucesso. Dimensões:" << m_pixmap.width() << "x" << m_pixmap.height();
    qDebug() << "Entidade é invisível:" << m_isInvisible;
    qDebug() << "Entidade tem sprite:" << m_hasSprite;
    return true;
}

void Entity::loadCustomSpriteDefinitions(const QString &xmlPath) {
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Não foi possível abrir o arquivo XML:" << xmlPath;
        return;
    }

    QXmlStreamReader xml(&file);
    m_spriteDefinitions.clear();

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (xml.name().compare(QLatin1String("sprite"), Qt::CaseInsensitive) == 0) {
                int x = xml.attributes().value("x").toInt();
                int y = xml.attributes().value("y").toInt();
                int w = xml.attributes().value("w").toInt();
                int h = xml.attributes().value("h").toInt();
                m_spriteDefinitions.append(QRectF(x, y, w, h));
                qDebug() << "Sprite definido:" << QRectF(x, y, w, h);
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "Erro ao ler o arquivo XML:" << xml.errorString();
    }

    file.close();
    qDebug() << "Total de definições de sprite carregadas do XML:" << m_spriteDefinitions.size();
}

void Entity::loadEntityDefinition(const QString &filePath)
{
    qDebug() << "Iniciando carregamento da definição da entidade de:" << filePath;
    if (!QFileInfo::exists(filePath)) {
        qWarning() << "Arquivo de definição da entidade não encontrado:" << filePath;
        return;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Falha ao abrir o arquivo de definição da entidade:" << filePath;
        return;
    }
    QXmlStreamReader xml(&file);
    QString spriteName;
    int spriteCutX = 1, spriteCutY = 1;
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name().compare(QLatin1String("Entity"), Qt::CaseInsensitive) == 0) {
                QString type = xml.attributes().value("type").toString().toLower();
                if (type == "vertical") {
                    m_type = EntityType::Vertical;
                } else if (type == "layerable") {
                    m_type = EntityType::Layerable;
                } else if (type == "invisible") {
                    m_type = EntityType::Invisible;
                    m_isInvisible = true;
                } else {
                    m_type = EntityType::Horizontal;
                }
            } else if (xml.name().compare(QLatin1String("Sprite"), Qt::CaseInsensitive) == 0) {
                spriteName = xml.readElementText();
                qDebug() << "Nome do sprite encontrado:" << spriteName;
                loadImage(spriteName, filePath);
            } else if (xml.name().compare(QLatin1String("SpriteCut"), Qt::CaseInsensitive) == 0) {
                bool ok;
                spriteCutX = xml.attributes().value("x").toInt(&ok);
                if (!ok) {
                    qWarning() << "Valor inválido para 'x' em SpriteCut";
                    spriteCutX = 1;
                }
                spriteCutY = xml.attributes().value("y").toInt(&ok);
                if (!ok) {
                    qWarning() << "Valor inválido para 'y' em SpriteCut";
                    spriteCutY = 1;
                }
                qDebug() << "SpriteCut encontrado:" << spriteCutX << "x" << spriteCutY;
            } else if (xml.name().compare(QLatin1String("Collision"), Qt::CaseInsensitive) == 0) {
                loadCollisionInfo(xml);
            }
        }
    }
    if (xml.hasError()) {
        qWarning() << "Erro ao analisar o arquivo XML:" << xml.errorString();
    }
    file.close();

    // Verificar se existe um arquivo XML personalizado para as definições de sprite
    QString xmlPath = QFileInfo(filePath).absolutePath() + "/" + QFileInfo(spriteName).baseName() + ".xml";
    qDebug() << "Procurando arquivo XML personalizado:" << xmlPath;
    if (QFile::exists(xmlPath)) {
        qDebug() << "Arquivo XML personalizado encontrado. Carregando definições...";
        loadCustomSpriteDefinitions(xmlPath);
        qDebug() << "Carregadas" << m_spriteDefinitions.size() << "definições de sprite personalizadas do XML";
    }

    // Se não há definições de sprite do XML e temos um SpriteCut válido, criar definições baseadas no SpriteCut
    if (m_spriteDefinitions.isEmpty() && m_hasSprite && spriteCutX > 0 && spriteCutY > 0) {
        float spriteWidth = m_pixmap.width() / static_cast<float>(spriteCutX);
        float spriteHeight = m_pixmap.height() / static_cast<float>(spriteCutY);
        for (int y = 0; y < spriteCutY; ++y) {
            for (int x = 0; x < spriteCutX; ++x) {
                m_spriteDefinitions.append(QRectF(x * spriteWidth, y * spriteHeight, spriteWidth, spriteHeight));
            }
        }
        qDebug() << "Criadas" << m_spriteDefinitions.size() << "definições de sprite baseadas no SpriteCut";
    }

    // Se ainda não tem definições de sprite, mas tem pixmap, cria uma definição para o pixmap inteiro
    if (m_spriteDefinitions.isEmpty() && !m_pixmap.isNull()) {
        m_spriteDefinitions.append(QRectF(0, 0, m_pixmap.width(), m_pixmap.height()));
        qDebug() << "Criada 1 definição de sprite para o pixmap inteiro";
    }

    // Se ainda não tem definições de sprite, mas tem tamanho de colisão, cria uma definição baseada no tamanho da colisão
    if (m_spriteDefinitions.isEmpty() && !m_collisionSize.isNull()) {
        m_spriteDefinitions.append(QRectF(0, 0, m_collisionSize.width(), m_collisionSize.height()));
        m_isInvisible = true;
        qDebug() << "Criada 1 definição de sprite para entidade invisível baseada no tamanho da colisão";
    }

    // Se o tamanho da colisão não foi definido, use o tamanho do primeiro sprite ou do pixmap
    if (m_collisionSize.isNull()) {
        if (!m_spriteDefinitions.isEmpty()) {
            m_collisionSize = m_spriteDefinitions[0].size();
        } else if (!m_pixmap.isNull()) {
            m_collisionSize = m_pixmap.size();
        }
        qDebug() << "Tamanho da colisão definido automaticamente:" << m_collisionSize;
    }

    if (m_spriteDefinitions.isEmpty()) {
        qWarning() << "Nenhuma definição de sprite criada para a entidade:" << m_name;
    }

    qDebug() << "Definições de sprite finais:";
    for (int i = 0; i < m_spriteDefinitions.size(); ++i) {
        qDebug() << i << ":" << m_spriteDefinitions[i];
    }
}

bool Entity::hasOnlyCollision() const {
    return !hasSprite() && !isInvisible();
}

void Entity::loadCollisionInfo(QXmlStreamReader &xml)
{
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name().compare(QLatin1String("Collision"), Qt::CaseInsensitive) == 0)) {
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            if (xml.name().compare(QLatin1String("Size"), Qt::CaseInsensitive) == 0) {
                bool ok;
                float width = xml.attributes().value("x").toFloat(&ok);
                float height = xml.attributes().value("y").toFloat(&ok);
                if (ok) {
                    m_collisionSize = QSizeF(width, height);
                    qDebug() << "Tamanho da colisão definido:" << m_collisionSize;
                } else {
                    qWarning() << "Valores inválidos para o tamanho da colisão";
                }
            }
        }
        xml.readNext();
    }
}
