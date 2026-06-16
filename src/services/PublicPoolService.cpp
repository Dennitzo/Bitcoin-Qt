#include "PublicPoolService.h"

#include "../core/RuntimePaths.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>

#include <algorithm>

namespace {

double numberValue(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return value.toDouble();
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        return ok ? number : 0.0;
    }
    return 0.0;
}

double sumNumberValue(const QJsonArray& array, const QString& key)
{
    double total = 0.0;
    for (const QJsonValue& value : array) {
        total += numberValue(value.toObject(), key);
    }
    return total;
}

QDateTime parseIsoDate(const QString& value)
{
    QDateTime date = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!date.isValid()) {
        date = QDateTime::fromString(value, Qt::ISODate);
    }
    return date.toUTC();
}

}

PublicPoolService::PublicPoolService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("public-pool", "Public Pool", config, logs, parent)
{
    attachProcess(m_backend, "public-pool");
    attachProcess(m_frontend, "public-pool-ui");
    m_backendHealth.setInterval(2500);
    m_frontendHealth.setInterval(2500);
    m_statsTimer.setInterval(5000);
    QObject::connect(&m_backendHealth, &QTimer::timeout, this, &PublicPoolService::checkBackend);
    QObject::connect(&m_frontendHealth, &QTimer::timeout, this, &PublicPoolService::checkFrontend);
    QObject::connect(&m_statsTimer, &QTimer::timeout, this, &PublicPoolService::refreshStats);
}

PublicPoolService::~PublicPoolService()
{
    stop();
}

void PublicPoolService::start()
{
    m_startRequested = true;
    startBackend();
}

void PublicPoolService::stop()
{
    beginManualStop();
    m_startRequested = false;
    m_backendHealth.stop();
    m_frontendHealth.stop();
    m_statsTimer.stop();
    m_statsRequestInFlight = false;
    for (QProcess* proc : {&m_frontend, &m_backend}) {
        if (proc->state() == QProcess::NotRunning) {
            continue;
        }
        proc->terminate();
        if (!proc->waitForFinished(3000)) {
            proc->kill();
            proc->waitForFinished(1000);
        }
    }
    setState(ServiceState::Stopped, "Gestoppt");
    emitOfflineStats();
    endManualStop();
}

QUrl PublicPoolService::frontendUrl() const
{
    return QUrl(QString("http://127.0.0.1:%1").arg(config().publicPoolFrontendPort()));
}

void PublicPoolService::startBackend()
{
    if (!m_startRequested || m_backend.state() != QProcess::NotRunning) {
        return;
    }
    const QString node = config().nodeExecutable();
    const QString script = QDir(RuntimePaths::runtimeRoot()).filePath("public-pool/backend/dist/main.js");
    if (!RuntimePaths::isExecutableAvailable(node) || !QFileInfo::exists(script)) {
        setState(ServiceState::Error, QString("Public Pool Runtime fehlt: %1 / %2").arg(node, script));
        return;
    }

    setState(ServiceState::Starting, "Stratum/API startet");
    QDir dataDir(config().publicPoolDataDir());
    dataDir.mkpath(".");
    dataDir.mkpath("DB");
    m_backend.setWorkingDirectory(dataDir.absolutePath());
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
    env.insert("NODE_APP_INSTANCE", "0");
    env.insert("NETWORK", config().network() == BitcoinNetwork::Mainnet ? "mainnet" : "testnet");
    env.insert("POOL_IDENTIFIER", "Bitcoin-Qt Public Pool");
    if (!config().publicPoolPayoutAddress().isEmpty()) {
        env.insert("DEV_FEE_ADDRESS", config().publicPoolPayoutAddress());
    }
    m_backend.setProcessEnvironment(env);
    m_backend.start(node, {script});
    m_backendHealth.start();
}

void PublicPoolService::startFrontend()
{
    if (!m_startRequested || m_frontend.state() != QProcess::NotRunning) {
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
    if (!m_startRequested) {
        return;
    }
    QNetworkReply* reply = m_network.get(QNetworkRequest(QUrl(QString("http://127.0.0.1:%1/api/client/pool/stats").arg(config().publicPoolApiPort()))));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (!m_startRequested) {
            return;
        }
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
    if (!m_startRequested) {
        return;
    }
    QNetworkReply* reply = m_network.get(QNetworkRequest(frontendUrl()));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (!m_startRequested) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            setState(ServiceState::Starting, "Warte auf Public Pool UI");
            return;
        }
        m_frontendHealth.stop();
        setState(ServiceState::Running, "Stratum und UI erreichbar");
        Q_EMIT frontendAvailable(frontendUrl());
        Q_EMIT ready(id());
        refreshStats();
        m_statsTimer.start();
    });
}

void PublicPoolService::refreshStats()
{
    if (m_statsRequestInFlight) {
        return;
    }
    if (!m_startRequested || m_backend.state() == QProcess::NotRunning) {
        emitOfflineStats();
        return;
    }

    m_statsRequestInFlight = true;
    const auto apiUrl = [this](const QString& path) {
        return QUrl(QString("http://127.0.0.1:%1/api/%2").arg(config().publicPoolApiPort()).arg(path));
    };

    auto* poolReply = m_network.get(QNetworkRequest(apiUrl("pool")));
    QObject::connect(poolReply, &QNetworkReply::finished, this, [this, poolReply, apiUrl]() {
        poolReply->deleteLater();
        if (!m_startRequested || poolReply->error() != QNetworkReply::NoError) {
            m_statsRequestInFlight = false;
            emitOfflineStats();
            return;
        }

        PublicPoolStats stats;
        stats.online = true;
        const QJsonObject pool = QJsonDocument::fromJson(poolReply->readAll()).object();
        stats.minerHashrate = numberValue(pool, "totalHashRate");
        stats.minerCount = pool.value("totalMiners").toInt();

        auto* networkReply = m_network.get(QNetworkRequest(apiUrl("network")));
        QObject::connect(networkReply, &QNetworkReply::finished, this, [this, networkReply, apiUrl, stats]() mutable {
            networkReply->deleteLater();
            if (!m_startRequested) {
                m_statsRequestInFlight = false;
                emitOfflineStats();
                return;
            }
            if (networkReply->error() == QNetworkReply::NoError) {
                const QJsonObject network = QJsonDocument::fromJson(networkReply->readAll()).object();
                stats.networkHashrate = numberValue(network, "networkhashps");
                stats.networkDifficulty = numberValue(network, "difficulty");
            }

            auto* infoReply = m_network.get(QNetworkRequest(apiUrl("info")));
            QObject::connect(infoReply, &QNetworkReply::finished, this, [this, infoReply, stats]() mutable {
                infoReply->deleteLater();
                m_statsRequestInFlight = false;
                if (!m_startRequested) {
                    emitOfflineStats();
                    return;
                }
                if (infoReply->error() == QNetworkReply::NoError) {
                    const QJsonObject info = QJsonDocument::fromJson(infoReply->readAll()).object();
                    const QJsonArray highScores = info.value("highScores").toArray();
                    const QJsonArray userAgents = info.value("userAgents").toArray();
                    if (!userAgents.isEmpty()) {
                        stats.minerCount = std::max(stats.minerCount, static_cast<int>(sumNumberValue(userAgents, "count")));
                        stats.minerHashrate = std::max(stats.minerHashrate, sumNumberValue(userAgents, "totalHashRate"));
                    }
                    if (!highScores.isEmpty()) {
                        stats.bestShare = numberValue(highScores.first().toObject(), "bestDifficulty");
                    }
                    if (stats.bestShare <= 0.0) {
                        for (const QJsonValue& value : userAgents) {
                            stats.bestShare = std::max(stats.bestShare, numberValue(value.toObject(), "bestDifficulty"));
                        }
                    }
                    const QDateTime startedAt = parseIsoDate(info.value("uptime").toString());
                    if (stats.minerCount > 0 && startedAt.isValid()) {
                        stats.minerUptimeSeconds = std::max<qint64>(0, startedAt.secsTo(QDateTime::currentDateTimeUtc()));
                    }
                }
                if (stats.minerCount <= 0) {
                    stats.minerHashrate = 0.0;
                    stats.networkHashrate = 0.0;
                    stats.bestShare = 0.0;
                    stats.networkDifficulty = 0.0;
                    stats.minerUptimeSeconds = 0;
                }
                Q_EMIT statsChanged(stats);
            });
        });
    });
}

void PublicPoolService::emitOfflineStats()
{
    Q_EMIT statsChanged(PublicPoolStats{});
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
        if (!m_startRequested || isManualStopRequested()) {
            return;
        }
        setState(ServiceState::Error, "Public Pool Prozessfehler");
    });
    QObject::connect(&child, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, logId](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_startRequested || isManualStopRequested()) {
            return;
        }
        if (exitStatus == QProcess::CrashExit || exitCode != 0) {
            setState(ServiceState::Error, QString("%1 beendet mit Code %2").arg(logId).arg(exitCode));
        }
    });
}
