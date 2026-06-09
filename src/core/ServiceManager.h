#pragma once

#include "AppTypes.h"
#include "ConfigManager.h"
#include "LogManager.h"

#include "../services/BitcoinCoreService.h"
#include "../services/ElectrsService.h"
#include "../services/MempoolService.h"

#include <QObject>
#include <QUrl>

class ServiceManager final : public QObject {
    Q_OBJECT

public:
    explicit ServiceManager(ConfigManager& config, LogManager& logs, QObject* parent = nullptr);

    void startAll();
    void stopAll();
    void restartAll();

    BitcoinNodeStatus bitcoinStatus() const;
    QList<ServiceStatus> statuses() const;
    QUrl mempoolUrl() const;

Q_SIGNALS:
    void serviceStatusChanged(const ServiceStatus& status);
    void bitcoinStatusChanged(const BitcoinNodeStatus& status);
    void mempoolFrontendAvailable(const QUrl& url);
    void errorRaised(const QString& title, const QString& message);

private:
    void wireService(ManagedService& service);

    BitcoinCoreService m_bitcoin;
    ElectrsService m_electrs;
    MempoolService m_mempool;
};
