#include "DashboardPage.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QProgressBar>
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

QLabel* metric(const QString& title, const QString& value, QWidget* parent)
{
    auto* label = new QLabel(QString("<span style='color:#697386'>%1</span><br><b style='font-size:24px'>%2</b>").arg(title, value), parent);
    label->setMinimumHeight(72);
    return label;
}

}

DashboardPage::DashboardPage(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(18);

    auto* title = new QLabel("Dashboard", this);
    title->setStyleSheet("font-size: 28px; font-weight: 700;");
    root->addWidget(title);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(18);

    m_blockHeight = metric("Blockhöhe", "0", this);
    m_sync = metric("Sync", "0.00 %", this);
    m_peers = metric("Peers", "0", this);
    m_network = metric("Netzwerk", "unknown", this);
    m_bitcoin = metric("Bitcoin Core", "Gestoppt", this);
    m_electrs = metric("Electrs", "Gestoppt", this);
    m_mempool = metric("Mempool", "Offline", this);
    m_system = metric("System", "CPU/RAM folgt", this);

    const QList<QLabel*> labels{m_blockHeight, m_sync, m_peers, m_network, m_bitcoin, m_electrs, m_mempool, m_system};
    for (int i = 0; i < labels.size(); ++i) {
        auto* box = new QGroupBox(this);
        auto* layout = new QVBoxLayout(box);
        layout->addWidget(labels.at(i));
        grid->addWidget(box, i / 4, i % 4);
    }
    root->addLayout(grid);
    root->addStretch();
}

void DashboardPage::updateBitcoinStatus(const BitcoinNodeStatus& status)
{
    m_blockHeight->setText(QString("<span style='color:#697386'>Blockhöhe</span><br><b style='font-size:24px'>%1</b>").arg(status.blockHeight));
    m_sync->setText(QString("<span style='color:#697386'>Sync</span><br><b style='font-size:24px'>%1 %</b>").arg(status.verificationProgress * 100.0, 0, 'f', 2));
    m_peers->setText(QString("<span style='color:#697386'>Peers</span><br><b style='font-size:24px'>%1</b>").arg(status.peers));
    m_network->setText(QString("<span style='color:#697386'>Netzwerk</span><br><b style='font-size:24px'>%1</b>").arg(status.network));
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
    }
    if (!target) {
        return;
    }
    target->setText(QString("<span style='color:#697386'>%1</span><br><b style='font-size:24px'>%2</b><br><span>%3</span>")
        .arg(status.label, stateText(status.state), status.detail));
}
