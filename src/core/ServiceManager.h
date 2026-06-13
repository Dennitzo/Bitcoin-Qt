#pragma once

#include "AppTypes.h"
#include "ConfigManager.h"
#include "LogManager.h"

#include "../services/BitcoinCoreService.h"
#include "../services/ElectrsService.h"
#include "../services/MempoolDatabaseService.h"
#include "../services/MempoolService.h"
#include "../services/PublicPoolService.h"

#include <QObject>
#include <QUrl>

class ServiceManager final : public QObject {
    Q_OBJECT

public:
    explicit ServiceManager(ConfigManager& config, LogManager& logs, QObject* parent = nullptr);

    void startAll();
    void startConfiguredServices();
    void stopAll();
    void restartAll();
    void startService(const QString& id);
    void stopService(const QString& id);

    BitcoinNodeStatus bitcoinStatus() const;
    QList<ServiceStatus> statuses() const;
    QUrl mempoolUrl() const;
    QUrl publicPoolUrl() const;

Q_SIGNALS:
    void serviceStatusChanged(const ServiceStatus& status);
    void bitcoinStatusChanged(const BitcoinNodeStatus& status);
    void mempoolFrontendAvailable(const QUrl& url);
    void publicPoolFrontendAvailable(const QUrl& url);
    void publicPoolStatsChanged(const PublicPoolStats& stats);
    void errorRaised(const QString& title, const QString& message);

private:
    void wireService(ManagedService& service);
    bool bitcoinIsSynced() const;
    void startSyncedDependents();

    ConfigManager& m_config;
    BitcoinCoreService m_bitcoin;
    ElectrsService m_electrs;
    MempoolDatabaseService m_mempoolDatabase;
    MempoolService m_mempool;
    PublicPoolService m_publicPool;
};
