#include "mainwindow.h"
#include "airplayworker.h"

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
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QTextStream>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    ensureSettingsFileExists();
    setupTray();
    setupUI();
    startServer();
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
    m_tray->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
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

    // Only add -ble and start beacon if checkbox is checked
    if (!m_bleCheckbox) return;
    if (m_bleCheckbox->isChecked()) {
        QString bleFilePath = QDir::toNativeSeparators(appData + "/uxplay_status.ble");
        args << "-ble" << bleFilePath;
        startBeacon(bleFilePath);
    } else {
        stopBeacon();
    }

    m_worker = new AirPlayWorker(this);
    m_worker->setArgs(args);

    connect(m_worker, &AirPlayWorker::started, this, &MainWindow::onAirplayStarted);
    connect(m_worker, &AirPlayWorker::stopped, this, &MainWindow::onAirplayStopped);
    connect(m_worker, &AirPlayWorker::finished, m_worker, &QObject::deleteLater);

    m_worker->start();
}

void MainWindow::stopServer() {
    stopBeacon();
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

void MainWindow::startBeacon(const QString &path) {
    if (m_beacon && m_beacon->state() != QProcess::NotRunning)
        return;

    QString exe = QApplication::applicationDirPath() + "/uxplay-beacon.exe";
    if (!QFile::exists(exe)) {
        qDebug() << "uxplay-beacon.exe not found";
        return;
    }

    m_beacon = new QProcess(this);
    m_beacon->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_beacon, &QProcess::readyRead, this, [this]() {
        // Forward beacon logs to our debug console
        qDebug() << "[beacon output]" << m_beacon->readAll().trimmed();
    });

    // Pass the explicit path to the beacon
    m_beacon->start(exe, {"--path", path});
    qDebug() << "Beacon process started watching:" << path;
}

void MainWindow::stopBeacon() {
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