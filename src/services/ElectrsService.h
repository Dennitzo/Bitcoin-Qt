#pragma once

#include "../core/ManagedService.h"

#include <QTcpSocket>
#include <QTimer>

class ElectrsService final : public ManagedService {
    Q_OBJECT

public:
    ElectrsService(ConfigManager& config, LogManager& logs, QObject* parent = nullptr);

    void start() override;

private:
    QStringList arguments() const;
    void checkPort();
    void handleStdout(const QString& line) override;
    void handleStderr(const QString& line) override;

    QTimer m_healthTimer;
};
