#include "ElectrsService.h"

#include <QDir>
#include <QFile>

ElectrsService::ElectrsService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("electrs", "Electrs", config, logs, parent),
      m_rpc(config, this)
{
    m_healthTimer.setInterval(3000);
    QObject::connect(&m_healthTimer, &QTimer::timeout, this, &ElectrsService::checkPort);

    m_readinessTimer.setInterval(5000);
    QObject::connect(&m_readinessTimer, &QTimer::timeout, this, &ElectrsService::checkBitcoinRpc);
    QObject::connect(&m_rpc, &RpcClient::result, this, [this](const QString& method, const QJsonValue&) {
        if (!m_startRequested || method != "getblockchaininfo") {
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
    setState(ServiceState::Starting, "Warte auf Bitcoin Core RPC");
    checkBitcoinRpc();
    m_readinessTimer.start();
}

void ElectrsService::stop()
{
    m_startRequested = false;
    m_readinessTimer.stop();
    m_healthTimer.stop();
    ManagedService::stop();
}

void ElectrsService::startElectrsProcess()
{
    if (!m_startRequested || process().state() != QProcess::NotRunning) {
        return;
    }
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
        m_healthTimer.stop();
        socket->disconnectFromHost();
        socket->deleteLater();
        setState(ServiceState::Synced, "Electrum-Port erreichbar");
        Q_EMIT ready(id());
    });
    QObject::connect(socket, &QTcpSocket::errorOccurred, socket, &QTcpSocket::deleteLater);
    socket->connectToHost("127.0.0.1", config().electrsPort());
}

void ElectrsService::handleStdout(const QString& line)
{
    if (line.contains("failed to shutdown TCP receiving Socket is not connected", Qt::CaseInsensitive)) {
        return;
    }
    ManagedService::handleStdout(line);
    if (line.contains("index", Qt::CaseInsensitive)) {
        setState(ServiceState::Indexing, "Indexiert Blockchain");
    }
    if (line.contains("serving", Qt::CaseInsensitive) || line.contains("listening", Qt::CaseInsensitive)) {
        setState(ServiceState::Synced, "Bereit");
        Q_EMIT ready(id());
    }
}

void ElectrsService::handleStderr(const QString& line)
{
    handleStdout(line);
}
