#pragma once

#include <QSet>
#include <QWebEngineUrlRequestInterceptor>

class LocalUrlInterceptor final : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

public:
    explicit LocalUrlInterceptor(QObject* parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo& info) override;
    void setAllowedPorts(const QSet<int>& ports);

private:
    QSet<int> m_allowedPorts;
};
