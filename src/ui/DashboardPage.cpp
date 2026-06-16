#include "DashboardPage.h"

#include "../core/Localization.h"

#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QColor>
#include <QFrame>
#include <QResizeEvent>
#include <QStorageInfo>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

QLabel* createMetricLabel(const QString& title, const QString& value, QWidget* parent)
{
    auto* label = new QLabel(QString("<span style='color:#8a93a3;font-size:12px;font-weight:700;letter-spacing:0'>%1</span><br><b style='font-size:26px'>%2</b>").arg(title, value), parent);
    label->setMinimumHeight(72);
    return label;
}

QWidget* createCard(QWidget* content, QWidget* parent, int height = 138)
{
    auto* card = new QWidget(parent);
    card->setObjectName("metricCard");
    card->setFixedHeight(height);
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(28);
    shadow->setColor(QColor(16, 24, 40, 24));
    shadow->setOffset(0, 10);
    card->setGraphicsEffect(shadow);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(content);
    return card;
}

QWidget* createGroup(QGridLayout** gridTarget, QWidget* parent)
{
    auto* group = new QWidget(parent);
    group->setObjectName("dashboardGroup");
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(0);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(18);
    *gridTarget = grid;
    layout->addLayout(grid);
    return group;
}

QString formatBytes(qint64 bytes)
{
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const double tib = gib * 1024.0;
    if (bytes >= tib) {
        return QString("%1 TB").arg(bytes / tib, 0, 'f', 1);
    }
    return QString("%1 GB").arg(bytes / gib, 0, 'f', 0);
}

}

DashboardPage::DashboardPage(ConfigManager& config, QWidget* parent)
    : QWidget(parent),
      m_config(config)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(20);

    auto* title = new QLabel("Dashboard", this);
    m_title = title;
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto* statisticsGroup = createGroup(&m_metricsGrid, this);

    m_blockHeight = createMetricLabel(text("dashboard.blockHeight"), "0", this);
    m_blockHeightProgress = new QProgressBar(this);
    m_blockHeightProgress->setRange(0, 10000);
    m_blockHeightProgress->setTextVisible(false);
    m_blockHeightProgress->setFixedHeight(8);
    m_sync = createMetricLabel(text("dashboard.sync"), "0.00 %", this);
    m_syncProgress = new QProgressBar(this);
    m_syncProgress->setRange(0, 10000);
    m_syncProgress->setTextVisible(false);
    m_syncProgress->setFixedHeight(8);
    m_storage = createMetricLabel(text("dashboard.storage"), text("dashboard.calculating"), this);
    m_storageProgress = new QProgressBar(this);
    m_storageProgress->setRange(0, 10000);
    m_storageProgress->setTextVisible(false);
    m_storageProgress->setFixedHeight(8);
    m_peers = createMetricLabel("Peers", "0", this);
    m_electrsSync = createMetricLabel(text("dashboard.electrsSync"), "0.00 %", this);
    m_electrsSyncProgress = new QProgressBar(this);
    m_electrsSyncProgress->setRange(0, 10000);
    m_electrsSyncProgress->setTextVisible(false);
    m_electrsSyncProgress->setFixedHeight(8);
    m_publicPoolMinerHashrate = createMetricLabel(text("publicPool.minerHashrate"), "0 TH/s", this);
    m_publicPoolMinerHashrateProgress = new QProgressBar(this);
    m_publicPoolMinerHashrateProgress->setRange(0, 10000);
    m_publicPoolMinerHashrateProgress->setTextVisible(false);
    m_publicPoolMinerHashrateProgress->setFixedHeight(8);
    m_publicPoolNetworkHashrate = createMetricLabel(text("publicPool.networkHashrate"), "0 TH/s", this);
    m_publicPoolNetworkHashrateProgress = new QProgressBar(this);
    m_publicPoolNetworkHashrateProgress->setRange(0, 10000);
    m_publicPoolNetworkHashrateProgress->setTextVisible(false);
    m_publicPoolNetworkHashrateProgress->setFixedHeight(8);
    m_publicPoolBestShare = createMetricLabel(text("publicPool.bestShare"), "0", this);
    m_publicPoolMinerUptime = createMetricLabel(text("publicPool.minerUptime"), "0m", this);
    m_publicPoolBestSharePercent = createMetricLabel(text("publicPool.bestSharePercent"), "0 %", this);
    m_publicPoolBestShareProgress = new QProgressBar(this);
    m_publicPoolBestShareProgress->setRange(0, 10000);
    m_publicPoolBestShareProgress->setTextVisible(false);
    m_publicPoolBestShareProgress->setFixedHeight(8);
    m_bitcoin = createMetricLabel("Bitcoin Core", text("state.stopped"), this);
    m_electrs = createMetricLabel("Electrs", text("state.stopped"), this);
    m_mempool = createMetricLabel("Mempool", text("dashboard.offline"), this);
    m_publicPool = createMetricLabel("Public Pool", text("dashboard.offline"), this);
    auto* blockHeightWidget = new QWidget(this);
    auto* blockHeightLayout = new QVBoxLayout(blockHeightWidget);
    blockHeightLayout->setContentsMargins(0, 0, 0, 0);
    blockHeightLayout->setSpacing(8);
    blockHeightLayout->addWidget(m_blockHeight);
    blockHeightLayout->addWidget(m_blockHeightProgress);

    auto* syncWidget = new QWidget(this);
    auto* syncLayout = new QVBoxLayout(syncWidget);
    syncLayout->setContentsMargins(0, 0, 0, 0);
    syncLayout->setSpacing(8);
    syncLayout->addWidget(m_sync);
    syncLayout->addWidget(m_syncProgress);

    auto* electrsSyncWidget = new QWidget(this);
    auto* electrsSyncLayout = new QVBoxLayout(electrsSyncWidget);
    electrsSyncLayout->setContentsMargins(0, 0, 0, 0);
    electrsSyncLayout->setSpacing(8);
    electrsSyncLayout->addWidget(m_electrsSync);
    electrsSyncLayout->addWidget(m_electrsSyncProgress);

    auto* storageWidget = new QWidget(this);
    auto* storageLayout = new QVBoxLayout(storageWidget);
    storageLayout->setContentsMargins(0, 0, 0, 0);
    storageLayout->setSpacing(8);
    storageLayout->addWidget(m_storage);
    storageLayout->addWidget(m_storageProgress);

    m_metricCards = {
        createCard(blockHeightWidget, this),
        createCard(syncWidget, this),
        createCard(electrsSyncWidget, this),
        createCard(storageWidget, this),
        createCard(m_peers, this),
    };
    root->addWidget(statisticsGroup);

    auto* publicPoolStatsGroup = createGroup(&m_publicPoolStatsGrid, this);
    m_publicPoolStatsGroup = publicPoolStatsGroup;
    auto* minerHashrateWidget = new QWidget(this);
    auto* minerHashrateLayout = new QVBoxLayout(minerHashrateWidget);
    minerHashrateLayout->setContentsMargins(0, 0, 0, 0);
    minerHashrateLayout->setSpacing(8);
    minerHashrateLayout->addWidget(m_publicPoolMinerHashrate);
    minerHashrateLayout->addWidget(m_publicPoolMinerHashrateProgress);

    auto* networkHashrateWidget = new QWidget(this);
    auto* networkHashrateLayout = new QVBoxLayout(networkHashrateWidget);
    networkHashrateLayout->setContentsMargins(0, 0, 0, 0);
    networkHashrateLayout->setSpacing(8);
    networkHashrateLayout->addWidget(m_publicPoolNetworkHashrate);
    networkHashrateLayout->addWidget(m_publicPoolNetworkHashrateProgress);

    auto* bestSharePercentWidget = new QWidget(this);
    auto* bestSharePercentLayout = new QVBoxLayout(bestSharePercentWidget);
    bestSharePercentLayout->setContentsMargins(0, 0, 0, 0);
    bestSharePercentLayout->setSpacing(8);
    bestSharePercentLayout->addWidget(m_publicPoolBestSharePercent);
    bestSharePercentLayout->addWidget(m_publicPoolBestShareProgress);

    m_publicPoolStatCards = {
        createCard(minerHashrateWidget, this),
        createCard(networkHashrateWidget, this),
        createCard(m_publicPoolMinerUptime, this),
        createCard(m_publicPoolBestShare, this),
        createCard(bestSharePercentWidget, this),
    };
    root->addWidget(publicPoolStatsGroup);

    auto* servicesGroup = createGroup(&m_servicesGrid, this);
    const QList<QPair<QString, QLabel*>> services{
        {"bitcoind", m_bitcoin},
        {"electrs", m_electrs},
        {"mempool", m_mempool},
        {"public-pool", m_publicPool},
    };
    for (int i = 0; i < services.size(); ++i) {
        auto* wrapper = new QWidget(this);
        auto* layout = new QVBoxLayout(wrapper);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);
        layout->addWidget(services.at(i).second);
        auto* controls = new QHBoxLayout();
        auto* start = new QPushButton(text("app.start"), wrapper);
        auto* stop = new QPushButton(text("app.stop"), wrapper);
        start->setObjectName("serviceActionButton");
        stop->setObjectName("serviceActionButton");
        controls->addWidget(start);
        controls->addWidget(stop);
        layout->addLayout(controls);
        m_startButtons.insert(services.at(i).first, start);
        m_stopButtons.insert(services.at(i).first, stop);
        QObject::connect(start, &QPushButton::clicked, this, [this, id = services.at(i).first]() {
            showActionOverlay(text("dashboard.serviceStartTitle"), text("dashboard.serviceStartText").arg(id));
            QTimer::singleShot(75, this, [this, id]() {
                Q_EMIT startServiceRequested(id);
            });
        });
        QObject::connect(stop, &QPushButton::clicked, this, [this, id = services.at(i).first]() {
            showActionOverlay(text("dashboard.serviceStopTitle"), text("dashboard.serviceStopText").arg(id));
            QTimer::singleShot(75, this, [this, id]() {
                Q_EMIT stopServiceRequested(id);
            });
        });
        m_serviceCards << createCard(wrapper, this, 168);
    }
    root->addWidget(servicesGroup);
    root->addStretch();

    m_actionOverlay = new QFrame(this);
    m_actionOverlay->setObjectName("settingsSavedOverlay");
    m_actionOverlay->setFixedSize(420, 126);
    m_actionOverlay->hide();
    auto* overlayLayout = new QVBoxLayout(m_actionOverlay);
    overlayLayout->setContentsMargins(24, 20, 24, 20);
    overlayLayout->setSpacing(6);
    m_actionOverlayTitle = new QLabel(m_actionOverlay);
    m_actionOverlayTitle->setObjectName("settingsSavedOverlayTitle");
    m_actionOverlayText = new QLabel(m_actionOverlay);
    m_actionOverlayText->setObjectName("settingsSavedOverlayText");
    m_actionOverlayText->setWordWrap(true);
    overlayLayout->addWidget(m_actionOverlayTitle);
    overlayLayout->addWidget(m_actionOverlayText);

    updateStorage();
    m_storageTimer.setInterval(30000);
    QObject::connect(&m_storageTimer, &QTimer::timeout, this, &DashboardPage::updateStorage);
    m_storageTimer.start();
    QObject::connect(&m_config, &ConfigManager::changed, this, &DashboardPage::retranslate);
    for (int i = 0; i < m_metricCards.size(); ++i) {
        m_metricsGrid->addWidget(m_metricCards.at(i), 0, i);
    }
    m_metricsGrid->setColumnStretch(0, 1);
    m_metricsGrid->setColumnStretch(1, 1);
    m_metricsGrid->setColumnStretch(2, 1);
    m_metricsGrid->setColumnStretch(3, 2);
    m_metricsGrid->setColumnStretch(4, 1);
    for (int i = 0; i < m_publicPoolStatCards.size(); ++i) {
        m_publicPoolStatsGrid->addWidget(m_publicPoolStatCards.at(i), 0, i);
        m_publicPoolStatsGrid->setColumnStretch(i, 1);
    }
    for (int i = 0; i < m_serviceCards.size(); ++i) {
        m_servicesGrid->addWidget(m_serviceCards.at(i), 0, i);
    }
    updatePublicPoolStatsVisibility();
}

void DashboardPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionActionOverlay();
}

void DashboardPage::updateBitcoinStatus(const BitcoinNodeStatus& status)
{
    m_lastBitcoinStatus = status;
    m_blockHeight->setText(metricHtml("dashboard.blockHeight", QString::number(status.blockHeight)));
    m_blockHeightProgress->setValue(ratioProgress(status.blockHeight, status.headerHeight));
    m_sync->setText(metricHtml("dashboard.sync", QString("%1 %").arg(status.verificationProgress * 100.0, 0, 'f', 2)));
    m_syncProgress->setValue(static_cast<int>(status.verificationProgress * 10000.0));
    m_peers->setText(metricHtml("dashboard.peers", QString::number(status.peers)));
    updateElectrsSyncStatus(m_lastElectrsSyncStatus);
}

void DashboardPage::updateElectrsSyncStatus(const ElectrsSyncStatus& status)
{
    m_lastElectrsSyncStatus = status;
    const int target = status.targetHeaderHeight > 0 ? status.targetHeaderHeight : m_lastBitcoinStatus.headerHeight;
    const int progress = ratioProgress(status.indexedHeaderHeight, target);
    m_electrsSync->setText(metricHtml("dashboard.electrsSync", QString("%1 %").arg(progress / 100.0, 0, 'f', 2)));
    m_electrsSyncProgress->setValue(progress);
}

void DashboardPage::updateServiceStatus(const ServiceStatus& status)
{
    QLabel* target = nullptr;
    if (status.id == "bitcoind") {
        target = m_bitcoin;
    } else if (status.id == "electrs") {
        target = m_electrs;
    } else if (status.id == "mempool") {
        target = m_mempool;
    } else if (status.id == "public-pool") {
        target = m_publicPool;
    }
    m_serviceStatuses.insert(status.id, status);
    if (!target) {
        return;
    }
    target->setText(QString("<span style='color:#8a93a3;font-size:12px;font-weight:700'>%1</span><br><b style='font-size:24px'>%2</b><br><span style='color:#8a93a3'>%3</span>")
        .arg(status.label, stateText(status.state), appServiceDetail(language(), status.detail)));
    if (auto* start = m_startButtons.value(status.id, nullptr)) {
        start->setEnabled(status.state == ServiceState::Stopped || status.state == ServiceState::Error);
    }
    if (auto* stop = m_stopButtons.value(status.id, nullptr)) {
        stop->setEnabled(status.state != ServiceState::Stopped);
    }
    if (status.id == "public-pool") {
        updatePublicPoolStatsVisibility();
    }
}

void DashboardPage::updatePublicPoolStats(const PublicPoolStats& stats)
{
    m_lastPublicPoolStats = stats;
    if (!stats.online || stats.minerCount <= 0) {
        m_publicPoolMinerHashrate->setText(metricHtml("publicPool.minerHashrate", "0 TH/s"));
        m_publicPoolMinerHashrateProgress->setValue(0);
        m_publicPoolNetworkHashrate->setText(metricHtml("publicPool.networkHashrate", "0 TH/s"));
        m_publicPoolNetworkHashrateProgress->setValue(0);
        m_publicPoolBestShare->setText(metricHtml("publicPool.bestShare", "0"));
        m_publicPoolMinerUptime->setText(metricHtml("publicPool.minerUptime", "0m"));
        m_publicPoolBestSharePercent->setText(metricHtml("publicPool.bestSharePercent", "0 %"));
        m_publicPoolBestShareProgress->setValue(0);
        return;
    }

    if (stats.minerHashrate > 0.0 && std::isfinite(stats.minerHashrate)) {
        if (m_sessionMinMinerHashrate <= 0.0 || stats.minerHashrate < m_sessionMinMinerHashrate) {
            m_sessionMinMinerHashrate = stats.minerHashrate;
        }
        if (stats.minerHashrate > m_sessionMaxMinerHashrate) {
            m_sessionMaxMinerHashrate = stats.minerHashrate;
        }
    }
    if (stats.networkHashrate > 0.0 && std::isfinite(stats.networkHashrate)) {
        if (m_sessionMinNetworkHashrate <= 0.0 || stats.networkHashrate < m_sessionMinNetworkHashrate) {
            m_sessionMinNetworkHashrate = stats.networkHashrate;
        }
        if (stats.networkHashrate > m_sessionMaxNetworkHashrate) {
            m_sessionMaxNetworkHashrate = stats.networkHashrate;
        }
    }

    m_publicPoolMinerHashrate->setText(metricHtml("publicPool.minerHashrate", formatHashrate(stats.minerHashrate)));
    m_publicPoolMinerHashrateProgress->setValue(rangeProgress(stats.minerHashrate, m_sessionMinMinerHashrate, m_sessionMaxMinerHashrate));
    m_publicPoolNetworkHashrate->setText(metricHtml("publicPool.networkHashrate", formatHashrate(stats.networkHashrate)));
    m_publicPoolNetworkHashrateProgress->setValue(rangeProgress(stats.networkHashrate, m_sessionMinNetworkHashrate, m_sessionMaxNetworkHashrate));
    m_publicPoolBestShare->setText(metricHtml("publicPool.bestShare", formatBestShare(stats.bestShare)));
    m_publicPoolMinerUptime->setText(metricHtml("publicPool.minerUptime", formatUptime(stats.minerUptimeSeconds)));
    m_publicPoolBestSharePercent->setText(metricHtml("publicPool.bestSharePercent", formatBestSharePercent(stats.bestShare, stats.networkDifficulty)));
    m_publicPoolBestShareProgress->setValue(ratioProgress(stats.bestShare, stats.networkDifficulty));
}

QString DashboardPage::language() const
{
    return m_config.language();
}

QString DashboardPage::text(const QString& key) const
{
    return appText(language(), key);
}

QString DashboardPage::metricHtml(const QString& titleKey, const QString& value, int valueSize) const
{
    return QString("<span style='color:#8a93a3;font-size:12px;font-weight:700'>%1</span><br><b style='font-size:%2px'>%3</b>")
        .arg(text(titleKey))
        .arg(valueSize)
        .arg(value);
}

QString DashboardPage::stateText(ServiceState state) const
{
    switch (state) {
    case ServiceState::Stopped:
        return text("state.stopped");
    case ServiceState::Starting:
        return text("state.starting");
    case ServiceState::Running:
        return text("state.online");
    case ServiceState::Indexing:
        return text("state.indexing");
    case ServiceState::Synced:
        return text("state.synced");
    case ServiceState::Error:
        return text("state.error");
    }
    return text("state.unknown");
}

QString DashboardPage::formatHashrate(double hashesPerSecond) const
{
    if (hashesPerSecond <= 0.0 || !std::isfinite(hashesPerSecond)) {
        return "0 TH/s";
    }

    static const QStringList units{"H/s", "KH/s", "MH/s", "GH/s", "TH/s", "PH/s", "EH/s"};
    int unitIndex = 0;
    double value = hashesPerSecond;
    while (value >= 1000.0 && unitIndex < units.size() - 1) {
        value /= 1000.0;
        ++unitIndex;
    }
    const int decimals = value >= 100.0 ? 0 : (value >= 10.0 ? 1 : 2);
    return QString("%1 %2").arg(value, 0, 'f', decimals).arg(units.at(unitIndex));
}

QString DashboardPage::formatBestShare(double bestShare) const
{
    if (bestShare <= 0.0 || !std::isfinite(bestShare)) {
        return "0";
    }
    if (bestShare >= 1000000.0) {
        return QString("%1M").arg(bestShare / 1000000.0, 0, 'f', 2);
    }
    if (bestShare >= 1000.0) {
        return QString("%1K").arg(bestShare / 1000.0, 0, 'f', 2);
    }
    return QString::number(bestShare, 'f', bestShare >= 100.0 ? 0 : 2);
}

QString DashboardPage::formatBestSharePercent(double bestShare, double networkDifficulty) const
{
    if (bestShare <= 0.0 || networkDifficulty <= 0.0 || !std::isfinite(bestShare) || !std::isfinite(networkDifficulty)) {
        return "0 %";
    }

    const double percent = (bestShare / networkDifficulty) * 100.0;
    if (percent >= 100.0) {
        return QString("%1 %").arg(QString::number(percent, 'f', 0));
    }
    return QString("%1 %").arg(QString::number(percent, 'g', 2));
}

int DashboardPage::ratioProgress(double value, double maximum) const
{
    if (value <= 0.0 || maximum <= 0.0 || !std::isfinite(value) || !std::isfinite(maximum)) {
        return 0;
    }
    const double ratio = std::clamp(value / maximum, 0.0, 1.0);
    return static_cast<int>(ratio * 10000.0);
}

int DashboardPage::rangeProgress(double value, double minimum, double maximum) const
{
    if (value <= 0.0 || minimum <= 0.0 || maximum <= 0.0
        || !std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum)) {
        return 0;
    }
    if (maximum <= minimum) {
        return value >= maximum ? 10000 : 0;
    }
    const double ratio = std::clamp((value - minimum) / (maximum - minimum), 0.0, 1.0);
    return static_cast<int>(ratio * 10000.0);
}

void DashboardPage::showActionOverlay(const QString& title, const QString& text)
{
    if (!m_actionOverlay || !m_actionOverlayTitle || !m_actionOverlayText) {
        return;
    }
    m_actionOverlayTitle->setText(title);
    m_actionOverlayText->setText(text);
    positionActionOverlay();
    m_actionOverlay->raise();
    m_actionOverlay->show();
    QTimer::singleShot(3200, m_actionOverlay, &QFrame::hide);
}

void DashboardPage::positionActionOverlay()
{
    if (!m_actionOverlay) {
        return;
    }
    const int x = (width() - m_actionOverlay->width()) / 2;
    const int y = (height() - m_actionOverlay->height()) / 2;
    m_actionOverlay->move(qMax(16, x), qMax(16, y));
}

void DashboardPage::updatePublicPoolStatsVisibility()
{
    if (!m_publicPoolStatsGroup) {
        return;
    }
    const ServiceState state = m_serviceStatuses.value("public-pool").state;
    m_publicPoolStatsGroup->setVisible(state == ServiceState::Running);
}

QString DashboardPage::formatUptime(qint64 seconds) const
{
    if (seconds <= 0) {
        return "0m";
    }
    const qint64 days = seconds / 86400;
    const qint64 hours = (seconds % 86400) / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (days > 0) {
        return QString("%1d %2h").arg(days).arg(hours);
    }
    if (hours > 0) {
        return QString("%1h %2m").arg(hours).arg(minutes);
    }
    return QString("%1m").arg(minutes);
}

void DashboardPage::retranslate()
{
    if (m_title) {
        m_title->setText(text("dashboard.title"));
    }
    for (auto* button : m_startButtons) {
        button->setText(text("app.start"));
    }
    for (auto* button : m_stopButtons) {
        button->setText(text("app.stop"));
    }
    updateBitcoinStatus(m_lastBitcoinStatus);
    updateElectrsSyncStatus(m_lastElectrsSyncStatus);
    updatePublicPoolStats(m_lastPublicPoolStats);
    const QList<ServiceStatus> statuses = m_serviceStatuses.values();
    for (const ServiceStatus& status : statuses) {
        updateServiceStatus(status);
    }
    updateStorage();
}

void DashboardPage::updateStorage()
{
    QStorageInfo storage(m_config.baseDataDir().isEmpty() ? QDir::rootPath() : m_config.baseDataDir());
    storage.refresh();
    const qint64 total = storage.bytesTotal();
    const qint64 available = storage.bytesAvailable();
    if (total <= 0) {
        m_storage->setText(metricHtml("dashboard.storage", text("dashboard.unavailable")));
        m_storageProgress->setValue(0);
        return;
    }
    const qint64 used = total - available;
    const double percent = static_cast<double>(used) / static_cast<double>(total);
    m_storage->setText(metricHtml("dashboard.storage", QString("%1 / %2").arg(formatBytes(used), formatBytes(total))));
    m_storageProgress->setValue(static_cast<int>(percent * 10000.0));
}
