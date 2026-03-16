#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSystemTrayIcon>

class QMenu;
class QAction;
class QTimer;
class AirPlayWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void restartServer();
    void toggleAutostart();
    void showLicense();
    void quit();
    void onAirplayStarted();
    void onAirplayStopped();
    void onAirplayError(const QString &message);
    void onRestartTimerTick();

private:
    void startServer();
    void stopServer();
    void setupTray();
    void setupUI();
    void updateStatus();
    bool isAutostartEnabled() const;
    void setAutostart(bool enabled);

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_autostartAction = nullptr;
    QAction *m_statusAction = nullptr;

    QPushButton *m_restartBtn = nullptr;
    QPushButton *m_autostartBtn = nullptr;
    QPushButton *m_licenseBtn = nullptr;
    QLabel *m_statusLabel = nullptr;

    AirPlayWorker *m_worker = nullptr;
    QTimer *m_restartTimer = nullptr;

    bool m_running = false;
    bool m_quitting = false;

    static constexpr int RESTART_INTERVAL_MS = 30 * 60 * 1000;
};
