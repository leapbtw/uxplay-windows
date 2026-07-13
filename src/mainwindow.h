#pragma once

#include <string>

#include <QLabel>
#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QProcess>
#include <QCheckBox>
#include <QMessageBox>
#include <QTimer>

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
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleAutostart();
    void showLicense();
    void openSettingsFile();
    void openListArgsFile();
    void quit();
    void onAirplayStarted();
    void onAirplayStopped();
    void onAirplayError(const QString &message);
    void toggleBle(bool checked); // bluetooth
    void toggleForceFullscreen(bool checked);
    void onRendererChanged(int index);
    void applyDisplayName();
    void restartApplicationAfterWake();


private:
    void startServer();
    void stopServer();
    void setupTray();
    void setupUI();
    void updateStatus();
    void startBluetoothBeacon(const QString &path);
    void stopBluetoothBeacon();
    QString displayNameFromSettingsOrArgs() const;
    void applyDisplayNameArg(QStringList &args) const;
    void scheduleWakeRestart();
    void applyRendererAndFullscreenArgs(QStringList &args);

    QProcess *m_beacon = nullptr;

    QStringList getArgumentsFromFile() const;
    void ensureSettingsFileExists();
    bool ensureBonjourServiceInstalled();
    bool isWindowsServicePresent(const std::wstring &serviceName) const;
    void restartApplication();

    bool isAutostartEnabled() const;
    void setAutostart(bool enabled);

    QCheckBox *m_bleCheckbox = nullptr;
    QCheckBox *m_fullscreenCheckbox = nullptr;
    QLineEdit *m_displayNameEdit = nullptr;
    QComboBox *m_rendererCombo = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_autostartAction = nullptr;
    QAction *m_statusAction = nullptr;

    QPushButton *m_autostartBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_listargsBtn = nullptr;
    QPushButton *m_licenseBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_wakeRestartTimer = nullptr;

    AirPlayWorker *m_worker = nullptr;

    bool m_running = false;
    bool m_quitting = false;
};
