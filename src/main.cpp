#include "mainwindow.h"
#include <QApplication>
#include "version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow w;
    w.setWindowTitle(QString("Atari Jaguar RISC Simulator/Debugger  -  v%1 (%2)").arg(APP_VERSION).arg(APP_BUILD_DATE));
    w.show();

    return app.exec();
}
