#include "ElectrsService.h"

#include <QDir>

ElectrsService::ElectrsService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("electrs", "Electrs", config, logs, parent)
{
    m_healthTimer.setInterval(3000);
    QObject::connect(&m_healthTimer, &QTimer::timeout, this, &ElectrsService::checkPort);
}

void ElectrsService::start()
{
    startProcess(config().electrsExecutable(), arguments(), config().electrsDataDir());
    m_healthTimer.start();
}

QStringList ElectrsService::arguments() const
{
    return {
        QString("--db-dir=%1").arg(config().electrsDataDir()),
        QString("--daemon-dir=%1").arg(config().bitcoinDataDir()),
        QString("--daemon-rpc-addr=127.0.0.1:%1").arg(config().bitcoinRpcPort()),
        QString("--electrum-rpc-addr=127.0.0.1:%1").arg(config().electrsPort()),
    };
}

void ElectrsService::checkPort()
{
    auto* socket = new QTcpSocket(this);
    QObject::connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        socket->deleteLater();
        setState(ServiceState::Synced, "Electrum-Port erreichbar");
        Q_EMIT ready(id());
    });
    QObject::connect(socket, &QTcpSocket::errorOccurred, socket, &QTcpSocket::deleteLater);
    socket->connectToHost("127.0.0.1", config().electrsPort());
}

void ElectrsService::handleStdout(const QString& line)
{
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
