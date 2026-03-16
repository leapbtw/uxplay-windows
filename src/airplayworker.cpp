#include "airplayworker.h"
#include "uxplay_api.h"

#include <QDebug>
#include <vector>

AirPlayWorker::AirPlayWorker(QObject *parent) : QThread(parent) {}

void AirPlayWorker::setServerName(const QString &name) {
    m_serverName = name;
}

void AirPlayWorker::setExtraArgs(const QStringList &args) {
    m_extraArgs = args;
}

void AirPlayWorker::run() {
    m_shouldStop = 0;

    QStringList argList;
    argList << "uxplay";
    argList << "-n" << m_serverName;
    argList << "-nh"; // no history

    for (const auto &arg : m_extraArgs) {
        argList << arg;
    }

    // build argc/argv
    std::vector<QByteArray> argBytes;
    std::vector<char *> argv;
    argBytes.reserve(argList.size());
    argv.reserve(argList.size());

    for (const auto &arg : argList) {
        argBytes.push_back(arg.toUtf8());
        argv.push_back(argBytes.back().data());
    }

    int argc = static_cast<int>(argv.size());

    qDebug() << "Starting UxPlay with args:" << argList;
    emit started();

    int ret = start_uxplay(argc, argv.data());

    if (ret != 0) {
        emit errorOccurred(
            QString("UxPlay exited with code %1").arg(ret));
    }

    emit stopped();
}

void AirPlayWorker::stopAirplay() {
    m_shouldStop = 1;
    stop_uxplay();
}
