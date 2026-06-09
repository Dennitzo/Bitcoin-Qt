#pragma once

#include <QCheckBox>
#include <QLineEdit>
#include <QMap>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QWidget>

class LogsPage final : public QWidget {
    Q_OBJECT

public:
    explicit LogsPage(QWidget* parent = nullptr);

public Q_SLOTS:
    void appendLogLine(const QString& serviceId, const QString& line);

private:
    QPlainTextEdit* editorFor(const QString& serviceId);

    QTabWidget* m_tabs = nullptr;
    QLineEdit* m_search = nullptr;
    QCheckBox* m_autoscroll = nullptr;
    QMap<QString, QPlainTextEdit*> m_editors;
};
