#include "ServiceManager.h"

#include "Localization.h"

ServiceManager::ServiceManager(ConfigManager& config, LogManager& logs, QObject* parent)
    : QObject(parent),
      m_config(config),
      m_bitcoin(config, logs, this),
      m_electrs(config, logs, this),
      m_mempoolDatabase(config, logs, this),
      m_mempool(config, logs, this),
      m_publicPool(config, logs, this)
{
    wireService(m_bitcoin);
    wireService(m_electrs);
    wireService(m_mempoolDatabase);
    wireService(m_mempool);
    wireService(m_publicPool);

    QObject::connect(&m_bitcoin, &BitcoinCoreService::nodeStatusChanged, this, [this](const BitcoinNodeStatus& status) {
        Q_EMIT bitcoinStatusChanged(status);
        if (status.rpcAvailable && !status.initialBlockDownload) {
            startSyncedDependents();
        }
    });
    QObject::connect(&m_mempool, &MempoolService::frontendAvailable, this, &ServiceManager::mempoolFrontendAvailable);
    QObject::connect(&m_publicPool, &PublicPoolService::frontendAvailable, this, &ServiceManager::publicPoolFrontendAvailable);
    QObject::connect(&m_publicPool, &PublicPoolService::statsChanged, this, &ServiceManager::publicPoolStatsChanged);
}

void ServiceManager::startAll()
{
    m_bitcoin.start();
    if (bitcoinIsSynced()) {
        m_electrs.start();
        m_mempoolDatabase.start();
        m_mempool.start();
    }
    if (bitcoinIsSynced()) {
        m_publicPool.start();
    }
}

void ServiceManager::startConfiguredServices()
{
    if (m_config.serviceEnabled("bitcoind")) {
        m_bitcoin.start();
    }
    if ((m_config.serviceEnabled("electrs") || m_config.serviceEnabled("mempool") || m_config.serviceEnabled("public-pool"))
        && m_bitcoin.state() == ServiceState::Stopped) {
        m_bitcoin.start();
    }
    if (m_config.serviceEnabled("electrs")) {
        if (!bitcoinIsSynced()) {
            m_electrs.markWaiting("Warte auf Bitcoin Core Sync");
        }
    }
    if (m_config.serviceEnabled("mempool")) {
        if (!bitcoinIsSynced()) {
            m_electrs.markWaiting("Warte auf Bitcoin Core Sync");
            m_mempool.markWaiting("Warte auf Bitcoin Core Sync");
        }
    }
    if (m_config.serviceEnabled("public-pool")) {
        if (!bitcoinIsSynced()) {
            m_publicPool.markWaiting("Warte auf Bitcoin Core Sync");
        }
    }
    if (bitcoinIsSynced() && m_config.serviceEnabled("electrs")) {
        m_electrs.start();
    }
    if (bitcoinIsSynced() && m_config.serviceEnabled("mempool")) {
        m_mempoolDatabase.start();
        m_mempool.start();
    }
    if (bitcoinIsSynced() && m_config.serviceEnabled("public-pool")) {
        m_publicPool.start();
    }
}

void ServiceManager::stopAll()
{
    m_publicPool.stop();
    m_mempool.stop();
    m_mempoolDatabase.stop();
    m_electrs.stop();
    m_bitcoin.stop();
}

void ServiceManager::restartAll()
{
    stopAll();
    startAll();
}

void ServiceManager::startService(const QString& id)
{
    if (id == "bitcoind") {
        m_config.setServiceEnabled(id, true);
        m_bitcoin.start();
    } else if (id == "electrs") {
        m_config.setServiceEnabled(id, true);
        if (!bitcoinIsSynced()) {
            if (m_bitcoin.state() == ServiceState::Stopped) {
                m_bitcoin.start();
            }
            m_electrs.markWaiting("Warte auf Bitcoin Core Sync");
            return;
        }
        m_electrs.start();
    } else if (id == "mempool") {
        m_config.setServiceEnabled(id, true);
        m_config.setServiceEnabled("mempool-db", true);
        m_config.setServiceEnabled("electrs", true);
        if (!bitcoinIsSynced()) {
            if (m_bitcoin.state() == ServiceState::Stopped) {
                m_bitcoin.start();
            }
            m_electrs.markWaiting("Warte auf Bitcoin Core Sync");
            m_mempool.markWaiting("Warte auf Bitcoin Core Sync");
            return;
        }
        m_electrs.start();
        m_mempoolDatabase.start();
        m_mempool.start();
    } else if (id == "mempool-db") {
        startService("mempool");
    } else if (id == "public-pool") {
        m_config.setServiceEnabled(id, true);
        if (!bitcoinIsSynced()) {
            if (m_bitcoin.state() == ServiceState::Stopped) {
                m_bitcoin.start();
            }
            m_publicPool.markWaiting("Warte auf Bitcoin Core Sync");
            return;
        }
        m_publicPool.start();
    }
}

void ServiceManager::stopService(const QString& id)
{
    if (id == "bitcoind") {
        m_config.setServiceEnabled(id, false);
        m_config.setServiceEnabled("electrs", false);
        m_config.setServiceEnabled("mempool", false);
        m_config.setServiceEnabled("mempool-db", false);
        m_config.setServiceEnabled("public-pool", false);
        m_publicPool.stop();
        m_mempool.stop();
        m_mempoolDatabase.stop();
        m_electrs.stop();
        m_bitcoin.stop();
    } else if (id == "electrs") {
        m_config.setServiceEnabled(id, false);
        m_electrs.stop();
    } else if (id == "mempool") {
        m_config.setServiceEnabled(id, false);
        m_config.setServiceEnabled("mempool-db", false);
        m_mempool.stop();
        m_mempoolDatabase.stop();
    } else if (id == "mempool-db") {
        stopService("mempool");
    } else if (id == "public-pool") {
        m_config.setServiceEnabled(id, false);
        m_publicPool.stop();
    }
}

bool ServiceManager::bitcoinIsSynced() const
{
    const BitcoinNodeStatus status = m_bitcoin.nodeStatus();
    return status.rpcAvailable && !status.initialBlockDownload;
}

void ServiceManager::startSyncedDependents()
{
    if (m_config.serviceEnabled("electrs")) {
        m_electrs.start();
    }
    if (m_config.serviceEnabled("mempool")) {
        m_mempoolDatabase.start();
        m_mempool.start();
    }
    if (m_config.serviceEnabled("public-pool")) {
        m_publicPool.start();
    }
}

BitcoinNodeStatus ServiceManager::bitcoinStatus() const
{
    return m_bitcoin.nodeStatus();
}

QList<ServiceStatus> ServiceManager::statuses() const
{
    return {m_bitcoin.status(), m_electrs.status(), m_mempool.status(), m_publicPool.status()};
}

QUrl ServiceManager::mempoolUrl() const
{
    return m_mempool.frontendUrl();
}

QUrl ServiceManager::publicPoolUrl() const
{
    return m_publicPool.frontendUrl();
}

void ServiceManager::wireService(ManagedService& service)
{
    QObject::connect(&service, &ManagedService::statusChanged, this, &ServiceManager::serviceStatusChanged);
    QObject::connect(&service, &ManagedService::crashed, this, [this](const QString& id, const QString& message) {
        const bool english = m_config.language().toLower().startsWith("en");
        Q_EMIT errorRaised(english ? QString("%1 crashed").arg(id) : QString("%1 abgestürzt").arg(id), appServiceDetail(m_config.language(), message));
    });
}
