#include "sessionlog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <windows.h>

namespace {
void qtMessage(QtMsgType type, const QMessageLogContext &, const QString &message) {
    const char *level = "DEBUG";
    switch (type) {
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARNING"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    default: break;
    }
    const QByteArray line = (QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
        + " [" + level + "] " + message + '\n').toUtf8();
    // One write prevents other threads from inserting text inside this message.
    _write(_fileno(stderr), line.constData(), unsigned(line.size()));
}

void restoreStream(FILE *stream, int saved) {
    if (saved >= 0) {
        _dup2(saved, _fileno(stream));
        _close(saved);
    } else {
        FILE *unused = nullptr;
        freopen_s(&unused, "NUL", "w", stream);
    }
}
}

bool SessionLog::start(const QString &directory) {
    if (m_redirected) return true;
    QString root = directory;
    if (root.isEmpty()) {
        root = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
            .filePath("uxplay-windows/logs");
    }
    if (!QDir().mkpath(root)) {
        m_error = "Unable to create the log folder: " + QDir::toNativeSeparators(root);
        return false;
    }
    const QString name = "uxplay-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")
        + "-" + QString::number(GetCurrentProcessId())
        + "-" + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".log";
    m_path = QDir(root).absoluteFilePath(name);
    // FILE_APPEND_DATA (without FILE_WRITE_DATA) makes each native write append
    // atomically. CRT append mode alone uses seek + write and can overwrite
    // concurrent stdout/stderr output on Windows.
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(m_path.utf16()),
        FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        m_error = QString("Unable to open the session log (Windows error %1): %2")
            .arg(GetLastError()).arg(QDir::toNativeSeparators(path()));
        return false;
    }
    const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_WRONLY | _O_BINARY);
    if (descriptor < 0) {
        CloseHandle(handle);
        m_error = "Unable to create the session log descriptor.";
        return false;
    }
    if (!m_file.open(descriptor, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
        _close(descriptor);
        m_error = "Unable to use the session log: " + m_file.errorString();
        return false;
    }

    fflush(stdout);
    fflush(stderr);
    if (_fileno(stdout) >= 0) m_stdout = _dup(_fileno(stdout));
    if (_fileno(stderr) >= 0) m_stderr = _dup(_fileno(stderr));
    // GUI launches may have no CRT descriptors at all. Give both streams valid
    // descriptors before duplicating the shared append-only log descriptor.
    FILE *unused = nullptr;
    if (_fileno(stdout) < 0) freopen_s(&unused, "NUL", "w", stdout);
    if (_fileno(stderr) < 0) freopen_s(&unused, "NUL", "w", stderr);
    if (_fileno(stdout) < 0 || _fileno(stderr) < 0
        || _dup2(m_file.handle(), _fileno(stdout)) != 0
        || _dup2(m_file.handle(), _fileno(stderr)) != 0) {
        m_error = "Unable to redirect the application output to the session log.";
        restoreStreams();
        m_file.close();
        return false;
    }
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
    SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
    m_redirected = true;
    m_previousHandler = qInstallMessageHandler(qtMessage);

    // A disk reader tees the combined log to an existing terminal / redirected
    // output. Producers never depend on the GUI pumping events to save logs.
    const int console = m_stdout >= 0 ? m_stdout : m_stderr;
    if (console >= 0) {
        m_consoleRelay = std::thread([this, console, path = path()] {
            QFile reader(path);
            if (!reader.open(QIODevice::ReadOnly)) return;
            for (;;) {
                const bool stopping = m_stopping.load();
                while (true) {
                    const QByteArray chunk = reader.read(64 * 1024);
                    if (chunk.isEmpty()) break;
                    qint64 offset = 0;
                    while (offset < chunk.size()) {
                        const int count = _write(console, chunk.constData() + offset,
                                                 unsigned(chunk.size() - offset));
                        if (count <= 0) return;
                        offset += count;
                    }
                }
                if (stopping) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }
    qInfo("Session started. Log file: %s", QDir::toNativeSeparators(path()).toUtf8().constData());

    // Only prune our session logs, never unrelated files. Active logs in another
    // Windows session remain protected by their open file handles.
    const QFileInfoList logs = QDir(root).entryInfoList({"uxplay-*.log"}, QDir::Files, QDir::Name);
    int remaining = logs.size();
    for (const QFileInfo &log : logs) {
        if (remaining <= 10) break;
        if (log.absoluteFilePath() != m_path
            && QFile::remove(log.absoluteFilePath())) --remaining;
    }
    return true;
}

void SessionLog::restoreStreams() {
    restoreStream(stdout, m_stdout);
    restoreStream(stderr, m_stderr);
    m_stdout = m_stderr = -1;
    SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
    SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
}

SessionLog::~SessionLog() {
    if (!m_redirected) return;
    qInfo("Session ended.");
    qInstallMessageHandler(m_previousHandler);
    fflush(stdout);
    fflush(stderr);
    m_stopping.store(true);
    if (m_consoleRelay.joinable()) m_consoleRelay.join();
    restoreStreams();
}
