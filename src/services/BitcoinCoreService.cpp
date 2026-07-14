#include "BitcoinCoreService.h"

#include "../core/RuntimePaths.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>

BitcoinCoreService::BitcoinCoreService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("bitcoind", RuntimePaths::versionedLabel("Bitcoin Core", "bitcoin"), config, logs, parent),
      m_rpc(config, this)
{
    QObject::connect(&m_pollTimer, &QTimer::timeout, this, &BitcoinCoreService::pollRpc);
    m_pollTimer.setInterval(2500);
    QObject::connect(&m_rpc, &RpcClient::result, this, &BitcoinCoreService::applyRpcResult);
    QObject::connect(&m_rpc, &RpcClient::failed, this, [this](const QString&, const QString&) {
        ++m_rpcFailureCount;
        if (m_rpcFailureCount < 3) {
            return;
        }

        m_status.rpcAvailable = false;
        Q_EMIT nodeStatusChanged(m_status);
        if (state() == ServiceState::Running) {
            setState(ServiceState::Starting, "RPC nicht verfügbar");
        }
    });
}

void BitcoinCoreService::start()
{
    startProcess(config().bitcoinExecutable(), arguments(), config().bitcoinDataDir());
    m_pollTimer.start();
}

void BitcoinCoreService::stop()
{
    m_pollTimer.stop();
    m_rpc.abortPendingRequests();
    m_rpcFailureCount = 0;
    m_status.rpcAvailable = false;
    m_status.peers = 0;
    Q_EMIT nodeStatusChanged(m_status);

    beginManualStop();
    if (process().state() == QProcess::NotRunning) {
        setState(ServiceState::Stopped, "Gestoppt");
        endManualStop();
        return;
    }

    setState(ServiceState::Stopped, "Wird beendet");

    const QFileInfo bitcoind(config().bitcoinExecutable());
    const QString suffix = bitcoind.fileName().endsWith(".exe", Qt::CaseInsensitive) ? ".exe" : "";
    const QString bitcoinCli = bitcoind.dir().filePath(QString("bitcoin-cli%1").arg(suffix));
    if (QFileInfo::exists(bitcoinCli)) {
        QProcess cli;
        cli.setProgram(bitcoinCli);
        cli.setArguments({
            QString("-rpcuser=%1").arg(config().rpcUser()),
            QString("-rpcpassword=%1").arg(config().rpcPassword()),
            QString("-rpcport=%1").arg(config().bitcoinRpcPort()),
            "stop",
        });
        cli.start();
        if (!cli.waitForFinished(5000) || cli.exitStatus() != QProcess::NormalExit || cli.exitCode() != 0) {
            logs().append(id(), QString("bitcoin-cli stop fehlgeschlagen: %1").arg(QString::fromLocal8Bit(cli.readAllStandardError()).trimmed()));
        }
    } else {
        logs().append(id(), QString("bitcoin-cli nicht gefunden: %1").arg(bitcoinCli));
    }

    if (!process().waitForFinished(120000)) {
        logs().append(id(), "Bitcoin Core reagiert nicht auf RPC stop, sende Terminate");
        process().terminate();
        if (!process().waitForFinished(30000)) {
            logs().append(id(), "Bitcoin Core reagiert nicht auf Terminate, erzwinge Stop");
            process().kill();
            process().waitForFinished(5000);
        }
    }

    setState(ServiceState::Stopped, "Gestoppt");
    endManualStop();
}

BitcoinNodeStatus BitcoinCoreService::nodeStatus() const
{
    return m_status;
}

QStringList BitcoinCoreService::arguments() const
{
    QStringList args{
        QString("-datadir=%1").arg(config().bitcoinDataDir()),
        "-server=1",
        "-rest=1",
        "-txindex=1",
        "-rpcbind=127.0.0.1",
        "-rpcallowip=127.0.0.1",
        "-rpcthreads=8",
        "-rpcworkqueue=128",
        "-whitelist=download,noban,mempool,relay@127.0.0.1",
        QString("-rpcuser=%1").arg(config().rpcUser()),
        QString("-rpcpassword=%1").arg(config().rpcPassword()),
        QString("-rpcport=%1").arg(config().bitcoinRpcPort()),
    };

    switch (config().network()) {
    case BitcoinNetwork::Testnet:
        args << "-testnet=1";
        break;
    case BitcoinNetwork::Signet:
        args << "-signet=1";
        break;
    case BitcoinNetwork::Regtest:
        args << "-regtest=1";
        break;
    case BitcoinNetwork::Mainnet:
        break;
    }
    return args;
}

void BitcoinCoreService::pollRpc()
{
    if (process().state() == QProcess::NotRunning) {
        return;
    }
    m_rpc.getBlockchainInfo();
    m_rpc.getNetworkInfo();
}

void BitcoinCoreService::applyRpcResult(const QString& method, const QJsonValue& value)
{
    const bool wasUnavailable = !m_status.rpcAvailable;
    m_rpcFailureCount = 0;
    m_status.rpcAvailable = true;

    if (method == "getblockchaininfo") {
        const QJsonObject object = value.toObject();
        m_status.blockHeight = object.value("blocks").toInt();
        m_status.headerHeight = object.value("headers").toInt(m_status.blockHeight);
        m_status.verificationProgress = object.value("verificationprogress").toDouble();
        m_status.network = object.value("chain").toString("main");
        m_status.initialBlockDownload = object.value("initialblockdownload").toBool(true);
    } else if (method == "getnetworkinfo") {
        m_status.peers = value.toObject().value("connections").toInt();
    }

    setState(ServiceState::Running, "RPC erreichbar");
    Q_EMIT nodeStatusChanged(m_status);
    if (wasUnavailable) {
        Q_EMIT rpcAvailable();
        Q_EMIT ready(id());
    }
}
