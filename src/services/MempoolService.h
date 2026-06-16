#pragma once

#include "../core/ManagedService.h"

#include <QNetworkAccessManager>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>

class MempoolService final : public ManagedService {
    Q_OBJECT

public:
    MempoolService(ConfigManager& config, LogManager& logs, QObject* parent = nullptr);
    ~MempoolService() override;

    void start() override;
    void stop() override;
    QUrl frontendUrl() const;

Q_SIGNALS:
    void frontendAvailable(const QUrl& url);

private:
    void waitForDatabase();
    void waitForElectrs();
    void startBackend();
    void startFrontend();
    void checkBackend();
    void checkFrontend();
    void attachProcess(QProcess& child, const QString& logId);
    QString configFilePath() const;
    bool databaseDirectoryInitialized() const;
    bool writeBackendConfig() const;

    QProcess m_backend;
    QProcess m_frontend;
    QTimer m_databaseHealth;
    QTimer m_electrsHealth;
    QTimer m_backendHealth;
    QTimer m_frontendHealth;
    QNetworkAccessManager m_network;
    bool m_startRequested = false;
};
