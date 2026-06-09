#pragma once

#include "../core/AppTypes.h"

#include <QLabel>
#include <QMap>
#include <QWidget>

class DashboardPage final : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget* parent = nullptr);

public Q_SLOTS:
    void updateBitcoinStatus(const BitcoinNodeStatus& status);
    void updateServiceStatus(const ServiceStatus& status);

private:
    QLabel* m_blockHeight = nullptr;
    QLabel* m_sync = nullptr;
    QLabel* m_peers = nullptr;
    QLabel* m_network = nullptr;
    QLabel* m_bitcoin = nullptr;
    QLabel* m_electrs = nullptr;
    QLabel* m_mempool = nullptr;
    QLabel* m_system = nullptr;
};
