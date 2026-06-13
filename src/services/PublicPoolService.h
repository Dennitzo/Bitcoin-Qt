#pragma once

#include "../core/ManagedService.h"

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

private:
    void startBackend();
    void startFrontend();
    void checkBackend();
    void checkFrontend();
    void attachProcess(QProcess& child, const QString& logId);

    QProcess m_backend;
    QProcess m_frontend;
    QTimer m_backendHealth;
    QTimer m_frontendHealth;
    QNetworkAccessManager m_network;
    bool m_startRequested = false;
};
