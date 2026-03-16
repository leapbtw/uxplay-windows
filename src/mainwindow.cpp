#include "mainwindow.h"
#include "airplayworker.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    setupTray();

    m_restartTimer = new QTimer(this);
    m_restartTimer->setInterval(RESTART_INTERVAL_MS);
    connect(m_restartTimer, &QTimer::timeout, this,
            &MainWindow::onRestartTimerTick);

    // Auto-start server on launch
    startServer();
}

MainWindow::~MainWindow() {
    m_quitting = true;
    stopServer();
}

// ── UI ──────────────────────────────────────────────────────────────

void MainWindow::setupUI() {
    setWindowTitle("uxplay-windows");
    setFixedSize(320, 180);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_restartBtn = new QPushButton("Restart Server", this);
    connect(m_restartBtn, &QPushButton::clicked, this,
            &MainWindow::restartServer);
    layout->addWidget(m_restartBtn);

    m_autostartBtn = new QPushButton(this);
    connect(m_autostartBtn, &QPushButton::clicked, this,
            &MainWindow::toggleAutostart);
    layout->addWidget(m_autostartBtn);

    m_licenseBtn = new QPushButton("License", this);
    connect(m_licenseBtn, &QPushButton::clicked, this,
            &MainWindow::showLicense);
    layout->addWidget(m_licenseBtn);

    layout->addStretch();
    updateStatus();
}

// ── Tray ────────────────────────────────────────────────────────────

void MainWindow::setupTray() {
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QApplication::style()->standardIcon(
        QStyle::SP_MediaPlay));
    m_tray->setToolTip("uxplay-windows");

    m_trayMenu = new QMenu(this);

    m_statusAction = m_trayMenu->addAction("Status: starting...");
    m_statusAction->setEnabled(false);
    m_trayMenu->addSeparator();

    m_trayMenu->addAction("Restart Server", this,
                          &MainWindow::restartServer);

    m_autostartAction =
        m_trayMenu->addAction("Run at Startup", this,
                              &MainWindow::toggleAutostart);
    m_autostartAction->setCheckable(true);
    m_autostartAction->setChecked(isAutostartEnabled());

    m_trayMenu->addAction("License", this, &MainWindow::showLicense);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Quit", this, &MainWindow::quit);

    m_tray->setContextMenu(m_trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            &MainWindow::onTrayActivated);

    m_tray->show();
}

void MainWindow::onTrayActivated(
    QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) { // left click
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            activateWindow();
            raise();
        }
    }
}

// ── Server lifecycle ────────────────────────────────────────────────

void MainWindow::startServer() {
    if (m_worker && m_worker->isRunning()) {
        return;
    }

    m_worker = new AirPlayWorker(this);
    m_worker->setServerName("uxplay-windows");

    connect(m_worker, &AirPlayWorker::started, this,
            &MainWindow::onAirplayStarted);
    connect(m_worker, &AirPlayWorker::stopped, this,
            &MainWindow::onAirplayStopped);
    connect(m_worker, &AirPlayWorker::errorOccurred, this,
            &MainWindow::onAirplayError);
    connect(m_worker, &AirPlayWorker::finished, m_worker,
            &QObject::deleteLater);

    m_worker->start();
    m_restartTimer->start();
}

void MainWindow::stopServer() {
    m_restartTimer->stop();
    if (m_worker && m_worker->isRunning()) {
        m_worker->stopAirplay();
        if (!m_worker->wait(1000)) { // Wait 3 seconds
            m_worker->terminate();   // Force kill the thread if it's stuck on 10038
            m_worker->wait();
        }
    }
    m_worker = nullptr;
    m_running = false;
    updateStatus();
}

void MainWindow::restartServer() {
    stopServer();
    startServer();
}

void MainWindow::onRestartTimerTick() {
    // Only restart if the worker is null or not running
    if (!m_worker || !m_worker->isRunning()) {
        startServer();
    }
}

void MainWindow::onAirplayStarted() {
    m_running = true;
    m_restartTimer->start(); // reset the 30-min countdown
    updateStatus();
}

void MainWindow::onAirplayStopped() {
    m_running = false;
    updateStatus();

    // Auto-restart unless we're quitting
    if (!m_quitting) {
        QTimer::singleShot(500, this, &MainWindow::startServer);
    }
}

void MainWindow::onAirplayError(const QString &message) {
    m_running = false;
    updateStatus();
    m_tray->showMessage("uxplay-windows", message,
                        QSystemTrayIcon::Warning, 3000);
}

// ── Autostart (Windows Registry) ────────────────────────────────────

static const QString REG_RUN_KEY =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const QString APP_NAME = "uxplay-windows";

bool MainWindow::isAutostartEnabled() const {
    QSettings reg(REG_RUN_KEY, QSettings::NativeFormat);
    return reg.contains(APP_NAME);
}

void MainWindow::setAutostart(bool enabled) {
    QSettings reg(REG_RUN_KEY, QSettings::NativeFormat);
    if (enabled) {
        QString exePath =
            QDir::toNativeSeparators(QApplication::applicationFilePath());
        reg.setValue(APP_NAME, "\"" + exePath + "\"");
    } else {
        reg.remove(APP_NAME);
    }
}

void MainWindow::toggleAutostart() {
    bool enable = !isAutostartEnabled();
    setAutostart(enable);
    m_autostartAction->setChecked(enable);
    m_autostartBtn->setText(enable ? "Disable Autostart"
                                  : "Enable Autostart");
}

// ── License ─────────────────────────────────────────────────────────

void MainWindow::showLicense() {
    QString licensePath =
        QApplication::applicationDirPath() + "/LICENSE.md";
    if (QFile::exists(licensePath)) {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(licensePath));
    } else {
        QMessageBox::information(
            this, "License",
            "LICENSE.md not found next to the executable.");
    }
}

// ── Quit / Close ────────────────────────────────────────────────────

void MainWindow::quit() {
    m_quitting = true;
    stopServer();
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Close button hides to tray instead of quitting
    if (!m_quitting) {
        hide();
        event->ignore();
        m_tray->showMessage(
            "uxplay-windows", "Still running in the system tray.",
            QSystemTrayIcon::Information, 2000);
    } else {
        event->accept();
    }
}

// ── Status ──────────────────────────────────────────────────────────
void MainWindow::updateStatus() {
    QString status = m_running ? "Server running" : "Server stopped";
    m_statusLabel->setText(status);
    m_autostartBtn->setText(isAutostartEnabled() ? "Disable Autostart"
                                                 : "Enable Autostart");

    if (m_statusAction) {
        m_statusAction->setText("Status: " + status.toLower());
    }
}
