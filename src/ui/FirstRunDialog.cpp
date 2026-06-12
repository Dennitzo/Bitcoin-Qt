#include "FirstRunDialog.h"

#include "../core/Localization.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

FirstRunDialog::FirstRunDialog(ConfigManager& config, QWidget* parent)
    : QDialog(parent),
      m_config(config)
{
    const QString lang = m_config.language();
    setWindowTitle(appText(lang, "firstRun.windowTitle"));
    setModal(true);
    resize(620, 220);

    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel(appText(lang, "firstRun.title"), this);
    title->setStyleSheet("font-size: 22px; font-weight: 700;");
    auto* description = new QLabel(appText(lang, "firstRun.description"), this);
    description->setWordWrap(true);

    auto* row = new QHBoxLayout();
    m_path = new QLineEdit(this);
    m_path->setPlaceholderText(appText(lang, "firstRun.placeholder"));
    auto* browse = new QPushButton(appText(lang, "app.select"), this);
    row->addWidget(m_path, 1);
    row->addWidget(browse);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(appText(lang, "app.save"));

    root->addWidget(title);
    root->addWidget(description);
    root->addSpacing(12);
    root->addLayout(row);
    root->addStretch();
    root->addWidget(buttons);

    QObject::connect(browse, &QPushButton::clicked, this, &FirstRunDialog::chooseDirectory);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &FirstRunDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &FirstRunDialog::reject);
}

void FirstRunDialog::chooseDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(this, appText(m_config.language(), "firstRun.chooseDirectory"));
    if (!dir.isEmpty()) {
        m_path->setText(dir);
    }
}

void FirstRunDialog::accept()
{
    const QString path = m_path->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, appText(m_config.language(), "firstRun.missingTitle"), appText(m_config.language(), "firstRun.missingText"));
        return;
    }
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(".")) {
        QMessageBox::critical(this, appText(m_config.language(), "firstRun.unusableTitle"), appText(m_config.language(), "firstRun.unusableText"));
        return;
    }
    m_config.setBaseDataDir(dir.absolutePath());
    QDialog::accept();
}
