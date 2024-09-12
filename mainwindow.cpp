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
    tileDock->setWidget(m_tileList);
    tileDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, tileDock);

    connect(m_tileList, &QListWidget::itemClicked, this, &MainWindow::onTileItemClicked);
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
        m_entityManager->loadEntitiesFromDirectory(entitiesPath);

        m_entityList->clear();
        for (Entity* entity : m_entityManager->getAllEntities()) {
            m_entityList->addItem(entity->getName());
        }
        qCInfo(mainWindowCategory) << "Entidades carregadas com sucesso";
    } catch (const std::exception& e) {
        handleException("Erro ao carregar entidades", e);
    }
}

void MainWindow::onProjectItemDoubleClicked(const QModelIndex &index)
{
    QString path = m_fileSystemModel->filePath(index);
    // Aqui você pode adicionar lógica para abrir arquivos .ent ou .png
    qCInfo(mainWindowCategory) << "Arquivo clicado:" << path;
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
        qCInfo(mainWindowCategory) << "Tile selecionado:" << tileIndex;
    } catch (const std::exception& e) {
        handleException("Erro ao selecionar tile", e);
    }
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

        QPixmap pixmap = m_selectedEntity->getPixmap();
        if (pixmap.isNull()) {
            qCWarning(mainWindowCategory) << "Pixmap da entidade é nulo";
            return;
        }

        const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
        if (m_selectedTileIndex < 0 || m_selectedTileIndex >= spriteDefinitions.size()) {
            qCWarning(mainWindowCategory) << "Índice de tile inválido:" << m_selectedTileIndex;
            return;
        }

        QRectF spriteRect = spriteDefinitions[m_selectedTileIndex];
        QPixmap tilePixmap = pixmap.copy(spriteRect.toRect());

        m_entityPreview = m_scene->addPixmap(tilePixmap);
        m_entityPreview->setOpacity(0.5);
        m_entityPreview->setFlag(QGraphicsItem::ItemIsMovable);
        qCInfo(mainWindowCategory) << "Preview da entidade atualizado";
    } catch (const std::exception& e) {
        handleException("Erro ao atualizar preview da entidade", e);
    }
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
        if (entityPixmap.isNull()) {
            qCWarning(mainWindowCategory) << "Pixmap da entidade é nulo";
            return;
        }

        // Criar um QLabel para mostrar o spritesheet completo
        QLabel* spritesheetLabel = new QLabel();
        spritesheetLabel->setPixmap(entityPixmap);
        spritesheetLabel->setScaledContents(true);

        // Adicionar o QLabel à lista de tiles
        QListWidgetItem* item = new QListWidgetItem(m_tileList);
        item->setSizeHint(entityPixmap.size());
        m_tileList->setItemWidget(item, spritesheetLabel);

        // Conectar o evento de clique do mouse no QLabel
        spritesheetLabel->installEventFilter(this);

        qCInfo(mainWindowCategory) << "Spritesheet adicionado à lista de tiles";
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
    } else if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
            QLabel* spritesheetLabel = qobject_cast<QLabel*>(watched);
            if (spritesheetLabel) {
                handleTileItemClick(spritesheetLabel, mouseEvent->pos());
                return true;
            }
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
        QPixmap pixmap = m_selectedEntity->getPixmap();
        if (pixmap.isNull()) {
            qCWarning(mainWindowCategory) << "Pixmap da entidade é nulo";
            return;
        }

        const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
        if (m_selectedTileIndex < 0 || m_selectedTileIndex >= spriteDefinitions.size()) {
            qCWarning(mainWindowCategory) << "Índice de tile inválido:" << m_selectedTileIndex;
            return;
        }

        QRectF spriteRect = spriteDefinitions[m_selectedTileIndex];
        QPixmap tilePixmap = pixmap.copy(spriteRect.toRect());

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

    for (int i = 0; i < spriteDefinitions.size(); ++i) {
        if (spriteDefinitions[i].contains(originalPos)) {
            m_selectedTileIndex = i;
            updateEntityPreview();
            qCInfo(mainWindowCategory) << "Tile selecionado:" << i;
            return;
        }
    }

    qCWarning(mainWindowCategory) << "Nenhum tile selecionado";
}
