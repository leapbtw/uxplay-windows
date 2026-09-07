#include "airplayworker.h"
#include "uxplay_api.h"
#include <QDebug>
#include <vector>

AirPlayWorker::AirPlayWorker(QObject *parent) : QThread(parent) {}

void AirPlayWorker::setArgs(const QStringList &args) {
    m_args = args;
}

void AirPlayWorker::run() {
    std::vector<QByteArray> argBytes;
    std::vector<char *> argv;
    argBytes.reserve(size_t(m_args.size()) + 1);
    argv.reserve(size_t(m_args.size()) + 2);
    
    argBytes.push_back(QByteArray("uxplay"));
    argv.push_back(argBytes.back().data());

    for (const auto &arg : m_args) {
        if (arg.trimmed().isEmpty()) continue;
        argBytes.push_back(arg.toUtf8());
        argv.push_back(argBytes.back().data());
    }

    const int argc = static_cast<int>(argv.size());
    argv.push_back(nullptr);
    qInfo() << "Starting UxPlay engine";
    emit started();

    int ret = 0;

    // Loop con controllo di interruzione
    while (!isInterruptionRequested()) {
        ret = start_uxplay(argc, argv.data());

        // Se il processo termina, esci dal loop
        if (ret != 0) {
            emit errorOccurred(QString("Engine exited with code %1").arg(ret));
            break;
        }
    }

    emit stopped();
}

void AirPlayWorker::stopAirplay() {
    stop_uxplay();
}
