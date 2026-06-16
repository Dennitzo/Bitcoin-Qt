#include "MempoolService.h"

#include "../core/RuntimePaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>

MempoolService::MempoolService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("mempool", "Mempool", config, logs, parent)
{
    attachProcess(m_backend, "mempool-backend");
    attachProcess(m_frontend, "mempool-frontend");
    m_databaseHealth.setInterval(1500);
    m_electrsHealth.setInterval(2500);
    m_backendHealth.setInterval(2500);
    m_frontendHealth.setInterval(2500);
    QObject::connect(&m_databaseHealth, &QTimer::timeout, this, &MempoolService::waitForDatabase);
    QObject::connect(&m_electrsHealth, &QTimer::timeout, this, &MempoolService::waitForElectrs);
    QObject::connect(&m_backendHealth, &QTimer::timeout, this, &MempoolService::checkBackend);
    QObject::connect(&m_frontendHealth, &QTimer::timeout, this, &MempoolService::checkFrontend);
}

MempoolService::~MempoolService()
{
    stop();
}

void MempoolService::start()
{
    m_startRequested = true;
    waitForDatabase();
    m_databaseHealth.start();
}

void MempoolService::stop()
{
    beginManualStop();
    m_startRequested = false;
    m_databaseHealth.stop();
    m_electrsHealth.stop();
    m_backendHealth.stop();
    m_frontendHealth.stop();
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
    endManualStop();
}

QUrl MempoolService::frontendUrl() const
{
    return QUrl(QString("http://%1:%2").arg(config().mempoolHost()).arg(config().mempoolFrontendPort()));
}

void MempoolService::waitForDatabase()
{
    if (!m_startRequested || m_backend.state() != QProcess::NotRunning) {
        return;
    }

    setState(ServiceState::Starting, "Warte auf Mempool DB");
    auto* socket = new QTcpSocket(this);
    QObject::connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        if (!databaseDirectoryInitialized()) {
            socket->disconnectFromHost();
            socket->deleteLater();
            m_databaseHealth.stop();
            setState(ServiceState::Error, "Mempool DB Port belegt, aber Datenbank ist nicht initialisiert");
            return;
        }
        m_databaseHealth.stop();
        socket->disconnectFromHost();
        socket->deleteLater();
        waitForElectrs();
        m_electrsHealth.start();
    });
    QObject::connect(socket, &QTcpSocket::errorOccurred, socket, &QTcpSocket::deleteLater);
    socket->connectToHost("127.0.0.1", config().mempoolDatabasePort());
}

void MempoolService::waitForElectrs()
{
    if (!m_startRequested || m_backend.state() != QProcess::NotRunning) {
        return;
    }

    setState(ServiceState::Starting, "Warte auf Electrs");
    auto* socket = new QTcpSocket(this);
    QObject::connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        m_electrsHealth.stop();
        socket->disconnectFromHost();
        socket->deleteLater();
        startBackend();
    });
    QObject::connect(socket, &QTcpSocket::errorOccurred, socket, &QTcpSocket::deleteLater);
    socket->connectToHost("127.0.0.1", config().electrsPort());
}

void MempoolService::startBackend()
{
    if (!m_startRequested || m_backend.state() != QProcess::NotRunning) {
        return;
    }
    const QString node = config().nodeExecutable();
    const QString script = QDir(RuntimePaths::runtimeRoot()).filePath("mempool/backend/server.js");
    if (!RuntimePaths::isExecutableAvailable(node) || !QFileInfo::exists(script)) {
        setState(ServiceState::Error, QString("Mempool Backend Runtime fehlt: %1 / %2").arg(node, script));
        return;
    }
    if (!writeBackendConfig()) {
        setState(ServiceState::Error, "Mempool Konfiguration konnte nicht geschrieben werden");
        return;
    }
    QFile::remove(QDir(config().mempoolDatabaseDir()).filePath("mempool-mempool.pid"));

    setState(ServiceState::Starting, "Backend startet");
    m_backend.setWorkingDirectory(QDir(RuntimePaths::runtimeRoot()).filePath("mempool"));
    QProcessEnvironment backendEnv = QProcessEnvironment::systemEnvironment();
    backendEnv.insert("MEMPOOL_CONFIG_FILE", configFilePath());
    m_backend.setProcessEnvironment(backendEnv);
    m_backend.start(node, {"backend/server.js"});
    m_backendHealth.start();
}

void MempoolService::startFrontend()
{
    if (!m_startRequested || m_frontend.state() != QProcess::NotRunning) {
        return;
    }
    const QString node = config().nodeExecutable();
    const QString script = QDir(RuntimePaths::runtimeRoot()).filePath("mempool/frontend/server.js");
    if (!RuntimePaths::isExecutableAvailable(node) || !QFileInfo::exists(script)) {
        setState(ServiceState::Error, QString("Mempool Frontend Runtime fehlt: %1 / %2").arg(node, script));
        return;
    }

    setState(ServiceState::Starting, "Frontend startet");
    m_frontend.setWorkingDirectory(QDir(RuntimePaths::runtimeRoot()).filePath("mempool"));
    QProcessEnvironment frontendEnv = QProcessEnvironment::systemEnvironment();
    frontendEnv.insert("MEMPOOL_FRONTEND_PORT", QString::number(config().mempoolFrontendPort()));
    frontendEnv.insert("MEMPOOL_BACKEND_PORT", QString::number(config().mempoolBackendPort()));
    frontendEnv.insert("MEMPOOL_BITCOIN_RPC_PORT", QString::number(config().bitcoinRpcPort()));
    frontendEnv.insert("MEMPOOL_BITCOIN_RPC_USER", config().rpcUser());
    frontendEnv.insert("MEMPOOL_BITCOIN_RPC_PASSWORD", config().rpcPassword());
    frontendEnv.insert("PORT", QString::number(config().mempoolFrontendPort()));
    m_frontend.setProcessEnvironment(frontendEnv);
    m_frontend.start(node, {"frontend/server.js"});
    m_frontendHealth.start();
}

void MempoolService::checkBackend()
{
    if (!m_startRequested) {
        return;
    }
    QNetworkReply* reply = m_network.get(QNetworkRequest(QUrl(QString("http://127.0.0.1:%1/api/v1/backend-info").arg(config().mempoolBackendPort()))));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (!m_startRequested) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            setState(ServiceState::Starting, "Warte auf Mempool Backend");
            return;
        }
        m_backendHealth.stop();
        startFrontend();
    });
}

void MempoolService::checkFrontend()
{
    if (!m_startRequested) {
        return;
    }
    QNetworkReply* reply = m_network.get(QNetworkRequest(frontendUrl().resolved(QUrl("/api/v1/init-data"))));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (!m_startRequested) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            setState(ServiceState::Starting, "Warte auf Mempool Frontend");
            return;
        }
        const QJsonObject initData = QJsonDocument::fromJson(body).object();
        const QJsonObject mempoolInfo = initData.value("mempoolInfo").toObject();
        if (!mempoolInfo.value("loaded").toBool(false)) {
            setState(ServiceState::Starting, "Warte auf Mempool Daten");
            return;
        }
        m_frontendHealth.stop();
        setState(ServiceState::Running, "Frontend erreichbar");
        Q_EMIT frontendAvailable(frontendUrl());
        Q_EMIT ready(id());
    });
}

void MempoolService::attachProcess(QProcess& child, const QString& logId)
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
        setState(ServiceState::Error, "Mempool Prozessfehler");
    });
    QObject::connect(&child, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, logId](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_startRequested || isManualStopRequested()) {
            return;
        }

        if (exitStatus == QProcess::CrashExit || exitCode != 0) {
            setState(ServiceState::Error, QString("%1 beendet mit Code %2").arg(logId).arg(exitCode));
        } else {
            setState(ServiceState::Starting, QString("%1 wurde beendet").arg(logId));
        }

        QTimer::singleShot(1500, this, [this, logId]() {
            if (!m_startRequested) {
                return;
            }

            if (logId == "mempool-frontend") {
                startFrontend();
                return;
            }

            if (logId == "mempool-backend") {
                if (m_frontend.state() != QProcess::NotRunning) {
                    m_frontend.terminate();
                    if (!m_frontend.waitForFinished(3000)) {
                        m_frontend.kill();
                    }
                }
                startBackend();
            }
        });
    });
}

QString MempoolService::configFilePath() const
{
    return QDir(config().mempoolDataDir()).filePath("mempool-config.json");
}

bool MempoolService::databaseDirectoryInitialized() const
{
    const QDir dir(config().mempoolDatabaseDir());
    return dir.exists("mysql") && QFileInfo::exists(dir.filePath("ibdata1"));
}

bool MempoolService::writeBackendConfig() const
{
    QDir().mkpath(config().mempoolDataDir());
    QDir().mkpath(QDir(config().mempoolDataDir()).filePath("cache"));

    const QString network = config().network() == BitcoinNetwork::Mainnet ? "mainnet"
        : config().network() == BitcoinNetwork::Signet ? "signet"
        : config().network() == BitcoinNetwork::Regtest ? "regtest"
        : "testnet";

    QJsonObject root;
    root.insert("MEMPOOL", QJsonObject{
        {"ENABLED", true},
        {"OFFICIAL", false},
        {"NETWORK", network},
        {"BACKEND", "electrum"},
        {"HTTP_PORT", static_cast<int>(config().mempoolBackendPort())},
        {"CACHE_DIR", QDir(config().mempoolDataDir()).filePath("cache")},
        {"CLEAR_PROTECTION_MINUTES", 20},
        {"INDEXING_BLOCKS_AMOUNT", 52560},
        {"POLL_RATE_MS", 2000},
        {"STDOUT_LOG_MIN_PRIORITY", "info"},
        {"EXTERNAL_ASSETS", QJsonArray{}},
        {"AUTOMATIC_POOLS_UPDATE", false},
    });
    root.insert("CORE_RPC", QJsonObject{
        {"HOST", "127.0.0.1"},
        {"PORT", static_cast<int>(config().bitcoinRpcPort())},
        {"USERNAME", config().rpcUser()},
        {"PASSWORD", config().rpcPassword()},
        {"TIMEOUT", 60000},
        {"COOKIE", false},
        {"COOKIE_PATH", ""},
        {"DEBUG_LOG_PATH", ""},
    });
    root.insert("SECOND_CORE_RPC", QJsonObject{
        {"HOST", "127.0.0.1"},
        {"PORT", static_cast<int>(config().bitcoinRpcPort())},
        {"USERNAME", config().rpcUser()},
        {"PASSWORD", config().rpcPassword()},
        {"TIMEOUT", 60000},
        {"COOKIE", false},
        {"COOKIE_PATH", ""},
    });
    root.insert("ELECTRUM", QJsonObject{
        {"HOST", "127.0.0.1"},
        {"PORT", static_cast<int>(config().electrsPort())},
        {"TLS_ENABLED", false},
    });
    root.insert("DATABASE", QJsonObject{
        {"ENABLED", true},
        {"HOST", "127.0.0.1"},
        {"SOCKET", ""},
        {"PORT", static_cast<int>(config().mempoolDatabasePort())},
        {"DATABASE", "mempool"},
        {"USERNAME", "mempool"},
        {"PASSWORD", "mempool"},
        {"TIMEOUT", 180000},
        {"PID_DIR", config().mempoolDatabaseDir()},
        {"POOL_SIZE", 50},
    });
    root.insert("SYSLOG", QJsonObject{{"ENABLED", false}});
    root.insert("STATISTICS", QJsonObject{{"ENABLED", false}});
    root.insert("LIGHTNING", QJsonObject{{"ENABLED", false}});
    root.insert("FIAT_PRICE", QJsonObject{{"ENABLED", false}});
    root.insert("MAXMIND", QJsonObject{{"ENABLED", false}});
    root.insert("SOCKS5PROXY", QJsonObject{{"ENABLED", false}});
    root.insert("REDIS", QJsonObject{{"ENABLED", false}});
    root.insert("WALLETS", QJsonObject{{"ENABLED", false}});

    QFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
