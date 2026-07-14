#include "ElectrsService.h"

#include "../core/RuntimePaths.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace {
int indexedHeaderHeight(const QByteArray& response)
{
    const QList<QByteArray> lines = response.split('\n');
    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }
        const QJsonObject result = document.object().value("result").toObject();
        const int height = result.value("height").toInt();
        if (height > 0) {
            return height;
        }
    }
    return 0;
}
}

ElectrsService::ElectrsService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("electrs", RuntimePaths::versionedLabel("Electrs", "electrs"), config, logs, parent),
      m_rpc(config, this)
{
    m_healthTimer.setInterval(3000);
    QObject::connect(&m_healthTimer, &QTimer::timeout, this, &ElectrsService::checkPort);

    m_readinessTimer.setInterval(5000);
    QObject::connect(&m_readinessTimer, &QTimer::timeout, this, &ElectrsService::checkBitcoinRpc);
    m_compactionTimer.setInterval(15000);
    QObject::connect(&m_compactionTimer, &QTimer::timeout, this, &ElectrsService::updateCompactionStatus);
    QObject::connect(&m_rpc, &RpcClient::result, this, [this](const QString& method, const QJsonValue& value) {
        if (!m_startRequested || method != "getblockchaininfo") {
            return;
        }
        const QJsonObject info = value.toObject();
        if (info.value("initialblockdownload").toBool(true)) {
            setState(ServiceState::Starting, "Warte auf Bitcoin Core Sync");
            return;
        }
        m_readinessTimer.stop();
        startElectrsProcess();
    });
    QObject::connect(&m_rpc, &RpcClient::failed, this, [this](const QString& method, const QString&) {
        if (!m_startRequested || method != "getblockchaininfo") {
            return;
        }
        setState(ServiceState::Starting, "Warte auf Bitcoin Core RPC");
    });
}

void ElectrsService::start()
{
    if (process().state() != QProcess::NotRunning) {
        return;
    }

    m_startRequested = true;
    if (!writeAuthCookie()) {
        setState(ServiceState::Error, "Electrs RPC-Cookie konnte nicht geschrieben werden");
        return;
    }
    setState(ServiceState::Starting, "Warte auf Bitcoin Core Sync");
    checkBitcoinRpc();
    m_readinessTimer.start();
}

void ElectrsService::stop()
{
    m_startRequested = false;
    m_readinessTimer.stop();
    m_healthTimer.stop();
    m_compactionTimer.stop();
    m_compactionPhase.clear();
    m_rpc.abortPendingRequests();
    ManagedService::stop();
}

ElectrsSyncStatus ElectrsService::syncStatus() const
{
    return m_syncStatus;
}

void ElectrsService::setTargetHeaderHeight(int height)
{
    if (height <= 0 || m_syncStatus.targetHeaderHeight == height) {
        return;
    }
    m_syncStatus.targetHeaderHeight = height;
    Q_EMIT syncStatusChanged(m_syncStatus);
}

void ElectrsService::startElectrsProcess()
{
    if (!m_startRequested || process().state() != QProcess::NotRunning) {
        return;
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("RUST_LOG", "electrs=debug,bitcoin=info,rocksdb=info");
    process().setProcessEnvironment(env);
    startProcess(config().electrsExecutable(), arguments(), config().electrsDataDir());
    m_healthTimer.start();
}

QStringList ElectrsService::arguments() const
{
    return {
        QString("--db-dir=%1").arg(config().electrsDataDir()),
        QString("--cookie-file=%1").arg(authCookieFile()),
        QString("--daemon-rpc-addr=127.0.0.1:%1").arg(config().bitcoinRpcPort()),
        QString("--electrum-rpc-addr=127.0.0.1:%1").arg(config().electrsPort()),
        "--wait-duration-secs=5",
        "--jsonrpc-timeout-secs=60",
    };
}

QString ElectrsService::authCookieFile() const
{
    return QDir(config().electrsDataDir()).filePath("bitcoin-qt-rpc.cookie");
}

bool ElectrsService::writeAuthCookie() const
{
    QDir().mkpath(config().electrsDataDir());
    QFile file(authCookieFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(QString("%1:%2").arg(config().rpcUser(), config().rpcPassword()).toUtf8());
    return true;
}

void ElectrsService::checkBitcoinRpc()
{
    if (!m_startRequested || process().state() != QProcess::NotRunning) {
        return;
    }
    m_rpc.getBlockchainInfo();
}

void ElectrsService::checkPort()
{
    auto* socket = new QTcpSocket(this);
    QObject::connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        socket->write("{\"id\":1,\"method\":\"server.version\",\"params\":[\"bitcoin-qt\",\"1.4\"]}\n");
        socket->write("{\"id\":2,\"method\":\"blockchain.headers.subscribe\",\"params\":[]}\n");
    });
    QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        const QByteArray response = socket->readAll();
        const int height = indexedHeaderHeight(response);
        if (height <= 0) {
            socket->disconnectFromHost();
            socket->deleteLater();
            return;
        }
        m_syncStatus.indexedHeaderHeight = height;
        m_syncStatus.available = true;
        Q_EMIT syncStatusChanged(m_syncStatus);
        socket->disconnectFromHost();
        socket->deleteLater();
        if (m_syncStatus.targetHeaderHeight > 0 && height < m_syncStatus.targetHeaderHeight) {
            setState(ServiceState::Indexing, "Indexiert Blockchain");
            return;
        }
        if (state() != ServiceState::Synced) {
            setState(ServiceState::Synced, "Electrum erreichbar");
            Q_EMIT ready(id());
        }
    });
    QObject::connect(socket, &QTcpSocket::errorOccurred, socket, &QTcpSocket::deleteLater);
    socket->connectToHost("127.0.0.1", config().electrsPort());
}

void ElectrsService::updateCompactionStatus()
{
    if (m_compactionPhase.isEmpty() || !m_compactionElapsed.isValid()) {
        return;
    }
    m_syncStatus.phase = m_compactionPhase;
    m_syncStatus.phaseElapsedSeconds = m_compactionElapsed.elapsed() / 1000;
    Q_EMIT syncStatusChanged(m_syncStatus);
    setState(ServiceState::Indexing, m_compactionPhase);
}

void ElectrsService::handleStdout(const QString& line)
{
    if (line.contains("\"method\":\"server.version\"") || line.contains("\"method\":\"blockchain.headers.subscribe\"")) {
        return;
    }
    if (line.contains("disconnected", Qt::CaseInsensitive)
        || line.contains("failed to shutdown TCP receiving Socket is not connected", Qt::CaseInsensitive)) {
        return;
    }
    ManagedService::handleStdout(line);
    if (line.contains("starting", Qt::CaseInsensitive) && line.contains("compaction", Qt::CaseInsensitive)) {
        m_compactionPhase = "Compaction";
        m_compactionElapsed.restart();
        m_compactionTimer.start();
        updateCompactionStatus();
    } else if (line.contains("finished", Qt::CaseInsensitive) && line.contains("compaction", Qt::CaseInsensitive)) {
        m_compactionTimer.stop();
        m_compactionPhase.clear();
        m_syncStatus.phase.clear();
        m_syncStatus.phaseElapsedSeconds = 0;
        Q_EMIT syncStatusChanged(m_syncStatus);
    }
    if (line.contains("index", Qt::CaseInsensitive)) {
        setState(ServiceState::Indexing, "Indexiert Blockchain");
    }
    if (line.contains("serving", Qt::CaseInsensitive) || line.contains("listening", Qt::CaseInsensitive)) {
        setState(ServiceState::Synced, "Electrum erreichbar");
        Q_EMIT ready(id());
    }
}

void ElectrsService::handleStderr(const QString& line)
{
    handleStdout(line);
}
