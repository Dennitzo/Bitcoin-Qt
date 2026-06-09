#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent),
      m_settings("BitcoinNodeDesktop", "BitcoinNodeDesktop")
{
}

bool ConfigManager::isFirstRun() const
{
    return baseDataDir().isEmpty();
}

QString ConfigManager::baseDataDir() const
{
    return m_settings.value("storage/baseDataDir").toString();
}

void ConfigManager::setBaseDataDir(const QString& path)
{
    QDir dir(path);
    dir.mkpath(".");
    dir.mkpath("bitcoin");
    dir.mkpath("electrs");
    dir.mkpath("mempool");
    dir.mkpath("logs");

    m_settings.setValue("storage/baseDataDir", dir.absolutePath());
    Q_EMIT changed();
}

QString ConfigManager::bitcoinDataDir() const
{
    return serviceDir("bitcoin");
}

QString ConfigManager::electrsDataDir() const
{
    return serviceDir("electrs");
}

QString ConfigManager::mempoolDataDir() const
{
    return serviceDir("mempool");
}

QString ConfigManager::bitcoinExecutable() const
{
    return m_settings.value("executables/bitcoind", "bitcoind").toString();
}

QString ConfigManager::electrsExecutable() const
{
    return m_settings.value("executables/electrs", "electrs").toString();
}

QString ConfigManager::nodeExecutable() const
{
    return m_settings.value("executables/node", "node").toString();
}

void ConfigManager::setExecutable(const QString& key, const QString& path)
{
    m_settings.setValue(QString("executables/%1").arg(key), path);
    Q_EMIT changed();
}

QString ConfigManager::rpcUser() const
{
    return m_settings.value("bitcoin/rpcUser", "bitcoin").toString();
}

QString ConfigManager::rpcPassword() const
{
    return m_settings.value("bitcoin/rpcPassword", "bitcoin").toString();
}

BitcoinNetwork ConfigManager::network() const
{
    const QString value = m_settings.value("bitcoin/network", "mainnet").toString();
    if (value == "testnet") {
        return BitcoinNetwork::Testnet;
    }
    if (value == "signet") {
        return BitcoinNetwork::Signet;
    }
    if (value == "regtest") {
        return BitcoinNetwork::Regtest;
    }
    return BitcoinNetwork::Mainnet;
}

quint16 ConfigManager::bitcoinRpcPort() const
{
    return static_cast<quint16>(m_settings.value("bitcoin/rpcPort", 8332).toUInt());
}

quint16 ConfigManager::electrsPort() const
{
    return static_cast<quint16>(m_settings.value("electrs/port", 50001).toUInt());
}

quint16 ConfigManager::mempoolBackendPort() const
{
    return static_cast<quint16>(m_settings.value("mempool/backendPort", 8999).toUInt());
}

quint16 ConfigManager::mempoolFrontendPort() const
{
    return static_cast<quint16>(m_settings.value("mempool/frontendPort", 8080).toUInt());
}

QString ConfigManager::mempoolHost() const
{
    return m_settings.value("mempool/host", "127.0.0.1").toString();
}

QString ConfigManager::theme() const
{
    return m_settings.value("app/theme", "system").toString();
}

QString ConfigManager::language() const
{
    return m_settings.value("app/language", "de").toString();
}

bool ConfigManager::autostart() const
{
    return m_settings.value("app/autostart", true).toBool();
}

QString ConfigManager::serviceDir(const QString& name) const
{
    const QString base = baseDataDir().isEmpty() ? defaultBaseDir() : baseDataDir();
    return QDir(base).filePath(name);
}

QString ConfigManager::defaultBaseDir() const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(root).filePath("node-data");
}
