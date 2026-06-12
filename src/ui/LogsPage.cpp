#include "LogsPage.h"

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
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setObjectName("logView");
    root->addWidget(m_editor, 1);
}

void LogsPage::appendLogLine(const QString& serviceId, const QString& line)
{
    if (!m_serviceIds.contains(serviceId)) {
        return;
    }
    m_editor->appendPlainText(line);
    m_editor->moveCursor(QTextCursor::End);
}
