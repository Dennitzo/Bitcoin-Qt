#include "LogsPage.h"

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

LogsPage::LogsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);

    auto* toolbar = new QHBoxLayout();
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Logs durchsuchen");
    m_autoscroll = new QCheckBox("Auto Scroll", this);
    m_autoscroll->setChecked(true);
    auto* exportButton = new QPushButton("Export", this);
    toolbar->addWidget(m_search, 1);
    toolbar->addWidget(m_autoscroll);
    toolbar->addWidget(exportButton);

    m_tabs = new QTabWidget(this);
    for (const QString& id : {"bitcoind", "electrs", "mempool-backend", "mempool-frontend"}) {
        editorFor(id);
    }

    root->addLayout(toolbar);
    root->addWidget(m_tabs, 1);

    QObject::connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.isEmpty()) {
            return;
        }
        if (auto* editor = qobject_cast<QPlainTextEdit*>(m_tabs->currentWidget())) {
            editor->find(text);
        }
    });
    QObject::connect(exportButton, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getSaveFileName(this, "Logs exportieren", "node-logs.txt");
        if (file.isEmpty()) {
            return;
        }
        QFile out(file);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return;
        }
        for (auto it = m_editors.cbegin(); it != m_editors.cend(); ++it) {
            out.write(QString("== %1 ==\n").arg(it.key()).toUtf8());
            out.write(it.value()->toPlainText().toUtf8());
            out.write("\n\n");
        }
    });
}

void LogsPage::appendLogLine(const QString& serviceId, const QString& line)
{
    auto* editor = editorFor(serviceId);
    editor->appendPlainText(line);
    if (m_autoscroll->isChecked()) {
        editor->moveCursor(QTextCursor::End);
    }
}

QPlainTextEdit* LogsPage::editorFor(const QString& serviceId)
{
    if (m_editors.contains(serviceId)) {
        return m_editors.value(serviceId);
    }
    auto* editor = new QPlainTextEdit(this);
    editor->setReadOnly(true);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setStyleSheet("font-family: Menlo, Consolas, monospace; font-size: 12px;");
    m_editors.insert(serviceId, editor);
    m_tabs->addTab(editor, serviceId);
    return editor;
}
