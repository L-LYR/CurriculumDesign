#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QFont font("Consolas",12);
    app.setFont(font);
    MainWindow mainWin;
    mainWin.show();
    return app.exec();
}
