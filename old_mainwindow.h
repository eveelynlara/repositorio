#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QGraphicsView>
#include <QDockWidget>
#include <QFileSystemModel>
#include <QGraphicsScene>
#include <QListWidget>
#include <QVector>
#include <QLabel>
#include <QMap>
#include <QApplication>
#include <exception>

class Entity;
class EntityManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void updateEntityPositions();
    void requestClearPreview() { clearPreview(); }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onProjectItemDoubleClicked(const QModelIndex &index);
    void onEntityItemClicked(QListWidgetItem *item);
    void onTileItemClicked(QListWidgetItem *item);
    void onSceneViewMousePress(QMouseEvent *event);
    void updatePreviewContinuously();
    void exportScene();

private:
    void clearSelection();
    void clearPreview();
    void onSceneViewMouseMove(QMouseEvent *event);
    void highlightSelectedTile();
    void drawGridOnSpritesheet();
    void setupUI();
    void createActions();
    void createMenus();
    void setupProjectExplorer();
    void setupEntityList();
    void setupTileList();
    void setupSceneView();
    void loadEntities();
    void updateEntityPreview();
    void updateTileList();
    void placeEntityInScene(const QPointF &pos);
    void handleException(const QString &context, const std::exception &e);
    void handleTileItemClick(QLabel* spritesheetLabel, const QPoint& pos);
    Entity* getEntityForPixmapItem(QGraphicsPixmapItem* item);
    int getTileIndexForPixmapItem(QGraphicsPixmapItem* item);
    void updateGrid();
    void updatePreviewPosition(const QPointF& scenePos);
    void updateShiftState(bool pressed);

    QPixmap createEntityPixmap(const QSizeF &size);
    QTreeView *m_projectExplorer;
    QFileSystemModel *m_fileSystemModel;
    QGraphicsView *m_sceneView;
    QGraphicsScene *m_scene;
    QDockWidget *m_propertiesDock;
    QListWidget *m_entityList;
    QListWidget *m_tileList;
    QString m_projectPath;
    EntityManager *m_entityManager;
    Entity* m_selectedEntity;
    int m_selectedTileIndex;
    QGraphicsPixmapItem *m_entityPreview;
    QLabel *m_spritesheetLabel;

    QList<QGraphicsItem*> m_gridLines;
    int m_gridSize;
    bool m_shiftPressed;

    struct EntityPlacement {
        Entity* entity;
        int tileIndex;
    };
    QMap<QGraphicsPixmapItem*, EntityPlacement> m_entityPlacements;
    QGraphicsPixmapItem* m_previewItem; // Item para mostrar o preview
    bool m_isPlacingEntity; // Flag para indicar se estamos no modo de colocação
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
