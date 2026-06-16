#pragma once

#include "../core/AppTypes.h"
#include "../core/ConfigManager.h"

#include <QFrame>
#include <QLabel>
#include <QMap>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QGridLayout>
#include <QWidget>

class DashboardPage final : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(ConfigManager& config, QWidget* parent = nullptr);

Q_SIGNALS:
    void startServiceRequested(const QString& id);
    void stopServiceRequested(const QString& id);

public Q_SLOTS:
    void updateBitcoinStatus(const BitcoinNodeStatus& status);
    void updateElectrsSyncStatus(const ElectrsSyncStatus& status);
    void updateServiceStatus(const ServiceStatus& status);
    void updatePublicPoolStats(const PublicPoolStats& stats);

private:
    void resizeEvent(QResizeEvent* event) override;

    QString language() const;
    QString text(const QString& key) const;
    QString metricHtml(const QString& titleKey, const QString& value, int valueSize = 26) const;
    QString stateText(ServiceState state) const;
    QString formatHashrate(double hashesPerSecond) const;
    QString formatBestShare(double bestShare) const;
    QString formatBestSharePercent(double bestShare, double networkDifficulty) const;
    int ratioProgress(double value, double maximum) const;
    QString formatUptime(qint64 seconds) const;
    void showActionOverlay(const QString& title, const QString& text);
    void positionActionOverlay();
    void retranslate();
    void updateStorage();

    ConfigManager& m_config;
    BitcoinNodeStatus m_lastBitcoinStatus;
    ElectrsSyncStatus m_lastElectrsSyncStatus;
    PublicPoolStats m_lastPublicPoolStats;
    QMap<QString, ServiceStatus> m_serviceStatuses;
    QLabel* m_title = nullptr;
    QLabel* m_blockHeight = nullptr;
    QLabel* m_sync = nullptr;
    QLabel* m_storage = nullptr;
    QLabel* m_peers = nullptr;
    QLabel* m_electrsSync = nullptr;
    QLabel* m_publicPoolMinerHashrate = nullptr;
    QLabel* m_publicPoolNetworkHashrate = nullptr;
    QLabel* m_publicPoolBestShare = nullptr;
    QLabel* m_publicPoolMinerUptime = nullptr;
    QLabel* m_publicPoolBestSharePercent = nullptr;
    QLabel* m_actionOverlayTitle = nullptr;
    QLabel* m_actionOverlayText = nullptr;
    QLabel* m_bitcoin = nullptr;
    QLabel* m_electrs = nullptr;
    QLabel* m_mempool = nullptr;
    QLabel* m_publicPool = nullptr;
    QProgressBar* m_syncProgress = nullptr;
    QProgressBar* m_electrsSyncProgress = nullptr;
    QProgressBar* m_blockHeightProgress = nullptr;
    QProgressBar* m_storageProgress = nullptr;
    QProgressBar* m_publicPoolMinerHashrateProgress = nullptr;
    QProgressBar* m_publicPoolNetworkHashrateProgress = nullptr;
    QProgressBar* m_publicPoolBestShareProgress = nullptr;
    QFrame* m_actionOverlay = nullptr;
    double m_sessionMaxMinerHashrate = 0.0;
    double m_sessionMaxNetworkHashrate = 0.0;
    QTimer m_storageTimer;
    QGridLayout* m_metricsGrid = nullptr;
    QGridLayout* m_publicPoolStatsGrid = nullptr;
    QGridLayout* m_servicesGrid = nullptr;
    QList<QWidget*> m_metricCards;
    QList<QWidget*> m_publicPoolStatCards;
    QList<QWidget*> m_serviceCards;
    QMap<QString, QPushButton*> m_startButtons;
    QMap<QString, QPushButton*> m_stopButtons;
};
