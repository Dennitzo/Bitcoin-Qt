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

    QObject::connect(&m_bitcoin, &BitcoinCoreService::nodeStatusChanged, this, &ServiceManager::bitcoinStatusChanged);
    QObject::connect(&m_mempool, &MempoolService::frontendAvailable, this, &ServiceManager::mempoolFrontendAvailable);
    QObject::connect(&m_publicPool, &PublicPoolService::frontendAvailable, this, &ServiceManager::publicPoolFrontendAvailable);
}

void ServiceManager::startAll()
{
    m_bitcoin.start();
    m_electrs.start();
    m_mempoolDatabase.start();
    m_mempool.start();
    m_publicPool.start();
}

void ServiceManager::startConfiguredServices()
{
    if (m_config.serviceEnabled("bitcoind")) {
        m_bitcoin.start();
    }
    if (m_config.serviceEnabled("electrs")) {
        m_electrs.start();
    }
    if (m_config.serviceEnabled("mempool-db") || m_config.serviceEnabled("mempool")) {
        m_mempoolDatabase.start();
    }
    if (m_config.serviceEnabled("mempool")) {
        m_mempool.start();
    }
    if (m_config.serviceEnabled("public-pool")) {
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
        m_electrs.start();
    } else if (id == "mempool") {
        m_config.setServiceEnabled(id, true);
        m_config.setServiceEnabled("mempool-db", true);
        m_mempoolDatabase.start();
        m_mempool.start();
    } else if (id == "mempool-db") {
        m_config.setServiceEnabled(id, true);
        m_mempoolDatabase.start();
    } else if (id == "public-pool") {
        m_config.setServiceEnabled(id, true);
        m_publicPool.start();
    }
}

void ServiceManager::stopService(const QString& id)
{
    if (id == "bitcoind") {
        m_config.setServiceEnabled(id, false);
        m_bitcoin.stop();
    } else if (id == "electrs") {
        m_config.setServiceEnabled(id, false);
        m_electrs.stop();
    } else if (id == "mempool") {
        m_config.setServiceEnabled(id, false);
        m_mempool.stop();
    } else if (id == "mempool-db") {
        m_config.setServiceEnabled(id, false);
        m_mempool.stop();
        m_mempoolDatabase.stop();
    } else if (id == "public-pool") {
        m_config.setServiceEnabled(id, false);
        m_publicPool.stop();
    }
}

BitcoinNodeStatus ServiceManager::bitcoinStatus() const
{
    return m_bitcoin.nodeStatus();
}

QList<ServiceStatus> ServiceManager::statuses() const
{
    return {m_bitcoin.status(), m_electrs.status(), m_mempoolDatabase.status(), m_mempool.status(), m_publicPool.status()};
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
