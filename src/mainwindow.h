#pragma once

#include <QMainWindow>

class QLineEdit;
class QPushButton;
class QLabel;
class AirPlayWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onAirplayStarted();
    void onAirplayStopped();
    void onAirplayError(const QString &message);

private:
    void setRunning(bool running);

    QLineEdit *m_nameEdit;
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
    QLabel *m_statusLabel;
    AirPlayWorker *m_worker = nullptr;
};
