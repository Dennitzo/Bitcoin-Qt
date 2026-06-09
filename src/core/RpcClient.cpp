#include "RpcClient.h"

#include "ConfigManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

RpcClient::RpcClient(ConfigManager& config, QObject* parent)
    : QObject(parent),
      m_config(config)
{
}

void RpcClient::call(const QString& method, const QJsonArray& params)
{
    QNetworkRequest request(rpcUrl());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QString credentials = QString("%1:%2").arg(m_config.rpcUser(), m_config.rpcPassword());
    request.setRawHeader("Authorization", "Basic " + credentials.toUtf8().toBase64());

    const QJsonObject payload{
        {"jsonrpc", "1.0"},
        {"id", "qt-node"},
        {"method", method},
        {"params", params},
    };
    QNetworkReply* reply = m_network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, method]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT failed(method, reply->errorString());
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject object = document.object();
        if (!object.value("error").isNull()) {
            Q_EMIT failed(method, QString::fromUtf8(QJsonDocument(object.value("error").toObject()).toJson(QJsonDocument::Compact)));
            return;
        }
        Q_EMIT result(method, object.value("result"));
    });
}

void RpcClient::getBlockchainInfo()
{
    call("getblockchaininfo");
}

void RpcClient::getNetworkInfo()
{
    call("getnetworkinfo");
}

void RpcClient::getBlockCount()
{
    call("getblockcount");
}

void RpcClient::getPeerInfo()
{
    call("getpeerinfo");
}

QUrl RpcClient::rpcUrl() const
{
    return QUrl(QString("http://127.0.0.1:%1").arg(m_config.bitcoinRpcPort()));
}
