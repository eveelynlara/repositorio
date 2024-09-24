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
#include <QXmlStreamWriter>
#include <QGraphicsView>
#include <QEvent>
#include <QTimer>
#include <QStack>
#include <QGraphicsSceneMouseEvent>
#include <QActionGroup>
#include <QScrollArea>
#include <QAction>

Q_LOGGING_CATEGORY(mainWindowCategory, "MainWindow")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(nullptr),
      m_scene(nullptr),
      m_sceneView(nullptr),
      m_entityManager(nullptr),
      m_projectExplorer(nullptr),
      m_fileSystemModel(nullptr),
      m_entityList(nullptr),
      m_tileList(nullptr),
      m_propertiesDock(nullptr),
      m_spritesheetLabel(nullptr),
      m_projectPath(""),
      m_selectedEntity(nullptr),
      m_selectedTileIndex(-1),
      m_previewItem(nullptr),
      m_shiftPressed(false),
      m_entityPreview(nullptr),
      m_gridSize(0),
      m_currentTool(SelectTool),
      m_movingItem(nullptr),
      m_oldPosition(),
      m_previewUpdateTimer(nullptr),
      m_lastCursorPosition(0, 0),
      updateCount(0)
{
    try {
        m_entityManager = new EntityManager();
        setupUI();
        setupSceneView();
        createActions();
        createMenus();
        setWindowTitle("Editor de Cena");
        resize(1024, 768);
        
        // Criar e configurar o timer para atualização do preview
        // m_previewUpdateTimer = new QTimer(this);
        // m_previewUpdateTimer->setInterval(16); // Aproximadamente 60 FPS
        // connect(m_previewUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePreviewContinuously);
        // m_previewUpdateTimer->start();
        
        qApp->installEventFilter(this);

        qCInfo(mainWindowCategory) << "MainWindow inicializado com sucesso";
    } catch (const std::exception& e) {
        handleException("Erro durante a inicialização", e);
    }
}

MainWindow::~MainWindow()
{
    if (m_previewUpdateTimer) {
        m_previewUpdateTimer->stop();
        delete m_previewUpdateTimer;
    }

    // Limpar todos os itens da cena
    if (m_scene) {
        m_scene->clear();
    }

    // Limpar o mapa de entidades
    m_entityPlacements.clear();

    // Deletar o gerenciador de entidades
    delete m_entityManager;

    qCInfo(mainWindowCategory) << "MainWindow destruído e recursos liberados";
}

void MainWindow::logToFile(const QString& message)
{
    static QFile logFile("app_log.txt");
    if (!logFile.isOpen()) {
        logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    
    QTextStream out(&logFile);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ")
        << message << "\n";
    out.flush();
}

void MainWindow::setupUI()
{
    // Criar o explorador de projetos
    m_projectExplorer = new QTreeView(this);
    QDockWidget *projectDock = new QDockWidget("Explorador de Projeto", this);
    projectDock->setWidget(m_projectExplorer);
    projectDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    this->addDockWidget(Qt::LeftDockWidgetArea, projectDock);

    QAction* exportAction = new QAction("Exportar Cena", this);
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportScene);

    QMenu* fileMenu = this->menuBar()->addMenu("Arquivo");

    fileMenu->addAction(exportAction);

    // Configurar o explorador de projetos
    setupProjectExplorer();

    // Criar a lista de entidades
    setupEntityList();

    // Criar a lista de tiles
    setupTileList();

    // Criar o painel de propriedades
    m_propertiesDock = new QDockWidget("Propriedades", this);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // Configurar a barra de status
    this->statusBar()->showMessage("Pronto");
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
    m_sceneView->setDragMode(QGraphicsView::ScrollHandDrag);

    // Configurar o tamanho da grade (pode ser ajustado posteriormente)
    m_gridSize = 32; // Tamanho da célula da grade em pixels

    // Instalar um event filter no viewport para detectar mudanças de geometria
    m_sceneView->viewport()->installEventFilter(this);

    // Desenhar a grade inicial
    updateGrid();

    setCentralWidget(m_sceneView);
}

void MainWindow::updateGrid()
{
    // Limpar linhas de grade existentes
    for (QGraphicsItem* item : m_gridLines) {
        m_scene->removeItem(item);
        delete item;
    }
    m_gridLines.clear();

    // Mostrar a grade apenas quando o Shift estiver pressionado
    if (m_shiftPressed && m_selectedEntity) {
        // Obter o tamanho da entidade selecionada
        QSizeF entitySize = m_selectedEntity->getCurrentSize();
        if (entitySize.isEmpty()) {
            entitySize = m_selectedEntity->getCollisionSize();
            if (entitySize.isEmpty()) {
                entitySize = QSizeF(32, 32);
            }
        }

        // Obter o tamanho visível da view
        QRectF visibleRect = m_sceneView->mapToScene(m_sceneView->viewport()->rect()).boundingRect();

        // Ajustar a posição inicial da grade com base na posição da câmera
        qreal startX = std::floor(visibleRect.left() / entitySize.width()) * entitySize.width();
        qreal startY = std::floor(visibleRect.top() / entitySize.height()) * entitySize.height();

        // Desenhar linhas verticais
        for (qreal x = startX; x < visibleRect.right(); x += entitySize.width()) {
            QGraphicsLineItem* line = m_scene->addLine(x, visibleRect.top(), x, visibleRect.bottom(), QPen(Qt::lightGray, 1, Qt::DotLine));
            m_gridLines.append(line);
        }

        // Desenhar linhas horizontais
        for (qreal y = startY; y < visibleRect.bottom(); y += entitySize.height()) {
            QGraphicsLineItem* line = m_scene->addLine(visibleRect.left(), y, visibleRect.right(), y, QPen(Qt::lightGray, 1, Qt::DotLine));
            m_gridLines.append(line);
        }

        qCInfo(mainWindowCategory) << "Grade atualizada com tamanho de célula:" << entitySize;
    }
}

void MainWindow::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        double scaleFactor = 1.15;
        if (event->angleDelta().y() < 0) {
            scaleFactor = 1.0 / scaleFactor;
        }
        m_sceneView->scale(scaleFactor, scaleFactor);
        updateGrid(); // Atualizar a grade após o zoom
    } else {
        // Scroll padrão
        QMainWindow::wheelEvent(event);
    }
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

    QVBoxLayout *layout = new QVBoxLayout();

    // Criar a toolbar
    QToolBar *tileToolbar = new QToolBar(this);
    tileToolbar->setIconSize(QSize(24, 24));
    
    // Adicionar ações à toolbar
    QAction *selectAction = tileToolbar->addAction(QIcon(":/select.png"), "Select Tool");
    selectAction->setShortcut(QKeySequence("S"));
    
    QAction *brushAction = tileToolbar->addAction(QIcon(":/brush.png"), "Brush Tool");
    brushAction->setShortcut(QKeySequence("B"));

    // Tornar as ações exclusivas (apenas uma pode estar ativa por vez)
    QActionGroup *toolGroup = new QActionGroup(this);
    toolGroup->addAction(selectAction);
    toolGroup->addAction(brushAction);
    toolGroup->setExclusive(true);

    // Conectar as ações aos slots correspondentes
    connect(selectAction, &QAction::triggered, this, &MainWindow::activateSelectTool);
    connect(brushAction, &QAction::triggered, this, &MainWindow::activateBrushTool);

    // Definir o Brush Tool como padrão
    brushAction->setChecked(true);
    m_currentTool = BrushTool;

    // Adicionar a toolbar ao layout
    layout->addWidget(tileToolbar);
    
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);

    m_spritesheetLabel = new QLabel();
    m_spritesheetLabel->setScaledContents(true);

    scrollArea->setWidget(m_spritesheetLabel);

    layout->addWidget(scrollArea);
    layout->addWidget(m_tileList);

    QWidget *containerWidget = new QWidget();
    containerWidget->setLayout(layout);

    tileDock->setWidget(containerWidget);
    tileDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, tileDock);

    m_spritesheetLabel->installEventFilter(this);

    // Conectar o sinal itemClicked do m_tileList ao slot onTileItemClicked
    connect(m_tileList, &QListWidget::itemClicked, this, &MainWindow::onTileItemClicked);
}

void MainWindow::activateBrushTool()
{
    m_currentTool = BrushTool;
    m_sceneView->setDragMode(QGraphicsView::NoDrag);
    m_sceneView->setCursor(Qt::CrossCursor);
    if (m_previewItem) {
        m_previewItem->show();
    }
    updateToolbarState();
    m_tileList->setFocus(); // Define o foco para a lista de tiles
    qCInfo(mainWindowCategory) << "Ferramenta de pincel ativada";
}

void MainWindow::updateToolbarState()
{
    // Assumindo que você tem ponteiros para as ações das ferramentas como membros da classe
    if (m_selectAction) {
        m_selectAction->setChecked(m_currentTool == SelectTool);
    }
    if (m_brushAction) {
        m_brushAction->setChecked(m_currentTool == BrushTool);
    }
}

void MainWindow::createActions()
{

    // Ação para desfazer
    QAction *undoAction = new QAction("Desfazer", this);
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &MainWindow::undo);

    // Ação para refazer
    QAction *redoAction = new QAction("Refazer", this);
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, &MainWindow::redo);

    // Adicione as ações ao menu Editar
    QMenu *editMenu = menuBar()->addMenu("&Editar");
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);

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

void MainWindow::activateSelectTool()
{
    m_currentTool = SelectTool;
    m_sceneView->setDragMode(QGraphicsView::RubberBandDrag);
    m_sceneView->setCursor(Qt::ArrowCursor);
    if (m_previewItem) {
        m_previewItem->hide();
    }
    qCInfo(mainWindowCategory) << "Ferramenta de seleção ativada";
}

// void MainWindow::activateMoveTool()
// {
//     m_currentTool = MoveTool;
//     m_sceneView->setDragMode(QGraphicsView::NoDrag);
//     qCInfo(mainWindowCategory) << "Ferramenta de movimento ativada";
// }

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

void CustomGraphicsView::leaveEvent(QEvent *event)
{
    QGraphicsView::leaveEvent(event);
    static_cast<MainWindow*>(window())->requestClearPreview();
}

void MainWindow::onEntityItemClicked(QListWidgetItem *item)
{
    clearPreview(); // Limpa a pré-visualização anterior

    if (!item) {
        qCWarning(mainWindowCategory) << "Item clicado é nulo";
        return;
    }

    updateGrid();

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
        ensureBrushToolActive(); // Ativa a ferramenta de pincel
        m_tileList->setFocus(); // Define o foco para a lista de tiles
        qCInfo(mainWindowCategory) << "Entidade selecionada:" << entityName;
        
        // Força a atualização do preview na posição atual do mouse
        QPoint globalPos = QCursor::pos();
        QPoint viewportPos = m_sceneView->viewport()->mapFromGlobal(globalPos);
        QPointF scenePos = m_sceneView->mapToScene(viewportPos);
        qCInfo(mainWindowCategory) << "Posição inicial do preview:" << scenePos;
        updatePreviewPosition(scenePos);
    } catch (const std::exception& e) {
        handleException("Erro ao selecionar entidade", e);
    }
}

void MainWindow::updatePreviewContinuously()
{
    try {
        if (!m_selectedEntity) {
            qCDebug(mainWindowCategory) << "updatePreviewContinuously: Nenhuma entidade selecionada";
            return;
        }
        if (!m_previewItem) {
            qCDebug(mainWindowCategory) << "updatePreviewContinuously: Nenhum item de preview";
            return;
        }
        if (!m_sceneView) {
            qCWarning(mainWindowCategory) << "updatePreviewContinuously: m_sceneView é nulo";
            return;
        }
        if (!m_sceneView->viewport()) {
            qCWarning(mainWindowCategory) << "updatePreviewContinuously: viewport é nulo";
            return;
        }

        QPoint globalPos = QCursor::pos();
        QPoint viewportPos = m_sceneView->viewport()->mapFromGlobal(globalPos);
        QPointF scenePos = m_sceneView->mapToScene(viewportPos);
        
        qCDebug(mainWindowCategory) << "updatePreviewContinuously: Atualizando posição do preview para" << scenePos;
        
        updatePreviewPosition(scenePos);
        
        qCInfo(mainWindowCategory) << "Preview atualizado continuamente para posição:" << scenePos;

        updateCount++;
        if (updateCount >= 100) {
            cleanupResources();
            updateCount = 0;
        }

    } catch (const std::exception& e) {
        handleException("Erro ao atualizar preview continuamente", e);
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift) {
        m_shiftPressed = true;
        updateGrid();
    } else if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && m_currentTool == BrushTool) {
        event->accept(); // Impede que o evento seja propagado para a cena
        if (m_selectedEntity && m_tileList->count() > 0) {
            int currentRow = m_tileList->currentRow();
            int newRow;
            
            if (event->key() == Qt::Key_Up) {
                newRow = (currentRow - 1 + m_tileList->count()) % m_tileList->count();
            } else {
                newRow = (currentRow + 1) % m_tileList->count();
            }
            
            m_tileList->setCurrentRow(newRow);
            QListWidgetItem *newItem = m_tileList->item(newRow);
            if (newItem) {
                m_selectedTileIndex = newItem->data(Qt::UserRole).toInt();
                qCInfo(mainWindowCategory) << "Tecla de seta pressionada. Novo índice de tile:" << m_selectedTileIndex;
                updateEntityPreview();
                highlightSelectedTile();
                updatePreviewPosition(m_lastCursorPosition);
            }
        }
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::ensureBrushToolActive()
{
    if (m_currentTool != BrushTool) {
        m_currentTool = BrushTool;
        // Atualize a interface do usuário para refletir a mudança de ferramenta
        updateToolbarState();
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift) {
        m_shiftPressed = false;
        updateGrid();
    }
    QMainWindow::keyReleaseEvent(event);
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
        activateBrushTool(); // Ativa automaticamente a ferramenta Brush

        // Atualizar a posição do preview se ele existir
        if (m_previewItem) {
            QPoint globalPos = QCursor::pos();
            QPoint viewportPos = m_sceneView->viewport()->mapFromGlobal(globalPos);
            QPointF scenePos = m_sceneView->mapToScene(viewportPos);
            updatePreviewPosition(scenePos);
        }

        qCInfo(mainWindowCategory) << "Tile selecionado:" << tileIndex;
    } catch (const std::exception& e) {
        handleException("Erro ao selecionar tile", e);
    }
}

void MainWindow::highlightSelectedTile()
{
    qCInfo(mainWindowCategory) << "Iniciando highlightSelectedTile com m_selectedTileIndex:" << m_selectedTileIndex;
    
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
    
    qCInfo(mainWindowCategory) << "Tile destacado:" << m_selectedTileIndex;
}

void MainWindow::updateEntityPreview()
{
    qCInfo(mainWindowCategory) << "Iniciando updateEntityPreview com m_selectedTileIndex:" << m_selectedTileIndex;
    
    if (!m_selectedEntity) {
        qCWarning(mainWindowCategory) << "Nenhuma entidade selecionada para atualizar o preview";
        return;
    }

    try {
        QSizeF size = m_selectedEntity->getCurrentSize();
        if (size.isEmpty()) {
            size = m_selectedEntity->getCollisionSize();
            if (size.isEmpty()) {
                size = QSizeF(32, 32);
            }
        }

        QPixmap previewPixmap = createEntityPixmap(size);

        if (!m_previewItem) {
            m_previewItem = m_scene->addPixmap(previewPixmap);
            m_previewItem->setOpacity(0.5);
            m_previewItem->setZValue(1000);
        } else {
            m_previewItem->setPixmap(previewPixmap);
        }

        m_previewItem->show();

        // Atualizar a posição do preview
        updatePreviewPosition(m_lastCursorPosition);

        qCInfo(mainWindowCategory) << "Preview da entidade atualizado para" << m_selectedEntity->getName() 
                                   << "com tamanho" << size << "e tile index" << m_selectedTileIndex;

    } catch (const std::exception& e) {
        handleException("Erro ao atualizar preview da entidade", e);
    }
}

void MainWindow::updatePreviewPosition(const QPointF& scenePos)
{
    m_lastCursorPosition = scenePos;
    if (!m_selectedEntity || !m_previewItem) {
        qCWarning(mainWindowCategory) << "updatePreviewPosition: m_selectedEntity ou m_previewItem é nulo";
        return;
    }

    QPointF adjustedPos = scenePos;
    if (m_shiftPressed) {
        QSizeF entitySize = m_selectedEntity->getCurrentSize();
        if (entitySize.isEmpty()) {
            entitySize = m_selectedEntity->getCollisionSize();
            if (entitySize.isEmpty()) {
                entitySize = QSizeF(32, 32);
            }
        }
        qreal gridX = qRound(scenePos.x() / entitySize.width()) * entitySize.width();
        qreal gridY = qRound(scenePos.y() / entitySize.height()) * entitySize.height();
        adjustedPos = QPointF(gridX, gridY);
    }
    m_previewItem->setPos(adjustedPos);
    m_previewItem->show();
    qCInfo(mainWindowCategory) << "Preview atualizado para posição:" << adjustedPos << "Shift:" << m_shiftPressed << "TileIndex:" << m_selectedTileIndex;
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
        QPixmap entityPixmap;

        if (m_selectedEntity->isInvisible()) {
            QSizeF collisionSize = m_selectedEntity->getCollisionSize();
            if (collisionSize.isEmpty()) {
                collisionSize = QSizeF(32, 32);
            }
            entityPixmap = QPixmap(collisionSize.toSize());
            entityPixmap.fill(Qt::transparent);
            QPainter painter(&entityPixmap);
            painter.setPen(QPen(Qt::red, 2));
            painter.drawRect(entityPixmap.rect().adjusted(1, 1, -1, -1));
            painter.setFont(QFont("Arial", 8));
            painter.drawText(entityPixmap.rect(), Qt::AlignCenter, "Invisible\n" + m_selectedEntity->getName());

            // Carregar e desenhar o ícone de entidade invisível
            QPixmap invisibleIcon(m_projectPath + "/entities/invisible.png");
            if (!invisibleIcon.isNull()) {
                painter.drawPixmap(entityPixmap.rect().center() - QPoint(invisibleIcon.width()/2, invisibleIcon.height()/2), invisibleIcon);
            }
        } else {
            entityPixmap = m_selectedEntity->getPixmap();
        }

        // Mostrar o spritesheet completo no QLabel
        m_spritesheetLabel->setPixmap(entityPixmap);
        m_spritesheetLabel->setFixedSize(entityPixmap.size());

        // Desenhar a grade no spritesheet
        drawGridOnSpritesheet();

        // Adicionar informações sobre os tiles à lista
        const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
        if (m_selectedEntity->isInvisible() || spriteDefinitions.isEmpty()) {
            QListWidgetItem* item = new QListWidgetItem("Invisible Entity");
            item->setData(Qt::UserRole, 0);
            m_tileList->addItem(item);
        } else {
            for (int i = 0; i < spriteDefinitions.size(); ++i) {
                QListWidgetItem* item = new QListWidgetItem(QString("Tile %1").arg(i));
                item->setData(Qt::UserRole, i);
                m_tileList->addItem(item);
                qCInfo(mainWindowCategory) << "Adicionado tile" << i << ":" << spriteDefinitions[i];
            }
        }

        qCInfo(mainWindowCategory) << "Spritesheet atualizado e lista de tiles preenchida";
        qCInfo(mainWindowCategory) << "Número de tiles:" << (m_selectedEntity->isInvisible() ? 1 : spriteDefinitions.size());
        qCInfo(mainWindowCategory) << "Tamanho do pixmap:" << entityPixmap.size();
        qCInfo(mainWindowCategory) << "Entidade é invisível:" << m_selectedEntity->isInvisible();
    } catch (const std::exception& e) {
        handleException("Erro ao atualizar lista de tiles", e);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            clearPreview();
        }
    }
    else if (event->type() == QEvent::ActivationChange) {
        if (!isActiveWindow()) {
            clearPreview();
        }
    }
}

void MainWindow::clearPreview()
{
    if (m_previewItem) {
        m_scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Captura global de eventos de teclado para o Shift e teclas de seta
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Shift) {
            updateShiftState(event->type() == QEvent::KeyPress);
            return true;
        } else if (event->type() == QEvent::KeyPress && 
                   (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) && 
                   m_currentTool == BrushTool) {
            handleArrowKeyPress(keyEvent);
            return true;
        }
    }

    if (watched == m_scene) {
        if (event->type() == QEvent::GraphicsSceneMousePress) {
            QGraphicsSceneMouseEvent *mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
            QGraphicsItem *item = m_scene->itemAt(mouseEvent->scenePos(), QTransform());
            if (item && item->flags() & QGraphicsItem::ItemIsMovable) {
                m_movingItem = dynamic_cast<QGraphicsPixmapItem*>(item);
                m_oldPosition = m_movingItem->pos();
            }
        } else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            if (m_movingItem) {
                Action action;
                action.type = Action::MOVE;
                action.item = m_movingItem;
                action.oldPos = m_oldPosition;
                action.newPos = m_movingItem->pos();
                addAction(action);

                m_movingItem = nullptr;
            }
        }
    }
    
    if (watched == m_sceneView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPointF scenePos = m_sceneView->mapToScene(mouseEvent->pos());
            
            if (m_currentTool == BrushTool) {
                updatePreviewPosition(scenePos);
                if (mouseEvent->buttons() & Qt::LeftButton) {
                    placeEntityInScene(scenePos);
                }
            } else if (m_currentTool == SelectTool) {
                // Atualizar o cursor ou realizar outras ações específicas da ferramenta de seleção
                updateCursor(scenePos);
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF scenePos = m_sceneView->mapToScene(mouseEvent->pos());
                if (m_currentTool == BrushTool) {
                    placeEntityInScene(scenePos);
                    return true;
                } else if (m_currentTool == SelectTool) {
                    // Lógica para selecionar entidades
                    QGraphicsItem *item = m_scene->itemAt(scenePos, QTransform());
                    if (item) {
                        item->setSelected(!item->isSelected());  // Toggle selection
                    } else {
                        m_scene->clearSelection();
                    }
                    return true;
                }
            }
        }
    } else if (watched == m_spritesheetLabel && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            handleTileItemClick(m_spritesheetLabel, mouseEvent->pos());
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::handleArrowKeyPress(QKeyEvent *event)
{
    if (m_selectedEntity && m_tileList->count() > 0) {
        int currentRow = m_tileList->currentRow();
        int newRow;
        
        if (event->key() == Qt::Key_Up) {
            newRow = (currentRow - 1 + m_tileList->count()) % m_tileList->count();
        } else {
            newRow = (currentRow + 1) % m_tileList->count();
        }
        
        m_tileList->setCurrentRow(newRow);
        QListWidgetItem *newItem = m_tileList->item(newRow);
        if (newItem) {
            m_selectedTileIndex = newItem->data(Qt::UserRole).toInt();
            qCInfo(mainWindowCategory) << "Tecla de seta pressionada. Novo índice de tile:" << m_selectedTileIndex;
            updateEntityPreview();
            highlightSelectedTile();
            updatePreviewPosition(m_lastCursorPosition);
        }
    }
}

void MainWindow::updateCursor(const QPointF& scenePos)
{
    if (m_currentTool == SelectTool) {
        QGraphicsItem *item = m_scene->itemAt(scenePos, QTransform());
        if (item) {
            m_sceneView->setCursor(Qt::PointingHandCursor);
        } else {
            m_sceneView->setCursor(Qt::ArrowCursor);
        }
    } else if (m_currentTool == BrushTool) {
        m_sceneView->setCursor(Qt::CrossCursor);
    }
}

void MainWindow::updateShiftState(bool pressed)
{
    m_shiftPressed = pressed;
    updateGrid(); // Atualiza a grid com base no novo estado do Shift
    if (m_previewItem) {
        updatePreviewPosition(m_previewItem->pos()); // Atualiza a posição do preview
    }
    qCInfo(mainWindowCategory) << "Estado do Shift atualizado:" << (pressed ? "pressionado" : "liberado");
}

void MainWindow::onSceneViewMousePress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selectedEntity) {
        QPointF scenePos = m_sceneView->mapToScene(event->pos());
        placeEntityInScene(scenePos);
    }
}

void MainWindow::placeEntityInScene(const QPointF &pos)
{
    if (!m_selectedEntity) {
        qCWarning(mainWindowCategory) << "Nenhuma entidade selecionada para colocar na cena";
        return;
    }

    try {
        qCInfo(mainWindowCategory) << "Iniciando colocação de entidade:" << m_selectedEntity->getName();
        qCInfo(mainWindowCategory) << "Posição inicial:" << pos;
        qCInfo(mainWindowCategory) << "Entidade é invisível:" << m_selectedEntity->isInvisible();

        QSizeF entitySize = m_selectedEntity->getCurrentSize();
        if (entitySize.isEmpty()) {
            entitySize = m_selectedEntity->getCollisionSize();
            if (entitySize.isEmpty()) {
                entitySize = QSizeF(32, 32);
            }
        }
        qCInfo(mainWindowCategory) << "Tamanho da entidade:" << entitySize;

        QPointF finalPos = pos;
        
        if (m_shiftPressed) {
            qreal gridX = qRound(pos.x() / entitySize.width()) * entitySize.width();
            qreal gridY = qRound(pos.y() / entitySize.height()) * entitySize.height();
            finalPos = QPointF(gridX, gridY);
            qCInfo(mainWindowCategory) << "Posição ajustada à grade:" << finalPos;
        }

        QPixmap tilePixmap = createEntityPixmap(entitySize);
        QGraphicsPixmapItem *item = m_scene->addPixmap(tilePixmap);
        item->setPos(finalPos);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        item->setFlag(QGraphicsItem::ItemIsSelectable);

        // Verificar se o item foi adicionado corretamente
        if (!m_scene->items().contains(item)) {
            qCWarning(mainWindowCategory) << "Item não foi adicionado à cena corretamente";
        }

        EntityPlacement placement;
        placement.entity = m_selectedEntity;
        placement.tileIndex = m_selectedTileIndex;
        m_entityPlacements[item] = placement;

        qCInfo(mainWindowCategory) << "Placement adicionado ao mapa de entidades";

        // Adicionar ação para Undo/Redo
        Action action;
        action.type = Action::ADD;
        action.item = item;
        action.entity = m_selectedEntity;
        action.tileIndex = m_selectedTileIndex;
        action.newPos = finalPos;
        addAction(action);
        qCInfo(mainWindowCategory) << "Ação adicionada para Undo/Redo";

        updateGrid();
        qCInfo(mainWindowCategory) << "Grade atualizada";

        // Atualizar o preview após colocar a entidade
        updateEntityPreview();
        qCInfo(mainWindowCategory) << "Preview da entidade atualizado";

        qCInfo(mainWindowCategory) << "Entidade colocada na cena na posição:" << finalPos << "com tamanho:" << entitySize;
        logToFile("Entidade colocada na cena: " + m_selectedEntity->getName());
        
        // Manter o foco na lista de tiles
        m_tileList->setFocus();
        qCInfo(mainWindowCategory) << "Foco definido para a lista de tiles";

        qCInfo(mainWindowCategory) << "Método placeEntityInScene concluído com sucesso";
        
    } catch (const std::exception& e) {
        handleException("Erro ao colocar entidade na cena", e);
    }
}

void MainWindow::updatePreviewIfNeeded()
{
    if (m_selectedEntity && m_previewItem) {
        QPoint globalPos = QCursor::pos();
        QPoint viewportPos = m_sceneView->viewport()->mapFromGlobal(globalPos);
        QPointF scenePos = m_sceneView->mapToScene(viewportPos);
        updatePreviewPosition(scenePos);
    }
}

QPixmap MainWindow::createEntityPixmap(const QSizeF &size)
{
    QPixmap pixmap(size.toSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);

    qCInfo(mainWindowCategory) << "Criando pixmap com tamanho:" << size << "e tile index:" << m_selectedTileIndex;

    if (m_selectedEntity->isInvisible()) {
        painter.setPen(QPen(Qt::red, 2));
        painter.drawRect(pixmap.rect().adjusted(1, 1, -1, -1));
        painter.setFont(QFont("Arial", 8));
        QString text = m_selectedEntity->getName();
        QRectF textRect = painter.boundingRect(pixmap.rect(), Qt::AlignCenter, text);
        if (textRect.width() > pixmap.width() - 4) {
            text = painter.fontMetrics().elidedText(text, Qt::ElideRight, pixmap.width() - 4);
        }
        painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
        qCInfo(mainWindowCategory) << "Desenhando entidade invisível";
    } else if (m_selectedEntity->hasOnlyCollision()) {
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(pixmap.rect().adjusted(1, 1, -1, -1));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "Collision");
        qCInfo(mainWindowCategory) << "Desenhando entidade apenas com colisão";
    } else {
        QPixmap fullPixmap = m_selectedEntity->getPixmap();
        const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
        
        qCInfo(mainWindowCategory) << "Número de sprites:" << spriteDefinitions.size();
        
        if (m_selectedTileIndex >= 0 && m_selectedTileIndex < spriteDefinitions.size()) {
            QRectF spriteRect = spriteDefinitions[m_selectedTileIndex];
            painter.drawPixmap(pixmap.rect(), fullPixmap, spriteRect);
            qCInfo(mainWindowCategory) << "Desenhando sprite" << m_selectedTileIndex << "na posição" << spriteRect;
        } else {
            qCWarning(mainWindowCategory) << "Índice de tile inválido:" << m_selectedTileIndex;
        }
    }

    return pixmap;
}

void MainWindow::cleanupResources()
{
    // Remover itens órfãos da cena
    QList<QGraphicsItem*> orphanItems = m_scene->items();
    for (QGraphicsItem* item : orphanItems) {
        if (!m_entityPlacements.contains(static_cast<QGraphicsPixmapItem*>(item))) {
            m_scene->removeItem(item);
            delete item;
        }
    }

    // Limpar o mapa de entidades
    for (auto it = m_entityPlacements.begin(); it != m_entityPlacements.end();) {
        if (!m_scene->items().contains(it.key())) {
            it = m_entityPlacements.erase(it);
        } else {
            ++it;
        }
    }

    qCInfo(mainWindowCategory) << "Recursos não utilizados foram limpos";
}

void MainWindow::removeSelectedEntities()
{
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        QGraphicsPixmapItem* pixmapItem = dynamic_cast<QGraphicsPixmapItem*>(item);
        if (pixmapItem) {
            Action action;
            action.type = Action::REMOVE;
            action.item = pixmapItem;
            action.entity = getEntityForPixmapItem(pixmapItem);
            action.tileIndex = getTileIndexForPixmapItem(pixmapItem);
            action.oldPos = pixmapItem->pos();
            addAction(action);

            m_scene->removeItem(pixmapItem);
            m_entityPlacements.remove(pixmapItem);
        }
    }
    updateGrid();
}

void MainWindow::leaveEvent(QEvent *event)
{
    QMainWindow::leaveEvent(event);
    clearPreview();
}

void MainWindow::onSceneViewMouseMove(QMouseEvent *event)
{
    if (m_selectedEntity) {
        QPointF scenePos = m_sceneView->mapToScene(event->pos());
        placeEntityInScene(scenePos);
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    QMainWindow::mouseMoveEvent(event);
    updatePreviewIfNeeded();
}

void MainWindow::updateEntityPositions()
{
    for (auto it = m_entityPlacements.begin(); it != m_entityPlacements.end(); ++it) {
        QGraphicsPixmapItem* item = it.key();
        const EntityPlacement& placement = it.value();
        QSizeF tileSize = placement.entity->getCurrentSize();
        QPointF currentPos = item->pos();
        qreal gridX = qRound(currentPos.x() / tileSize.width()) * tileSize.width();
        qreal gridY = qRound(currentPos.y() / tileSize.height()) * tileSize.height();
        item->setPos(gridX, gridY);
    }
}

void MainWindow::handleException(const QString &context, const std::exception &e)
{
    QString errorMessage = QString("%1: %2").arg(context).arg(e.what());
    qCCritical(mainWindowCategory) << errorMessage;
    QMessageBox::critical(this, "Erro", errorMessage);
}

void MainWindow::undo()
{
    if (undoStack.isEmpty()) {
        return;
    }

    Action action = undoStack.pop();
    switch (action.type) {
        case Action::ADD:
            m_scene->removeItem(action.item);
            m_entityPlacements.remove(action.item);
            break;
        case Action::REMOVE:
            m_scene->addItem(action.item);
            m_entityPlacements[action.item] = {action.entity, action.tileIndex};
            break;
        case Action::MOVE:
            action.item->setPos(action.oldPos);
            break;
    }

    redoStack.push(action);
    updateGrid();
}

void MainWindow::redo()
{
    if (redoStack.isEmpty()) {
        return;
    }

    Action action = redoStack.pop();
    switch (action.type) {
        case Action::ADD:
            m_scene->addItem(action.item);
            m_entityPlacements[action.item] = {action.entity, action.tileIndex};
            break;
        case Action::REMOVE:
            m_scene->removeItem(action.item);
            m_entityPlacements.remove(action.item);
            break;
        case Action::MOVE:
            action.item->setPos(action.newPos);
            break;
    }

    undoStack.push(action);
    updateGrid();
}

void MainWindow::addAction(const Action& action)
{
    undoStack.push(action);
    redoStack.clear();
}

void MainWindow::exportScene()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Exportar Cena"), "", tr("Arquivos de Cena (*.esc);;Todos os Arquivos (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Erro"), tr("Não foi possível abrir o arquivo para escrita."));
        return;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();

    xml.writeStartElement("Ethanon");

    // Escrever propriedades da cena
    xml.writeStartElement("SceneProperties");
    xml.writeAttribute("lightIntensity", "2");
    xml.writeAttribute("parallaxIntensity", "0");

    xml.writeStartElement("Ambient");
    xml.writeAttribute("r", "1");
    xml.writeAttribute("g", "1");
    xml.writeAttribute("b", "1");
    xml.writeEndElement(); // Ambient

    xml.writeStartElement("ZAxisDirection");
    xml.writeAttribute("x", "0");
    xml.writeAttribute("y", "-1");
    xml.writeEndElement(); // ZAxisDirection

    xml.writeEndElement(); // SceneProperties

    // Escrever entidades na cena
    xml.writeStartElement("EntitiesInScene");

    int entityId = 1;
    for (const auto& item : m_scene->items()) {
        QGraphicsPixmapItem* pixmapItem = dynamic_cast<QGraphicsPixmapItem*>(item);
        if (pixmapItem) {
            auto it = m_entityPlacements.find(pixmapItem);
            if (it != m_entityPlacements.end()) {
                Entity* entity = it->entity;
                int tileIndex = it->tileIndex;

                xml.writeStartElement("Entity");
                xml.writeAttribute("id", QString::number(entityId++));
                xml.writeAttribute("spriteFrame", QString::number(tileIndex));

                xml.writeTextElement("EntityName", entity->getName() + ".ent");

                // Calcular a posição corrigida
                QSizeF entitySize = entity->getCurrentSize();
                if (entitySize.isEmpty()) {
                    entitySize = entity->getCollisionSize();
                    if (entitySize.isEmpty()) {
                        entitySize = QSizeF(32, 32);
                    }
                }
                QPointF correctedPos = pixmapItem->pos() + QPointF(entitySize.width() / 2, entitySize.height() / 2);

                xml.writeStartElement("Position");
                xml.writeAttribute("x", QString::number(static_cast<int>(correctedPos.x())));
                xml.writeAttribute("y", QString::number(static_cast<int>(correctedPos.y())));
                xml.writeAttribute("z", "0");
                xml.writeAttribute("angle", "0");
                xml.writeEndElement(); // Position

                xml.writeStartElement("Entity");
                xml.writeTextElement("FileName", entity->getName() + ".ent");
                xml.writeEndElement(); // Entity

                xml.writeEndElement(); // Entity

                qCInfo(mainWindowCategory) << "Exportando entidade:" << entity->getName()
                                           << "na posição:" << correctedPos
                                           << "tamanho:" << entitySize;
            }
        }
    }

    xml.writeEndElement(); // EntitiesInScene

    xml.writeEndElement(); // Ethanon

    xml.writeEndDocument();

    file.close();

    QMessageBox::information(this, tr("Sucesso"), tr("Cena exportada com sucesso."));
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
            highlightSelectedTile();
            activateBrushTool(); // Ativa automaticamente a ferramenta Brush
            qCInfo(mainWindowCategory) << "Tile selecionado:" << i;
            return;
        }
    }

    qCWarning(mainWindowCategory) << "Nenhum tile selecionado";
}

Entity* MainWindow::getEntityForPixmapItem(QGraphicsPixmapItem* item)
{
    auto it = m_entityPlacements.find(item);
    if (it != m_entityPlacements.end()) {
        return it->entity;
    }
    return nullptr;
}

int MainWindow::getTileIndexForPixmapItem(QGraphicsPixmapItem* item)
{
    auto it = m_entityPlacements.find(item);
    if (it != m_entityPlacements.end()) {
        return it->tileIndex;
    }
    return -1;
}
