#pragma once

#include <QLabel>
#include <QUrl>
#include <QWebEngineView>
#include <QWidget>

class NodePage final : public QWidget {
    Q_OBJECT

public:
    explicit NodePage(QWidget* parent = nullptr);
    QWebEngineView* webView() const;

public Q_SLOTS:
    void loadMempool(const QUrl& url);

private:
    QLabel* m_status = nullptr;
    QWebEngineView* m_webView = nullptr;
};
