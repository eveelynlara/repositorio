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
#include <exception>

class Entity;
class EntityManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onProjectItemDoubleClicked(const QModelIndex &index);
    void onEntityItemClicked(QListWidgetItem *item);
    void onTileItemClicked(QListWidgetItem *item);
    void onSceneViewMousePress(QMouseEvent *event);
    void exportScene();

private:
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
    bool eventFilter(QObject *watched, QEvent *event) override;
    void handleException(const QString &context, const std::exception &e);
    void handleTileItemClick(QLabel* spritesheetLabel, const QPoint& pos);
    Entity* getEntityForPixmapItem(QGraphicsPixmapItem* item);
    int getTileIndexForPixmapItem(QGraphicsPixmapItem* item);

    QTreeView *m_projectExplorer;
    QFileSystemModel *m_fileSystemModel;
    QGraphicsView *m_sceneView;
    QGraphicsScene *m_scene;
    QDockWidget *m_propertiesDock;
    QListWidget *m_entityList;
    QListWidget *m_tileList;
    QString m_projectPath;
    EntityManager *m_entityManager;
    Entity *m_selectedEntity;
    int m_selectedTileIndex;
    QGraphicsPixmapItem *m_entityPreview;
    QLabel *m_spritesheetLabel;

    struct EntityPlacement {
        Entity* entity;
        int tileIndex;
    };
    QMap<QGraphicsPixmapItem*, EntityPlacement> m_entityPlacements;
};

#endif // MAINWINDOW_H
