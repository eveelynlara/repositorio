#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    try {
        QApplication a(argc, argv);
        MainWindow w;
        w.show();
        return a.exec();
    } catch (const std::exception& e) {
        qCritical() << "Exceção não tratada:" << e.what();
        QMessageBox::critical(nullptr, "Erro Fatal", QString("Uma exceção não tratada ocorreu: %1").arg(e.what()));
        return 1;
    } catch (...) {
        qCritical() << "Exceção desconhecida não tratada";
        QMessageBox::critical(nullptr, "Erro Fatal", "Uma exceção desconhecida não tratada ocorreu.");
        return 1;
    }
}