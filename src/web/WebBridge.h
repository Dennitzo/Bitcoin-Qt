#pragma once

#include "../core/ServiceManager.h"

#include <QObject>
#include <QVariantMap>

class WebBridge final : public QObject {
    Q_OBJECT

public:
    explicit WebBridge(ServiceManager& services, QObject* parent = nullptr);

    Q_INVOKABLE void startNode();
    Q_INVOKABLE void stopNode();
    Q_INVOKABLE void restartNode();
    Q_INVOKABLE QVariantMap getNodeStatus() const;

Q_SIGNALS:
    void nodeStatusChanged(const QVariantMap& status);

private:
    static QVariantMap toMap(const BitcoinNodeStatus& status);

    ServiceManager& m_services;
};
