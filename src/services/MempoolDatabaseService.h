#pragma once

#include "../core/ManagedService.h"

#include <QProcess>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

class MempoolDatabaseService final : public ManagedService {
    Q_OBJECT

public:
    MempoolDatabaseService(ConfigManager& config, LogManager& logs, QObject* parent = nullptr);
    ~MempoolDatabaseService() override;

    void start() override;
    void stop() override;

private:
    void handleStdout(const QString& line) override;
    void handleStderr(const QString& line) override;
    void initializeDatabase();
    void startDatabase();
    void checkPort();
    void handleStartupFailure(const QString& output);
    bool resetDatabaseDirectory();
    void cleanupInitializationDir();
    bool isInitialized() const;
    bool writeInitSql() const;
    QString socketPath() const;
    QString pidPath() const;
    QString initSqlPath() const;
    QString baseDir() const;

    QProcess m_initializer;
    QTimer m_healthTimer;
    QStringList m_recentOutput;
    QString m_initializationDir;
    bool m_startRequested = false;
    bool m_attemptedReset = false;
};
