#pragma once

#include <string>

#include <QLabel>
#include <QMainWindow>
#include <QComboBox>
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
    void toggleForceFullscreen(bool checked);
    void onRendererChanged(int index);


private:
    void startServer();
    void stopServer();
    void setupTray();
    void setupUI();
    void updateStatus();
    void startBluetoothBeacon(const QString &path);
    void stopBluetoothBeacon();
    void applyRendererAndFullscreenArgs(QStringList &args);

    QProcess *m_beacon = nullptr;

    QStringList getArgumentsFromFile();
    void ensureSettingsFileExists();
    bool ensureBonjourServiceInstalled();
    bool isWindowsServicePresent(const std::wstring &serviceName) const;
    void restartApplication();

    bool isAutostartEnabled() const;
    void setAutostart(bool enabled);

    QCheckBox *m_bleCheckbox = nullptr;
    QCheckBox *m_fullscreenCheckbox = nullptr;
    QComboBox *m_rendererCombo = nullptr;
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
