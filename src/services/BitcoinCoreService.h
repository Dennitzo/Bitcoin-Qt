#pragma once

#include "../core/ManagedService.h"
#include "../core/RpcClient.h"

#include <QJsonValue>
#include <QTimer>

class BitcoinCoreService final : public ManagedService {
    Q_OBJECT

public:
    BitcoinCoreService(ConfigManager& config, LogManager& logs, QObject* parent = nullptr);

    void start() override;
    void stop() override;
    BitcoinNodeStatus nodeStatus() const;

Q_SIGNALS:
    void nodeStatusChanged(const BitcoinNodeStatus& status);
    void rpcAvailable();

private:
    QStringList arguments() const;
    void pollRpc();
    void applyRpcResult(const QString& method, const QJsonValue& value);

    RpcClient m_rpc;
    QTimer m_pollTimer;
    BitcoinNodeStatus m_status;
    int m_rpcFailureCount = 0;
};
