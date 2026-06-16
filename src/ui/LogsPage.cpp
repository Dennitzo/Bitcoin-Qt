#include "LogsPage.h"

#include <QScrollBar>
#include <QTextCursor>
#include <QTextOption>
#include <QVBoxLayout>

LogsPage::LogsPage(const QString& title, const QStringList& serviceIds, QWidget* parent)
    : QWidget(parent),
      m_serviceIds(serviceIds.isEmpty() ? QStringList{title.toLower()} : serviceIds)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(0);

    m_editor = new QPlainTextEdit(this);
    m_editor->setReadOnly(true);
    m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_editor->setObjectName("logView");
    root->addWidget(m_editor, 1);

    QObject::connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        const QScrollBar* bar = m_editor->verticalScrollBar();
        m_autoScroll = value >= bar->maximum() - 2;
    });
}

void LogsPage::appendLogLine(const QString& serviceId, const QString& line)
{
    if (!m_serviceIds.contains(serviceId)) {
        return;
    }
    QScrollBar* bar = m_editor->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 2;
    const int previousValue = bar->value();
    m_editor->appendPlainText(line);
    if (m_autoScroll || atBottom) {
        m_editor->moveCursor(QTextCursor::End);
        bar->setValue(bar->maximum());
        return;
    }
    bar->setValue(previousValue);
}
