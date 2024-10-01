#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTreeView>
#include <QFileSystemModel>
#include <QListWidget>
#include <QDockWidget>
#include <QLabel>
#include <QStack>
#include <QDoubleSpinBox>
#include "entitymanager.h"
#include "entity.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QTimer* m_previewUpdateTimer;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void updateEntityPositions();
    void requestClearPreview() { clearPreview(); }
    enum Tool {
        SelectTool,
        MoveTool,
        BrushTool
    };
    QGraphicsItem* placeEntityInScene(const QPointF &pos, bool addToUndoStack = true, Entity* entity = nullptr, int tileIndex = -1, bool updatePreview = true);

private:
    void onSelectionChanged();
    Entity* getEntityForGraphicsItem(QGraphicsItem* item);
    QGraphicsItem* m_currentSelectedItem;
    void updatePropertiesPanel();
    void updateSelectedEntityPosition();
    QList<QGraphicsItem*> m_selectedItems;
    QDoubleSpinBox *m_posXSpinBox;
    QDoubleSpinBox *m_posYSpinBox;
    QPixmap m_preservedPreviewPixmap;
    QPixmap createErasePreviewPixmap();
    Entity* m_preservedPreviewEntity;
    int m_preservedPreviewTileIndex;
    Entity* m_previewEntity;
    int m_previewTileIndex;
    QMap<QPair<int, int>, bool> m_occupiedPositions;
    QAction *m_selectAction;
    QAction *m_brushAction;
    QAction *undoAction;
    QAction *redoAction;
    Ui::MainWindow *ui;
    QGraphicsScene *m_scene;
    QGraphicsView *m_sceneView;
    EntityManager *m_entityManager;
    QTreeView *m_projectExplorer;
    QFileSystemModel *m_fileSystemModel;
    QListWidget *m_entityList;
    QListWidget *m_tileList;
    QDockWidget *m_propertiesDock;
    QLabel *m_spritesheetLabel;
    QString m_projectPath;
    QString m_currentScenePath;
    Entity *m_selectedEntity;
    int m_selectedTileIndex;
    QGraphicsItem* m_previewItem;
    bool m_shiftPressed;
    int m_gridSize;
    Tool m_currentTool;
    bool m_ctrlPressed;

    struct EntityPlacement {
        Entity *entity;
        int tileIndex;
        QGraphicsItem *item;
    };

    void logToFile(const QString& message);
    void cleanupResources();
    void clearPreviewIfNotBrushTool();

    // Adicione esta variável de membro
    int updateCount;

    QMap<QGraphicsItem*, EntityPlacement> m_entityPlacements;

    // Estrutura para armazenar ações
    struct Action {
        enum Type { ADD, REMOVE, MOVE };
        Type type;
        Entity* entity;
        int m_previewTileIndex;
        int tileIndex;
        QPointF oldPos;
        QPointF newPos;
        QString entityName;  // Adicione isso
    };

    QStack<Action> undoStack;
    QStack<Action> redoStack;
    QGraphicsItem* m_movingItem;
    QGraphicsPixmapItem *m_entityPreview;
    QPointF m_oldPosition;
    QVector<QGraphicsLineItem*> m_gridLines;
    QPointF m_lastCursorPosition;

    void updateCursor(const QPointF& scenePos);
    void setupUI();
    void setupProjectExplorer();
    void setupSceneView();
    void setupEntityList();
    void setupTileList();
    void createActions();
    void loadEntities();
    void updateGrid();
    void updateEntityPreview();
    void updatePreviewPosition(const QPointF& scenePos);
    void clearPreview();
    void clearSelection();
    void updateTileList();
    void drawGridOnSpritesheet();
    void highlightSelectedTile();
    void handleException(const QString &context, const std::exception &e);
    void updateToolbarState();
    void updateSpritesheetCursor(const QPoint& pos);
    void handleTileItemClick(QLabel* spritesheetLabel, const QPoint& pos);
    void ensureBrushToolActive();
    void handleArrowKeyPress(QKeyEvent *event);
    void updatePreviewIfNeeded();
    void paintWithBrush(const QPointF &pos);
    void eraseEntity();
    void checkConsistency();
    void saveCrashReport();
    void recoverSceneState();
    void enterEvent(QEvent *event);
    void checkStackConsistency();
    void updatePaintingMode();
    void preserveCurrentPreview();
    void restorePreservedPreview();
    void placeImportedEntityInScene(const QPointF &pos, Entity* entity, int tileIndex);
    
    Entity* getEntityForPixmapItem(QGraphicsPixmapItem* item);
    int getTileIndexForPixmapItem(QGraphicsPixmapItem* item);

    bool undo();
    bool redo();
    bool m_paintingMode = false;
    void addAction(const Action& action);
    QPixmap createEntityPixmap(const QSizeF &size, Entity* entity = nullptr, int tileIndex = -1);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;

public slots:
    void importScene();  

private slots:
    void clearCurrentScene();
    void onProjectItemDoubleClicked(const QModelIndex &index);
    void onEntityItemClicked(QListWidgetItem *item);
    void onTileItemClicked(QListWidgetItem *item);
    void updatePreviewContinuously();
    void exportScene();
    void saveScene();
    void saveSceneAs();
    void onSceneViewMousePress(QMouseEvent *event);
    void onSceneViewMouseMove(QMouseEvent *event);
    void updateShiftState(bool pressed);
    void removeSelectedEntities();
    void activateSelectTool();
    void activateBrushTool();
};

class CustomGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    CustomGraphicsView(QWidget* parent = nullptr) : QGraphicsView(parent) {}
protected:
    void leaveEvent(QEvent *event) override;
};

#endif // MAINWINDOW_H
