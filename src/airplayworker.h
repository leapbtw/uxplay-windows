#pragma once

#include <QThread>
#include <QStringList>

class AirPlayWorker : public QThread {
    Q_OBJECT

public:
    explicit AirPlayWorker(QObject *parent = nullptr);

    void setServerName(const QString &name);
    void setExtraArgs(const QStringList &args);
    void stopAirplay();

signals:
    void started();
    void stopped();
    void errorOccurred(const QString &message);

protected:
    void run() override;

private:
    QAtomicInt m_shouldStop{0};
    QString m_serverName = "uxplay-windows";
    QStringList m_extraArgs;
};
