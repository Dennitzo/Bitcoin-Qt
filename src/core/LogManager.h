#pragma once

#include <QObject>
#include <QString>

class LogManager final : public QObject {
    Q_OBJECT

public:
    explicit LogManager(QObject* parent = nullptr);

    void append(const QString& serviceId, const QString& line);

Q_SIGNALS:
    void lineAppended(const QString& serviceId, const QString& line);
};
