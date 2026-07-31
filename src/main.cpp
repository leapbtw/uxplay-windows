#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QSystemTrayIcon>
#include <QTextStream>

#include <gst/gst.h>

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
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

    app.setQuitOnLastWindowClosed(false);

    // Bỏ qua kiểm tra System Tray để có thể chạy ngầm hoàn toàn
    /*
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "Error", "System tray not available.");
        return 1;
    }
    */

    MainWindow window;
    return app.exec();
}
