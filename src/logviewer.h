#pragma once

#include <QDialog>
#include <QFile>
#include <QStringDecoder>

class QLabel;
class QPlainTextEdit;

class LogViewer : public QDialog {
public:
    explicit LogViewer(const QString &path, QWidget *parent = nullptr);

private:
    void refresh();
    void copyAll();
    QFile m_file;
    QStringDecoder m_decoder{QStringDecoder::Utf8};
    QPlainTextEdit *m_text;
    QLabel *m_status;
};
