#pragma once

#include "../core/ConfigManager.h"

#include <QLabel>
#include <QWidget>

class SettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(ConfigManager& config, QWidget* parent = nullptr);

private:
    ConfigManager& m_config;
};
