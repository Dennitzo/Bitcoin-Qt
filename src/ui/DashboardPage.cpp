#include "DashboardPage.h"

#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QColor>
#include <QStorageInfo>
#include <QVBoxLayout>

namespace {

QString stateText(ServiceState state)
{
    switch (state) {
    case ServiceState::Stopped:
        return "Gestoppt";
    case ServiceState::Starting:
        return "Startet";
    case ServiceState::Running:
        return "Online";
    case ServiceState::Indexing:
        return "Indexiert";
    case ServiceState::Synced:
        return "Synchron";
    case ServiceState::Error:
        return "Fehler";
    }
    return "Unbekannt";
}

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
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto* metricsRow = new QHBoxLayout();
    metricsRow->setSpacing(18);

    m_blockHeight = createMetricLabel("Blockhöhe", "0", this);
    m_sync = createMetricLabel("Sync", "0.00 %", this);
    m_syncProgress = new QProgressBar(this);
    m_syncProgress->setRange(0, 10000);
    m_syncProgress->setTextVisible(false);
    m_syncProgress->setFixedHeight(8);
    m_storage = createMetricLabel("Speicher", "Berechne", this);
    m_storageProgress = new QProgressBar(this);
    m_storageProgress->setRange(0, 10000);
    m_storageProgress->setTextVisible(false);
    m_storageProgress->setFixedHeight(8);
    m_peers = createMetricLabel("Peers", "0", this);
    m_network = createMetricLabel("Netzwerk", "unknown", this);
    m_bitcoin = createMetricLabel("Bitcoin Core", "Gestoppt", this);
    m_electrs = createMetricLabel("Electrs", "Gestoppt", this);
    m_mempoolDatabase = createMetricLabel("Mempool DB", "Gestoppt", this);
    m_mempool = createMetricLabel("Mempool", "Offline", this);
    m_publicPool = createMetricLabel("Public Pool", "Offline", this);
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

    metricsRow->addWidget(createCard(m_blockHeight, this), 1);
    metricsRow->addWidget(createCard(syncWidget, this), 1);
    metricsRow->addWidget(createCard(storageWidget, this), 2);
    metricsRow->addWidget(createCard(m_peers, this), 1);
    metricsRow->addWidget(createCard(m_network, this), 1);
    root->addLayout(metricsRow);

    auto* servicesGrid = new QGridLayout();
    servicesGrid->setHorizontalSpacing(18);
    servicesGrid->setVerticalSpacing(18);
    const QList<QPair<QString, QLabel*>> services{
        {"bitcoind", m_bitcoin},
        {"electrs", m_electrs},
        {"mempool-db", m_mempoolDatabase},
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
        auto* start = new QPushButton("Start", wrapper);
        auto* stop = new QPushButton("Stop", wrapper);
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
        servicesGrid->addWidget(createCard(wrapper, this, 168), i / 5, i % 5);
    }
    root->addLayout(servicesGrid);
    root->addStretch();

    updateStorage();
    m_storageTimer.setInterval(30000);
    QObject::connect(&m_storageTimer, &QTimer::timeout, this, &DashboardPage::updateStorage);
    m_storageTimer.start();
}

void DashboardPage::updateBitcoinStatus(const BitcoinNodeStatus& status)
{
    m_blockHeight->setText(QString("<span style='color:#8a93a3;font-size:12px;font-weight:700'>Blockhöhe</span><br><b style='font-size:26px'>%1</b>").arg(status.blockHeight));
    m_sync->setText(QString("<span style='color:#8a93a3;font-size:12px;font-weight:700'>Sync</span><br><b style='font-size:26px'>%1 %</b>").arg(status.verificationProgress * 100.0, 0, 'f', 2));
    m_syncProgress->setValue(static_cast<int>(status.verificationProgress * 10000.0));
    m_peers->setText(QString("<span style='color:#8a93a3;font-size:12px;font-weight:700'>Peers</span><br><b style='font-size:26px'>%1</b>").arg(status.peers));
    m_network->setText(QString("<span style='color:#8a93a3;font-size:12px;font-weight:700'>Netzwerk</span><br><b style='font-size:26px'>%1</b>").arg(status.network));
}

void DashboardPage::updateServiceStatus(const ServiceStatus& status)
{
    QLabel* target = nullptr;
    if (status.id == "bitcoind") {
        target = m_bitcoin;
    } else if (status.id == "electrs") {
        target = m_electrs;
    } else if (status.id == "mempool-db") {
        target = m_mempoolDatabase;
    } else if (status.id == "mempool") {
        target = m_mempool;
    } else if (status.id == "public-pool") {
        target = m_publicPool;
    }
    if (!target) {
        return;
    }
    target->setText(QString("<span style='color:#8a93a3;font-size:12px;font-weight:700'>%1</span><br><b style='font-size:24px'>%2</b><br><span style='color:#8a93a3'>%3</span>")
        .arg(status.label, stateText(status.state), status.detail));
    if (auto* start = m_startButtons.value(status.id, nullptr)) {
        start->setEnabled(status.state == ServiceState::Stopped || status.state == ServiceState::Error);
    }
    if (auto* stop = m_stopButtons.value(status.id, nullptr)) {
        stop->setEnabled(status.state != ServiceState::Stopped);
    }
}

void DashboardPage::updateStorage()
{
    QStorageInfo storage(m_config.baseDataDir().isEmpty() ? QDir::rootPath() : m_config.baseDataDir());
    storage.refresh();
    const qint64 total = storage.bytesTotal();
    const qint64 available = storage.bytesAvailable();
    if (total <= 0) {
        m_storage->setText("<span style='color:#8a93a3;font-size:12px;font-weight:700'>Speicher</span><br><b style='font-size:26px'>Nicht verfügbar</b>");
        m_storageProgress->setValue(0);
        return;
    }
    const qint64 used = total - available;
    const double percent = static_cast<double>(used) / static_cast<double>(total);
    m_storage->setText(QString("<span style='color:#8a93a3;font-size:12px;font-weight:700'>Speicher</span><br><b style='font-size:26px'>%1 / %2</b>")
        .arg(formatBytes(used), formatBytes(total)));
    m_storageProgress->setValue(static_cast<int>(percent * 10000.0));
}
