#include "FirstRunDialog.h"

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
    setWindowTitle("Speicherort einrichten");
    setModal(true);
    resize(620, 220);

    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("Bitcoin Full Node Speicherort", this);
    title->setStyleSheet("font-size: 22px; font-weight: 700;");
    auto* description = new QLabel(
        "Wähle eine interne oder externe Festplatte. Bitcoin Core, electrs und Mempool speichern "
        "ihre Daten vollständig in diesem Ordner.",
        this);
    description->setWordWrap(true);

    auto* row = new QHBoxLayout();
    m_path = new QLineEdit(this);
    m_path->setPlaceholderText("/Volumes/BitcoinNode oder /media/bitcoin-node");
    auto* browse = new QPushButton("Auswählen", this);
    row->addWidget(m_path, 1);
    row->addWidget(browse);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Speichern");

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
    const QString dir = QFileDialog::getExistingDirectory(this, "Datenträger oder Ordner auswählen");
    if (!dir.isEmpty()) {
        m_path->setText(dir);
    }
}

void FirstRunDialog::accept()
{
    const QString path = m_path->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Speicherort fehlt", "Bitte wähle zuerst eine Festplatte oder einen Ordner aus.");
        return;
    }
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(".")) {
        QMessageBox::critical(this, "Speicherort nicht nutzbar", "Der Ordner konnte nicht erstellt werden.");
        return;
    }
    m_config.setBaseDataDir(dir.absolutePath());
    QDialog::accept();
}
