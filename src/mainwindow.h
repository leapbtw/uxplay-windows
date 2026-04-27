#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QProcess>
#include <QCheckBox>
#include <QMessageBox>

class QMenu;
class QAction;
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
    void toggleAutostart();
    void showLicense();
    void openSettingsFile();
    void quit();
    void onAirplayStarted();
    void onAirplayStopped();
    void onAirplayError(const QString &message);
    void toggleBle(bool checked); // bluetooth

private:
    void startServer();
    void stopServer();
    void setupTray();
    void setupUI();
    void updateStatus();
    void startBeacon(const QString &path);
    void stopBeacon();
    
    QProcess *m_beacon = nullptr;

    QStringList getArgumentsFromFile();
    void ensureSettingsFileExists();
    
    bool isAutostartEnabled() const;
    void setAutostart(bool enabled);

    QCheckBox *m_bleCheckbox = nullptr;
    
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_autostartAction = nullptr;
    QAction *m_statusAction = nullptr;

    QPushButton *m_autostartBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_licenseBtn = nullptr;
    QLabel *m_statusLabel = nullptr;

    AirPlayWorker *m_worker = nullptr;

    bool m_running = false;
    bool m_quitting = false;
};
