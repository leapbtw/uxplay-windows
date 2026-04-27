#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QDir>
#include <QProcessEnvironment>

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#endif

int main(int argc, char *argv[]) {
#ifdef _WIN32
    // if the process was started from a console (CMD/PowerShell), attach to it so we can see qDebug() output.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // redirect stdout and stderr to the console
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        std::ios::sync_with_stdio();
    }
#endif

    QApplication app(argc, argv);
    app.setOrganizationName("leapbtw");
    app.setApplicationName("uxplay-windows");
    
    QString appPath = QApplication::applicationDirPath();
    
    QString pluginPath = QDir::toNativeSeparators(appPath + "/lib/gstreamer-1.0");
    qputenv("GST_PLUGIN_PATH", pluginPath.toUtf8());

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = QDir::toNativeSeparators(appPath) + ";" + env.value("PATH");
    qputenv("PATH", path.toUtf8());

    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "Error", "System tray not available.");
        return 1;
    }

    MainWindow window;
    return app.exec();
}