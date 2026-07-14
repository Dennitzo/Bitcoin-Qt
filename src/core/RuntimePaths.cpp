#include "RuntimePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

QString RuntimePaths::runtimeRoot()
{
    for (const QString& root : searchRoots()) {
        if (QDir(root).exists()) {
            return root;
        }
    }
    return searchRoots().constFirst();
}

QString RuntimePaths::executable(const QString& relativePath)
{
    for (const QString& root : searchRoots()) {
        const QString candidate = normalizeExecutable(QDir(root).filePath(relativePath));
        if (isExecutableAvailable(candidate)) {
            return candidate;
        }
    }
    return normalizeExecutable(QDir(runtimeRoot()).filePath(relativePath));
}

QString RuntimePaths::componentVersion(const QString& component)
{
    const QDir componentDir(QDir(runtimeRoot()).filePath(component));
    QFile versionFile(componentDir.filePath("VERSION"));
    if (versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString version = QString::fromUtf8(versionFile.readAll()).trimmed();
        if (!version.isEmpty() && version.compare("master", Qt::CaseInsensitive) != 0) {
            return version.startsWith('v') ? version.mid(1) : version;
        }
    }

    QFile packageFile(componentDir.filePath("backend/package.json"));
    if (packageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString version = QJsonDocument::fromJson(packageFile.readAll()).object().value("version").toString().trimmed();
        if (!version.isEmpty()) {
            return version.startsWith('v') ? version.mid(1) : version;
        }
    }
    return {};
}

QString RuntimePaths::versionedLabel(const QString& label, const QString& component)
{
    const QString version = componentVersion(component);
    return version.isEmpty() ? label : QString("%1 %2").arg(label, version);
}

bool RuntimePaths::isExecutableAvailable(const QString& absolutePath)
{
    const QFileInfo info(absolutePath);
    return info.exists() && info.isFile() && info.isExecutable();
}

QStringList RuntimePaths::searchRoots()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    QStringList roots;

#ifdef Q_OS_MAC
    roots << appDir.filePath("../Resources/runtime");
#endif

#ifdef Q_OS_WIN
    roots << appDir.filePath("windows/runtime");
    roots << appDir.filePath("../windows/runtime");
    roots << appDir.filePath("../../windows/runtime");
    roots << appDir.filePath("../../../windows/runtime");
    roots << appDir.filePath("../../../../windows/runtime");
    roots << appDir.filePath("../../../../../windows/runtime");
    roots << QDir::current().filePath("windows/runtime");
#endif

    roots << appDir.filePath("runtime");
    roots << appDir.filePath("../runtime");
    roots << appDir.filePath("../../runtime");
    roots << appDir.filePath("../../../runtime");
    roots << appDir.filePath("../../../../runtime");
    roots << appDir.filePath("../../../../../runtime");
    roots << appDir.filePath("../../../../../../runtime");
    roots << QDir::current().filePath("runtime");

    QStringList normalized;
    for (const QString& root : roots) {
        const QString clean = QDir::cleanPath(root);
        if (!normalized.contains(clean)) {
            normalized << clean;
        }
    }
    return normalized;
}

QString RuntimePaths::normalizeExecutable(const QString& path)
{
#ifdef Q_OS_WIN
    if (!path.endsWith(".exe", Qt::CaseInsensitive)) {
        return path + ".exe";
    }
#endif
    return QDir::cleanPath(path);
}
