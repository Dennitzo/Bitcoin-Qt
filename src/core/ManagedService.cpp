#include "ManagedService.h"

#include <QDir>
#include <QFileInfo>

ManagedService::ManagedService(QString id, QString label, ConfigManager& config, LogManager& logs, QObject* parent)
    : QObject(parent),
      m_id(std::move(id)),
      m_label(std::move(label)),
      m_config(config),
      m_logs(logs)
{
    connectProcess();
    m_restartTimer.setSingleShot(true);
    QObject::connect(&m_restartTimer, &QTimer::timeout, this, [this]() {
        if (shouldAutoRestart()) {
            start();
        }
    });
}

ManagedService::~ManagedService()
{
    setRestartEnabled(false);
    stop();
}

QString ManagedService::id() const
{
    return m_id;
}

QString ManagedService::label() const
{
    return m_label;
}

ServiceState ManagedService::state() const
{
    return m_state;
}

QString ManagedService::detail() const
{
    return m_detail;
}

ServiceStatus ManagedService::status() const
{
    return ServiceStatus{m_id, m_label, m_state, m_detail, QDateTime::currentDateTimeUtc()};
}

void ManagedService::markWaiting(const QString& detail)
{
    if (m_process.state() == QProcess::NotRunning) {
        setState(ServiceState::Starting, detail);
    }
}

void ManagedService::stop()
{
    beginManualStop();
    m_restartTimer.stop();
    if (m_process.state() == QProcess::NotRunning) {
        setState(ServiceState::Stopped, "Gestoppt");
        endManualStop();
        return;
    }

    setState(ServiceState::Stopped, "Wird beendet");
    m_process.terminate();
    if (!m_process.waitForFinished(3000)) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    endManualStop();
}

void ManagedService::restart()
{
    stop();
    start();
}

void ManagedService::startProcess(const QString& program, const QStringList& arguments, const QString& workingDirectory)
{
    if (m_process.state() != QProcess::NotRunning) {
        return;
    }

    m_stopping = false;

    const QFileInfo programInfo(program);
    if (!programInfo.exists() || !programInfo.isExecutable()) {
        const QString message = QString("%1 nicht gefunden oder nicht ausführbar: %2").arg(m_label, program);
        logs().append(m_id, message);
        setState(ServiceState::Error, message);
        Q_EMIT crashed(m_id, message);
        return;
    }

    setState(ServiceState::Starting, "Startet");

    if (!workingDirectory.isEmpty()) {
        QDir().mkpath(workingDirectory);
        m_process.setWorkingDirectory(workingDirectory);
    }

    m_process.setProgram(program);
    m_process.setArguments(arguments);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
}

void ManagedService::setState(ServiceState state, const QString& detail)
{
    m_state = state;
    m_detail = detail;
    Q_EMIT statusChanged(status());
}

void ManagedService::setRestartEnabled(bool enabled)
{
    m_restartEnabled = enabled;
}

void ManagedService::scheduleRestart()
{
    if (m_restartEnabled) {
        setState(ServiceState::Error, "Abgestürzt, Neustart wird vorbereitet");
        m_restartTimer.start(3000);
    }
}

void ManagedService::beginManualStop()
{
    m_stopping = true;
}

void ManagedService::endManualStop()
{
    m_stopping = false;
}

bool ManagedService::isManualStopRequested() const
{
    return m_stopping;
}

QProcess& ManagedService::process()
{
    return m_process;
}

ConfigManager& ManagedService::config()
{
    return m_config;
}

const ConfigManager& ManagedService::config() const
{
    return m_config;
}

LogManager& ManagedService::logs()
{
    return m_logs;
}

const LogManager& ManagedService::logs() const
{
    return m_logs;
}

void ManagedService::handleStdout(const QString& line)
{
    m_logs.append(m_id, line);
}

void ManagedService::handleStderr(const QString& line)
{
    m_logs.append(m_id, line);
}

bool ManagedService::shouldAutoRestart() const
{
    return m_restartEnabled;
}

void ManagedService::connectProcess()
{
    QObject::connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendOutput(QProcess::StandardOutput);
    });
    QObject::connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        appendOutput(QProcess::StandardError);
    });
    QObject::connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_UNUSED(error)
        if (m_stopping) {
            return;
        }
        const QString message = m_process.errorString();
        logs().append(m_id, message);
        setState(ServiceState::Error, message);
        Q_EMIT crashed(m_id, message);
        scheduleRestart();
    });
    QObject::connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool crashed = exitStatus == QProcess::CrashExit || (!m_stopping && exitCode != 0);
        if (crashed) {
            const QString message = QString("%1 beendet mit Code %2").arg(m_label).arg(exitCode);
            logs().append(m_id, message);
            Q_EMIT this->crashed(m_id, message);
            scheduleRestart();
            return;
        }
        setState(ServiceState::Stopped, "Gestoppt");
    });
    QObject::connect(&m_process, &QProcess::started, this, [this]() {
        setState(ServiceState::Running, "Läuft");
    });
}

void ManagedService::appendOutput(QProcess::ProcessChannel channel)
{
    const QByteArray data = channel == QProcess::StandardOutput
        ? m_process.readAllStandardOutput()
        : m_process.readAllStandardError();
    const QStringList lines = QString::fromLocal8Bit(data).split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (channel == QProcess::StandardOutput) {
            handleStdout(line.trimmed());
        } else {
            handleStderr(line.trimmed());
        }
    }
}
