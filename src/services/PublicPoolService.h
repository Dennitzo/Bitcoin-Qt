#pragma once

#include "../core/ManagedService.h"

#include "../core/AppTypes.h"

#include <QNetworkAccessManager>
#include <QProcess>
#include <QTimer>

class PublicPoolService final : public ManagedService {
    Q_OBJECT

public:
    PublicPoolService(ConfigManager& config, LogManager& logs, QObject* parent = nullptr);
    ~PublicPoolService() override;

    void start() override;
    void stop() override;
    QUrl frontendUrl() const;

Q_SIGNALS:
    void frontendAvailable(const QUrl& url);
    void statsChanged(const PublicPoolStats& stats);

private:
    void startBackend();
    void startFrontend();
    void checkBackend();
    void checkFrontend();
    void refreshStats();
    void fetchNetworkHashrateMaximum(PublicPoolStats stats);
    void emitOfflineStats();
    void attachProcess(QProcess& child, const QString& logId);

    QProcess m_backend;
    QProcess m_frontend;
    QTimer m_backendHealth;
    QTimer m_frontendHealth;
    QTimer m_statsTimer;
    QNetworkAccessManager m_network;
    bool m_startRequested = false;
    bool m_statsRequestInFlight = false;
};
