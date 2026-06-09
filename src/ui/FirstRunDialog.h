#pragma once

#include "../core/ConfigManager.h"

#include <QDialog>
#include <QLineEdit>

class FirstRunDialog final : public QDialog {
    Q_OBJECT

public:
    explicit FirstRunDialog(ConfigManager& config, QWidget* parent = nullptr);

private:
    void chooseDirectory();
    void accept() override;

    ConfigManager& m_config;
    QLineEdit* m_path = nullptr;
};
