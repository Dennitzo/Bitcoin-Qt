#pragma once

#include "../core/ConfigManager.h"

#include <QTimer>
#include <QUrl>
#include <QWebEngineView>
#include <QWidget>

class NodePage final : public QWidget {
    Q_OBJECT

public:
    NodePage(ConfigManager& config, QString storageId, const QString& waitingText, QWidget* parent = nullptr);
    QWebEngineView* webView() const;

public Q_SLOTS:
    void loadUrl(const QUrl& url);
    void setAutoReloadInterval(int milliseconds);

private:
    void showEvent(QShowEvent* event) override;
    bool isSameOrigin(const QUrl& left, const QUrl& right) const;

    ConfigManager& m_config;
    QString m_storageId;
    QWebEngineView* m_webView = nullptr;
    QTimer m_autoReloadTimer;
};
