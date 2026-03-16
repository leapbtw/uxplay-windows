#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("uxplay-windows");
    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(
            nullptr, "uxplay-windows",
            "System tray is not available on this system.");
        return 1;
    }

    MainWindow window;
    // don't show window on launch — tray only
    return app.exec();
}
