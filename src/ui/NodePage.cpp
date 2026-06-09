#include "NodePage.h"

#include <QVBoxLayout>

NodePage::NodePage(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_status = new QLabel("Mempool wird geladen, sobald Backend und Frontend bereit sind.", this);
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setMinimumHeight(48);
    m_status->setStyleSheet("background:#f4f6f8;color:#30313d;");

    m_webView = new QWebEngineView(this);
    root->addWidget(m_status);
    root->addWidget(m_webView, 1);
}

QWebEngineView* NodePage::webView() const
{
    return m_webView;
}

void NodePage::loadMempool(const QUrl& url)
{
    m_status->setText(QString("Mempool lokal geladen: %1").arg(url.toString()));
    m_webView->load(url);
}
