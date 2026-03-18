#pragma once

#include <QThread>
#include <QStringList>

class AirPlayWorker : public QThread {
    Q_OBJECT

public:
    explicit AirPlayWorker(QObject *parent = nullptr);
    void setArgs(const QStringList &args);
    void stopAirplay();

signals:
    void started();
    void stopped();
    void errorOccurred(const QString &message);

protected:
    void run() override;

private:
    QStringList m_args;
};