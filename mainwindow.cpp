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
#include <QEnterEvent>
#include <QMap>

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
      m_gridSize(0),
      m_currentTool(SelectTool),
      m_previewUpdateTimer(nullptr),
      m_ctrlPressed(false),
      m_movingItem(nullptr),
      m_entityPreview(nullptr),
      m_oldPosition(),
      updateCount(0),
      m_lastCursorPosition(0, 0)
{
    try {
        m_entityManager = new EntityManager();
        setupUI();
        setupSceneView();
        createActions();
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
    clearPreview();
    
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

void MainWindow::eraseEntity()
{
    if (!m_previewItem) return;

    QRectF eraseRect = m_previewItem->sceneBoundingRect();
    QList<QGraphicsItem*> itemsInRect = m_scene->items(eraseRect, Qt::IntersectsItemShape);
    
    QGraphicsItem* itemToErase = nullptr;
    qreal maxOverlapRatio = 0;
    qreal eraseThreshold = 0.25; // 25% da borracha deve sobrepor para apagar

    qreal eraseArea = eraseRect.width() * eraseRect.height();

    for (QGraphicsItem* item : itemsInRect) {
        if (m_entityPlacements.contains(item) && item != m_previewItem) {
            QRectF intersection = eraseRect.intersected(item->sceneBoundingRect());
            qreal overlap = intersection.width() * intersection.height();
            qreal overlapRatio = overlap / eraseArea;

            if (overlapRatio > eraseThreshold && overlapRatio > maxOverlapRatio) {
                maxOverlapRatio = overlapRatio;
                itemToErase = item;
            }
        }
    }

    if (itemToErase) {
        Action action;
        action.type = Action::REMOVE;
        action.entity = m_entityPlacements[itemToErase].entity;
        action.tileIndex = m_entityPlacements[itemToErase].tileIndex;
        action.oldPos = itemToErase->pos();
        action.entityName = action.entity->getName();
        addAction(action);

        m_scene->removeItem(itemToErase);
        m_entityPlacements.remove(itemToErase);
        delete itemToErase;
        qCInfo(mainWindowCategory) << "Entidade removida e ação adicionada à pilha de undo:" 
                                   << action.entityName << "na posição:" << action.oldPos
                                   << "Sobreposição:" << (maxOverlapRatio * 100) << "%";
    } else {
        qCInfo(mainWindowCategory) << "Nenhuma entidade para apagar na posição do preview:" << eraseRect;
    }
}

QPixmap MainWindow::createErasePreviewPixmap()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setPen(QPen(Qt::red, 2, Qt::DashLine));
    painter.drawRect(0, 0, 31, 31);
    painter.drawLine(0, 0, 31, 31);
    painter.drawLine(0, 31, 31, 0);
    return pixmap;
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

    undoAction = new QAction("Undo", this);
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &MainWindow::undo);

    redoAction = new QAction("Redo", this);
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, &MainWindow::redo);

    // Configurar o explorador de projetos
    setupProjectExplorer();

    // Criar a lista de entidades
    setupEntityList();

    // Criar a lista de tiles
    setupTileList();

    // Criar o painel de propriedades
    m_propertiesDock = new QDockWidget("Propriedades", this);
    QWidget *propertiesWidget = new QWidget(m_propertiesDock);
    QVBoxLayout *propertiesLayout = new QVBoxLayout(propertiesWidget);

    QLabel *posLabel = new QLabel("Posição:", propertiesWidget);
    propertiesLayout->addWidget(posLabel);

    QHBoxLayout *posLayout = new QHBoxLayout();
    m_posXSpinBox = new QDoubleSpinBox(propertiesWidget);
    m_posYSpinBox = new QDoubleSpinBox(propertiesWidget);
    m_posXSpinBox->setRange(-10000, 10000);
    m_posYSpinBox->setRange(-10000, 10000);
    posLayout->addWidget(new QLabel("X:"));
    posLayout->addWidget(m_posXSpinBox);
    posLayout->addWidget(new QLabel("Y:"));
    posLayout->addWidget(m_posYSpinBox);
    propertiesLayout->addLayout(posLayout);

    propertiesWidget->setLayout(propertiesLayout);
    m_propertiesDock->setWidget(propertiesWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    connect(m_posXSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::updateSelectedEntityPosition);
    connect(m_posYSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::updateSelectedEntityPosition);

    // Configurar a barra de status
    this->statusBar()->showMessage("Pronto");
}

void MainWindow::updateSelectedEntityPosition()
{
    if (m_currentTool == SelectTool && m_currentSelectedItem) {
        QPointF newPos(m_posXSpinBox->value(), m_posYSpinBox->value());
        QPointF oldPos = m_currentSelectedItem->pos();
        m_currentSelectedItem->setPos(newPos);
        m_scene->update();
        
        Entity* entity = getEntityForGraphicsItem(m_currentSelectedItem);
        if (entity) {
            Action action;
            action.type = Action::MOVE;
            action.entity = entity;
            action.oldPos = oldPos;
            action.newPos = newPos;
            addAction(action);
            qCInfo(mainWindowCategory) << "Entidade movida:" << entity->getName() 
                                       << "de" << oldPos << "para" << newPos;
        } else {
            qCWarning(mainWindowCategory) << "Não foi possível encontrar a entidade para o item selecionado";
        }
    }
}

Entity* MainWindow::getEntityForGraphicsItem(QGraphicsItem* item)
{
    QGraphicsPixmapItem* pixmapItem = dynamic_cast<QGraphicsPixmapItem*>(item);
    if (pixmapItem) {
        auto it = m_entityPlacements.find(pixmapItem);
        if (it != m_entityPlacements.end()) {
            qCInfo(mainWindowCategory) << "Entidade encontrada para o item na posição:" << pixmapItem->pos();
            return it.value().entity;
        }
    }
    qCWarning(mainWindowCategory) << "Nenhuma entidade encontrada para o item na posição:" << item->pos();
    return nullptr;
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
    clearSelection();
    updatePaintingMode();
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
    QAction *undoAction = new QAction("Undo", this);
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &MainWindow::undo);

    // Ação para refazer
    QAction *redoAction = new QAction("Redo", this);
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, &MainWindow::redo);

    // Ação para abrir um projeto
    QAction *openProjectAction = new QAction("Open Project", this);
    connect(openProjectAction, &QAction::triggered, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Project Directory",
                                                        QDir::homePath(),
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dir.isEmpty()) {
            m_projectPath = dir;
            m_fileSystemModel->setRootPath(m_projectPath);
            m_projectExplorer->setRootIndex(m_fileSystemModel->index(m_projectPath));
            statusBar()->showMessage("Project opened: " + m_projectPath);
            loadEntities();
        }
    });

    // Ação para importar a cena
    QAction *importAction = new QAction("Import Scene", this);
    connect(importAction, &QAction::triggered, this, &MainWindow::importScene);

    // Ação para salvar a cena
    QAction *saveAction = new QAction("Save Scene", this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveScene);

    // Ação para salvar como
    QAction *saveAsAction = new QAction("Save As...", this);
    saveAsAction->setShortcut(QKeySequence::fromString("Ctrl+Shift+S"));
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveSceneAs);

    // Ação para exportar a cena
    QAction *exportAction = new QAction("Export Scene", this);
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportScene);

    // Criar o menu File
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(openProjectAction);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addAction(importAction);
    fileMenu->addAction(exportAction);

    // Criar o menu Edit
    QMenu *editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
}

void MainWindow::activateSelectTool()
{
    m_currentTool = SelectTool;
    updatePaintingMode();
    m_sceneView->setDragMode(QGraphicsView::RubberBandDrag);
    m_sceneView->setCursor(Qt::ArrowCursor);
    clearPreview(); // Limpa qualquer preview existente
    updateToolbarState();
    qCInfo(mainWindowCategory) << "Ferramenta de seleção ativada";
}

// void MainWindow::activateMoveTool()
// {
//     m_currentTool = MoveTool;
//     m_sceneView->setDragMode(QGraphicsView::NoDrag);
//     qCInfo(mainWindowCategory) << "Ferramenta de movimento ativada";
// }

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
    } else if (event->matches(QKeySequence::Undo)) {
        undo();
        event->accept();
    } else if (event->matches(QKeySequence::Redo)) {
        redo();
        event->accept();
    } else if (event->matches(QKeySequence::Save)) {
        saveScene();
        event->accept();
    } else if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Meta) {
        m_ctrlPressed = true;
        if (m_currentTool == BrushTool) {
            updatePreviewPosition(m_lastCursorPosition);
        }
    } else {
        QMainWindow::keyPressEvent(event);
    }
    clearPreviewIfNotBrushTool();
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
    } else if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Meta) {
        m_ctrlPressed = false;
        if (m_currentTool == BrushTool) {
            updatePreviewPosition(m_lastCursorPosition);
        }
    }
    QMainWindow::keyReleaseEvent(event);
    clearPreviewIfNotBrushTool();
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
        QSizeF size = m_previewEntity->getCurrentSize();
        if (size.isEmpty()) {
            size = m_previewEntity->getCollisionSize();
            if (size.isEmpty()) {
                size = QSizeF(32, 32);
            }
        }

        // Remover o preview existente, se houver
        if (m_previewItem) {
            m_scene->removeItem(m_previewItem);
            delete m_previewItem;
            m_previewItem = nullptr;
        }

        if (m_selectedEntity->isInvisible()) {
            QGraphicsRectItem* invisiblePreview = new QGraphicsRectItem(QRectF(0, 0, size.width(), size.height()));
            invisiblePreview->setPen(QPen(Qt::red, 2, Qt::DashLine));
            invisiblePreview->setBrush(Qt::transparent);
            invisiblePreview->setOpacity(0.5);
            invisiblePreview->setZValue(1000);
            m_previewItem = invisiblePreview;
        } else {
            QPixmap previewPixmap = createEntityPixmap(size, m_previewEntity, m_previewTileIndex);
            QGraphicsPixmapItem* pixmapPreview = new QGraphicsPixmapItem(previewPixmap);
            pixmapPreview->setOpacity(0.5);
            pixmapPreview->setZValue(1000);
            m_previewItem = pixmapPreview;
        }

        m_scene->addItem(m_previewItem);
        m_previewItem->show();

        // Atualizar a posição do preview
        updatePreviewPosition(m_lastCursorPosition);

        qCInfo(mainWindowCategory) << "Preview da entidade atualizado para" << m_selectedEntity->getName() 
                                   << "com tamanho" << size << "e tile index" << m_selectedTileIndex;

    } catch (const std::exception& e) {
        handleException("Erro ao atualizar preview da entidade", e);
    }
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
        
        m_previewEntity = m_selectedEntity;
        m_previewTileIndex = m_selectedTileIndex;
        updateEntityPreview();
        
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

void MainWindow::importScene()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Importar Cena"), "", tr("Arquivos de Cena (*.esc);;Todos os Arquivos (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Erro"), tr("Não foi possível abrir o arquivo para leitura."));
        return;
    }

    clearCurrentScene();

    QXmlStreamReader xml(&file);

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (xml.name().compare(QLatin1String("Entity")) == 0) {
                QString entityName;
                QPointF position;
                int spriteFrame = 0;

                QXmlStreamAttributes attributes = xml.attributes();
                if (attributes.hasAttribute(QLatin1String("spriteFrame"))) {
                    spriteFrame = attributes.value(QLatin1String("spriteFrame")).toInt();
                }

                while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
                         xml.name().compare(QLatin1String("Entity")) == 0)) {
                    xml.readNext();

                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        if (xml.name().compare(QLatin1String("EntityName")) == 0) {
                            entityName = xml.readElementText().replace(QLatin1String(".ent"), QString());
                        } else if (xml.name().compare(QLatin1String("Position")) == 0) {
                            QXmlStreamAttributes posAttributes = xml.attributes();
                            qreal x = posAttributes.value(QLatin1String("x")).toDouble();
                            qreal y = posAttributes.value(QLatin1String("y")).toDouble();
                            position = QPointF(x, y);
                        }
                    }
                }

                Entity* entity = m_entityManager->getEntityByName(entityName);
                if (entity) {
                    QSizeF entitySize = entity->getCurrentSize();
                    if (entitySize.isEmpty()) {
                        entitySize = entity->getCollisionSize();
                        if (entitySize.isEmpty()) {
                            entitySize = QSizeF(32, 32);
                        }
                    }
                    QPointF correctedPos = position - QPointF(entitySize.width() / 2, entitySize.height() / 2);
                    
                    // Verificar se o spriteFrame é válido
                    if (spriteFrame < 0 || spriteFrame >= entity->getSpriteDefinitions().size()) {
                        spriteFrame = 0;
                    }
                    
                    // Usar uma função separada para colocar a entidade na cena
                    placeImportedEntityInScene(correctedPos, entity, spriteFrame);
                } else {
                    qCWarning(mainWindowCategory) << "Entidade não encontrada:" << entityName;
                }
            }
        }
    }

    if (xml.hasError()) {
        QMessageBox::warning(this, tr("Erro de XML"), tr("Erro ao ler o arquivo XML: %1").arg(xml.errorString()));
    }

    file.close();
    updateGrid();
    QMessageBox::information(this, tr("Sucesso"), tr("Cena importada com sucesso."));

    m_currentScenePath = fileName;
    qCInfo(mainWindowCategory) << "Cena importada de:" << m_currentScenePath;    
}

void MainWindow::placeImportedEntityInScene(const QPointF &pos, Entity* entity, int tileIndex)
{
    if (!entity) {
        qCWarning(mainWindowCategory) << "Tentativa de colocar entidade nula na cena";
        return;
    }

    QSizeF entitySize = entity->getCurrentSize();
    if (entitySize.isEmpty()) {
        entitySize = entity->getCollisionSize();
        if (entitySize.isEmpty()) {
            entitySize = QSizeF(32, 32);
        }
    }

    QPixmap tilePixmap = createEntityPixmap(entitySize, entity, tileIndex);
    QGraphicsPixmapItem *item = m_scene->addPixmap(tilePixmap);
    if (!item) {
        qCWarning(mainWindowCategory) << "Falha ao adicionar item à cena";
        return;
    }
    item->setPos(pos);
    item->setFlag(QGraphicsItem::ItemIsMovable);
    item->setFlag(QGraphicsItem::ItemIsSelectable);

    EntityPlacement placement;
    placement.entity = entity;
    placement.tileIndex = tileIndex;
    m_entityPlacements[item] = placement;

    qCInfo(mainWindowCategory) << "Entidade importada colocada na cena:" << entity->getName() 
                               << "na posição:" << pos 
                               << "com tile index:" << tileIndex;
}

void MainWindow::clearCurrentScene()
{
    for (auto it = m_entityPlacements.begin(); it != m_entityPlacements.end(); ++it) {
        m_scene->removeItem(it.key());
        delete it.key();
    }
    m_entityPlacements.clear();
    undoStack.clear();
    redoStack.clear();
}

void MainWindow::updatePreviewPosition(const QPointF& scenePos)
{
    if (m_currentTool != BrushTool) {
        clearPreview();
        return;
    }

    m_lastCursorPosition = scenePos;
    if (!m_selectedEntity || !m_previewItem) {
        if (m_selectedEntity && !m_previewItem) {
            updateEntityPreview();
        }
        if (!m_selectedEntity || !m_previewItem) {
            qCWarning(mainWindowCategory) << "updatePreviewPosition: m_selectedEntity ou m_previewItem é nulo";
            return;
        }
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

    if (m_ctrlPressed) {
        // Usar o mesmo preview de borracha para todos os tipos de entidades
        QPixmap erasePixmap = createErasePreviewPixmap();
        if (QGraphicsPixmapItem* pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(m_previewItem)) {
            pixmapItem->setPixmap(erasePixmap);
        } else if (QGraphicsRectItem* rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(m_previewItem)) {
            // Remover o retângulo existente e substituí-lo por um QGraphicsPixmapItem
            m_scene->removeItem(m_previewItem);
            delete m_previewItem;
            m_previewItem = m_scene->addPixmap(erasePixmap);
            m_previewItem->setZValue(1000);
        }
        adjustedPos -= QPointF(16, 16);  // Centraliza o preview da borracha
        m_previewItem->setOpacity(1.0);  // Totalmente visível no modo de apagar
    } else {
        QSizeF entitySize = m_selectedEntity->getCurrentSize();
        if (entitySize.isEmpty()) {
            entitySize = m_selectedEntity->getCollisionSize();
            if (entitySize.isEmpty()) {
                entitySize = QSizeF(32, 32);
            }
        }
        if (QGraphicsPixmapItem* pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(m_previewItem)) {
            pixmapItem->setPixmap(createEntityPixmap(entitySize, m_selectedEntity, m_selectedTileIndex));
        } else if (QGraphicsRectItem* rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(m_previewItem)) {
            rectItem->setRect(QRectF(0, 0, entitySize.width(), entitySize.height()));
            rectItem->setPen(QPen(Qt::red, 2, Qt::DashLine));
            rectItem->setBrush(Qt::transparent);
        }
        m_previewItem->setOpacity(0.5);  // Opacidade normal
    }

    m_previewItem->setPos(adjustedPos);
    m_previewItem->show();

    qCInfo(mainWindowCategory) << "Preview atualizado para posição:" << adjustedPos 
                               << "Shift:" << m_shiftPressed 
                               << "Ctrl:" << m_ctrlPressed;
}

void MainWindow::clearPreviewIfNotBrushTool()
{
    if (m_currentTool != BrushTool) {
        clearPreview();
    }
}

void MainWindow::recoverSceneState()
{
    if (!m_selectedEntity) {
        // Tente selecionar a última entidade usada
        if (!m_entityList->selectedItems().isEmpty()) {
            onEntityItemClicked(m_entityList->selectedItems().first());
        }
    }

    if (!m_previewItem && m_selectedEntity) {
        updateEntityPreview();
    }

    updateGrid();
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

void MainWindow::paintWithBrush(const QPointF &pos)
{
    if (!m_selectedEntity || !m_previewItem) {
        qCInfo(mainWindowCategory) << "paintWithBrush: Nenhuma entidade selecionada ou sem preview";
        return;
    }

    QSizeF entitySize = m_selectedEntity->getCurrentSize();
    if (entitySize.isEmpty()) {
        entitySize = m_selectedEntity->getCollisionSize();
        if (entitySize.isEmpty()) {
            entitySize = QSizeF(32, 32);
        }
    }

    QPointF finalPos = pos;
    QPair<int, int> gridPos;

    if (m_shiftPressed) {
        int gridX = qRound(pos.x() / entitySize.width());
        int gridY = qRound(pos.y() / entitySize.height());
        gridPos = qMakePair(gridX, gridY);
        finalPos = QPointF(gridX * entitySize.width(), gridY * entitySize.height());
    } else {
        gridPos = qMakePair(qRound(pos.x()), qRound(pos.y()));
    }

    qCInfo(mainWindowCategory) << "paintWithBrush: Tentando colocar entidade em" << finalPos << "Modo de pintura:" << m_paintingMode;

    bool canPlace = true;

    if (m_paintingMode && m_occupiedPositions.contains(gridPos)) {
        qCInfo(mainWindowCategory) << "Entidade já existe na posição:" << finalPos << "(Modo de pintura)";
        canPlace = false;
    }

    if (canPlace) {
        QGraphicsItem* newItem = placeEntityInScene(finalPos);
        if (newItem) {
            qCInfo(mainWindowCategory) << "Nova entidade adicionada na posição:" << newItem->pos();
            if (m_paintingMode) {
                m_occupiedPositions.insert(gridPos, true);
            }
        } else {
            qCWarning(mainWindowCategory) << "Falha ao adicionar nova entidade na posição:" << finalPos;
        }
    } else {
        qCInfo(mainWindowCategory) << "Não foi possível colocar a entidade na posição:" << finalPos;
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Captura global de eventos de teclado para o Shift, Ctrl/Cmd e teclas de seta
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Shift) {
            updateShiftState(event->type() == QEvent::KeyPress);
            return true;
        } else if (keyEvent->key() == Qt::Key_Control || keyEvent->key() == Qt::Key_Meta) {
            m_ctrlPressed = (event->type() == QEvent::KeyPress);
            updatePreviewPosition(m_lastCursorPosition);
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
                if (QGraphicsRectItem *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
                    m_movingItem = rectItem;
                } else if (QGraphicsPixmapItem *pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) {
                    m_movingItem = pixmapItem;
                }
                if (m_movingItem) {
                    m_oldPosition = m_movingItem->pos();
                    qCDebug(mainWindowCategory) << "Iniciando potencial movimento de item na posição:" << m_oldPosition;
                }
            }
        } else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            if (m_movingItem) {
                QPointF newPosition = m_movingItem->pos();
                if (newPosition != m_oldPosition) {
                    Action action;
                    action.type = Action::MOVE;
                    action.entity = m_entityPlacements[m_movingItem].entity;
                    action.tileIndex = m_entityPlacements[m_movingItem].tileIndex;
                    action.oldPos = m_oldPosition;
                    action.newPos = newPosition;
                    addAction(action);
                    qCDebug(mainWindowCategory) << "Movimento de item finalizado. Nova posição:" << newPosition;
                } else {
                    qCDebug(mainWindowCategory) << "Item clicado, mas não movido. Posição:" << newPosition;
                }
                m_movingItem = nullptr;
            }
        }
    }
    
    if (watched == m_sceneView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPointF scenePos = m_sceneView->mapToScene(mouseEvent->pos());
            m_lastCursorPosition = scenePos;
            
            if (m_currentTool == BrushTool) {
                updatePreviewPosition(scenePos);
                if (mouseEvent->buttons() & Qt::LeftButton) {
                    if (m_ctrlPressed) {
                        eraseEntity();
                    } else if (m_paintingMode) {
                        paintWithBrush(scenePos);
                    }
                }
            } else if (m_currentTool == SelectTool) {
                updateCursor(scenePos);
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF scenePos = m_sceneView->mapToScene(mouseEvent->pos());
                if (m_currentTool == BrushTool) {
                    if (m_ctrlPressed) {
                        eraseEntity();
                    } else {
                        paintWithBrush(scenePos);
                    }
                    return true;
                } else if (m_currentTool == SelectTool) {
                    // Lógica para selecionar entidades (mantenha o código existente)
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

void MainWindow::clearSelection()
{
    m_scene->clearSelection();
    m_currentSelectedItem = nullptr;
    updatePropertiesPanel();
    qCInfo(mainWindowCategory) << "Seleção limpa";
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
        if (m_ctrlPressed) {
            m_sceneView->setCursor(Qt::ForbiddenCursor);
        } else {
            m_sceneView->setCursor(Qt::CrossCursor);
        }
    }
}

void MainWindow::updateShiftState(bool pressed)
{
    m_shiftPressed = pressed;
    if (!pressed) {
        m_occupiedPositions.clear();
        qCInfo(mainWindowCategory) << "Posições ocupadas resetadas";
    }
    updateGrid();
    updatePaintingMode();
    if (m_previewItem) {
        updatePreviewPosition(m_lastCursorPosition);
    }
    qCInfo(mainWindowCategory) << "Estado do Shift atualizado:" << (pressed ? "pressionado" : "liberado");
}

void MainWindow::updatePaintingMode()
{
    bool oldPaintingMode = m_paintingMode;
    m_paintingMode = (m_currentTool == BrushTool) && m_shiftPressed;
    if (oldPaintingMode != m_paintingMode) {
        qCInfo(mainWindowCategory) << "Modo de pintura alterado:" << (m_paintingMode ? "ativado" : "desativado");
    }
}

void MainWindow::onSceneViewMousePress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selectedEntity) {
        QPointF scenePos = m_sceneView->mapToScene(event->pos());
        placeEntityInScene(scenePos);
    }
}

QGraphicsItem* MainWindow::placeEntityInScene(const QPointF &pos, bool addToUndoStack, Entity* entity, int tileIndex, bool updatePreview)
{
    if (!entity) {
        entity = m_selectedEntity;
        tileIndex = m_selectedTileIndex;
    }

    if (!entity) {
        qCWarning(mainWindowCategory) << "Nenhuma entidade selecionada para colocar na cena";
        return nullptr;
    }

    if (updatePreview) {
        updateEntityPreview();
    }

    try {
        qCInfo(mainWindowCategory) << "Iniciando colocação de entidade:" << entity->getName();
        qCInfo(mainWindowCategory) << "Posição inicial:" << pos;
        qCInfo(mainWindowCategory) << "Entidade é invisível:" << entity->isInvisible();

        QSizeF entitySize = entity->getCurrentSize();
        if (entitySize.isEmpty()) {
            entitySize = entity->getCollisionSize();
            if (entitySize.isEmpty()) {
                entitySize = QSizeF(32, 32);
            }
        }

        QPointF finalPos = pos;
        
        if (m_shiftPressed) {
            qreal gridX = qRound(pos.x() / entitySize.width()) * entitySize.width();
            qreal gridY = qRound(pos.y() / entitySize.height()) * entitySize.height();
            finalPos = QPointF(gridX, gridY);
        }

        QGraphicsItem* item;
        if (entity->isInvisible()) {
            QGraphicsRectItem* invisibleItem = new QGraphicsRectItem(QRectF(0, 0, entitySize.width(), entitySize.height()));
            invisibleItem->setPen(QPen(Qt::red, 2, Qt::DashLine));
            invisibleItem->setBrush(Qt::transparent);
            invisibleItem->setPos(finalPos);
            invisibleItem->setFlag(QGraphicsItem::ItemIsSelectable);
            invisibleItem->setFlag(QGraphicsItem::ItemIsMovable);
            m_scene->addItem(invisibleItem);
            item = invisibleItem;
        } else {
            QPixmap tilePixmap = createEntityPixmap(entitySize, entity, tileIndex);
            QGraphicsPixmapItem *pixmapItem = m_scene->addPixmap(tilePixmap);
            pixmapItem->setPos(finalPos);
            pixmapItem->setFlag(QGraphicsItem::ItemIsMovable);
            pixmapItem->setFlag(QGraphicsItem::ItemIsSelectable);
            item = pixmapItem;
        }

        EntityPlacement placement;
        placement.entity = entity;
        placement.tileIndex = tileIndex;
        placement.item = item;
        m_entityPlacements[item] = placement;
        qCInfo(mainWindowCategory) << "Entidade adicionada ao m_entityPlacements:" 
                           << entity->getName() << "na posição:" << item->pos();

        // qCInfo(mainWindowCategory) << "Placement adicionado ao mapa de entidades. Total de placements:" << m_entityPlacements.size();

        // Adicionar ação para Undo/Redo apenas se addToUndoStack for true
        if (addToUndoStack) {
            Action action;
            action.type = Action::ADD;
            action.entity = entity;
            action.tileIndex = tileIndex;
            action.newPos = finalPos;
            addAction(action);
            qCInfo(mainWindowCategory) << "Ação adicionada para Undo/Redo. Tamanho da pilha de undo:" << undoStack.size();
        }

        updateGrid();
        qCInfo(mainWindowCategory) << "Grade atualizada";

        // Atualizar o preview após colocar a entidade
        updateEntityPreview();
        qCInfo(mainWindowCategory) << "Preview da entidade atualizado";

        qCInfo(mainWindowCategory) << "Entidade colocada na cena na posição:" << finalPos << "com tamanho:" << entitySize;
        logToFile("Entidade colocada na cena: " + entity->getName());

        qCInfo(mainWindowCategory) << "Método placeEntityInScene concluído com sucesso";

        return item;
        
    } catch (const std::exception& e) {
        handleException("Erro ao colocar entidade na cena", e);
    }

    return nullptr; 
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

QPixmap MainWindow::createEntityPixmap(const QSizeF &size, Entity* entity, int tileIndex)
{
    QPixmap pixmap(size.toSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);

    qCInfo(mainWindowCategory) << "Criando pixmap com tamanho:" << size << "e tile index:" << tileIndex;

    if (!entity) {
        qCWarning(mainWindowCategory) << "Entidade nula passada para createEntityPixmap";
        return pixmap;
    }

    if (entity->isInvisible()) {
        pixmap.fill(Qt::transparent);
        painter.setPen(QPen(Qt::red, 2));
        painter.drawRect(pixmap.rect().adjusted(1, 1, -1, -1));
        painter.setFont(QFont("Arial", 8));
        QString text = entity->getName();
        QRectF textRect = painter.boundingRect(pixmap.rect(), Qt::AlignCenter, text);
        if (textRect.width() > pixmap.width() - 4) {
            text = painter.fontMetrics().elidedText(text, Qt::ElideRight, pixmap.width() - 4);
        }
        painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
        return pixmap;
    } else if (entity->hasOnlyCollision()) {
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(pixmap.rect().adjusted(1, 1, -1, -1));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "Collision");
        qCInfo(mainWindowCategory) << "Desenhando entidade apenas com colisão";
    } else {
        QPixmap fullPixmap = entity->getPixmap();
        const QVector<QRectF>& spriteDefinitions = entity->getSpriteDefinitions();
        
        qCInfo(mainWindowCategory) << "Número de sprites:" << spriteDefinitions.size();
        
        if (tileIndex >= 0 && tileIndex < spriteDefinitions.size()) {
            QRectF spriteRect = spriteDefinitions[tileIndex];
            painter.drawPixmap(pixmap.rect(), fullPixmap, spriteRect);
            qCInfo(mainWindowCategory) << "Desenhando sprite" << tileIndex << "na posição" << spriteRect;
        } else {
            qCWarning(mainWindowCategory) << "Índice de tile inválido:" << tileIndex << ". Usando o primeiro sprite.";
            if (!spriteDefinitions.isEmpty()) {
                QRectF spriteRect = spriteDefinitions[0];
                painter.drawPixmap(pixmap.rect(), fullPixmap, spriteRect);
            }
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

void MainWindow::enterEvent(QEnterEvent *event)
{
    QMainWindow::enterEvent(event);
    recoverSceneState();
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
        QGraphicsItem* item = it.key();
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

bool MainWindow::undo()
{
    if (undoStack.isEmpty()) {
        qCInfo(mainWindowCategory) << "Pilha de undo está vazia";
        return false;
    }

    Entity* originalSelectedEntity = m_selectedEntity;
    int originalSelectedTileIndex = m_selectedTileIndex;
    bool actionPerformed = false;

    try {
        preserveCurrentPreview();
        Action action = undoStack.pop();
        
        if (!action.entity) {
            qCWarning(mainWindowCategory) << "Ação inválida encontrada na pilha de undo";
            return false;
        }

        switch (action.type) {
            case Action::ADD:
                {
                    for (auto it = m_entityPlacements.begin(); it != m_entityPlacements.end(); ++it) {
                        if (it.key()->pos() == action.newPos && it.value().entity == action.entity) {
                            QGraphicsItem* item = it.key();
                            m_scene->removeItem(item);
                            m_entityPlacements.erase(it);
                            delete item;
                            actionPerformed = true;
                            qCInfo(mainWindowCategory) << "Entidade removida da cena na posição:" << action.newPos;
                            break;
                        }
                    }
                }
                break;
            case Action::REMOVE:
                {
                    Entity* entityToRestore = m_entityManager->getEntityByName(action.entityName);
                    if (entityToRestore) {
                        m_selectedEntity = entityToRestore;
                        m_selectedTileIndex = action.tileIndex;
                        QGraphicsItem* newItem = placeEntityInScene(action.oldPos, false, entityToRestore, action.tileIndex, false);
                        if (newItem) {
                            actionPerformed = true;
                            qCInfo(mainWindowCategory) << "Entidade restaurada na cena:" << action.entityName
                                                    << "na posição:" << action.oldPos
                                                    << "com tile index:" << action.tileIndex;
                        }
                    }
                }
                break;
            case Action::MOVE:
                {
                    for (auto it = m_entityPlacements.begin(); it != m_entityPlacements.end(); ++it) {
                        if (it.key()->pos() == action.newPos && it.value().entity == action.entity) {
                            it.key()->setPos(action.oldPos);
                            actionPerformed = true;
                            qCInfo(mainWindowCategory) << "Entidade movida de volta para a posição:" << action.oldPos;
                            break;
                        }
                    }
                }
                break;
        }

        m_selectedEntity = originalSelectedEntity;
        m_selectedTileIndex = originalSelectedTileIndex;

        redoStack.push(action);
        if (actionPerformed) {
            updateGrid();
            update();
            qCInfo(mainWindowCategory) << "Undo realizado com sucesso para ação do tipo:" << action.type 
                                       << ". Tamanho da pilha de undo:" << undoStack.size()
                                       << ". Tamanho da pilha de redo:" << redoStack.size();
        }

    } catch (const std::exception& e) {
        qCCritical(mainWindowCategory) << "Erro durante a operação de desfazer:" << e.what();
    } catch (...) {
        qCCritical(mainWindowCategory) << "Erro desconhecido durante a operação de desfazer";
    }

    restorePreservedPreview();
    return actionPerformed;
}

bool MainWindow::redo()
{
    if (redoStack.isEmpty()) {
        qCInfo(mainWindowCategory) << "Pilha de redo está vazia";
        return false;
    }

    Entity* originalSelectedEntity = m_selectedEntity;
    int originalSelectedTileIndex = m_selectedTileIndex;
    bool actionPerformed = false;

    try {
        preserveCurrentPreview();
        Action action = redoStack.pop();
        
        if (!action.entity) {
            qCWarning(mainWindowCategory) << "Ação inválida encontrada na pilha de redo";
            return false;
        }
        switch (action.type) {
            case Action::ADD:
                {
                    m_selectedEntity = action.entity;
                    m_selectedTileIndex = action.tileIndex;
                    QGraphicsItem* newItem = placeEntityInScene(action.newPos, false, action.entity, action.tileIndex, false);
                    if (newItem) {
                        QGraphicsPixmapItem* pixmapItem = dynamic_cast<QGraphicsPixmapItem*>(newItem);
                        if (pixmapItem) {
                            // Use pixmapItem aqui se necessário
                        }
                        qCInfo(mainWindowCategory) << "Entidade restaurada na cena:" << action.entityName
                                                << "na posição:" << action.newPos
                                                << "com tile index:" << action.tileIndex;
                    }
                }
                break;
            case Action::REMOVE:
                {
                    for (auto it = m_entityPlacements.begin(); it != m_entityPlacements.end(); ++it) {
                        if (it.key()->pos() == action.oldPos && it.value().entity->getName() == action.entityName) {
                            QGraphicsItem* item = it.key();
                            m_scene->removeItem(item);
                            m_entityPlacements.erase(it);
                            delete item;
                            actionPerformed = true;
                            qCInfo(mainWindowCategory) << "Entidade removida da cena:" << action.entityName
                                                       << "na posição:" << action.oldPos;
                            break;
                        }
                    }
                }
                break;
            case Action::MOVE:
            {
                for (auto it = m_entityPlacements.begin(); it != m_entityPlacements.end(); ++it) {
                    if (it.key()->pos() == action.oldPos && it.value().entity == action.entity) {
                        QGraphicsItem* item = it.key();
                        item->setPos(action.newPos);
                        actionPerformed = true;
                        qCInfo(mainWindowCategory) << "Entidade movida na cena:" << action.entity->getName()
                                                << "da posição:" << action.oldPos
                                                << "para:" << action.newPos;
                        break;
                    }
                }
            }
            break;
        }

        // Restaure a entidade e tile index originais
        m_selectedEntity = originalSelectedEntity;
        m_selectedTileIndex = originalSelectedTileIndex;
        
        undoStack.push(action);  // Sempre adicione a ação à pilha de undo        
        if (actionPerformed) {
            updateGrid();
            update();
            qCInfo(mainWindowCategory) << "Redo realizado com sucesso para ação do tipo:" << action.type 
                                       << ". Tamanho da pilha de undo:" << undoStack.size()
                                       << ". Tamanho da pilha de redo:" << redoStack.size();
        }

    } catch (const std::exception& e) {
        qCCritical(mainWindowCategory) << "Erro durante a operação de refazer:" << e.what();
    } catch (...) {
        qCCritical(mainWindowCategory) << "Erro desconhecido durante a operação de refazer";
    }

    restorePreservedPreview();
    return actionPerformed;
}

void MainWindow::preserveCurrentPreview()
{
    m_preservedPreviewEntity = m_previewEntity;
    m_preservedPreviewTileIndex = m_previewTileIndex;
    if (QGraphicsPixmapItem* pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(m_previewItem)) {
        m_preservedPreviewPixmap = pixmapItem->pixmap();
    } else {
        m_preservedPreviewPixmap = QPixmap();
    }
}

void MainWindow::restorePreservedPreview()
{
    if (m_preservedPreviewEntity) {
        m_previewEntity = m_preservedPreviewEntity;
        m_previewTileIndex = m_preservedPreviewTileIndex;
        if (QGraphicsPixmapItem* pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(m_previewItem)) {
            if (!m_preservedPreviewPixmap.isNull()) {
                pixmapItem->setPixmap(m_preservedPreviewPixmap);
            }
        } else if (QGraphicsRectItem* rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(m_previewItem)) {
            // Restaurar as propriedades do retângulo, se necessário
            rectItem->setPen(QPen(Qt::red, 2, Qt::DashLine));
            rectItem->setBrush(Qt::transparent);
        }
    }
}

void MainWindow::addAction(const Action& action)
{
    if (action.type == Action::ADD || action.type == Action::REMOVE || action.type == Action::MOVE) {
        if (action.type == Action::MOVE && action.oldPos == action.newPos) {
            qCDebug(mainWindowCategory) << "Ignorando ação de movimento sem mudança de posição";
            return;
        }
        undoStack.push(action);
        redoStack.clear();
        qCInfo(mainWindowCategory) << "Ação adicionada à pilha de undo. Tipo:" << action.type 
                                   << "Posição:" << (action.type == Action::ADD ? action.newPos : action.oldPos)
                                   << "Entidade:" << (action.entity ? action.entity->getName() : "Nenhuma")
                                   << "Tamanho da pilha de undo:" << undoStack.size();
        qCDebug(mainWindowCategory) << "Ação adicionada em" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    } else {
        qCWarning(mainWindowCategory) << "Tentativa de adicionar ação inválida ignorada. Tipo:" << action.type;
    }
}

void MainWindow::checkStackConsistency()
{
    qDebug() << "Verificando consistência das pilhas:";
    qDebug() << "  Tamanho da pilha de undo:" << undoStack.size();
    qDebug() << "  Tamanho da pilha de redo:" << redoStack.size();
    
    // Verifique se todos os itens nas pilhas são válidos
    for (const Action& action : undoStack) {
        if (!action.entity) {
            qWarning() << "Ação inválida encontrada na pilha de undo";
        }
    }
    for (const Action& action : redoStack) {
        if (!action.entity) {
            qWarning() << "Ação inválida encontrada na pilha de redo";
        }
    }
}

void MainWindow::checkConsistency()
{
    qDebug() << "Verificando consistência:";
    qDebug() << "  Itens na cena:" << m_scene->items().count();
    qDebug() << "  Entidades no mapa:" << m_entityPlacements.size();
    qDebug() << "  Tamanho da pilha de undo:" << undoStack.size();
    qDebug() << "  Tamanho da pilha de redo:" << redoStack.size();
}

void MainWindow::saveCrashReport()
{
    QFile file("crash_report.txt");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "Crash Report\n";
        out << "Timestamp: " << QDateTime::currentDateTime().toString() << "\n";
        out << "Número de entidades: " << m_entityPlacements.size() << "\n";
        out << "Estado do Shift: " << (m_shiftPressed ? "Pressionado" : "Liberado") << "\n";
        // Adicione mais informações relevantes
    }
}

void MainWindow::saveScene()
{
    if (m_currentScenePath.isEmpty()) {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Salvar Cena"), 
                                                        m_projectPath, 
                                                        tr("Arquivos de Cena (*.esc)"));
        if (fileName.isEmpty())
            return;
        
        if (!fileName.endsWith(".esc")) {
            fileName += ".esc";
        }
        
        m_currentScenePath = fileName;
    }

    QFile file(m_currentScenePath);
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
            }
        }
    }

    xml.writeEndElement(); // EntitiesInScene

    xml.writeEndElement(); // Ethanon

    xml.writeEndDocument();

    file.close();

    statusBar()->showMessage(tr("Cena salva com sucesso: %1").arg(m_currentScenePath), 3000);
    qCInfo(mainWindowCategory) << "Cena salva em:" << m_currentScenePath;
}

void MainWindow::saveSceneAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Scene As"), 
                                                    m_projectPath, 
                                                    tr("Scene Files (*.esc)"));
    if (fileName.isEmpty())
        return;
    
    if (!fileName.endsWith(".esc")) {
        fileName += ".esc";
    }
    
    m_currentScenePath = fileName;
    saveScene(); 
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

void MainWindow::updatePropertiesPanel()
{
    if (m_currentTool == SelectTool && m_currentSelectedItem) {
        QPointF pos = m_currentSelectedItem->pos();
        m_posXSpinBox->setValue(pos.x());
        m_posYSpinBox->setValue(pos.y());
        m_propertiesDock->setEnabled(true);
        qCInfo(mainWindowCategory) << "Painel de propriedades ativado. Posição:" << pos;
    } else {
        m_propertiesDock->setEnabled(false);
        qCInfo(mainWindowCategory) << "Painel de propriedades desativado";
    }
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
