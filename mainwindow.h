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
    QGraphicsPixmapItem* placeEntityInScene(const QPointF &pos, bool addToUndoStack = true);

private:
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
    Entity *m_selectedEntity;
    int m_selectedTileIndex;
    QGraphicsPixmapItem *m_previewItem;
    bool m_shiftPressed;
    int m_gridSize;
    Tool m_currentTool;
    bool m_ctrlPressed;

    struct EntityPlacement {
        Entity *entity;
        int tileIndex;
    };

    void logToFile(const QString& message);
    void cleanupResources();

    // Adicione esta variável de membro
    int updateCount;

    QMap<QGraphicsPixmapItem*, EntityPlacement> m_entityPlacements;

    // Estrutura para armazenar ações
    struct Action {
        enum Type { ADD, REMOVE, MOVE };
        Type type;
        QPointF oldPos;
        QPointF newPos;
        Entity* entity;
        int tileIndex;
    };

    QStack<Action> undoStack;
    QStack<Action> redoStack;
    QGraphicsPixmapItem* m_movingItem;
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
    
    Entity* getEntityForPixmapItem(QGraphicsPixmapItem* item);
    int getTileIndexForPixmapItem(QGraphicsPixmapItem* item);

    bool undo();
    bool redo();
    bool m_paintingMode = false;
    void addAction(const Action& action);
    QPixmap createEntityPixmap(const QSizeF &size);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;

private slots:
    void onProjectItemDoubleClicked(const QModelIndex &index);
    void onEntityItemClicked(QListWidgetItem *item);
    void onTileItemClicked(QListWidgetItem *item);
    void updatePreviewContinuously();
    void exportScene();
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
