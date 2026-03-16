#include "mainwindow.h"
#include "airplayworker.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("uxplay-windows");
    setMinimumSize(400, 200);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *layout = new QVBoxLayout(central);

    // server name row
    auto *nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel("Server Name:"));
    m_nameEdit = new QLineEdit("uxplay-windows");
    nameRow->addWidget(m_nameEdit);
    layout->addLayout(nameRow);

    // buttons row
    auto *btnRow = new QHBoxLayout();
    m_startBtn = new QPushButton("Start");
    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    layout->addLayout(btnRow);

    // status
    m_statusLabel = new QLabel("Stopped");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    layout->addStretch();

    connect(m_startBtn, &QPushButton::clicked, this,
            &MainWindow::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this,
            &MainWindow::onStopClicked);
}

MainWindow::~MainWindow() {
    if (m_worker && m_worker->isRunning()) {
        m_worker->stopAirplay();
        m_worker->wait(5000);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_worker && m_worker->isRunning()) {
        m_worker->stopAirplay();
        m_worker->wait(5000);
    }
    event->accept();
}

void MainWindow::onStartClicked() {
    if (m_worker && m_worker->isRunning()) {
        return;
    }

    m_worker = new AirPlayWorker(this);
    m_worker->setServerName(m_nameEdit->text());

    connect(m_worker, &AirPlayWorker::started, this,
            &MainWindow::onAirplayStarted);
    connect(m_worker, &AirPlayWorker::stopped, this,
            &MainWindow::onAirplayStopped);
    connect(m_worker, &AirPlayWorker::errorOccurred, this,
            &MainWindow::onAirplayError);
    connect(m_worker, &AirPlayWorker::finished, m_worker,
            &QObject::deleteLater);

    m_worker->start();
}

void MainWindow::onStopClicked() {
    if (m_worker && m_worker->isRunning()) {
        m_statusLabel->setText("Stopping...");
        m_worker->stopAirplay();
    }
}

void MainWindow::onAirplayStarted() {
    setRunning(true);
}

void MainWindow::onAirplayStopped() {
    setRunning(false);
}

void MainWindow::onAirplayError(const QString &message) {
    setRunning(false);
    QMessageBox::warning(this, "AirPlay Error", message);
}

void MainWindow::setRunning(bool running) {
    m_startBtn->setEnabled(!running);
    m_stopBtn->setEnabled(running);
    m_nameEdit->setEnabled(!running);
    m_statusLabel->setText(running ? "Running — waiting for connections..."
                                  : "Stopped");
}
