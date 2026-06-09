#include "LogManager.h"

LogManager::LogManager(QObject* parent)
    : QObject(parent)
{
}

void LogManager::append(const QString& serviceId, const QString& line)
{
    Q_EMIT lineAppended(serviceId, line);
}
