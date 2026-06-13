#pragma once

#include "../core/ConfigManager.h"
#include "../core/LogManager.h"
#include "../core/ServiceManager.h"
#include "../web/LocalUrlInterceptor.h"
#include "../web/WebBridge.h"

#include "DashboardPage.h"
#include "LogsPage.h"
#include "NodePage.h"
#include "SettingsPage.h"

#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QWebChannel>
#include <QWebEngineProfile>
#include <QWebEngineView>

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(ConfigManager& config, LogManager& logs, ServiceManager& services, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    void connectSignals();
    void configureWebEngine();
    void shutdownWebEngine();
    void applyStyle();
    void retranslate();
    QString lightStyle() const;
    QString darkStyle() const;
    void configureWebPage(QWebEngineView* view);
    void updateSidebarAvailability();

protected:
    void closeEvent(QCloseEvent* event) override;

    ConfigManager& m_config;
    LogManager& m_logs;
    ServiceManager& m_services;

    QListWidget* m_sidebar = nullptr;
    QListWidgetItem* m_dashboardItem = nullptr;
    QListWidgetItem* m_bitcoindItem = nullptr;
    QListWidgetItem* m_electrsItem = nullptr;
    QListWidgetItem* m_mempoolItem = nullptr;
    QListWidgetItem* m_publicPoolItem = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QPushButton* m_quitButton = nullptr;
    QStackedWidget* m_pages = nullptr;
    DashboardPage* m_dashboard = nullptr;
    LogsPage* m_bitcoindLog = nullptr;
    LogsPage* m_electrsLog = nullptr;
    NodePage* m_mempoolPage = nullptr;
    NodePage* m_publicPoolPage = nullptr;
    SettingsPage* m_settings = nullptr;

    QWebEngineProfile* m_profile = nullptr;
    LocalUrlInterceptor* m_interceptor = nullptr;
    QList<QWebChannel*> m_channels;
    QList<WebBridge*> m_bridges;
};
