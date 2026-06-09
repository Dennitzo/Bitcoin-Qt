#include "SettingsPage.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

SettingsPage::SettingsPage(ConfigManager& config, QWidget* parent)
    : QWidget(parent),
      m_config(config)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    auto* title = new QLabel("Einstellungen", this);
    title->setStyleSheet("font-size: 28px; font-weight: 700;");
    auto* form = new QFormLayout();
    form->addRow("Datenverzeichnis", new QLabel(m_config.baseDataDir(), this));
    form->addRow("Bitcoin RPC Benutzer", new QLabel(m_config.rpcUser(), this));
    form->addRow("Bitcoin RPC Port", new QLabel(QString::number(m_config.bitcoinRpcPort()), this));
    form->addRow("Electrs Port", new QLabel(QString::number(m_config.electrsPort()), this));
    form->addRow("Mempool Host", new QLabel(m_config.mempoolHost(), this));
    form->addRow("Mempool Frontend Port", new QLabel(QString::number(m_config.mempoolFrontendPort()), this));
    form->addRow("Theme", new QLabel(m_config.theme(), this));
    form->addRow("Sprache", new QLabel(m_config.language(), this));
    root->addWidget(title);
    root->addLayout(form);
    root->addStretch();
}
