#pragma once

#include <QUrl>
#include <QWebEngineView>
#include <QWidget>

class NodePage final : public QWidget {
    Q_OBJECT

public:
    explicit NodePage(const QString& waitingText, QWidget* parent = nullptr);
    QWebEngineView* webView() const;

public Q_SLOTS:
    void loadUrl(const QUrl& url);

private:
    QWebEngineView* m_webView = nullptr;
};
