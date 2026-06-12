#include "NodePage.h"

#include <QVBoxLayout>

#include <utility>

NodePage::NodePage(ConfigManager& config, QString storageId, const QString& waitingText, QWidget* parent)
    : QWidget(parent),
      m_config(config),
      m_storageId(std::move(storageId))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    Q_UNUSED(waitingText)
    m_webView = new QWebEngineView(this);
    QObject::connect(m_webView, &QWebEngineView::urlChanged, this, [this](const QUrl& url) {
        m_config.setWebViewUrl(m_storageId, url);
    });
    root->addWidget(m_webView, 1);
}

QWebEngineView* NodePage::webView() const
{
    return m_webView;
}

void NodePage::loadUrl(const QUrl& url)
{
    const QUrl savedUrl = m_config.webViewUrl(m_storageId);
    m_webView->load(isSameOrigin(savedUrl, url) ? savedUrl : url);
}

bool NodePage::isSameOrigin(const QUrl& left, const QUrl& right) const
{
    return left.isValid()
        && right.isValid()
        && left.scheme() == right.scheme()
        && left.host() == right.host()
        && left.port() == right.port();
}
