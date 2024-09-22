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

Q_LOGGING_CATEGORY(mainWindowCategory, "MainWindow")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_projectPath(""), m_selectedEntity(nullptr), 
      m_selectedTileIndex(-1), m_entityPreview(nullptr), m_previewItem(nullptr), m_shiftPressed(false)
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
        QTimer *previewUpdateTimer = new QTimer(this);
        previewUpdateTimer->setInterval(16); // Aproximadamente 60 FPS
        connect(previewUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePreviewContinuously);
        previewUpdateTimer->start();
        
        qApp->installEventFilter(this);

        qCInfo(mainWindowCategory) << "MainWindow inicializado com sucesso";
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

    QAction* exportAction = new QAction("Exportar Cena", this);
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportScene);

    QMenu* fileMenu = menuBar()->addMenu("Arquivo");
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
    if (m_selectedEntity && m_previewItem) {
        QPoint globalPos = QCursor::pos();
        QPoint viewportPos = m_sceneView->viewport()->mapFromGlobal(globalPos);
        QPointF scenePos = m_sceneView->mapToScene(viewportPos);
        updatePreviewPosition(scenePos);
        qCInfo(mainWindowCategory) << "Preview atualizado continuamente para posição:" << scenePos;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift) {
        m_shiftPressed = true;
        updateGrid();
    }
    QMainWindow::keyPressEvent(event);
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
        clearPreview();

        QSizeF size = m_selectedEntity->getCurrentSize();
        if (size.isEmpty()) {
            size = m_selectedEntity->getCollisionSize();
            if (size.isEmpty()) {
                size = QSizeF(32, 32);
            }
        }

        QPixmap previewPixmap(size.toSize());
        previewPixmap.fill(Qt::transparent);
        QPainter painter(&previewPixmap);

        if (m_selectedEntity->isInvisible()) {
            painter.setPen(QPen(Qt::red, 2));
            painter.drawRect(previewPixmap.rect().adjusted(1, 1, -1, -1));
            painter.setFont(QFont("Arial", 8));
            QString text = m_selectedEntity->getName();
            QRectF textRect = painter.boundingRect(previewPixmap.rect(), Qt::AlignCenter, text);
            if (textRect.width() > previewPixmap.width() - 4) {
                text = painter.fontMetrics().elidedText(text, Qt::ElideRight, previewPixmap.width() - 4);
            }
            painter.drawText(previewPixmap.rect(), Qt::AlignCenter, text);
        } else if (m_selectedEntity->hasOnlyCollision()) {
            painter.setPen(QPen(Qt::blue, 2));
            painter.drawRect(previewPixmap.rect().adjusted(1, 1, -1, -1));
            painter.drawText(previewPixmap.rect(), Qt::AlignCenter, "Collision");
        } else {
            QPixmap fullPixmap = m_selectedEntity->getPixmap();
            const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();

            if (m_selectedTileIndex < 0 || m_selectedTileIndex >= spriteDefinitions.size()) {
                qCWarning(mainWindowCategory) << "Índice de tile inválido:" << m_selectedTileIndex;
                return;
            }

            QRectF spriteRect = spriteDefinitions[m_selectedTileIndex];
            painter.drawPixmap(previewPixmap.rect(), fullPixmap, spriteRect);
        }

        m_previewItem = m_scene->addPixmap(previewPixmap);
        m_previewItem->setOpacity(0.5);
        m_previewItem->setZValue(1000); // Garantir que fique acima de outros itens
        m_previewItem->show();

        qCInfo(mainWindowCategory) << "Preview da entidade atualizado para" << m_selectedEntity->getName() 
                                   << "com tamanho" << size;

    } catch (const std::exception& e) {
        handleException("Erro ao atualizar preview da entidade", e);
    }
}

void MainWindow::updatePreviewPosition(const QPointF& scenePos)
{
    if (m_selectedEntity && m_previewItem) {
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
        qCInfo(mainWindowCategory) << "Preview atualizado para posição:" << adjustedPos << "Shift:" << m_shiftPressed;
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
        qCInfo(mainWindowCategory) << "Preview limpo";
    }
}

void MainWindow::clearSelection()
{
    clearPreview();
    m_selectedEntity = nullptr;
    m_selectedTileIndex = -1;
    qCInfo(mainWindowCategory) << "Seleção limpa";
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Captura global de eventos de teclado para o Shift
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Shift) {
            updateShiftState(event->type() == QEvent::KeyPress);
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
            qCInfo(mainWindowCategory) << "Movimento do mouse detectado na cena:" << scenePos;
            updatePreviewPosition(scenePos);
            return true;
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF scenePos = m_sceneView->mapToScene(mouseEvent->pos());
                qCInfo(mainWindowCategory) << "Clique do mouse detectado na cena:" << scenePos;
                placeEntityInScene(scenePos);
                return true;
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

        QPixmap tilePixmap(entitySize.toSize());
        tilePixmap.fill(Qt::transparent);
        QPainter painter(&tilePixmap);

        if (m_selectedEntity->isInvisible()) {
            qCInfo(mainWindowCategory) << "Desenhando entidade invisível";
            painter.setPen(QPen(Qt::red, 2));
            painter.drawRect(tilePixmap.rect().adjusted(1, 1, -1, -1));
            painter.setFont(QFont("Arial", 8));
            QString text = m_selectedEntity->getName();
            QRectF textRect = painter.boundingRect(tilePixmap.rect(), Qt::AlignCenter, text);
            if (textRect.width() > tilePixmap.width() - 4) {
                text = painter.fontMetrics().elidedText(text, Qt::ElideRight, tilePixmap.width() - 4);
            }
            painter.drawText(tilePixmap.rect(), Qt::AlignCenter, text);
        } else if (m_selectedEntity->hasOnlyCollision()) {
            qCInfo(mainWindowCategory) << "Desenhando entidade apenas com colisão";
            painter.setPen(QPen(Qt::blue, 2));
            painter.drawRect(tilePixmap.rect().adjusted(1, 1, -1, -1));
            painter.drawText(tilePixmap.rect(), Qt::AlignCenter, "Collision");
        } else {
            qCInfo(mainWindowCategory) << "Desenhando entidade normal";
            QPixmap fullPixmap = m_selectedEntity->getPixmap();
            const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
            
            if (m_selectedTileIndex < 0 || m_selectedTileIndex >= spriteDefinitions.size()) {
                qCWarning(mainWindowCategory) << "Índice de tile inválido:" << m_selectedTileIndex;
                return;
            }

            QRectF spriteRect = spriteDefinitions[m_selectedTileIndex];
            painter.drawPixmap(tilePixmap.rect(), fullPixmap, spriteRect);
        }

        QGraphicsPixmapItem *item = m_scene->addPixmap(tilePixmap);
        item->setPos(finalPos);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        item->setFlag(QGraphicsItem::ItemIsSelectable);

        EntityPlacement placement;
        placement.entity = m_selectedEntity;
        placement.tileIndex = m_selectedTileIndex;
        m_entityPlacements[item] = placement;

        // Adicionar ação para Undo/Redo
        Action action;
        action.type = Action::ADD;
        action.item = item;
        action.entity = m_selectedEntity;
        action.tileIndex = m_selectedTileIndex;
        action.newPos = finalPos;
        addAction(action);

        updateGrid();

        qCInfo(mainWindowCategory) << "Entidade colocada na cena na posição:" << finalPos << "com tamanho:" << entitySize;
        
    } catch (const std::exception& e) {
        handleException("Erro ao colocar entidade na cena", e);
    }
}

QPixmap MainWindow::createEntityPixmap(const QSizeF &size)
{
    QPixmap pixmap(size.toSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);

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
    } else if (m_selectedEntity->hasOnlyCollision()) {
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(pixmap.rect().adjusted(1, 1, -1, -1));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "Collision");
    } else {
        QPixmap fullPixmap = m_selectedEntity->getPixmap();
        const QVector<QRectF>& spriteDefinitions = m_selectedEntity->getSpriteDefinitions();
        
        if (m_selectedTileIndex >= 0 && m_selectedTileIndex < spriteDefinitions.size()) {
            QRectF spriteRect = spriteDefinitions[m_selectedTileIndex];
            painter.drawPixmap(pixmap.rect(), fullPixmap, spriteRect);
        }
    }

    return pixmap;
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
