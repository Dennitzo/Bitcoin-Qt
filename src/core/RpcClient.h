#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>

class ConfigManager;

class RpcClient final : public QObject {
    Q_OBJECT

public:
    explicit RpcClient(ConfigManager& config, QObject* parent = nullptr);

    void call(const QString& method, const QJsonArray& params = {});
    void getBlockchainInfo();
    void getNetworkInfo();
    void getBlockCount();
    void getPeerInfo();

Q_SIGNALS:
    void result(const QString& method, const QJsonValue& value);
    void failed(const QString& method, const QString& error);

private:
    QUrl rpcUrl() const;

    ConfigManager& m_config;
    QNetworkAccessManager m_network;
};
