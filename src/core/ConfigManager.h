#pragma once

#include "AppTypes.h"

#include <QObject>
#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QUrl>

class ConfigManager final : public QObject {
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);

    bool isFirstRun() const;
    QString baseDataDir() const;
    void setBaseDataDir(const QString& path);
    void setStringValue(const QString& key, const QString& value);
    void setUIntValue(const QString& key, quint16 value);
    void setBoolValue(const QString& key, bool value);
    bool serviceEnabled(const QString& id) const;
    void setServiceEnabled(const QString& id, bool enabled);

    QString bitcoinDataDir() const;
    QString electrsDataDir() const;
    QString mempoolDataDir() const;
    QString mempoolDatabaseDir() const;
    QString publicPoolDataDir() const;

    QString bitcoinExecutable() const;
    QString electrsExecutable() const;
    QString mariadbExecutable() const;
    QString mariadbInstallDbExecutable() const;
    QString nodeExecutable() const;
    void setExecutable(const QString& key, const QString& path);

    QString rpcUser() const;
    QString rpcPassword() const;
    BitcoinNetwork network() const;
    quint16 bitcoinRpcPort() const;
    quint16 electrsPort() const;
    quint16 mempoolDatabasePort() const;
    quint16 mempoolBackendPort() const;
    quint16 mempoolFrontendPort() const;
    QString mempoolHost() const;
    quint16 publicPoolApiPort() const;
    quint16 publicPoolStratumPort() const;
    quint16 publicPoolFrontendPort() const;
    QString publicPoolPayoutAddress() const;

    QString theme() const;
    QString language() const;
    bool autostart() const;
    QUrl webViewUrl(const QString& id) const;
    void setWebViewUrl(const QString& id, const QUrl& url);
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

Q_SIGNALS:
    void changed();

private:
    QString serviceDir(const QString& name) const;
    QString defaultBaseDir() const;

    QSettings m_settings;
};
