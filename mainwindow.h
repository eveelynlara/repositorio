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

private:
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
};

#endif // MAINWINDOW_H
