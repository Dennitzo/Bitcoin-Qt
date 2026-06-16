#include "MempoolDatabaseService.h"

#include "../core/RuntimePaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

MempoolDatabaseService::MempoolDatabaseService(ConfigManager& config, LogManager& logs, QObject* parent)
    : ManagedService("mempool-db", "Mempool DB", config, logs, parent)
{
    setRestartEnabled(false);
    m_healthTimer.setInterval(1500);
    QObject::connect(&m_healthTimer, &QTimer::timeout, this, &MempoolDatabaseService::checkPort);

    m_initializer.setProcessChannelMode(QProcess::SeparateChannels);
    QObject::connect(&m_initializer, &QProcess::readyReadStandardOutput, this, [this]() {
        const QStringList lines = QString::fromLocal8Bit(m_initializer.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            this->logs().append(id(), line.trimmed());
        }
    });
    QObject::connect(&m_initializer, &QProcess::readyReadStandardError, this, [this]() {
        const QStringList lines = QString::fromLocal8Bit(m_initializer.readAllStandardError()).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            this->logs().append(id(), line.trimmed());
        }
    });
    QObject::connect(&m_initializer, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_startRequested) {
            return;
        }
        const QString output = QString::fromLocal8Bit(m_initializer.readAllStandardOutput()) + QString::fromLocal8Bit(m_initializer.readAllStandardError());
        if (exitStatus == QProcess::CrashExit || exitCode != 0) {
            handleStartupFailure(output.isEmpty() ? QString("MariaDB Initialisierung beendet mit Code %1").arg(exitCode) : output);
            return;
        }
        cleanupInitializationDir();
        if (!isInitialized()) {
            setState(ServiceState::Error, "MariaDB Datenbank wurde nicht vollständig initialisiert");
            return;
        }
        if (!writeInitSql()) {
            setState(ServiceState::Error, "MariaDB Init-SQL konnte nicht geschrieben werden");
            return;
        }
        startDatabase();
    });

    QObject::connect(&process(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_startRequested || isManualStopRequested() || (exitStatus != QProcess::CrashExit && exitCode == 0)) {
            return;
        }

        const QString output = m_recentOutput.join('\n');
        handleStartupFailure(output.isEmpty() ? QString("MariaDB beendet mit Code %1").arg(exitCode) : output);
    });

    QObject::connect(&process(), &QProcess::started, this, [this]() {
        setState(ServiceState::Starting, "Warte auf Datenbank");
    });
}

MempoolDatabaseService::~MempoolDatabaseService()
{
    stop();
}

void MempoolDatabaseService::start()
{
    m_startRequested = true;
    if (process().state() != QProcess::NotRunning) {
        checkPort();
        return;
    }

    QDir().mkpath(config().mempoolDatabaseDir());
    if (isInitialized()) {
        startDatabase();
        return;
    }
    if (!QDir(config().mempoolDatabaseDir()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty() && !resetDatabaseDirectory()) {
        setState(ServiceState::Error, "Mempool DB konnte nicht zurückgesetzt werden");
        return;
    }
    initializeDatabase();
}

void MempoolDatabaseService::stop()
{
    m_startRequested = false;
    m_healthTimer.stop();
    if (m_initializer.state() != QProcess::NotRunning) {
        m_initializer.terminate();
        if (!m_initializer.waitForFinished(3000)) {
            m_initializer.kill();
        }
    }
    cleanupInitializationDir();
    ManagedService::stop();
}

void MempoolDatabaseService::initializeDatabase()
{
    const QString installer = config().mariadbInstallDbExecutable();
    if (!RuntimePaths::isExecutableAvailable(installer)) {
        setState(ServiceState::Error, QString("MariaDB Initializer fehlt: %1").arg(installer));
        return;
    }

    setState(ServiceState::Starting, "Initialisiert Datenbank");
    cleanupInitializationDir();
    m_initializationDir = config().mempoolDatabaseDir();
    QDir(m_initializationDir).removeRecursively();
    QDir().mkpath(m_initializationDir);
    m_initializer.setWorkingDirectory(baseDir());
    m_initializer.setProgram(installer);
    m_initializer.setArguments({
        QString("--basedir=%1").arg(baseDir()),
        QString("--datadir=%1").arg(m_initializationDir),
        "--auth-root-authentication-method=normal",
        "--skip-test-db",
    });
    m_initializer.start();
}

void MempoolDatabaseService::startDatabase()
{
    if (!m_startRequested || process().state() != QProcess::NotRunning) {
        return;
    }

    const QString program = config().mariadbExecutable();
    if (!RuntimePaths::isExecutableAvailable(program)) {
        setState(ServiceState::Error, QString("MariaDB Runtime fehlt: %1").arg(program));
        return;
    }
    if (!writeInitSql()) {
        setState(ServiceState::Error, "MariaDB Init-SQL konnte nicht geschrieben werden");
        return;
    }

    startProcess(program, {
        QString("--basedir=%1").arg(baseDir()),
        QString("--datadir=%1").arg(config().mempoolDatabaseDir()),
        QString("--port=%1").arg(config().mempoolDatabasePort()),
        QString("--socket=%1").arg(socketPath()),
        QString("--pid-file=%1").arg(pidPath()),
        QString("--init-file=%1").arg(initSqlPath()),
        "--bind-address=127.0.0.1",
        "--skip-networking=0",
        "--character-set-server=utf8mb4",
        "--collation-server=utf8mb4_unicode_ci",
        "--max-connections=100",
    }, config().mempoolDatabaseDir());
    m_healthTimer.start();
}

void MempoolDatabaseService::handleStdout(const QString& line)
{
    m_recentOutput << line;
    while (m_recentOutput.size() > 80) {
        m_recentOutput.removeFirst();
    }
    ManagedService::handleStdout(line);
}

void MempoolDatabaseService::handleStderr(const QString& line)
{
    m_recentOutput << line;
    while (m_recentOutput.size() > 80) {
        m_recentOutput.removeFirst();
    }
    ManagedService::handleStderr(line);
}

void MempoolDatabaseService::handleStartupFailure(const QString& output)
{
    const QString lower = output.toLower();
    if (!m_attemptedReset && (lower.contains("wrong space id")
        || lower.contains("data structure corruption")
        || lower.contains("unsupported storage engine")
        || lower.contains("mysqld upgrade")
        || lower.contains("run mariadb-upgrade")
        || lower.contains("table already exists"))) {
        m_attemptedReset = true;
        if (resetDatabaseDirectory()) {
            setState(ServiceState::Starting, "Mempool DB wird neu initialisiert");
            initializeDatabase();
            return;
        }
    }
    setState(ServiceState::Error, QString("MariaDB Fehler: %1").arg(output.trimmed().left(300)));
}

bool MempoolDatabaseService::resetDatabaseDirectory()
{
    m_recentOutput.clear();
    const QString dbDir = config().mempoolDatabaseDir();
    const QString backupDir = QDir(config().baseDataDir()).filePath(QString("mempool-db-broken-%1").arg(QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmss")));
    if (QDir(dbDir).exists()) {
        QDir().mkpath(config().baseDataDir());
        if (QDir().rename(dbDir, backupDir)) {
            QDir().mkpath(dbDir);
            return true;
        }
        QDir(dbDir).removeRecursively();
        QDir().mkpath(dbDir);
        return true;
    }
    QDir().mkpath(dbDir);
    return true;
}

void MempoolDatabaseService::cleanupInitializationDir()
{
    if (m_initializationDir.isEmpty()) {
        return;
    }
    m_initializationDir.clear();
}

void MempoolDatabaseService::checkPort()
{
    if (!m_startRequested || process().state() == QProcess::NotRunning) {
        return;
    }

    auto* socket = new QTcpSocket(this);
    QObject::connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        m_healthTimer.stop();
        socket->disconnectFromHost();
        socket->deleteLater();
        setState(ServiceState::Running, "Datenbank erreichbar");
        Q_EMIT ready(id());
    });
    QObject::connect(socket, &QTcpSocket::errorOccurred, socket, &QTcpSocket::deleteLater);
    socket->connectToHost("127.0.0.1", config().mempoolDatabasePort());
}

bool MempoolDatabaseService::isInitialized() const
{
    return QDir(config().mempoolDatabaseDir()).exists("mysql") && QFileInfo::exists(QDir(config().mempoolDatabaseDir()).filePath("ibdata1"));
}

bool MempoolDatabaseService::writeInitSql() const
{
    QFile file(initSqlPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write("CREATE DATABASE IF NOT EXISTS mempool CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n");
    file.write("CREATE USER IF NOT EXISTS 'mempool'@'127.0.0.1' IDENTIFIED BY 'mempool';\n");
    file.write("CREATE USER IF NOT EXISTS 'mempool'@'localhost' IDENTIFIED BY 'mempool';\n");
    file.write("GRANT ALL PRIVILEGES ON mempool.* TO 'mempool'@'127.0.0.1';\n");
    file.write("GRANT ALL PRIVILEGES ON mempool.* TO 'mempool'@'localhost';\n");
    file.write("FLUSH PRIVILEGES;\n");
    return true;
}

QString MempoolDatabaseService::socketPath() const
{
    return QDir(config().mempoolDatabaseDir()).filePath("mariadb.sock");
}

QString MempoolDatabaseService::pidPath() const
{
    return QDir(config().mempoolDatabaseDir()).filePath("mariadb.pid");
}

QString MempoolDatabaseService::initSqlPath() const
{
    return QDir(config().mempoolDatabaseDir()).filePath("init.sql");
}

QString MempoolDatabaseService::baseDir() const
{
    QDir dir(QFileInfo(config().mariadbExecutable()).absoluteDir());
    if (dir.dirName() == "bin" || dir.dirName() == "libexec") {
        dir.cdUp();
    }
    return dir.absolutePath();
}
