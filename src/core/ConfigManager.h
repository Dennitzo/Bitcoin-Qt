#pragma once

#include "AppTypes.h"

#include <QObject>
#include <QSettings>
#include <QString>

class ConfigManager final : public QObject {
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);

    bool isFirstRun() const;
    QString baseDataDir() const;
    void setBaseDataDir(const QString& path);

    QString bitcoinDataDir() const;
    QString electrsDataDir() const;
    QString mempoolDataDir() const;

    QString bitcoinExecutable() const;
    QString electrsExecutable() const;
    QString nodeExecutable() const;
    void setExecutable(const QString& key, const QString& path);

    QString rpcUser() const;
    QString rpcPassword() const;
    BitcoinNetwork network() const;
    quint16 bitcoinRpcPort() const;
    quint16 electrsPort() const;
    quint16 mempoolBackendPort() const;
    quint16 mempoolFrontendPort() const;
    QString mempoolHost() const;

    QString theme() const;
    QString language() const;
    bool autostart() const;

Q_SIGNALS:
    void changed();

private:
    QString serviceDir(const QString& name) const;
    QString defaultBaseDir() const;

    QSettings m_settings;
};
