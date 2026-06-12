#include "NodePage.h"

#include <QVBoxLayout>

NodePage::NodePage(const QString& waitingText, QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    Q_UNUSED(waitingText)
    m_webView = new QWebEngineView(this);
    root->addWidget(m_webView, 1);
}

QWebEngineView* NodePage::webView() const
{
    return m_webView;
}

void NodePage::loadUrl(const QUrl& url)
{
    m_webView->load(url);
}
