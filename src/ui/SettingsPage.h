#pragma once

#include "../core/ConfigManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

class SettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(ConfigManager& config, QWidget* parent = nullptr);

private:
    void buildUi();
    void save();

    ConfigManager& m_config;
    QLineEdit* m_baseDir = nullptr;
    QLineEdit* m_rpcUser = nullptr;
    QLineEdit* m_rpcPassword = nullptr;
    QComboBox* m_network = nullptr;
    QSpinBox* m_rpcPort = nullptr;
    QSpinBox* m_electrsPort = nullptr;
    QSpinBox* m_mempoolBackendPort = nullptr;
    QSpinBox* m_mempoolFrontendPort = nullptr;
    QSpinBox* m_publicPoolApiPort = nullptr;
    QSpinBox* m_publicPoolStratumPort = nullptr;
    QSpinBox* m_publicPoolFrontendPort = nullptr;
    QLineEdit* m_publicPoolAddress = nullptr;
    QComboBox* m_theme = nullptr;
    QComboBox* m_language = nullptr;
    QCheckBox* m_autostart = nullptr;
};
