#include "DashboardPage.h"

#include "../core/Localization.h"

#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QColor>
#include <QStorageInfo>
#include <QVBoxLayout>

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

QWidget* createGroup(QLabel** titleTarget, QGridLayout** gridTarget, QWidget* parent)
{
    auto* group = new QWidget(parent);
    group->setObjectName("dashboardGroup");
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(14);

    auto* title = new QLabel(group);
    title->setObjectName("dashboardGroupTitle");
    *titleTarget = title;
    layout->addWidget(title);

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

    auto* statisticsGroup = createGroup(&m_statisticsTitle, &m_metricsGrid, this);

    m_blockHeight = createMetricLabel(text("dashboard.blockHeight"), "0", this);
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
    m_network = createMetricLabel(text("dashboard.network"), "unknown", this);
    m_bitcoin = createMetricLabel("Bitcoin Core", text("state.stopped"), this);
    m_electrs = createMetricLabel("Electrs", text("state.stopped"), this);
    m_mempool = createMetricLabel("Mempool", text("dashboard.offline"), this);
    m_publicPool = createMetricLabel("Public Pool", text("dashboard.offline"), this);
    auto* syncWidget = new QWidget(this);
    auto* syncLayout = new QVBoxLayout(syncWidget);
    syncLayout->setContentsMargins(0, 0, 0, 0);
    syncLayout->setSpacing(8);
    syncLayout->addWidget(m_sync);
    syncLayout->addWidget(m_syncProgress);

    auto* storageWidget = new QWidget(this);
    auto* storageLayout = new QVBoxLayout(storageWidget);
    storageLayout->setContentsMargins(0, 0, 0, 0);
    storageLayout->setSpacing(8);
    storageLayout->addWidget(m_storage);
    storageLayout->addWidget(m_storageProgress);

    m_metricCards = {
        createCard(m_blockHeight, this),
        createCard(syncWidget, this),
        createCard(storageWidget, this),
        createCard(m_peers, this),
        createCard(m_network, this),
    };
    root->addWidget(statisticsGroup);

    auto* servicesGroup = createGroup(&m_servicesTitle, &m_servicesGrid, this);
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
        start->setObjectName("secondaryButton");
        stop->setObjectName("secondaryButton");
        controls->addWidget(start);
        controls->addWidget(stop);
        layout->addLayout(controls);
        m_startButtons.insert(services.at(i).first, start);
        m_stopButtons.insert(services.at(i).first, stop);
        QObject::connect(start, &QPushButton::clicked, this, [this, id = services.at(i).first]() {
            Q_EMIT startServiceRequested(id);
        });
        QObject::connect(stop, &QPushButton::clicked, this, [this, id = services.at(i).first]() {
            Q_EMIT stopServiceRequested(id);
        });
        m_serviceCards << createCard(wrapper, this, 168);
    }
    root->addWidget(servicesGroup);
    root->addStretch();

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
    m_metricsGrid->setColumnStretch(2, 2);
    m_metricsGrid->setColumnStretch(3, 1);
    m_metricsGrid->setColumnStretch(4, 1);
    for (int i = 0; i < m_serviceCards.size(); ++i) {
        m_servicesGrid->addWidget(m_serviceCards.at(i), 0, i);
    }
}

void DashboardPage::updateBitcoinStatus(const BitcoinNodeStatus& status)
{
    m_lastBitcoinStatus = status;
    m_blockHeight->setText(metricHtml("dashboard.blockHeight", QString::number(status.blockHeight)));
    m_sync->setText(metricHtml("dashboard.sync", QString("%1 %").arg(status.verificationProgress * 100.0, 0, 'f', 2)));
    m_syncProgress->setValue(static_cast<int>(status.verificationProgress * 10000.0));
    m_peers->setText(metricHtml("dashboard.peers", QString::number(status.peers)));
    m_network->setText(metricHtml("dashboard.network", status.network));
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

void DashboardPage::retranslate()
{
    const QString groupTitleStyle = QString("color:%1;font-size:15px;font-weight:800;background:transparent;")
        .arg(m_config.theme().toLower() == "dark" ? "#ffffff" : "#303746");
    if (m_title) {
        m_title->setText(text("dashboard.title"));
    }
    if (m_statisticsTitle) {
        m_statisticsTitle->setText(text("dashboard.statistics"));
        m_statisticsTitle->setStyleSheet(groupTitleStyle);
    }
    if (m_servicesTitle) {
        m_servicesTitle->setText(text("dashboard.services"));
        m_servicesTitle->setStyleSheet(groupTitleStyle);
    }
    for (auto* button : m_startButtons) {
        button->setText(text("app.start"));
    }
    for (auto* button : m_stopButtons) {
        button->setText(text("app.stop"));
    }
    updateBitcoinStatus(m_lastBitcoinStatus);
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
