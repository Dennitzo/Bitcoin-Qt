#include "ConfigManager.h"

#include "RuntimePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

QString executableSetting(const QSettings& settings, const QString& key, const QString& fallback)
{
    const QString configured = settings.value(key).toString();
    if (configured.isEmpty()) {
        return fallback;
    }
    if (configured.contains('/') || configured.contains('\\')) {
        return configured;
    }
    return fallback;
}

}

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent),
      m_settings("Bitcoin-Qt", "Bitcoin-Qt")
{
    if (m_settings.value("storage/baseDataDir").toString().isEmpty()) {
        QSettings legacy("BitcoinNodeDesktop", "BitcoinNodeDesktop");
        const QString legacyBaseDir = legacy.value("storage/baseDataDir").toString();
        if (!legacyBaseDir.isEmpty()) {
            const QStringList keys = legacy.allKeys();
            for (const QString& key : keys) {
                m_settings.setValue(key, legacy.value(key));
            }
        }
    }
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
    dir.mkpath("mempool-db");
    dir.mkpath("mempool");
    dir.mkpath("public-pool");
    dir.mkpath("logs");

    m_settings.setValue("storage/baseDataDir", dir.absolutePath());
    Q_EMIT changed();
}

void ConfigManager::setStringValue(const QString& key, const QString& value)
{
    m_settings.setValue(key, value);
    Q_EMIT changed();
}

void ConfigManager::setUIntValue(const QString& key, quint16 value)
{
    m_settings.setValue(key, value);
    Q_EMIT changed();
}

void ConfigManager::setBoolValue(const QString& key, bool value)
{
    m_settings.setValue(key, value);
    Q_EMIT changed();
}

bool ConfigManager::serviceEnabled(const QString& id) const
{
    return m_settings.value(QString("services/%1/enabled").arg(id), id == "bitcoind").toBool();
}

void ConfigManager::setServiceEnabled(const QString& id, bool enabled)
{
    m_settings.setValue(QString("services/%1/enabled").arg(id), enabled);
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

QString ConfigManager::mempoolDatabaseDir() const
{
    return serviceDir("mempool-db");
}

QString ConfigManager::bitcoinExecutable() const
{
    return executableSetting(m_settings, "executables/bitcoind", RuntimePaths::executable("bitcoin/bin/bitcoind"));
}

QString ConfigManager::electrsExecutable() const
{
    return executableSetting(m_settings, "executables/electrs", RuntimePaths::executable("electrs/bin/electrs"));
}

QString ConfigManager::mariadbExecutable() const
{
    const QString configured = m_settings.value("executables/mariadbd").toString();
    if (!configured.isEmpty()) {
        return configured;
    }

    const QString mariadbd = RuntimePaths::executable("mariadb/bin/mariadbd");
    if (RuntimePaths::isExecutableAvailable(mariadbd)) {
        return mariadbd;
    }

    const QString mysqld = RuntimePaths::executable("mariadb/bin/mysqld");
    if (RuntimePaths::isExecutableAvailable(mysqld)) {
        return mysqld;
    }

    const QString libexecMariadbd = RuntimePaths::executable("mariadb/libexec/mariadbd");
    if (RuntimePaths::isExecutableAvailable(libexecMariadbd)) {
        return libexecMariadbd;
    }
    return RuntimePaths::executable("mariadb/libexec/mysqld");
}

QString ConfigManager::mariadbInstallDbExecutable() const
{
    const QString configured = m_settings.value("executables/mariadbInstallDb").toString();
    if (!configured.isEmpty()) {
        return configured;
    }

    const QString script = RuntimePaths::executable("mariadb/scripts/mariadb-install-db");
    if (RuntimePaths::isExecutableAvailable(script)) {
        return script;
    }

    const QString bin = RuntimePaths::executable("mariadb/bin/mariadb-install-db");
    if (RuntimePaths::isExecutableAvailable(bin)) {
        return bin;
    }
    return RuntimePaths::executable("mariadb/scripts/mysql_install_db");
}

QString ConfigManager::nodeExecutable() const
{
    return executableSetting(m_settings, "executables/node", RuntimePaths::executable("node/bin/node"));
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

quint16 ConfigManager::mempoolDatabasePort() const
{
    return static_cast<quint16>(m_settings.value("mempool/databasePort", 3306).toUInt());
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

quint16 ConfigManager::publicPoolApiPort() const
{
    return static_cast<quint16>(m_settings.value("publicPool/apiPort", 3334).toUInt());
}

quint16 ConfigManager::publicPoolStratumPort() const
{
    return static_cast<quint16>(m_settings.value("publicPool/stratumPort", 3333).toUInt());
}

quint16 ConfigManager::publicPoolFrontendPort() const
{
    return static_cast<quint16>(m_settings.value("publicPool/frontendPort", 3335).toUInt());
}

QString ConfigManager::publicPoolPayoutAddress() const
{
    return m_settings.value("publicPool/payoutAddress").toString();
}

QString ConfigManager::theme() const
{
    return m_settings.value("app/theme", "system").toString();
}

QString ConfigManager::language() const
{
    return m_settings.value("app/language", "en").toString();
}

bool ConfigManager::autostart() const
{
    return m_settings.value("app/autostart", true).toBool();
}

QUrl ConfigManager::webViewUrl(const QString& id) const
{
    return QUrl(m_settings.value(QString("webviews/%1/url").arg(id)).toString());
}

void ConfigManager::setWebViewUrl(const QString& id, const QUrl& url)
{
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        return;
    }
    m_settings.setValue(QString("webviews/%1/url").arg(id), url.toString(QUrl::FullyEncoded));
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
