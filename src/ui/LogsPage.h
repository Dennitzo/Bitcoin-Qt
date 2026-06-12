#pragma once

#include <QPlainTextEdit>
#include <QStringList>
#include <QWidget>

class LogsPage final : public QWidget {
    Q_OBJECT

public:
    explicit LogsPage(const QString& title = "Logs", const QStringList& serviceIds = {}, QWidget* parent = nullptr);

public Q_SLOTS:
    void appendLogLine(const QString& serviceId, const QString& line);

private:
    QPlainTextEdit* m_editor = nullptr;
    QStringList m_serviceIds;
};
