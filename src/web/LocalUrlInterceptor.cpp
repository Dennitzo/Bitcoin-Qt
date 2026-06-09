#include "LocalUrlInterceptor.h"

#include <QWebEngineUrlRequestInfo>

LocalUrlInterceptor::LocalUrlInterceptor(QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

void LocalUrlInterceptor::setAllowedPorts(const QSet<int>& ports)
{
    m_allowedPorts = ports;
}

void LocalUrlInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info)
{
    const QUrl url = info.requestUrl();
    if (url.scheme() == "qrc" || url.scheme() == "data" || url.scheme() == "blob") {
        return;
    }
    const bool localHost = url.host() == "127.0.0.1" || url.host() == "localhost";
    if ((url.scheme() == "http" || url.scheme() == "https") && localHost && m_allowedPorts.contains(url.port())) {
        return;
    }
    info.block(true);
}
