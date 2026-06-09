#include "core/AppTypes.h"
#include "core/ConfigManager.h"
#include "core/LogManager.h"
#include "core/ServiceManager.h"
#include "ui/FirstRunDialog.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Bitcoin Node Desktop");
    QApplication::setApplicationVersion(APP_VERSION);
    QApplication::setOrganizationName("BitcoinNodeDesktop");

    qRegisterMetaType<BitcoinNodeStatus>("BitcoinNodeStatus");
    qRegisterMetaType<ServiceStatus>("ServiceStatus");
    qRegisterMetaType<ServiceState>("ServiceState");

    ConfigManager config;
    LogManager logs;

    if (config.isFirstRun()) {
        FirstRunDialog dialog(config);
        if (dialog.exec() != QDialog::Accepted) {
            return 0;
        }
    }

    ServiceManager services(config, logs);
    MainWindow window(config, logs, services);
    window.show();

    if (config.autostart()) {
        services.startAll();
    }

    const int result = app.exec();
    services.stopAll();
    return result;
}
