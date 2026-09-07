#pragma once

#include <QFile>
#include <QString>
#include <QtLogging>
#include <atomic>
#include <thread>

// Redirect at the CRT boundary so Qt, UxPlay, GLib and GStreamer all reach disk,
// including diagnostics emitted before the GUI event loop starts or after it stops.
class SessionLog {
public:
    SessionLog() = default;
    ~SessionLog();
    SessionLog(const SessionLog &) = delete;
    SessionLog &operator=(const SessionLog &) = delete;

    bool start(const QString &directory = {});
    QString path() const { return m_path; }
    QString errorString() const { return m_error; }

private:
    void restoreStreams();
    QFile m_file;
    QString m_path;
    QString m_error;
    int m_stdout = -1;
    int m_stderr = -1;
    bool m_redirected = false;
    QtMessageHandler m_previousHandler = nullptr;
    std::atomic<bool> m_stopping{false};
    std::thread m_consoleRelay;
};
