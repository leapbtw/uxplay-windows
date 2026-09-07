#include "logviewer.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

LogViewer::LogViewer(const QString &path, QWidget *parent)
    : QDialog(parent), m_file(path) {
    setWindowTitle("Current log — uxplay-windows");
    resize(900, 550);
    auto *layout = new QVBoxLayout(this);
    auto *location = new QLabel(QDir::toNativeSeparators(path), this);
    location->setTextFormat(Qt::PlainText);
    location->setWordWrap(true);
    location->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(location);
    m_text = new QPlainTextEdit(this);
    m_text->setObjectName("logText");
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setMaximumBlockCount(10000);
    layout->addWidget(m_text);
    m_status = new QLabel("Live • Recent 10,000 lines • Copy all includes the entire session", this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *copy = new QPushButton("Copy all", this);
    copy->setObjectName("copyLog");
    connect(copy, &QPushButton::clicked, this, &LogViewer::copyAll);
    buttons->addWidget(copy);
    auto *close = new QPushButton("Close", this);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    if (!m_file.open(QIODevice::ReadOnly)) {
        m_status->setText("Unable to read the log: " + m_file.errorString());
        copy->setEnabled(false);
        return;
    }
    // Avoid freezing the window when debug logging has produced a large file.
    constexpr qint64 initialBytes = 2 * 1024 * 1024;
    if (m_file.size() > initialBytes) {
        m_file.seek(m_file.size() - initialBytes);
        m_file.readLine(); // discard a possibly partial UTF-8 line
    }
    refresh();
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &LogViewer::refresh);
    timer->start(100);
}

void LogViewer::refresh() {
    // A bounded read keeps the window responsive even with packet-level logging.
    const QByteArray bytes = m_file.read(256 * 1024);
    if (bytes.isEmpty()) {
        if (m_file.error() != QFileDevice::NoError)
            m_status->setText("Unable to read the log: " + m_file.errorString());
        return;
    }
    const QString text = m_decoder(bytes);
    auto *scroll = m_text->verticalScrollBar();
    const bool follow = scroll->value() == scroll->maximum();
    const int position = scroll->value();
    // Use a separate cursor to preserve the user's selection while logs arrive.
    QTextCursor cursor(m_text->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    scroll->setValue(follow ? scroll->maximum() : position);
}

void LogViewer::copyAll() {
    QFile file(m_file.fileName());
    if (!file.open(QIODevice::ReadOnly)) {
        m_status->setText("Unable to copy the log: " + file.errorString());
        return;
    }
    // Snapshot the size so continued output does not extend this read forever.
    const QByteArray bytes = file.read(file.size());
    if (file.error() != QFileDevice::NoError) {
        m_status->setText("Unable to copy the log: " + file.errorString());
        return;
    }
    QApplication::clipboard()->setText(QString::fromUtf8(bytes));
    m_status->setText("Copied the entire session. Live updates continue.");
}
