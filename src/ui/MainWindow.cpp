#include "MainWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

MainWindow::MainWindow(ConfigManager& config, LogManager& logs, ServiceManager& services, QWidget* parent)
    : QMainWindow(parent),
      m_config(config),
      m_logs(logs),
      m_services(services)
{
    buildUi();
    configureWebEngine();
    connectSignals();
}

void MainWindow::buildUi()
{
    setWindowTitle("Bitcoin Node Desktop");
    resize(1320, 860);

    auto* central = new QWidget(this);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sidebarFrame = new QWidget(central);
    sidebarFrame->setFixedWidth(232);
    sidebarFrame->setStyleSheet(
        "QWidget{background:#161821;color:#f5f7fa;}"
        "QLabel#brand{font-size:20px;font-weight:700;padding:24px 20px;}"
        "QListWidget{border:none;outline:none;background:#161821;padding:8px;}"
        "QListWidget::item{height:44px;border-radius:6px;padding-left:14px;color:#d7dce2;}"
        "QListWidget::item:selected{background:#2f80ed;color:white;}"
        "QListWidget::item:hover{background:#252936;}"
        "QPushButton{margin:14px 16px 18px 16px;height:40px;border:none;border-radius:6px;background:#20bf6b;color:white;font-weight:700;}"
    );
    auto* sidebarLayout = new QVBoxLayout(sidebarFrame);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    auto* brand = new QLabel("Bitcoin Node", sidebarFrame);
    brand->setObjectName("brand");
    m_sidebar = new QListWidget(sidebarFrame);
    m_sidebar->addItems({"Dashboard", "Node", "Logs", "Einstellungen"});
    m_sidebar->setCurrentRow(0);
    auto* startButton = new QPushButton("Node starten", sidebarFrame);
    sidebarLayout->addWidget(brand);
    sidebarLayout->addWidget(m_sidebar, 1);
    sidebarLayout->addWidget(startButton);

    m_pages = new QStackedWidget(central);
    m_dashboard = new DashboardPage(m_pages);
    m_node = new NodePage(m_pages);
    m_logPage = new LogsPage(m_pages);
    m_settings = new SettingsPage(m_config, m_pages);
    m_pages->addWidget(m_dashboard);
    m_pages->addWidget(m_node);
    m_pages->addWidget(m_logPage);
    m_pages->addWidget(m_settings);

    root->addWidget(sidebarFrame);
    root->addWidget(m_pages, 1);
    setCentralWidget(central);

    QObject::connect(m_sidebar, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    QObject::connect(startButton, &QPushButton::clicked, &m_services, &ServiceManager::startAll);
}

void MainWindow::connectSignals()
{
    QObject::connect(&m_services, &ServiceManager::bitcoinStatusChanged, m_dashboard, &DashboardPage::updateBitcoinStatus);
    QObject::connect(&m_services, &ServiceManager::serviceStatusChanged, m_dashboard, &DashboardPage::updateServiceStatus);
    QObject::connect(&m_logs, &LogManager::lineAppended, m_logPage, &LogsPage::appendLogLine);
    QObject::connect(&m_services, &ServiceManager::mempoolFrontendAvailable, m_node, &NodePage::loadMempool);
    QObject::connect(&m_services, &ServiceManager::errorRaised, this, [this](const QString& title, const QString& message) {
        statusBar()->showMessage(QString("%1: %2").arg(title, message), 8000);
    });
}

void MainWindow::configureWebEngine()
{
    m_profile = new QWebEngineProfile("mempool", this);
    m_interceptor = new LocalUrlInterceptor(m_profile);
    m_interceptor->setAllowedPorts({static_cast<int>(m_config.mempoolFrontendPort()), static_cast<int>(m_config.mempoolBackendPort())});
    m_profile->setUrlRequestInterceptor(m_interceptor);

    auto* page = new QWebEnginePage(m_profile, m_node->webView());
    m_channel = new QWebChannel(page);
    m_bridge = new WebBridge(m_services, m_channel);
    m_channel->registerObject("qt", m_bridge);
    page->setWebChannel(m_channel);

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
            document.documentElement.appendChild(script);
        })();
    )JS");
    page->scripts().insert(bridgeScript);
    m_node->webView()->setPage(page);
}
