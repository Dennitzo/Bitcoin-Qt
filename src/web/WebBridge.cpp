#include "WebBridge.h"

WebBridge::WebBridge(ServiceManager& services, QObject* parent)
    : QObject(parent),
      m_services(services)
{
    QObject::connect(&m_services, &ServiceManager::bitcoinStatusChanged, this, [this](const BitcoinNodeStatus& status) {
        Q_EMIT nodeStatusChanged(toMap(status));
    });
}

void WebBridge::startNode()
{
    m_services.startAll();
}

void WebBridge::stopNode()
{
    m_services.stopAll();
}

void WebBridge::restartNode()
{
    m_services.restartAll();
}

QVariantMap WebBridge::getNodeStatus() const
{
    return toMap(m_services.bitcoinStatus());
}

QVariantMap WebBridge::toMap(const BitcoinNodeStatus& status)
{
    return {
        {"blockHeight", status.blockHeight},
        {"peers", status.peers},
        {"verificationProgress", status.verificationProgress},
        {"network", status.network},
        {"rpcAvailable", status.rpcAvailable},
    };
}
