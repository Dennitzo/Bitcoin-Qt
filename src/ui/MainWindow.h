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
#include <QStackedWidget>
#include <QWebChannel>
#include <QWebEngineProfile>

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(ConfigManager& config, LogManager& logs, ServiceManager& services, QWidget* parent = nullptr);

private:
    void buildUi();
    void connectSignals();
    void configureWebEngine();

    ConfigManager& m_config;
    LogManager& m_logs;
    ServiceManager& m_services;

    QListWidget* m_sidebar = nullptr;
    QStackedWidget* m_pages = nullptr;
    DashboardPage* m_dashboard = nullptr;
    NodePage* m_node = nullptr;
    LogsPage* m_logPage = nullptr;
    SettingsPage* m_settings = nullptr;

    QWebEngineProfile* m_profile = nullptr;
    LocalUrlInterceptor* m_interceptor = nullptr;
    QWebChannel* m_channel = nullptr;
    WebBridge* m_bridge = nullptr;
};
