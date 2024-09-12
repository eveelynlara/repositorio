#include "entity.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QFileInfo>

Entity::Entity(const QString &name, const QString &filePath)
    : m_name(name), m_selectedTileIndex(0)
{
    if (name.isEmpty() || filePath.isEmpty()) {
        qWarning() << "Nome ou caminho do arquivo vazio para a entidade";
        return;
    }

    QString imagePath = filePath;
    imagePath.replace(".ent", ".png");
    
    if (!QFileInfo::exists(imagePath)) {
        qWarning() << "Arquivo de imagem não encontrado:" << imagePath;
        return;
    }

    if (!m_pixmap.load(imagePath)) {
        qWarning() << "Falha ao carregar a imagem:" << imagePath;
        return;
    }

    loadSpriteDefinitions(filePath);
}

void Entity::loadSpriteDefinitions(const QString &filePath)
{
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
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement && xml.name().toString() == "sprite") {
            bool ok;
            float x = xml.attributes().value("x").toFloat(&ok);
            if (!ok) {
                qWarning() << "Valor inválido para 'x' no arquivo XML";
                continue;
            }
            float y = xml.attributes().value("y").toFloat(&ok);
            if (!ok) {
                qWarning() << "Valor inválido para 'y' no arquivo XML";
                continue;
            }
            float width = xml.attributes().value("w").toFloat(&ok);
            if (!ok) {
                qWarning() << "Valor inválido para 'w' no arquivo XML";
                continue;
            }
            float height = xml.attributes().value("h").toFloat(&ok);
            if (!ok) {
                qWarning() << "Valor inválido para 'h' no arquivo XML";
                continue;
            }
            m_spriteDefinitions.append(QRectF(x, y, width, height));
        }
    }

    if (xml.hasError()) {
        qWarning() << "Erro ao analisar o arquivo XML:" << xml.errorString();
    }

    file.close();

    if (m_spriteDefinitions.isEmpty()) {
        qWarning() << "Nenhuma definição de sprite encontrada no arquivo:" << filePath;
    }
}