#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("uxplay-windows");

    MainWindow window;
    window.show();

    return app.exec();
}
