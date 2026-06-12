#include "core/AppTypes.h"
#include "core/ConfigManager.h"
#include "core/LogManager.h"
#include "core/ServiceManager.h"
#include "ui/FirstRunDialog.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QIcon>
#include <QMessageBox>

namespace {

void configureProcessLogging()
{
    qputenv("QT_LOGGING_RULES", "qt.webengine*.debug=false;qt.webengine*.info=false;qt.webengine*.warning=false");

    QByteArray chromiumFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (!chromiumFlags.isEmpty()) {
        chromiumFlags.append(' ');
    }
    chromiumFlags.append("--disable-logging --log-level=3 --disable-speech-api");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", chromiumFlags);
}

}

int main(int argc, char* argv[])
{
    configureProcessLogging();

    QApplication app(argc, argv);
    QApplication::setApplicationName("Bitcoin-Qt");
    QApplication::setApplicationVersion(APP_VERSION);
    QApplication::setOrganizationName("Bitcoin-Qt");
    QApplication::setWindowIcon(QIcon(":/icons/Bitcoin.png"));

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

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&services]() {
        services.stopAll();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    });

    if (config.autostart()) {
        services.startConfiguredServices();
    }

    const int result = app.exec();
    return result;
}
