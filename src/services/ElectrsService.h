#pragma once

#include "../core/ManagedService.h"
#include "../core/RpcClient.h"

#include <QTcpSocket>
#include <QTimer>

class ElectrsService final : public ManagedService {
    Q_OBJECT

public:
    ElectrsService(ConfigManager& config, LogManager& logs, QObject* parent = nullptr);

    void start() override;
    void stop() override;

private:
    QStringList arguments() const;
    QString authCookieFile() const;
    bool writeAuthCookie() const;
    void checkBitcoinRpc();
    void checkPort();
    void startElectrsProcess();
    void handleStdout(const QString& line) override;
    void handleStderr(const QString& line) override;

    QTimer m_healthTimer;
    QTimer m_readinessTimer;
    RpcClient m_rpc;
    bool m_startRequested = false;
};
