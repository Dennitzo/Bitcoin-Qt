#include "RuntimePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
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
