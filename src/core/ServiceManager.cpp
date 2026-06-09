#include "ServiceManager.h"

ServiceManager::ServiceManager(ConfigManager& config, LogManager& logs, QObject* parent)
    : QObject(parent),
      m_bitcoin(config, logs, this),
      m_electrs(config, logs, this),
      m_mempool(config, logs, this)
{
    wireService(m_bitcoin);
    wireService(m_electrs);
    wireService(m_mempool);

    QObject::connect(&m_bitcoin, &BitcoinCoreService::rpcAvailable, &m_electrs, &ElectrsService::start);
    QObject::connect(&m_electrs, &ElectrsService::ready, &m_mempool, [this](const QString&) {
        m_mempool.start();
    });
    QObject::connect(&m_bitcoin, &BitcoinCoreService::nodeStatusChanged, this, &ServiceManager::bitcoinStatusChanged);
    QObject::connect(&m_mempool, &MempoolService::frontendAvailable, this, &ServiceManager::mempoolFrontendAvailable);
}

void ServiceManager::startAll()
{
    m_bitcoin.start();
}

void ServiceManager::stopAll()
{
    m_mempool.stop();
    m_electrs.stop();
    m_bitcoin.stop();
}

void ServiceManager::restartAll()
{
    stopAll();
    startAll();
}

BitcoinNodeStatus ServiceManager::bitcoinStatus() const
{
    return m_bitcoin.nodeStatus();
}

QList<ServiceStatus> ServiceManager::statuses() const
{
    return {m_bitcoin.status(), m_electrs.status(), m_mempool.status()};
}

QUrl ServiceManager::mempoolUrl() const
{
    return m_mempool.frontendUrl();
}

void ServiceManager::wireService(ManagedService& service)
{
    QObject::connect(&service, &ManagedService::statusChanged, this, &ServiceManager::serviceStatusChanged);
    QObject::connect(&service, &ManagedService::crashed, this, [this](const QString& id, const QString& message) {
        Q_EMIT errorRaised(QString("%1 abgestürzt").arg(id), message);
    });
}
