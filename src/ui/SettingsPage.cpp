#include "SettingsPage.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

QSpinBox* portBox(quint16 value, QWidget* parent)
{
    auto* box = new QSpinBox(parent);
    box->setRange(1, 65535);
    box->setValue(value);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return box;
}

QLabel* fieldLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName("settingsFieldLabel");
    label->setFixedWidth(190);
    return label;
}

}

SettingsPage::SettingsPage(ConfigManager& config, QWidget* parent)
    : QWidget(parent),
      m_config(config)
{
    buildUi();
}

void SettingsPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(32, 28, 32, 28);
    root->setSpacing(20);

    auto* title = new QLabel("Einstellungen", this);
    title->setObjectName("pageTitle");

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("settingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    auto* content = new QWidget(scroll);
    content->setObjectName("settingsContent");
    content->setMaximumWidth(900);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(18);

    auto createSection = [this, contentLayout](const QString& heading) {
        auto* section = new QWidget(this);
        section->setObjectName("settingsSection");
        auto* sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(22, 20, 22, 22);
        sectionLayout->setSpacing(16);

        auto* title = new QLabel(heading, section);
        title->setObjectName("settingsSectionTitle");
        sectionLayout->addWidget(title);

        auto* form = new QFormLayout();
        form->setLabelAlignment(Qt::AlignLeft);
        form->setFormAlignment(Qt::AlignTop);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setHorizontalSpacing(28);
        form->setVerticalSpacing(13);
        form->setRowWrapPolicy(QFormLayout::DontWrapRows);
        sectionLayout->addLayout(form);
        contentLayout->addWidget(section);
        return form;
    };

    auto addRow = [](QFormLayout* form, const QString& label, QWidget* field) {
        form->addRow(fieldLabel(label), field);
    };
    auto addLayoutRow = [](QFormLayout* form, const QString& label, QLayout* field) {
        form->addRow(fieldLabel(label), field);
    };

    auto* storageForm = createSection("Speicher");
    m_baseDir = new QLineEdit(m_config.baseDataDir(), this);
    auto* browse = new QPushButton("Auswählen", this);
    browse->setObjectName("secondaryButton");
    auto* baseRow = new QHBoxLayout();
    baseRow->addWidget(m_baseDir, 1);
    baseRow->addWidget(browse);
    addLayoutRow(storageForm, "Datenverzeichnis", baseRow);

    auto* bitcoinForm = createSection("Bitcoin Core");
    m_rpcUser = new QLineEdit(m_config.rpcUser(), this);
    m_rpcPassword = new QLineEdit(m_config.rpcPassword(), this);
    m_rpcPassword->setEchoMode(QLineEdit::Password);
    m_rpcPort = portBox(m_config.bitcoinRpcPort(), this);
    m_network = new QComboBox(this);
    m_network->addItem("Mainnet", "mainnet");
    m_network->addItem("Testnet", "testnet");
    m_network->addItem("Signet", "signet");
    m_network->addItem("Regtest", "regtest");
    m_network->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_network->setCurrentIndex(static_cast<int>(m_config.network()));
    addRow(bitcoinForm, "RPC Benutzer", m_rpcUser);
    addRow(bitcoinForm, "RPC Passwort", m_rpcPassword);
    addRow(bitcoinForm, "RPC Port", m_rpcPort);
    addRow(bitcoinForm, "Netzwerk", m_network);

    auto* serviceForm = createSection("Dienste");
    m_electrsPort = portBox(m_config.electrsPort(), this);
    m_mempoolDatabasePort = portBox(m_config.mempoolDatabasePort(), this);
    m_mempoolBackendPort = portBox(m_config.mempoolBackendPort(), this);
    m_mempoolFrontendPort = portBox(m_config.mempoolFrontendPort(), this);
    m_publicPoolApiPort = portBox(m_config.publicPoolApiPort(), this);
    m_publicPoolStratumPort = portBox(m_config.publicPoolStratumPort(), this);
    m_publicPoolFrontendPort = portBox(m_config.publicPoolFrontendPort(), this);
    m_publicPoolAddress = new QLineEdit(m_config.publicPoolPayoutAddress(), this);
    addRow(serviceForm, "Electrs Port", m_electrsPort);
    addRow(serviceForm, "Mempool DB Port", m_mempoolDatabasePort);
    addRow(serviceForm, "Mempool Backend Port", m_mempoolBackendPort);
    addRow(serviceForm, "Mempool Web Port", m_mempoolFrontendPort);
    addRow(serviceForm, "Public Pool API Port", m_publicPoolApiPort);
    addRow(serviceForm, "Public Pool Stratum Port", m_publicPoolStratumPort);
    addRow(serviceForm, "Public Pool Web Port", m_publicPoolFrontendPort);
    addRow(serviceForm, "Public Pool Adresse", m_publicPoolAddress);

    auto* appForm = createSection("App");
    m_theme = new QComboBox(this);
    m_theme->addItems({"system", "light", "dark"});
    m_theme->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_theme->setCurrentText(m_config.theme());
    m_language = new QComboBox(this);
    m_language->addItems({"de", "en"});
    m_language->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_language->setCurrentText(m_config.language());
    m_autostart = new QCheckBox("Dienste beim App-Start automatisch starten", this);
    m_autostart->setChecked(m_config.autostart());
    addRow(appForm, "Theme", m_theme);
    addRow(appForm, "Sprache", m_language);
    addRow(appForm, "Autostart", m_autostart);

    contentLayout->addStretch();
    scroll->setWidget(content);

    auto* saveButton = new QPushButton("Speichern", this);
    saveButton->setObjectName("primaryButton");

    root->addWidget(title);
    root->addWidget(scroll, 1);
    root->addWidget(saveButton, 0, Qt::AlignRight);

    QObject::connect(browse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, "Datenverzeichnis auswählen", m_baseDir->text());
        if (!dir.isEmpty()) {
            m_baseDir->setText(dir);
        }
    });
    QObject::connect(saveButton, &QPushButton::clicked, this, &SettingsPage::save);
}

void SettingsPage::save()
{
    if (!m_baseDir->text().trimmed().isEmpty()) {
        m_config.setBaseDataDir(m_baseDir->text().trimmed());
    }
    m_config.setStringValue("bitcoin/rpcUser", m_rpcUser->text().trimmed());
    m_config.setStringValue("bitcoin/rpcPassword", m_rpcPassword->text());
    m_config.setStringValue("bitcoin/network", m_network->currentData().toString());
    m_config.setUIntValue("bitcoin/rpcPort", static_cast<quint16>(m_rpcPort->value()));
    m_config.setUIntValue("electrs/port", static_cast<quint16>(m_electrsPort->value()));
    m_config.setUIntValue("mempool/databasePort", static_cast<quint16>(m_mempoolDatabasePort->value()));
    m_config.setUIntValue("mempool/backendPort", static_cast<quint16>(m_mempoolBackendPort->value()));
    m_config.setUIntValue("mempool/frontendPort", static_cast<quint16>(m_mempoolFrontendPort->value()));
    m_config.setUIntValue("publicPool/apiPort", static_cast<quint16>(m_publicPoolApiPort->value()));
    m_config.setUIntValue("publicPool/stratumPort", static_cast<quint16>(m_publicPoolStratumPort->value()));
    m_config.setUIntValue("publicPool/frontendPort", static_cast<quint16>(m_publicPoolFrontendPort->value()));
    m_config.setStringValue("publicPool/payoutAddress", m_publicPoolAddress->text().trimmed());
    m_config.setStringValue("app/theme", m_theme->currentText());
    m_config.setStringValue("app/language", m_language->currentText());
    m_config.setBoolValue("app/autostart", m_autostart->isChecked());
}
