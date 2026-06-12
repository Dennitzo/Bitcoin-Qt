#include "MainWindow.h"

#include <QHBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace {

class EmbeddedWebPage final : public QWebEnginePage {
public:
    explicit EmbeddedWebPage(QWebEngineProfile* profile, QObject* parent = nullptr)
        : QWebEnginePage(profile, parent)
    {
    }

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID) override
    {
        if (level == InfoMessageLevel) {
            return;
        }
        if (message == "ERROR [object Object]") {
            return;
        }

        const char* levelName = "info";
        if (level == WarningMessageLevel) {
            levelName = "warning";
        } else if (level == ErrorMessageLevel) {
            levelName = "error";
        }
        qWarning().noquote() << QString("WebView JS %1: %2 (%3:%4)")
            .arg(levelName, message, sourceID)
            .arg(lineNumber);
    }
};

}

MainWindow::MainWindow(ConfigManager& config, LogManager& logs, ServiceManager& services, QWidget* parent)
    : QMainWindow(parent),
      m_config(config),
      m_logs(logs),
      m_services(services)
{
    buildUi();
    applyStyle();
    configureWebEngine();
    connectSignals();
}

MainWindow::~MainWindow()
{
    shutdownWebEngine();
}

void MainWindow::buildUi()
{
    setWindowTitle("Bitcoin-Qt");
    resize(1320, 860);
    setMinimumSize(1040, 700);

    auto* central = new QWidget(this);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sidebarFrame = new QWidget(central);
    sidebarFrame->setFixedWidth(232);
    sidebarFrame->setObjectName("sidebar");
    auto* sidebarLayout = new QVBoxLayout(sidebarFrame);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);
    auto* brand = new QWidget(sidebarFrame);
    brand->setObjectName("brand");
    auto* brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(22, 24, 22, 16);
    brandLayout->setSpacing(10);
    auto* brandIcon = new QLabel(brand);
    brandIcon->setObjectName("brandIcon");
    brandIcon->setFixedSize(34, 34);
    brandIcon->setPixmap(QPixmap(":/icons/Bitcoin.png").scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto* brandText = new QLabel("Bitcoin-Qt", brand);
    brandText->setObjectName("brandText");
    brandLayout->addWidget(brandIcon);
    brandLayout->addWidget(brandText, 1);
    m_sidebar = new QListWidget(sidebarFrame);
    m_sidebar->addItems({"Dashboard", "Bitcoind", "Electrs", "Mempool", "Public Pool"});
    m_sidebar->setCurrentRow(0);
    m_settingsButton = new QPushButton("Einstellungen", sidebarFrame);
    m_settingsButton->setObjectName("sidebarNavButton");
    m_settingsButton->setCheckable(true);
    sidebarLayout->addWidget(brand);
    sidebarLayout->addWidget(m_sidebar, 1);
    sidebarLayout->addWidget(m_settingsButton);

    m_pages = new QStackedWidget(central);
    m_dashboard = new DashboardPage(m_config, m_pages);
    m_bitcoindLog = new LogsPage("Bitcoind Log", {"bitcoind"}, m_pages);
    m_electrsLog = new LogsPage("Electrs Log", {"electrs"}, m_pages);
    m_mempoolPage = new NodePage(m_config, "mempool", "Mempool wird geladen, sobald Backend und Frontend bereit sind.", m_pages);
    m_publicPoolPage = new NodePage(m_config, "publicPool", "Public Pool wird geladen, sobald Stratum/API und UI bereit sind.", m_pages);
    m_settings = new SettingsPage(m_config, m_pages);
    m_pages->addWidget(m_dashboard);
    m_pages->addWidget(m_bitcoindLog);
    m_pages->addWidget(m_electrsLog);
    m_pages->addWidget(m_mempoolPage);
    m_pages->addWidget(m_publicPoolPage);
    m_pages->addWidget(m_settings);

    root->addWidget(sidebarFrame);
    root->addWidget(m_pages, 1);
    setCentralWidget(central);

    QObject::connect(m_sidebar, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    QObject::connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            m_settingsButton->setChecked(false);
        }
    });
    QObject::connect(m_settingsButton, &QPushButton::clicked, this, [this]() {
        m_sidebar->clearSelection();
        m_sidebar->setCurrentRow(-1);
        m_pages->setCurrentWidget(m_settings);
        m_settingsButton->setChecked(true);
    });
}

void MainWindow::connectSignals()
{
    QObject::connect(&m_services, &ServiceManager::bitcoinStatusChanged, m_dashboard, &DashboardPage::updateBitcoinStatus);
    QObject::connect(&m_services, &ServiceManager::serviceStatusChanged, m_dashboard, &DashboardPage::updateServiceStatus);
    QObject::connect(m_dashboard, &DashboardPage::startServiceRequested, &m_services, &ServiceManager::startService);
    QObject::connect(m_dashboard, &DashboardPage::stopServiceRequested, &m_services, &ServiceManager::stopService);
    QObject::connect(&m_logs, &LogManager::lineAppended, m_bitcoindLog, &LogsPage::appendLogLine);
    QObject::connect(&m_logs, &LogManager::lineAppended, m_electrsLog, &LogsPage::appendLogLine);
    QObject::connect(&m_services, &ServiceManager::mempoolFrontendAvailable, m_mempoolPage, &NodePage::loadUrl);
    QObject::connect(&m_services, &ServiceManager::publicPoolFrontendAvailable, m_publicPoolPage, &NodePage::loadUrl);
    QObject::connect(&m_services, &ServiceManager::errorRaised, this, [this](const QString& title, const QString& message) {
        statusBar()->showMessage(QString("%1: %2").arg(title, message), 8000);
    });
    QObject::connect(&m_config, &ConfigManager::changed, this, &MainWindow::applyStyle);

    m_dashboard->updateBitcoinStatus(m_services.bitcoinStatus());
    for (const ServiceStatus& status : m_services.statuses()) {
        m_dashboard->updateServiceStatus(status);
    }
}

void MainWindow::configureWebEngine()
{
    m_profile = new QWebEngineProfile("bitcoin-qt", this);
    m_interceptor = new LocalUrlInterceptor(m_profile);
    m_interceptor->setAllowedPorts({
        static_cast<int>(m_config.mempoolFrontendPort()),
        static_cast<int>(m_config.mempoolBackendPort()),
        static_cast<int>(m_config.publicPoolFrontendPort()),
        static_cast<int>(m_config.publicPoolApiPort()),
    });
    m_profile->setUrlRequestInterceptor(m_interceptor);

    configureWebPage(m_mempoolPage->webView());
    configureWebPage(m_publicPoolPage->webView());
}

void MainWindow::shutdownWebEngine()
{
    const QList<QWebEngineView*> views = {
        m_mempoolPage ? m_mempoolPage->webView() : nullptr,
        m_publicPoolPage ? m_publicPoolPage->webView() : nullptr,
    };

    for (QWebEngineView* view : views) {
        if (!view) {
            continue;
        }
        view->stop();
        QWebEnginePage* page = view->page();
        view->setPage(nullptr);
        delete page;
    }

    m_channels.clear();
    m_bridges.clear();

    if (m_profile) {
        m_profile->setUrlRequestInterceptor(nullptr);
        delete m_profile;
        m_profile = nullptr;
        m_interceptor = nullptr;
    }
}

void MainWindow::applyStyle()
{
    const QString theme = m_config.theme().toLower();
    setStyleSheet(theme == "dark" ? darkStyle() : lightStyle());
}

QString MainWindow::lightStyle() const
{
    return R"QSS(
        QMainWindow, QWidget {
            background: #f6f7fb;
            color: #1d1d1f;
            font-family: "Helvetica Neue", Arial;
            font-size: 14px;
        }
        QLabel#pageTitle {
            font-size: 30px;
            font-weight: 700;
            color: #1d1d1f;
        }
        QWidget#sidebar {
            background: #ffffff;
            border: none;
        }
        QWidget#brand, QWidget#brand QLabel {
            background: #ffffff;
        }
        QLabel#brandText {
            font-size: 22px;
            font-weight: 800;
            color: #1d1d1f;
        }
        QLabel#brandIcon {
            background: transparent;
        }
        QListWidget {
            border: none;
            outline: none;
            background: #ffffff;
            padding: 10px;
        }
        QListWidget::item {
            height: 42px;
            border-radius: 12px;
            padding-left: 14px;
            color: #5f6673;
        }
        QListWidget::item:selected {
            background: #111827;
            color: #ffffff;
        }
        QListWidget::item:hover:!selected {
            background: #f0f3f8;
        }
        QPushButton#sidebarNavButton {
            margin: 14px 16px 18px 16px;
            min-height: 42px;
            text-align: left;
            padding-left: 14px;
            border: none;
            border-radius: 12px;
            background: #f0f3f8;
            color: #303746;
            font-weight: 700;
        }
        QPushButton#sidebarNavButton:checked {
            background: #111827;
            color: #ffffff;
        }
        QWidget#metricCard {
            background: #ffffff;
            border: none;
            border-radius: 18px;
        }
        QWidget#metricCard QLabel, QWidget#metricCard QWidget {
            background: transparent;
        }
        QWidget#settingsContent {
            background: transparent;
        }
        QWidget#settingsSection {
            background: #ffffff;
            border: none;
            border-radius: 18px;
        }
        QWidget#settingsSection QLabel, QWidget#settingsSection QWidget {
            background: transparent;
        }
        QLabel#settingsSectionTitle {
            color: #1d1d1f;
            font-size: 16px;
            font-weight: 700;
        }
        QLabel#settingsFieldLabel {
            color: #687386;
            font-weight: 600;
        }
        QPushButton {
            min-height: 36px;
            border-radius: 11px;
            border: none;
            background: #eef1f6;
            color: #1d1d1f;
            padding: 0 16px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #e5eaf2;
        }
        QPushButton#primaryButton {
            border: none;
            background: #111827;
            color: #ffffff;
        }
        QPushButton#secondaryButton {
            background: #eef1f6;
            border: 1px solid #cfd7e3;
            color: #1d1d1f;
        }
        QPushButton#secondaryButton:disabled {
            background: #f4f6fa;
            border: 1px solid #e1e6ef;
            color: #a4adbb;
        }
        QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {
            min-height: 36px;
            border: 1px solid #e2e7ef;
            border-radius: 11px;
            background: #ffffff;
            padding: 0 11px;
            selection-background-color: #111827;
        }
        QWidget#settingsSection QLineEdit,
        QWidget#settingsSection QSpinBox,
        QWidget#settingsSection QComboBox {
            background: #ffffff;
        }
        QPlainTextEdit {
            padding: 10px;
        }
        QPlainTextEdit#logView {
            background: #ffffff;
            border: none;
            border-radius: 18px;
            padding: 18px;
            font-family: Menlo, Consolas, monospace;
            font-size: 12px;
        }
        QPlainTextEdit#logView QScrollBar:vertical,
        QPlainTextEdit#logView QScrollBar:horizontal,
        QScrollArea#settingsScroll QScrollBar:vertical,
        QScrollArea#settingsScroll QScrollBar:horizontal {
            width: 0px;
            height: 0px;
            background: transparent;
        }
        QTabWidget::pane {
            border: none;
            border-radius: 18px;
            background: #ffffff;
            top: -1px;
        }
        QTabBar::tab {
            background: #eef1f6;
            color: #4b5563;
            padding: 9px 16px;
            border-radius: 11px;
            margin-right: 4px;
        }
        QTabBar::tab:selected {
            background: #111827;
            color: #ffffff;
            border: none;
        }
        QProgressBar {
            border: none;
            border-radius: 6px;
            background: #e4e8f0;
        }
        QProgressBar::chunk {
            border-radius: 6px;
            background: #30d158;
        }
        QScrollArea {
            border: none;
            background: transparent;
        }
        QCheckBox {
            spacing: 9px;
        }
    )QSS";
}

QString MainWindow::darkStyle() const
{
    return R"QSS(
        QMainWindow, QWidget {
            background: #0f1117;
            color: #f5f7fb;
            font-family: "Helvetica Neue", Arial;
            font-size: 14px;
        }
        QLabel#pageTitle {
            font-size: 30px;
            font-weight: 700;
            color: #f5f7fb;
        }
        QWidget#sidebar {
            background: #151922;
            border: none;
        }
        QWidget#brand, QWidget#brand QLabel {
            background: #151922;
        }
        QLabel#brandText {
            font-size: 22px;
            font-weight: 800;
            color: #f5f7fb;
        }
        QLabel#brandIcon {
            background: transparent;
        }
        QListWidget {
            border: none;
            outline: none;
            background: #151922;
            padding: 10px;
        }
        QListWidget::item {
            height: 42px;
            border-radius: 12px;
            padding-left: 14px;
            color: #aab2c0;
        }
        QListWidget::item:selected {
            background: #f5f7fb;
            color: #111827;
        }
        QListWidget::item:hover:!selected {
            background: #202633;
        }
        QPushButton#sidebarNavButton {
            margin: 14px 16px 18px 16px;
            min-height: 42px;
            text-align: left;
            padding-left: 14px;
            border: none;
            border-radius: 12px;
            background: #202633;
            color: #d7dce5;
            font-weight: 700;
        }
        QPushButton#sidebarNavButton:checked {
            background: #f5f7fb;
            color: #111827;
        }
        QWidget#metricCard {
            background: #191e29;
            border: none;
            border-radius: 18px;
        }
        QWidget#metricCard QLabel, QWidget#metricCard QWidget {
            background: transparent;
        }
        QWidget#settingsContent {
            background: transparent;
        }
        QWidget#settingsSection {
            background: #191e29;
            border: none;
            border-radius: 18px;
        }
        QWidget#settingsSection QLabel, QWidget#settingsSection QWidget {
            background: transparent;
        }
        QLabel#settingsSectionTitle {
            color: #f5f7fb;
            font-size: 16px;
            font-weight: 700;
        }
        QLabel#settingsFieldLabel {
            color: #aab2c0;
            font-weight: 600;
        }
        QPushButton {
            min-height: 36px;
            border-radius: 11px;
            border: none;
            background: #262d3a;
            color: #f5f7fb;
            padding: 0 16px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #30394a;
        }
        QPushButton#primaryButton {
            background: #f5f7fb;
            color: #111827;
        }
        QPushButton#secondaryButton {
            background: #262d3a;
            border: 1px solid #3b4557;
            color: #f5f7fb;
        }
        QPushButton#secondaryButton:disabled {
            background: #1d2330;
            border: 1px solid #2a3241;
            color: #687386;
        }
        QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {
            min-height: 36px;
            border: 1px solid #303746;
            border-radius: 11px;
            background: #11151e;
            color: #f5f7fb;
            padding: 0 11px;
            selection-background-color: #f5f7fb;
            selection-color: #111827;
        }
        QWidget#settingsSection QLineEdit,
        QWidget#settingsSection QSpinBox,
        QWidget#settingsSection QComboBox {
            background: #11151e;
        }
        QPlainTextEdit {
            padding: 10px;
        }
        QPlainTextEdit#logView {
            background: #191e29;
            border: none;
            border-radius: 18px;
            padding: 18px;
            font-family: Menlo, Consolas, monospace;
            font-size: 12px;
        }
        QPlainTextEdit#logView QScrollBar:vertical,
        QPlainTextEdit#logView QScrollBar:horizontal,
        QScrollArea#settingsScroll QScrollBar:vertical,
        QScrollArea#settingsScroll QScrollBar:horizontal {
            width: 0px;
            height: 0px;
            background: transparent;
        }
        QTabWidget::pane {
            border: none;
            border-radius: 18px;
            background: #191e29;
            top: -1px;
        }
        QTabBar::tab {
            background: #262d3a;
            color: #aab2c0;
            padding: 9px 16px;
            border-radius: 11px;
            margin-right: 4px;
        }
        QTabBar::tab:selected {
            background: #f5f7fb;
            color: #1d1d1f;
            border: none;
        }
        QProgressBar {
            border: none;
            border-radius: 6px;
            background: #262d3a;
        }
        QProgressBar::chunk {
            border-radius: 6px;
            background: #30d158;
        }
        QScrollArea {
            border: none;
            background: transparent;
        }
        QCheckBox {
            spacing: 9px;
        }
    )QSS";
}

void MainWindow::configureWebPage(QWebEngineView* view)
{
    auto* page = new EmbeddedWebPage(m_profile, view);
    auto* channel = new QWebChannel(page);
    auto* bridge = new WebBridge(m_services, channel);
    channel->registerObject("qt", bridge);
    page->setWebChannel(channel);
    m_channels.append(channel);
    m_bridges.append(bridge);

    QWebEngineScript bridgeScript;
    bridgeScript.setName("bitcoin-node-webchannel");
    bridgeScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    bridgeScript.setRunsOnSubFrames(false);
    bridgeScript.setWorldId(QWebEngineScript::MainWorld);
    bridgeScript.setSourceCode(R"JS(
        (function() {
            function install() {
                if (!window.QWebChannel || !window.qt || !window.qt.webChannelTransport) {
                    return;
                }
                new QWebChannel(window.qt.webChannelTransport, function(channel) {
                    window.qt = channel.objects.qt;
                });
            }
            var script = document.createElement('script');
            script.src = 'qrc:///qtwebchannel/qwebchannel.js';
            script.onload = install;
            function appendScript() {
                (document.head || document.documentElement).appendChild(script);
            }
            if (document.head || document.documentElement) {
                appendScript();
            } else {
                document.addEventListener('DOMContentLoaded', appendScript, { once: true });
            }
        })();
    )JS");
    page->scripts().insert(bridgeScript);
    view->setPage(page);
}
