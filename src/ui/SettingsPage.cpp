#include "SettingsPage.h"

#include "../core/Localization.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
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

QLabel* fieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
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

    m_title = new QLabel(this);
    m_title->setObjectName("pageTitle");

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

    auto createSection = [this, contentLayout](QLabel** titleTarget) {
        auto* section = new QWidget(this);
        section->setObjectName("settingsSection");
        auto* sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(22, 20, 22, 22);
        sectionLayout->setSpacing(16);

        auto* title = new QLabel(section);
        title->setObjectName("settingsSectionTitle");
        *titleTarget = title;
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

    auto addRow = [this](QFormLayout* form, QLabel** labelTarget, QWidget* field) {
        auto* label = fieldLabel(QString(), this);
        *labelTarget = label;
        form->addRow(label, field);
    };
    auto addLayoutRow = [this](QFormLayout* form, QLabel** labelTarget, QLayout* field) {
        auto* label = fieldLabel(QString(), this);
        *labelTarget = label;
        form->addRow(label, field);
    };

    auto* storageForm = createSection(&m_storageTitle);
    m_baseDir = new QLineEdit(m_config.baseDataDir(), this);
    m_browseButton = new QPushButton(this);
    m_browseButton->setObjectName("secondaryButton");
    auto* baseRow = new QHBoxLayout();
    baseRow->addWidget(m_baseDir, 1);
    baseRow->addWidget(m_browseButton);
    addLayoutRow(storageForm, &m_baseDirLabel, baseRow);

    auto* bitcoinForm = createSection(&m_bitcoinTitle);
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
    addRow(bitcoinForm, &m_rpcUserLabel, m_rpcUser);
    addRow(bitcoinForm, &m_rpcPasswordLabel, m_rpcPassword);
    addRow(bitcoinForm, &m_rpcPortLabel, m_rpcPort);
    addRow(bitcoinForm, &m_networkLabel, m_network);

    auto* serviceForm = createSection(&m_servicesTitle);
    m_electrsPort = portBox(m_config.electrsPort(), this);
    m_mempoolDatabasePort = portBox(m_config.mempoolDatabasePort(), this);
    m_mempoolBackendPort = portBox(m_config.mempoolBackendPort(), this);
    m_mempoolFrontendPort = portBox(m_config.mempoolFrontendPort(), this);
    m_publicPoolApiPort = portBox(m_config.publicPoolApiPort(), this);
    m_publicPoolStratumPort = portBox(m_config.publicPoolStratumPort(), this);
    m_publicPoolFrontendPort = portBox(m_config.publicPoolFrontendPort(), this);
    m_publicPoolAddress = new QLineEdit(m_config.publicPoolPayoutAddress(), this);
    addRow(serviceForm, &m_electrsPortLabel, m_electrsPort);
    addRow(serviceForm, &m_mempoolDatabasePortLabel, m_mempoolDatabasePort);
    addRow(serviceForm, &m_mempoolBackendPortLabel, m_mempoolBackendPort);
    addRow(serviceForm, &m_mempoolFrontendPortLabel, m_mempoolFrontendPort);
    addRow(serviceForm, &m_publicPoolApiPortLabel, m_publicPoolApiPort);
    addRow(serviceForm, &m_publicPoolStratumPortLabel, m_publicPoolStratumPort);
    addRow(serviceForm, &m_publicPoolFrontendPortLabel, m_publicPoolFrontendPort);
    addRow(serviceForm, &m_publicPoolAddressLabel, m_publicPoolAddress);

    auto* appForm = createSection(&m_appTitle);
    m_theme = new QComboBox(this);
    m_theme->addItems({"system", "light", "dark"});
    m_theme->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_theme->setCurrentText(m_config.theme());
    m_language = new QComboBox(this);
    m_language->addItems({"en", "de"});
    m_language->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_language->setCurrentText(m_config.language());
    m_autostart = new QCheckBox(this);
    m_autostart->setChecked(m_config.autostart());
    addRow(appForm, &m_themeLabel, m_theme);
    addRow(appForm, &m_languageLabel, m_language);
    addRow(appForm, &m_autostartLabel, m_autostart);

    contentLayout->addStretch();
    scroll->setWidget(content);

    m_saveButton = new QPushButton(this);
    m_saveButton->setObjectName("primaryButton");

    root->addWidget(m_title);
    root->addWidget(scroll, 1);
    root->addWidget(m_saveButton, 0, Qt::AlignRight);

    m_savedOverlay = new QFrame(this);
    m_savedOverlay->setObjectName("settingsSavedOverlay");
    m_savedOverlay->setFixedSize(420, 126);
    m_savedOverlay->hide();
    auto* overlayLayout = new QVBoxLayout(m_savedOverlay);
    overlayLayout->setContentsMargins(24, 20, 24, 20);
    overlayLayout->setSpacing(6);
    m_savedOverlayTitle = new QLabel(m_savedOverlay);
    m_savedOverlayTitle->setObjectName("settingsSavedOverlayTitle");
    m_savedOverlayText = new QLabel(m_savedOverlay);
    m_savedOverlayText->setObjectName("settingsSavedOverlayText");
    m_savedOverlayText->setWordWrap(true);
    overlayLayout->addWidget(m_savedOverlayTitle);
    overlayLayout->addWidget(m_savedOverlayText);

    QObject::connect(m_browseButton, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, appText(m_config.language(), "firstRun.chooseDirectory"), m_baseDir->text());
        if (!dir.isEmpty()) {
            m_baseDir->setText(dir);
        }
    });
    QObject::connect(m_saveButton, &QPushButton::clicked, this, &SettingsPage::save);
    QObject::connect(&m_config, &ConfigManager::changed, this, &SettingsPage::retranslate);
    retranslate();
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
    showSavedOverlay();
}

void SettingsPage::showSavedOverlay()
{
    if (!m_savedOverlay) {
        return;
    }
    positionSavedOverlay();
    m_savedOverlay->raise();
    m_savedOverlay->show();
    QTimer::singleShot(4200, m_savedOverlay, &QFrame::hide);
}

void SettingsPage::positionSavedOverlay()
{
    if (!m_savedOverlay) {
        return;
    }
    const int x = (width() - m_savedOverlay->width()) / 2;
    const int y = (height() - m_savedOverlay->height()) / 2;
    m_savedOverlay->move(qMax(16, x), y);
}

void SettingsPage::retranslate()
{
    const QString lang = m_config.language();
    if (m_title) m_title->setText(appText(lang, "app.settings"));
    if (m_browseButton) m_browseButton->setText(appText(lang, "app.select"));
    if (m_saveButton) m_saveButton->setText(appText(lang, "app.save"));
    if (m_storageTitle) m_storageTitle->setText(appText(lang, "settings.storage"));
    if (m_bitcoinTitle) m_bitcoinTitle->setText("Bitcoin Core");
    if (m_servicesTitle) m_servicesTitle->setText(appText(lang, "settings.services"));
    if (m_appTitle) m_appTitle->setText(appText(lang, "settings.app"));
    if (m_baseDirLabel) m_baseDirLabel->setText(appText(lang, "settings.dataDirectory"));
    if (m_rpcUserLabel) m_rpcUserLabel->setText(appText(lang, "settings.rpcUser"));
    if (m_rpcPasswordLabel) m_rpcPasswordLabel->setText(appText(lang, "settings.rpcPassword"));
    if (m_rpcPortLabel) m_rpcPortLabel->setText(appText(lang, "settings.rpcPort"));
    if (m_networkLabel) m_networkLabel->setText(appText(lang, "settings.network"));
    if (m_electrsPortLabel) m_electrsPortLabel->setText("Electrs Port");
    if (m_mempoolDatabasePortLabel) m_mempoolDatabasePortLabel->setText("Mempool DB Port");
    if (m_mempoolBackendPortLabel) m_mempoolBackendPortLabel->setText("Mempool Backend Port");
    if (m_mempoolFrontendPortLabel) m_mempoolFrontendPortLabel->setText("Mempool Web Port");
    if (m_publicPoolApiPortLabel) m_publicPoolApiPortLabel->setText("Public Pool API Port");
    if (m_publicPoolStratumPortLabel) m_publicPoolStratumPortLabel->setText("Public Pool Stratum Port");
    if (m_publicPoolFrontendPortLabel) m_publicPoolFrontendPortLabel->setText("Public Pool Web Port");
    if (m_publicPoolAddressLabel) m_publicPoolAddressLabel->setText(appText(lang, "settings.publicPoolAddress"));
    if (m_themeLabel) m_themeLabel->setText(appText(lang, "settings.theme"));
    if (m_languageLabel) m_languageLabel->setText(appText(lang, "settings.language"));
    if (m_autostartLabel) m_autostartLabel->setText(appText(lang, "settings.autostart"));
    if (m_autostart) m_autostart->setText(appText(lang, "settings.autostartText"));
    if (m_savedOverlayTitle) m_savedOverlayTitle->setText(appText(lang, "settings.savedTitle"));
    if (m_savedOverlayText) m_savedOverlayText->setText(appText(lang, "settings.savedText"));
}

void SettingsPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionSavedOverlay();
}
