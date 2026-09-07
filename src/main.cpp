#include "mainwindow.h"
#include "sessionlog.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QThread>

#include <gst/gst.h>

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#include <io.h>
#endif

#ifdef _WIN32
static DWORD currentSessionId() {
    DWORD sessionId = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId)) {
        sessionId = 0;
    }

    return sessionId;
}

static QString singleInstanceMutexName() {
    return QStringLiteral("Local\\leapbtw.uxplay-windows.%1")
        .arg(currentSessionId());
}

static QString singleInstanceServerName() {
    return QStringLiteral("leapbtw.uxplay-windows.%1")
        .arg(currentSessionId());
}

static bool notifyRunningInstance(const QString &serverName) {
    // The mutex is created just before the pipe starts listening. Retry for a
    // short time so a simultaneous launch cannot lose that startup race.
    QElapsedTimer timer;
    timer.start();

    do {
        QLocalSocket socket;
        socket.connectToServer(serverName, QIODevice::ReadWrite);
        if (socket.waitForConnected(250)) {
            socket.write("activate\n");
            if (!socket.waitForBytesWritten(1000) ||
                !socket.waitForReadyRead(2000)) {
                return false;
            }

            const bool acknowledged = socket.readAll().startsWith("ok\n");
            socket.disconnectFromServer();
            return acknowledged;
        }

        QThread::msleep(50);
    } while (timer.elapsed() < 2000);

    return false;
}

static void activateWindow(MainWindow *window) {
    if (!window) {
        return;
    }

    window->showNormal();
    window->raise();
    window->activateWindow();
}
#endif

static int runRuntimeSelfTest(const QString &appPath) {
    QStringList requiredFiles = {
        "uxplay-bluetooth-beacon.exe",
        "dnssd.dll",
        "mDNSResponder.exe",
        "platforms/qwindows.dll",
        "libexec/gstreamer-1.0/gst-plugin-scanner.exe",
        "resources/gstreamer-features.txt",
        "resources/gstreamer-plugins.json",
        "resources/build-manifest.json",
        "resources/bundle-files.json"
    };

    bool passed = true;
    for (const QString &relativePath : requiredFiles) {
        if (!QFileInfo::exists(QDir(appPath).filePath(relativePath))) {
            fprintf(stderr, "SELF-TEST ERROR: missing %s\n",
                    relativePath.toUtf8().constData());
            passed = false;
        }
    }

#ifdef _WIN32
    const QString dnssdPath = QDir::toNativeSeparators(
        QDir(appPath).filePath("dnssd.dll")
    );
    HMODULE dnssd = LoadLibraryW(
        reinterpret_cast<LPCWSTR>(dnssdPath.utf16())
    );
    if (!dnssd) {
        fprintf(stderr, "SELF-TEST ERROR: dnssd.dll could not be loaded\n");
        passed = false;
    } else {
        FreeLibrary(dnssd);
    }
#endif

    gst_init(nullptr, nullptr);
    GstRegistry *registry = gst_registry_get();
    QFile featureFile(QDir(appPath).filePath(
        "resources/gstreamer-features.txt"
    ));

    if (!featureFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fprintf(stderr, "SELF-TEST ERROR: cannot read GStreamer feature list\n");
        passed = false;
    } else {
        QTextStream stream(&featureFile);
        while (!stream.atEnd()) {
            const QString featureName = stream.readLine().trimmed();
            if (featureName.isEmpty() || featureName.startsWith('#')) {
                continue;
            }

            const QByteArray featureUtf8 = featureName.toUtf8();
            GstPluginFeature *feature = gst_registry_find_feature(
                registry,
                featureUtf8.constData(),
                GST_TYPE_ELEMENT_FACTORY
            );
            if (!feature) {
                fprintf(stderr, "SELF-TEST ERROR: GStreamer feature missing: %s\n",
                        featureUtf8.constData());
                passed = false;
            } else {
                gst_object_unref(feature);
            }
        }
    }

    if (passed) {
        fprintf(stdout, "SELF-TEST OK: runtime bundle is complete\n");
        return 0;
    }
    return 2;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    // if the process was started from a console (CMD/PowerShell), attach to it so we can see qDebug() output.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // Preserve an existing output redirection when attaching the terminal.
        FILE* fp;
        if (_fileno(stdout) < 0) freopen_s(&fp, "CONOUT$", "w", stdout);
        if (_fileno(stderr) < 0) freopen_s(&fp, "CONOUT$", "w", stderr);
        SetConsoleOutputCP(CP_UTF8);
    }
#endif

    // Keep logging alive through QApplication destruction as well.
    SessionLog sessionLog;
    const bool loggingReady = sessionLog.start();
    QApplication app(argc, argv);
    app.setOrganizationName("leapbtw");
    app.setApplicationName("uxplay-windows");
    app.setProperty("sessionLogPath", loggingReady ? sessionLog.path() : QString());
    if (!loggingReady) {
        QMessageBox::warning(nullptr, "Logging unavailable", sessionLog.errorString());
    }
    qputenv("GST_DEBUG_NO_COLOR", "1");
    app.setWindowIcon(QIcon(QApplication::applicationDirPath() + "/resources/icon.ico"));
    
    QString appPath = QApplication::applicationDirPath();
    
    QString pluginPath = QDir::toNativeSeparators(appPath + "/lib/gstreamer-1.0");
    qputenv("GST_PLUGIN_PATH", pluginPath.toUtf8());
    qputenv("GST_PLUGIN_PATH_1_0", pluginPath.toUtf8());
    qputenv("GST_PLUGIN_SYSTEM_PATH", pluginPath.toUtf8());
    qputenv("GST_PLUGIN_SYSTEM_PATH_1_0", pluginPath.toUtf8());

    QString scannerPath = QDir::toNativeSeparators(
        appPath + "/libexec/gstreamer-1.0/gst-plugin-scanner.exe"
    );
    qputenv("GST_PLUGIN_SCANNER", scannerPath.toUtf8());
    qputenv("GST_PLUGIN_SCANNER_1_0", scannerPath.toUtf8());
    qputenv(
        "GIO_EXTRA_MODULES",
        QDir::toNativeSeparators(appPath + "/lib/gio/modules").toUtf8()
    );
    qputenv(
        "FONTCONFIG_PATH",
        QDir::toNativeSeparators(appPath + "/etc/fonts").toUtf8()
    );

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = QDir::toNativeSeparators(appPath) + ";" + env.value("PATH");
    qputenv("PATH", path.toUtf8());

    if (app.arguments().contains("--self-test")) {
        return runRuntimeSelfTest(appPath);
    }

#ifdef _WIN32
    const QString mutexName = singleInstanceMutexName();
    SetLastError(ERROR_SUCCESS);
    HANDLE singleInstanceMutex = CreateMutexW(
        nullptr,
        FALSE,
        reinterpret_cast<LPCWSTR>(mutexName.utf16())
    );
    DWORD mutexError = GetLastError();
    bool alreadyRunning =
        singleInstanceMutex && mutexError == ERROR_ALREADY_EXISTS;

    // An elevated first instance can deny CreateMutex's requested access to a
    // non-elevated second instance. Opening it with minimal access is enough to
    // establish that the first instance exists.
    if (!singleInstanceMutex && mutexError == ERROR_ACCESS_DENIED) {
        singleInstanceMutex = OpenMutexW(
            SYNCHRONIZE,
            FALSE,
            reinterpret_cast<LPCWSTR>(mutexName.utf16())
        );
        alreadyRunning = singleInstanceMutex != nullptr;
        if (!singleInstanceMutex) {
            mutexError = GetLastError();
        }
    }

    if (!singleInstanceMutex) {
        QMessageBox::critical(
            nullptr,
            "uxplay-windows",
            QStringLiteral("Unable to create the single-instance lock "
                           "(Windows error %1).")
                .arg(mutexError)
        );
        return 1;
    }

    const QString serverName = singleInstanceServerName();
    if (alreadyRunning) {
        const bool notified = notifyRunningInstance(serverName);
        CloseHandle(singleInstanceMutex);

        if (!notified) {
            QMessageBox::critical(
                nullptr,
                "uxplay-windows",
                "uxplay-windows is already running but did not respond."
            );
            return 1;
        }
        return 0;
    }

    QLocalServer singleInstanceServer;
    singleInstanceServer.setSocketOptions(QLocalServer::UserAccessOption);

    if (!singleInstanceServer.listen(serverName)) {
        QMessageBox::critical(
            nullptr,
            "uxplay-windows",
            "Unable to create the single-instance communication pipe.\n\n" +
                singleInstanceServer.errorString()
        );
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    QPointer<MainWindow> window;
    bool activationPending = false;
    QObject::connect(
        &singleInstanceServer,
        &QLocalServer::newConnection,
        &app,
        [&singleInstanceServer, &window, &activationPending]() {
            while (singleInstanceServer.hasPendingConnections()) {
                QLocalSocket *socket =
                    singleInstanceServer.nextPendingConnection();
                socket->readAll();

                if (window) {
                    activateWindow(window);
                } else {
                    activationPending = true;
                }

                socket->write("ok\n");
                socket->flush();
                socket->disconnectFromServer();
                socket->deleteLater();
            }
        }
    );
#endif

    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "Error", "System tray not available.");
#ifdef _WIN32
        singleInstanceServer.close();
        CloseHandle(singleInstanceMutex);
#endif
        return 1;
    }

    int exitCode = 0;
    {
        MainWindow mainWindow;
#ifdef _WIN32
        window = &mainWindow;
        if (activationPending) {
            activateWindow(window);
        }
#endif
        exitCode = app.exec();
#ifdef _WIN32
        window.clear();
#endif
    }

    if (exitCode == MainWindow::RestartExitCode) {
#ifdef _WIN32
        // The pipe must be gone before the replacement checks for an existing
        // instance.
        singleInstanceServer.close();
        CloseHandle(singleInstanceMutex);
        singleInstanceMutex = nullptr;
#endif
        QStringList arguments = QCoreApplication::arguments();
        if (!arguments.isEmpty()) {
            arguments.removeFirst();
        }

        if (!QProcess::startDetached(
                QCoreApplication::applicationFilePath(), arguments)) {
            QMessageBox::critical(
                nullptr,
                "uxplay-windows",
                "Unable to restart uxplay-windows."
            );
            return 1;
        }
        return 0;
    }

#ifdef _WIN32
    singleInstanceServer.close();
    CloseHandle(singleInstanceMutex);
#endif
    return exitCode;
}
