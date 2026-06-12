#include "PublicPoolService.h"

#include "../core/RuntimePaths.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>

PublicPoolService::PublicPoolService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("public-pool", "Public Pool", config, logs, parent)
{
    attachProcess(m_backend, "public-pool");
    attachProcess(m_frontend, "public-pool-ui");
    m_backendHealth.setInterval(2500);
    m_frontendHealth.setInterval(2500);
    QObject::connect(&m_backendHealth, &QTimer::timeout, this, &PublicPoolService::checkBackend);
    QObject::connect(&m_frontendHealth, &QTimer::timeout, this, &PublicPoolService::checkFrontend);
}

PublicPoolService::~PublicPoolService()
{
    stop();
}

void PublicPoolService::start()
{
    startBackend();
}

void PublicPoolService::stop()
{
    m_backendHealth.stop();
    m_frontendHealth.stop();
    for (QProcess* proc : {&m_frontend, &m_backend}) {
        if (proc->state() == QProcess::NotRunning) {
            continue;
        }
        proc->terminate();
        if (!proc->waitForFinished(3000)) {
            proc->kill();
        }
    }
    setState(ServiceState::Stopped, "Gestoppt");
}

QUrl PublicPoolService::frontendUrl() const
{
    return QUrl(QString("http://127.0.0.1:%1").arg(config().publicPoolFrontendPort()));
}

void PublicPoolService::startBackend()
{
    if (m_backend.state() != QProcess::NotRunning) {
        return;
    }
    const QString node = config().nodeExecutable();
    const QString script = QDir(RuntimePaths::runtimeRoot()).filePath("public-pool/backend/dist/main.js");
    if (!RuntimePaths::isExecutableAvailable(node) || !QFileInfo::exists(script)) {
        setState(ServiceState::Error, QString("Public Pool Runtime fehlt: %1 / %2").arg(node, script));
        return;
    }

    setState(ServiceState::Starting, "Stratum/API startet");
    m_backend.setWorkingDirectory(QDir(RuntimePaths::runtimeRoot()).filePath("public-pool/backend"));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("BITCOIN_RPC_URL", "http://127.0.0.1");
    env.insert("BITCOIN_RPC_USER", config().rpcUser());
    env.insert("BITCOIN_RPC_PASSWORD", config().rpcPassword());
    env.insert("BITCOIN_RPC_PORT", QString::number(config().bitcoinRpcPort()));
    env.insert("BITCOIN_RPC_TIMEOUT", "10000");
    env.insert("BITCOIN_RPC_COOKIEFILE", "");
    env.insert("API_PORT", QString::number(config().publicPoolApiPort()));
    env.insert("STRATUM_PORT", QString::number(config().publicPoolStratumPort()));
    env.insert("STRATUM_MAX_CONNECTIONS_PER_LISTENER", "10000");
    env.insert("API_SECURE", "false");
    env.insert("NETWORK", config().network() == BitcoinNetwork::Mainnet ? "mainnet" : "testnet");
    env.insert("POOL_IDENTIFIER", "Bitcoin-Qt Public Pool");
    if (!config().publicPoolPayoutAddress().isEmpty()) {
        env.insert("DEV_FEE_ADDRESS", config().publicPoolPayoutAddress());
    }
    m_backend.setProcessEnvironment(env);
    m_backend.start(node, {"dist/main.js"});
    m_backendHealth.start();
}

void PublicPoolService::startFrontend()
{
    if (m_frontend.state() != QProcess::NotRunning) {
        return;
    }
    const QString node = config().nodeExecutable();
    const QString script = QDir(RuntimePaths::runtimeRoot()).filePath("public-pool/frontend/server.js");
    if (!RuntimePaths::isExecutableAvailable(node) || !QFileInfo::exists(script)) {
        setState(ServiceState::Error, QString("Public Pool UI fehlt: %1 / %2").arg(node, script));
        return;
    }

    setState(ServiceState::Starting, "Web UI startet");
    m_frontend.setWorkingDirectory(QDir(RuntimePaths::runtimeRoot()).filePath("public-pool/frontend"));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PUBLIC_POOL_FRONTEND_PORT", QString::number(config().publicPoolFrontendPort()));
    env.insert("PORT", QString::number(config().publicPoolFrontendPort()));
    env.insert("PUBLIC_POOL_API_PORT", QString::number(config().publicPoolApiPort()));
    m_frontend.setProcessEnvironment(env);
    m_frontend.start(node, {"server.js"});
    m_frontendHealth.start();
}

void PublicPoolService::checkBackend()
{
    QNetworkReply* reply = m_network.get(QNetworkRequest(QUrl(QString("http://127.0.0.1:%1/api/client/pool/stats").arg(config().publicPoolApiPort()))));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setState(ServiceState::Starting, "Warte auf Public Pool API");
            return;
        }
        m_backendHealth.stop();
        startFrontend();
    });
}

void PublicPoolService::checkFrontend()
{
    QNetworkReply* reply = m_network.get(QNetworkRequest(frontendUrl()));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setState(ServiceState::Starting, "Warte auf Public Pool UI");
            return;
        }
        m_frontendHealth.stop();
        setState(ServiceState::Running, "Stratum und UI erreichbar");
        Q_EMIT frontendAvailable(frontendUrl());
        Q_EMIT ready(id());
    });
}

void PublicPoolService::attachProcess(QProcess& child, const QString& logId)
{
    child.setProcessChannelMode(QProcess::SeparateChannels);
    QObject::connect(&child, &QProcess::readyReadStandardOutput, this, [&child, this, logId]() {
        const QStringList lines = QString::fromLocal8Bit(child.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            logs().append(logId, line.trimmed());
        }
    });
    QObject::connect(&child, &QProcess::readyReadStandardError, this, [&child, this, logId]() {
        const QStringList lines = QString::fromLocal8Bit(child.readAllStandardError()).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            logs().append(logId, line.trimmed());
        }
    });
    QObject::connect(&child, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        setState(ServiceState::Error, "Public Pool Prozessfehler");
    });
    QObject::connect(&child, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, logId](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::CrashExit || exitCode != 0) {
            setState(ServiceState::Error, QString("%1 beendet mit Code %2").arg(logId).arg(exitCode));
        }
    });
}
