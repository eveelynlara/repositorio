#include "mainwindow.h"
#include "entity.h"
#include "entitymanager.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsPixmapItem>
#include <QMouseEvent>
#include <QDebug>
#include <QLoggingCategory>
#include <QLabel>
#include <QDir>

Q_LOGGING_CATEGORY(mainWindowCategory, "MainWindow")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_projectPath(""), m_selectedEntity(nullptr), m_selectedTileIndex(-1), m_entityPreview(nullptr)
{
    try {
        m_entityManager = new EntityManager();
        setupUI();
        createActions();
        createMenus();

        setWindowTitle("Editor de Cena");
        resize(1024, 768);
    } catch (const std::exception& e) {
        handleException("Erro durante a inicialização", e);
    }
}

MainWindow::~MainWindow()
{
    delete m_entityManager;
}

void MainWindow::setupUI()
{
    // Criar o explorador de projetos
    m_projectExplorer = new QTreeView(this);
    QDockWidget *projectDock = new QDockWidget("Explorador de Projeto", this);
    projectDock->setWidget(m_projectExplorer);
    projectDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::LeftDockWidgetArea, projectDock);

    // Configurar o explorador de projetos
    setupProjectExplorer();

    // Criar a visualização da cena
    setupSceneView();

    // Criar a lista de entidades
    setupEntityList();

    // Criar a lista de tiles
    setupTileList();

    // Criar o painel de propriedades
    m_propertiesDock = new QDockWidget("Propriedades", this);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // Configurar a barra de status
    statusBar()->showMessage("Pronto");
}

void MainWindow::setupProjectExplorer()
{
    m_fileSystemModel = new QFileSystemModel(this);
    m_fileSystemModel->setReadOnly(false);
    m_fileSystemModel->setNameFilterDisables(false);
    m_fileSystemModel->setNameFilters(QStringList() << "*.png" << "*.jpg" << "*.ent");

    m_projectExplorer->setModel(m_fileSystemModel);
    m_projectExplorer->setColumnWidth(0, 200);
    m_projectExplorer->setAnimated(true);
    m_projectExplorer->setSortingEnabled(true);
    m_projectExplorer->sortByColumn(0, Qt::AscendingOrder);

    // Esconder colunas desnecessárias
    m_projectExplorer->hideColumn(1);
    m_projectExplorer->hideColumn(2);
    m_projectExplorer->hideColumn(3);

    connect(m_projectExplorer, &QTreeView::doubleClicked, this, &MainWindow::onProjectItemDoubleClicked);
}

void MainWindow::setupSceneView()
{
    m_scene = new QGraphicsScene(this);
    m_sceneView = new QGraphicsView(m_scene);
    m_sceneView->setRenderHint(QPainter::Antialiasing);
    m_sceneView->setDragMode(QGraphicsView::RubberBandDrag);
    setCentralWidget(m_sceneView);

    // Conectar o evento de clique do mouse na cena
    m_sceneView->viewport()->installEventFilter(this);
}

void MainWindow::setupEntityList()
{
    m_entityList = new QListWidget(this);
    QDockWidget *entityDock = new QDockWidget("Entidades", this);
    entityDock->setWidget(m_entityList);
    entityDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::LeftDockWidgetArea, entityDock);

    connect(m_entityList, &QListWidget::itemClicked, this, &MainWindow::onEntityItemClicked);
}

void MainWindow::setupTileList()
{
    m_tileList = new QListWidget(this);
    QDockWidget *tileDock = new QDockWidget("Tiles", this);

    // Criar um widget de rolagem para conter o QLabel do spritesheet
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);

    // Criar um QLabel para mostrar o spritesheet
    m_spritesheetLabel = new QLabel();
    m_spritesheetLabel->setScaledContents(true);

    scrollArea->setWidget(m_spritesheetLabel);

    // Criar um layout vertical para conter o scrollArea e o m_tileList
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(scrollArea);
    layout->addWidget(m_tileList);

    QWidget *containerWidget = new QWidget();
    containerWidget->setLayout(layout);

    tileDock->setWidget(containerWidget);
    tileDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, tileDock);

    // Conectar o evento de clique do mouse no QLabel
    m_spritesheetLabel->installEventFilter(this);
}

void MainWindow::createActions()
{
    // Ação para abrir um projeto
    QAction *openProjectAction = new QAction("Abrir Projeto", this);
    connect(openProjectAction, &QAction::triggered, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Selecione o Diretório do Projeto",
                                                        QDir::homePath(),
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dir.isEmpty()) {
            m_projectPath = dir;
            m_fileSystemModel->setRootPath(m_projectPath);
            m_projectExplorer->setRootIndex(m_fileSystemModel->index(m_projectPath));
            statusBar()->showMessage("Projeto aberto: " + m_projectPath);
            loadEntities();
        }
    });

    // Adicione a ação ao menu Arquivo
    QMenu *fileMenu = menuBar()->addMenu("&Arquivo");
    fileMenu->addAction(openProjectAction);
}

void MainWindow::createMenus()
{
    // Os menus já foram criados em createActions()
}

void MainWindow::loadEntities()
{
    try {
        QString entitiesPath = m_projectPath + "/entities";
        qCInfo(mainWindowCategory) << "Carregando entidades do diretório:" << entitiesPath;

        QDir entitiesDir(entitiesPath);
        if (!entitiesDir.exists()) {
            qCWarning(mainWindowCategory) << "Diretório de entidades não encontrado:" << entitiesPath;
            return;
        }

        m_entityManager->loadEntitiesFromDirectory(entitiesPath);

        m_entityList->clear();
        QStringList entityNames;
        for (Entity* entity : m_entityManager->getAllEntities()) {
            entityNames << entity->getName();
            m_entityList->addItem(entity->getName());
        }
        qCInfo(mainWindowCategory) << "Entidades carregadas:" << entityNames.join(", ");

        if (m_entityList->count() == 0) {
            qCWarning(mainWindowCategory) << "Nenhuma entidade foi carregada";
        } else {
            qCInfo(mainWindowCategory) << "Total de entidades carregadas:" << m_entityList->count();
        }
    } catch (const std::exception& e) {
        handleException("Erro ao carregar entidades", e);
    }
}

void MainWindow::onProjectItemDoubleClicked(const QModelIndex &index)
{
    QString path = m_fileSystemModel->filePath(index);
    qCInfo(mainWindowCategory) << "Arquivo clicado:" << path;

    // Se o item clicado for o diretório "entities", recarregue as entidades
    if (path.endsWith("/entities") || path.endsWith("\\entities")) {
        loadEntities();
    }
}

void MainWindow::onEntityItemClicked(QListWidgetItem *item)
{
    if (!item) {
        qCWarning(mainWindowCategory) << "Item clicado é nulo";
        return;
    }

    QString entityName = item->text();
    try {
        m_selectedEntity = m_entityManager->getEntityByName(entityName);
        if (!m_selectedEntity) {
            qCWarning(mainWindowCategory) << "Entidade não encontrada:" << entityName;
            return;
        }
        m_selectedTileIndex = 0;
        updateEntityPreview();
        updateTileList();
        qCInfo(mainWindowCategory) << "Entidade selecionada:" << entityName;
    } catch (const std::exception& e) {
        handleException("Erro ao selecionar entidade", e);
    }
}

void MainWindow::onTileItemClicked(QListWidgetItem *item)
{
    if (!item) {
        qCWarning(mainWindowCategory) << "Item de tile clicado é nulo";
        return;
    }

    bool ok;
    int tileIndex = item->data(Qt::UserRole).toInt(&ok);
    if (!ok) {
        qCWarning(mainWindowCategory) << "Falha ao obter o índice do tile";
        return;
    }

    try {
        m_selectedTileIndex = tileIndex;
        updateEntityPreview();
        highlightSelectedTile();
        qCInfo(mainWindowCategory) << "Tile selecionado:" << tileIndex;
    } catch (const std::exception& e) {
        handleException("Erro ao selecionar tile", e);
    }
}

void MainWindow::highlightSelectedTile()
{
    if (!m_selectedEntity) return;

    QPixmap originalPixmap = m_selectedEntity->getPixmap();
    QPixmap highlightPixmap = originalPixmap.copy();
    QPainter painter(&highlightPixmap);
    painter.setPen(QPen(Qt::red, 2));

    const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
    if (m_selectedTileIndex >= 0 && m_selectedTileIndex < spriteDefinitions.size()) {
        painter.drawRect(spriteDefinitions[m_selectedTileIndex]);
    }

    m_spritesheetLabel->setPixmap(highlightPixmap);
}

void MainWindow::updateEntityPreview()
{
    if (!m_selectedEntity) {
        qCWarning(mainWindowCategory) << "Nenhuma entidade selecionada para atualizar o preview";
        return;
    }

    try {
        if (m_entityPreview) {
            m_scene->removeItem(m_entityPreview);
            delete m_entityPreview;
            m_entityPreview = nullptr;
        }

        QPixmap fullPixmap = m_selectedEntity->getPixmap();
        const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();

        if (m_selectedTileIndex < 0 || m_selectedTileIndex >= spriteDefinitions.size()) {
            qCWarning(mainWindowCategory) << "Índice de tile inválido:" << m_selectedTileIndex;
            return;
        }

        QRectF spriteRect = spriteDefinitions[m_selectedTileIndex];
        QPixmap tilePixmap = fullPixmap.copy(spriteRect.toRect());

        m_entityPreview = m_scene->addPixmap(tilePixmap);
        m_entityPreview->setOpacity(0.5);
        m_entityPreview->setFlag(QGraphicsItem::ItemIsMovable);
        qCInfo(mainWindowCategory) << "Preview da entidade atualizado para o tile" << m_selectedTileIndex;
    } catch (const std::exception& e) {
        handleException("Erro ao atualizar preview da entidade", e);
    }
}

void MainWindow::drawGridOnSpritesheet()
{
    if (!m_selectedEntity) return;

    QPixmap originalPixmap = m_selectedEntity->getPixmap();
    QPixmap gridPixmap = originalPixmap.copy();
    QPainter painter(&gridPixmap);
    painter.setPen(QPen(Qt::red, 1, Qt::DotLine));

    const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();

    for (int i = 0; i < spriteDefinitions.size(); ++i) {
        painter.drawRect(spriteDefinitions[i]);
        painter.drawText(spriteDefinitions[i], Qt::AlignCenter, QString::number(i));
    }

    m_spritesheetLabel->setPixmap(gridPixmap);
    qCInfo(mainWindowCategory) << "Grade desenhada com" << spriteDefinitions.size() << "sprites";
}

void MainWindow::updateTileList()
{
    m_tileList->clear();
    if (!m_selectedEntity) {
        qCWarning(mainWindowCategory) << "Nenhuma entidade selecionada para atualizar a lista de tiles";
        return;
    }

    try {
        QPixmap entityPixmap = m_selectedEntity->getPixmap();

        if (m_selectedEntity->isInvisible()) {
            // Para entidades invisíveis, crie um pixmap com borda vermelha
            QSizeF collisionSize = m_selectedEntity->getCollisionSize();
            entityPixmap = QPixmap(collisionSize.toSize());
            entityPixmap.fill(Qt::transparent);
            QPainter painter(&entityPixmap);
            painter.setPen(QPen(Qt::red, 2));
            painter.drawRect(entityPixmap.rect().adjusted(1, 1, -1, -1));
            painter.drawText(entityPixmap.rect(), Qt::AlignCenter, m_selectedEntity->getName());
        }

        // Mostrar o spritesheet completo no QLabel
        m_spritesheetLabel->setPixmap(entityPixmap);
        m_spritesheetLabel->setFixedSize(entityPixmap.size());

        // Desenhar a grade no spritesheet
        drawGridOnSpritesheet();

        // Adicionar informações sobre os tiles à lista
        const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
        for (int i = 0; i < spriteDefinitions.size(); ++i) {
            QListWidgetItem* item = new QListWidgetItem(QString("Tile %1").arg(i));
            item->setData(Qt::UserRole, i);
            m_tileList->addItem(item);
            qCInfo(mainWindowCategory) << "Adicionado tile" << i << ":" << spriteDefinitions[i];
        }

        qCInfo(mainWindowCategory) << "Spritesheet atualizado e lista de tiles preenchida";
        qCInfo(mainWindowCategory) << "Número de tiles:" << spriteDefinitions.size();
        qCInfo(mainWindowCategory) << "Tamanho do pixmap:" << entityPixmap.size();
        qCInfo(mainWindowCategory) << "Entidade é invisível:" << m_selectedEntity->isInvisible();
    } catch (const std::exception& e) {
        handleException("Erro ao atualizar lista de tiles", e);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_sceneView->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
            onSceneViewMousePress(mouseEvent);
            return true;
        }
    } else if (watched == m_spritesheetLabel && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
            handleTileItemClick(m_spritesheetLabel, mouseEvent->pos());
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onSceneViewMousePress(QMouseEvent *event)
{
    QPointF scenePos = m_sceneView->mapToScene(event->pos());
    placeEntityInScene(scenePos);
}

void MainWindow::placeEntityInScene(const QPointF &pos)
{
    if (!m_selectedEntity) {
        qCWarning(mainWindowCategory) << "Nenhuma entidade selecionada para colocar na cena";
        return;
    }

    try {
        QPixmap fullPixmap = m_selectedEntity->getPixmap();
        const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
        QSizeF collisionSize = m_selectedEntity->getCollisionSize();

        if (m_selectedTileIndex < 0 || m_selectedTileIndex >= spriteDefinitions.size()) {
            qCWarning(mainWindowCategory) << "Índice de tile inválido:" << m_selectedTileIndex;
            return;
        }

        QRectF spriteRect = spriteDefinitions[m_selectedTileIndex];
        QPixmap tilePixmap;

        if (m_selectedEntity->isInvisible()) {
            // Para entidades invisíveis, crie um pixmap com borda vermelha
            tilePixmap = QPixmap(collisionSize.toSize());
            tilePixmap.fill(Qt::transparent);
            QPainter painter(&tilePixmap);
            painter.setPen(QPen(Qt::red, 2));
            painter.drawRect(tilePixmap.rect().adjusted(1, 1, -1, -1));
            painter.drawText(tilePixmap.rect(), Qt::AlignCenter, m_selectedEntity->getName());
        } else {
            // Use o tamanho da colisão para recortar o sprite, se disponível
            if (collisionSize.isValid()) {
                QRect sourceRect(spriteRect.topLeft().toPoint(), collisionSize.toSize());
                tilePixmap = fullPixmap.copy(sourceRect);
            } else {
                tilePixmap = fullPixmap.copy(spriteRect.toRect());
            }
        }

        QGraphicsPixmapItem *item = m_scene->addPixmap(tilePixmap);
        item->setPos(pos);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        qCInfo(mainWindowCategory) << "Entidade colocada na cena na posição:" << pos;
    } catch (const std::exception& e) {
        handleException("Erro ao colocar entidade na cena", e);
    }
}

void MainWindow::handleException(const QString &context, const std::exception &e)
{
    QString errorMessage = QString("%1: %2").arg(context).arg(e.what());
    qCCritical(mainWindowCategory) << errorMessage;
    QMessageBox::critical(this, "Erro", errorMessage);
}

void MainWindow::handleTileItemClick(QLabel* spritesheetLabel, const QPoint& pos)
{
    if (!m_selectedEntity) {
        qCWarning(mainWindowCategory) << "Nenhuma entidade selecionada";
        return;
    }

    const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
    QPixmap spritesheet = m_selectedEntity->getPixmap();

    // Calcular a escala do spritesheet no QLabel
    qreal scaleX = static_cast<qreal>(spritesheetLabel->width()) / spritesheet.width();
    qreal scaleY = static_cast<qreal>(spritesheetLabel->height()) / spritesheet.height();

    // Converter a posição do clique para as coordenadas do spritesheet original
    QPointF originalPos(pos.x() / scaleX, pos.y() / scaleY);

    qCInfo(mainWindowCategory) << "Clique no spritesheet:" << originalPos;

    for (int i = 0; i < spriteDefinitions.size(); ++i) {
        qCInfo(mainWindowCategory) << "Verificando sprite" << i << ":" << spriteDefinitions[i];
        if (spriteDefinitions[i].contains(originalPos)) {
            m_selectedTileIndex = i;
            updateEntityPreview();
            m_tileList->setCurrentRow(i);
            qCInfo(mainWindowCategory) << "Tile selecionado:" << i;
            return;
        }
    }

    qCWarning(mainWindowCategory) << "Nenhum tile selecionado";
}
