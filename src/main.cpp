#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QDir>
#include <QProcessEnvironment>
#include <QIcon>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#endif

void customMessageHandler(QtMsgType type, const QMessageLogContext &/*context*/, const QString &msg) {
    QString txt;
    switch (type) {
    case QtDebugMsg:
        txt = QString("Debug: %1").arg(msg);
        break;
    case QtInfoMsg:
        txt = QString("Info: %1").arg(msg);
        break;
    case QtWarningMsg:
        txt = QString("Warning: %1").arg(msg);
        break;
    case QtCriticalMsg:
        txt = QString("Critical: %1").arg(msg);
        break;
    case QtFatalMsg:
        txt = QString("Fatal: %1").arg(msg);
        break;
    }
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/uxplay.log";
    QFile outFile(logPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&outFile);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") << txt << Qt::endl;
        outFile.close();
    }
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
}

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
    qInstallMessageHandler(customMessageHandler);
    app.setOrganizationName("leapbtw");
    app.setApplicationName("uxplay-windows");
    app.setWindowIcon(QIcon(QApplication::applicationDirPath() + "/resources/icon.ico"));
    
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