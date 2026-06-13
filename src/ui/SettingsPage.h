#pragma once

#include "../core/ConfigManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class SettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(ConfigManager& config, QWidget* parent = nullptr);

private:
    void buildUi();
    void save();
    void showSavedOverlay();
    void positionSavedOverlay();
    void retranslate();

    ConfigManager& m_config;
    QLabel* m_title = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QLabel* m_storageTitle = nullptr;
    QLabel* m_bitcoinTitle = nullptr;
    QLabel* m_servicesTitle = nullptr;
    QLabel* m_appTitle = nullptr;
    QLabel* m_baseDirLabel = nullptr;
    QLabel* m_rpcUserLabel = nullptr;
    QLabel* m_rpcPasswordLabel = nullptr;
    QLabel* m_rpcPortLabel = nullptr;
    QLabel* m_networkLabel = nullptr;
    QLabel* m_electrsPortLabel = nullptr;
    QLabel* m_mempoolDatabasePortLabel = nullptr;
    QLabel* m_mempoolBackendPortLabel = nullptr;
    QLabel* m_mempoolFrontendPortLabel = nullptr;
    QLabel* m_publicPoolApiPortLabel = nullptr;
    QLabel* m_publicPoolStratumPortLabel = nullptr;
    QLabel* m_publicPoolFrontendPortLabel = nullptr;
    QLabel* m_publicPoolAddressLabel = nullptr;
    QLabel* m_themeLabel = nullptr;
    QLabel* m_languageLabel = nullptr;
    QLabel* m_autostartLabel = nullptr;
    QLabel* m_savedOverlayTitle = nullptr;
    QLabel* m_savedOverlayText = nullptr;
    QLineEdit* m_baseDir = nullptr;
    QLineEdit* m_rpcUser = nullptr;
    QLineEdit* m_rpcPassword = nullptr;
    QComboBox* m_network = nullptr;
    QLineEdit* m_rpcPort = nullptr;
    QLineEdit* m_electrsPort = nullptr;
    QLineEdit* m_mempoolDatabasePort = nullptr;
    QLineEdit* m_mempoolBackendPort = nullptr;
    QLineEdit* m_mempoolFrontendPort = nullptr;
    QLineEdit* m_publicPoolApiPort = nullptr;
    QLineEdit* m_publicPoolStratumPort = nullptr;
    QLineEdit* m_publicPoolFrontendPort = nullptr;
    QLineEdit* m_publicPoolAddress = nullptr;
    QComboBox* m_theme = nullptr;
    QComboBox* m_language = nullptr;
    QCheckBox* m_autostart = nullptr;
    QFrame* m_savedOverlay = nullptr;
    QVBoxLayout* m_leftColumn = nullptr;
    QVBoxLayout* m_rightColumn = nullptr;
    QList<QWidget*> m_sections;
};
