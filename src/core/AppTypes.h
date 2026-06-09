#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

enum class ServiceState {
    Stopped,
    Starting,
    Running,
    Indexing,
    Synced,
    Error,
};

enum class BitcoinNetwork {
    Mainnet,
    Testnet,
    Signet,
    Regtest,
};

struct BitcoinNodeStatus {
    int blockHeight = 0;
    int peers = 0;
    double verificationProgress = 0.0;
    QString network = "unknown";
    bool rpcAvailable = false;
};

struct ServiceStatus {
    QString id;
    QString label;
    ServiceState state = ServiceState::Stopped;
    QString detail;
    QDateTime updatedAt = QDateTime::currentDateTimeUtc();
};

Q_DECLARE_METATYPE(BitcoinNodeStatus)
Q_DECLARE_METATYPE(ServiceStatus)
Q_DECLARE_METATYPE(ServiceState)
