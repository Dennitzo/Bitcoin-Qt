#include "MempoolService.h"

#include "../core/RuntimePaths.h"

#include <QDir>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>

MempoolService::MempoolService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("mempool", "Mempool", config, logs, parent)
{
    attachProcess(m_backend, "mempool-backend");
    attachProcess(m_frontend, "mempool-frontend");
    m_backendHealth.setInterval(2500);
    m_frontendHealth.setInterval(2500);
    QObject::connect(&m_backendHealth, &QTimer::timeout, this, &MempoolService::checkBackend);
    QObject::connect(&m_frontendHealth, &QTimer::timeout, this, &MempoolService::checkFrontend);
}

MempoolService::~MempoolService()
{
    stop();
}

void MempoolService::start()
{
    startBackend();
}

void MempoolService::stop()
{
    m_backendHealth.stop();
    m_frontendHealth.stop();
    for (QProcess* proc : {&m_frontend, &m_backend}) {
        if (proc->state() == QProcess::NotRunning) {
            continue;
        }
        proc->terminate();
        if (!proc->waitForFinished(3000)) {
            proc->kill();
        }
    }
    setState(ServiceState::Stopped, "Gestoppt");
}

QUrl MempoolService::frontendUrl() const
{
    return QUrl(QString("http://%1:%2").arg(config().mempoolHost()).arg(config().mempoolFrontendPort()));
}

void MempoolService::startBackend()
{
    const QString node = config().nodeExecutable();
    const QString script = QDir(RuntimePaths::runtimeRoot()).filePath("mempool/backend/server.js");
    if (!RuntimePaths::isExecutableAvailable(node) || !QFileInfo::exists(script)) {
        setState(ServiceState::Error, QString("Mempool Backend Runtime fehlt: %1 / %2").arg(node, script));
        return;
    }

    setState(ServiceState::Starting, "Backend startet");
    m_backend.setWorkingDirectory(QDir(RuntimePaths::runtimeRoot()).filePath("mempool"));
    QProcessEnvironment backendEnv = QProcessEnvironment::systemEnvironment();
    backendEnv.insert("MEMPOOL_BACKEND_PORT", QString::number(config().mempoolBackendPort()));
    backendEnv.insert("MEMPOOL_HTTP_PORT", QString::number(config().mempoolBackendPort()));
    m_backend.setProcessEnvironment(backendEnv);
    m_backend.start(node, {"backend/server.js"});
    m_backendHealth.start();
}

void MempoolService::startFrontend()
{
    if (m_frontend.state() != QProcess::NotRunning) {
        return;
    }
    const QString node = config().nodeExecutable();
    const QString script = QDir(RuntimePaths::runtimeRoot()).filePath("mempool/frontend/server.js");
    if (!RuntimePaths::isExecutableAvailable(node) || !QFileInfo::exists(script)) {
        setState(ServiceState::Error, QString("Mempool Frontend Runtime fehlt: %1 / %2").arg(node, script));
        return;
    }

    setState(ServiceState::Starting, "Frontend startet");
    m_frontend.setWorkingDirectory(QDir(RuntimePaths::runtimeRoot()).filePath("mempool"));
    QProcessEnvironment frontendEnv = QProcessEnvironment::systemEnvironment();
    frontendEnv.insert("MEMPOOL_FRONTEND_PORT", QString::number(config().mempoolFrontendPort()));
    frontendEnv.insert("PORT", QString::number(config().mempoolFrontendPort()));
    m_frontend.setProcessEnvironment(frontendEnv);
    m_frontend.start(node, {"frontend/server.js"});
    m_frontendHealth.start();
}

void MempoolService::checkBackend()
{
    QNetworkReply* reply = m_network.get(QNetworkRequest(QUrl(QString("http://127.0.0.1:%1").arg(config().mempoolBackendPort()))));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setState(ServiceState::Starting, "Warte auf Mempool Backend");
            return;
        }
        m_backendHealth.stop();
        startFrontend();
    });
}

void MempoolService::checkFrontend()
{
    QNetworkReply* reply = m_network.get(QNetworkRequest(frontendUrl()));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setState(ServiceState::Starting, "Warte auf Mempool Frontend");
            return;
        }
        m_frontendHealth.stop();
        setState(ServiceState::Running, "Frontend erreichbar");
        Q_EMIT frontendAvailable(frontendUrl());
        Q_EMIT ready(id());
    });
}

void MempoolService::attachProcess(QProcess& child, const QString& logId)
{
    child.setProcessChannelMode(QProcess::SeparateChannels);
    QObject::connect(&child, &QProcess::readyReadStandardOutput, this, [&child, this, logId]() {
        const QStringList lines = QString::fromLocal8Bit(child.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            logs().append(logId, line.trimmed());
        }
    });
    QObject::connect(&child, &QProcess::readyReadStandardError, this, [&child, this, logId]() {
        const QStringList lines = QString::fromLocal8Bit(child.readAllStandardError()).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            logs().append(logId, line.trimmed());
        }
    });
    QObject::connect(&child, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        setState(ServiceState::Error, "Mempool Prozessfehler");
    });
}
