#include "mainwindow.h"
#include "airplayworker.h"
#include "mdns_responder.hpp"
#include <windows.h>
#include <winsvc.h>

#include <QProcess>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QComboBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QTextStream>
#include <QMessageBox>

struct RenameData {
    DWORD pid;
    QString newTitle;
};

// Callback method that Windows calls for every opened window
BOOL CALLBACK EnumWindowsProcRename(HWND hwnd, LPARAM lParam) {
    RenameData *data = reinterpret_cast<RenameData*>(lParam);
    DWORD windowPid;
    GetWindowThreadProcessId(hwnd, &windowPid);

    if (windowPid == data->pid) {
        char windowTitle[512];
        if (GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle))) {
            QString title = QString::fromLocal8Bit(windowTitle);
            
            if (title.contains("Direct") && title.contains("enderer")) {
                printf("found window to rename, setting new name...\n");
                SetWindowTextW(hwnd, reinterpret_cast<const wchar_t*>(data->newTitle.utf16()));
                return FALSE;
            }
        }
    }
    return TRUE;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    ensureSettingsFileExists();
    setupTray();
    setupUI();

    // If Bonjour Service is missing, we must install it; otherwise we exit.
    if (ensureBonjourServiceInstalled()) {
        startServer();
    } else {
        m_quitting = true;
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }
}

MainWindow::~MainWindow() {
    m_quitting = true;
    stopServer();
}

void MainWindow::ensureSettingsFileExists() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appDataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString filePath = appDataPath + "/arguments.txt";
    QFile file(filePath);
    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "-n uxplay-windows -nh";
            file.close();
        }
    }
}

QStringList MainWindow::getArgumentsFromFile() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile file(appDataPath + "/arguments.txt");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QTextStream(&file).readAll().trimmed();
        file.close();
        return content.split(" ", Qt::SkipEmptyParts);
    }
    return QStringList() << "-n" << "uxplay-windows" << "-nh";
}

void MainWindow::setupUI() {
    setWindowTitle("uxplay-windows");
    setWindowIcon(QApplication::windowIcon());
    setFixedSize(300, 220);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    m_statusLabel = new QLabel("Initializing...", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    // Bluetooth Discovery Checkbox
    m_bleCheckbox = new QCheckBox("Enable Bluetooth Discovery", this);
    QSettings settings;
    m_bleCheckbox->setChecked(settings.value("ble_enabled", true).toBool());
    connect(m_bleCheckbox, &QCheckBox::toggled, this, &MainWindow::toggleBle);
    layout->addWidget(m_bleCheckbox);

    // Force Fullscreen Checkbox
    m_fullscreenCheckbox = new QCheckBox("Force Fullscreen", this);
    m_fullscreenCheckbox->setChecked(
        settings.value("force_fs_enabled", false).toBool()
    );
    connect(m_fullscreenCheckbox, &QCheckBox::toggled, this,
            &MainWindow::toggleForceFullscreen);
    layout->addWidget(m_fullscreenCheckbox);

    // Renderer dropdown
    m_rendererCombo = new QComboBox(this);
    m_rendererCombo->addItem("Video Renderer (Auto)", "auto");
    m_rendererCombo->addItem("D3D11", "d3d11");
    m_rendererCombo->addItem("D3D12", "d3d12");

    {
        QString saved = settings.value("renderer_mode", "auto").toString();
        int idx = m_rendererCombo->findData(saved);
        if (idx >= 0) m_rendererCombo->setCurrentIndex(idx);
    }

    connect(m_rendererCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onRendererChanged);

    layout->addWidget(m_rendererCombo);


    m_settingsBtn = new QPushButton("Edit Server Settings", this);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettingsFile);
    layout->addWidget(m_settingsBtn);

    m_autostartBtn = new QPushButton(this);
    connect(m_autostartBtn, &QPushButton::clicked, this, &MainWindow::toggleAutostart);
    layout->addWidget(m_autostartBtn);

    m_licenseBtn = new QPushButton("License Information", this);
    connect(m_licenseBtn, &QPushButton::clicked, this, &MainWindow::showLicense);
    layout->addWidget(m_licenseBtn);

    layout->addStretch();
    updateStatus();
}

void MainWindow::openSettingsFile() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString filePath = appDataPath + "/arguments.txt";
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    
    m_tray->showMessage("Settings", "Restart the app or disconnect current session to apply new arguments.", 
                        QSystemTrayIcon::Information, 3000);
}

void MainWindow::setupTray() {
    m_tray = new QSystemTrayIcon(this);
    QIcon trayIcon;
    QString icoPath = QApplication::applicationDirPath() + "/resources/icon.ico";
    trayIcon = QIcon(icoPath);
    if (trayIcon.isNull()) {
        trayIcon = QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
    }
    m_tray->setIcon(trayIcon);
    m_tray->setToolTip("uxplay-windows");

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction("Quit", this, &MainWindow::quit);

    m_tray->setContextMenu(m_trayMenu);
    
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    m_tray->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        isVisible() ? hide() : showNormal();
    }
}

void MainWindow::toggleBle(bool checked) {
    QSettings settings;
    
    if (settings.value("ble_enabled").toBool() == checked) {
        return;
    }

    settings.setValue("ble_enabled", checked);
    
    QMessageBox::information(this, "uxplay-windows", "Please restart the uxplay-windows to apply changes.\n\n(Right click the tray icon to Quit.)");
}

void MainWindow::toggleForceFullscreen(bool checked) {
    QSettings settings;
    if (settings.value("force_fs_enabled").toBool() == checked) {
        return;
    }

    settings.setValue("force_fs_enabled", checked);
    QMessageBox::information(
        this, "uxplay-windows",
        "Please restart the uxplay-windows to apply changes.\n\n"
        "(Right click the tray icon to Quit.)"
    );
}

void MainWindow::onRendererChanged(int /*index*/) {
    if (!m_rendererCombo) return;

    QString mode = m_rendererCombo->currentData().toString();

    QSettings settings;
    QString saved = settings.value("renderer_mode", "auto").toString();
    if (saved == mode) return;

    settings.setValue("renderer_mode", mode);
    QMessageBox::information(
        this, "uxplay-windows",
        "Please restart the uxplay-windows to apply changes.\n\n"
        "(Right click the tray icon to Quit.)"
    );
}

void MainWindow::applyRendererAndFullscreenArgs(QStringList &args) {
    while (true) {
        int idx = args.indexOf("-fs");
        if (idx < 0) break;
        args.removeAt(idx);
    }
    if (m_fullscreenCheckbox && m_fullscreenCheckbox->isChecked()) {
        args << "-fs";
    }

    // Remove existing "-vs <sink>" pairs
    for (int i = 0; i < args.size();) {
        if (args[i] == "-vs") {
            args.removeAt(i); // -vs
            if (i < args.size()) {
                args.removeAt(i); // sink
            }
            continue;
        }
        ++i;
    }

    QString mode = "auto";
    if (m_rendererCombo) {
        mode = m_rendererCombo->currentData().toString();
    }

    if (mode == "d3d11") {
        args << "-vs" << "d3d11videosink";
    } else if (mode == "d3d12") {
        args << "-vs" << "d3d12videosink";
    }
}

void MainWindow::startServer() {
    if (m_worker && m_worker->isRunning()) return;

    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    
    QStringList args = getArgumentsFromFile();
    
    int bleIdx = args.indexOf("-ble");
    if (bleIdx != -1) {
        args.removeAt(bleIdx);
        if (bleIdx < args.size() && !args[bleIdx].startsWith("-")) {
            args.removeAt(bleIdx);
        }
    }

    applyRendererAndFullscreenArgs(args);

    // Only add -ble and start beacon if checkbox is checked
    if (!m_bleCheckbox) return;
    if (m_bleCheckbox->isChecked()) {
        QString bleFilePath = QDir::toNativeSeparators(appData + "/uxplay_status.ble");
        args << "-ble" << bleFilePath;
        startBluetoothBeacon
    (bleFilePath);
    } else {
        stopBluetoothBeacon();
    }

    m_worker = new AirPlayWorker(this);
    m_worker->setArgs(args);

    connect(m_worker, &AirPlayWorker::started, this, &MainWindow::onAirplayStarted);
    connect(m_worker, &AirPlayWorker::stopped, this, &MainWindow::onAirplayStopped);
    connect(m_worker, &AirPlayWorker::finished, m_worker, &QObject::deleteLater);

    m_worker->start();
}

void MainWindow::stopServer() {
    stopBluetoothBeacon();
    if (m_worker) {
        m_worker->disconnect();
        if (m_worker->isRunning()) {
            QMetaObject::invokeMethod(m_worker, &AirPlayWorker::stopAirplay, Qt::QueuedConnection);
            if (!m_worker->wait(1000)) {
                m_worker->terminate();
            }
        }
        m_worker = nullptr;
    }
    m_running = false;
    updateStatus();
}


void MainWindow::onAirplayStarted() {
    m_running = true;
    updateStatus();

    // rename video window when it pops up
    printf("onAirplayStarted()!\n");
    QTimer *monitorTimer = new QTimer(this);
    connect(monitorTimer, &QTimer::timeout, this, [this, monitorTimer]() {
        if (!m_running) {
            monitorTimer->stop();
            monitorTimer->deleteLater();
            return;
        }

        RenameData info;
        info.pid = GetCurrentProcessId();
        info.newTitle = "AirPlay Video Stream"; 

        EnumWindows(EnumWindowsProcRename, reinterpret_cast<LPARAM>(&info));
    });
    monitorTimer->start(5000);

}

void MainWindow::onAirplayStopped() {
    m_running = false;
    updateStatus();
    
    if (!m_quitting) {
        qDebug() << "Session ended, restarting server to stay ready...";
        QTimer::singleShot(1000, this, &MainWindow::startServer);
    }
}

void MainWindow::onAirplayError(const QString &message) {
    m_tray->showMessage("uxplay-windows", message, QSystemTrayIcon::Warning, 3000);
}

void MainWindow::toggleAutostart() {
    bool enable = !isAutostartEnabled();
    setAutostart(enable);
    if (m_autostartAction)
        m_autostartAction->setChecked(enable);
    updateStatus();
}

bool MainWindow::isAutostartEnabled() const {
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return reg.contains("uxplay-windows");
}

void MainWindow::setAutostart(bool enabled) {
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enabled) {
        QString path = QDir::toNativeSeparators(QApplication::applicationFilePath());
        reg.setValue("uxplay-windows", "\"" + path + "\"");
    } else {
        reg.remove("uxplay-windows");
    }
}

void MainWindow::startBluetoothBeacon(const QString &path) {
    if (m_beacon && m_beacon->state() != QProcess::NotRunning)
        return;

    QString exe = QApplication::applicationDirPath() + "/uxplay-bluetooth-beacon.exe";
    if (!QFile::exists(exe)) {
        qDebug() << "uxplay-bluetooth-beacon.exe not found";
        return;
    }

    m_beacon = new QProcess(this);
    m_beacon->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_beacon, &QProcess::readyRead, this, [this]() {
        // Forward beacon logs to our debug console
        qDebug() << "[beacon output]" << m_beacon->readAll().trimmed();
    });

    // Pass the explicit path to the beacon file
    m_beacon->start(exe, {"--path", path});
    qDebug() << "Beacon process started watching:" << path;
}

void MainWindow::stopBluetoothBeacon() {
    if (!m_beacon) return;
    
    qDebug() << "Stopping beacon process";
    if (m_beacon->state() != QProcess::NotRunning) {
        m_beacon->terminate();
        if (!m_beacon->waitForFinished(500)) {
            qDebug() << "Beacon didn't terminate, killing";
            m_beacon->kill();
            m_beacon->waitForFinished(100);
        }
    }
    
    delete m_beacon;
    m_beacon = nullptr;
    qDebug() << "Beacon stopped and cleaned up";
}

void MainWindow::showLicense() {
    QString path = QApplication::applicationDirPath() + "/LICENSE.md";
    if (QFile::exists(path)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void MainWindow::quit() {
    m_quitting = true;
    stopServer();
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!m_quitting) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::updateStatus() {
    QString status = m_running ? "Server running" : "Server stopped";
    m_statusLabel->setText(status);
    m_autostartBtn->setText(isAutostartEnabled() ? "Disable Autostart" : "Enable Autostart");
}

bool MainWindow::isWindowsServicePresent(const std::wstring& serviceName) const {
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;

    SC_HANDLE hSvc = OpenServiceW(hSCM, serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (!hSvc) {
        DWORD err = GetLastError();
        CloseServiceHandle(hSCM);
        return err != ERROR_SERVICE_DOES_NOT_EXIST;
    }

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return true;
}

bool MainWindow::ensureBonjourServiceInstalled() {
    const std::wstring serviceName = L"Bonjour Service";

    if (isWindowsServicePresent(serviceName)) {
        return true;
    }

    int choice = QMessageBox::question(
        this,
        "Bonjour Service Required",
        "Bonjour Service is required for discovery (mDNS). It is not "
        "installed.\n\nDo you want to install it now?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );

    if (choice != QMessageBox::Yes) {
        QMessageBox::critical(
            this,
            "Bonjour Service Missing",
            "Bonjour Service is required. The application will now exit."
        );
        return false;
    }

    QMessageBox::information(
        this,
        "Installing Bonjour Service",
        "Starting installation of 'Bonjour Service'. Follow any UAC prompt."
    );

    int rc = mdns::MdnsResponder::install();

    // Re-check after install to be safe.
    if (rc == 0 && isWindowsServicePresent(serviceName)) {
        QMessageBox::information(
            this,
            "Installation Complete",
            "Bonjour Service installed successfully.\nThe application will restart."
        );
        restartApplication();
        return false; // do not start server in this instance
    }

    QMessageBox::critical(
        this,
        "Installation Failed",
        "Failed to install 'Bonjour Service'.\n\n"
        "The application will now exit."
    );
    return false;
}

void MainWindow::restartApplication() {
    m_quitting = true;
    stopServer();

    QString exePath = QApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();
    if (!args.isEmpty()) args.removeFirst(); // remove exe path

    QProcess::startDetached(exePath, args);

    // close GUI of current process after a short delay
    QTimer::singleShot(200, qApp, &QCoreApplication::quit);
}
