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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    ensureSettingsFileExists();
    setupUI();
    setupTray();
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
    setFixedSize(300, 180);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    m_statusLabel = new QLabel("Initializing...", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

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
    m_statusAction = m_trayMenu->addAction("Status: offline");
    m_statusAction->setEnabled(false);
    m_trayMenu->addSeparator();

    m_trayMenu->addAction("Edit Settings", this, &MainWindow::openSettingsFile);

    m_autostartAction = m_trayMenu->addAction("Run at Startup", this, &MainWindow::toggleAutostart);
    m_autostartAction->setCheckable(true);
    m_autostartAction->setChecked(isAutostartEnabled());

    m_trayMenu->addSeparator();
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

void MainWindow::startServer() {
    if (m_worker && m_worker->isRunning()) return;

    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    QString bleFilePath = QDir::toNativeSeparators(appData + "/uxplay_status.ble");

    QStringList args = getArgumentsFromFile();
    
    int bleIdx = args.indexOf("-ble");
    if (bleIdx != -1) {
        args.removeAt(bleIdx);
        if (bleIdx < args.size() && !args[bleIdx].startsWith("-")) {
            args.removeAt(bleIdx);
        }
    }
    args << "-ble" << bleFilePath;

    m_worker = new AirPlayWorker(this);
    m_worker->setArgs(args);

    connect(m_worker, &AirPlayWorker::started, this, &MainWindow::onAirplayStarted);
    connect(m_worker, &AirPlayWorker::stopped, this, &MainWindow::onAirplayStopped);
    connect(m_worker, &AirPlayWorker::finished, m_worker, &QObject::deleteLater);

    m_worker->start();

    startBeacon(bleFilePath);
}

void MainWindow::stopServer() {
    stopBeacon();
    if (m_worker) {
        m_worker->disconnect();
        if (m_worker->isRunning()) {
            m_worker->stopAirplay();
            m_worker->wait(2000);
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
        QTimer::singleShot(2000, this, &MainWindow::startServer);
    }
}

void MainWindow::onAirplayError(const QString &message) {
    m_tray->showMessage("uxplay-windows", message, QSystemTrayIcon::Warning, 3000);
}

void MainWindow::toggleAutostart() {
    bool enable = !isAutostartEnabled();
    setAutostart(enable);
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
    if (m_beacon->state() != QProcess::NotRunning) {
        m_beacon->terminate();
        if (!m_beacon->waitForFinished(2000))
            m_beacon->kill();
    }
    m_beacon->deleteLater();
    m_beacon = nullptr;
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
    if (m_statusAction) m_statusAction->setText("Status: " + status.toLower());
}
