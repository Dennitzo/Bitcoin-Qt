#pragma once

#include "AppTypes.h"
#include "ConfigManager.h"
#include "LogManager.h"

#include <QObject>
#include <QProcess>
#include <QTimer>

class ManagedService : public QObject {
    Q_OBJECT

public:
    ManagedService(QString id, QString label, ConfigManager& config, LogManager& logs, QObject* parent = nullptr);
    ~ManagedService() override;

    QString id() const;
    QString label() const;
    ServiceState state() const;
    QString detail() const;
    ServiceStatus status() const;

    virtual void start() = 0;
    virtual void stop();
    virtual void restart();

Q_SIGNALS:
    void statusChanged(const ServiceStatus& status);
    void crashed(const QString& id, const QString& message);
    void ready(const QString& id);

protected:
    void startProcess(const QString& program, const QStringList& arguments, const QString& workingDirectory = {});
    void setState(ServiceState state, const QString& detail = {});
    void setRestartEnabled(bool enabled);
    void scheduleRestart();
    void beginManualStop();
    void endManualStop();
    bool isManualStopRequested() const;
    QProcess& process();
    ConfigManager& config();
    const ConfigManager& config() const;
    LogManager& logs();
    const LogManager& logs() const;

    virtual void handleStdout(const QString& line);
    virtual void handleStderr(const QString& line);
    virtual bool shouldAutoRestart() const;

private:
    void connectProcess();
    void appendOutput(QProcess::ProcessChannel channel);

    QString m_id;
    QString m_label;
    ServiceState m_state = ServiceState::Stopped;
    QString m_detail;
    ConfigManager& m_config;
    LogManager& m_logs;
    QProcess m_process;
    QTimer m_restartTimer;
    bool m_restartEnabled = true;
    bool m_stopping = false;
};
