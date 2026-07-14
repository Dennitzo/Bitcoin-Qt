#pragma once

#include <QString>
#include <QStringList>

class RuntimePaths final {
public:
    static QString runtimeRoot();
    static QString executable(const QString& relativePath);
    static QString componentVersion(const QString& component);
    static QString versionedLabel(const QString& label, const QString& component);
    static bool isExecutableAvailable(const QString& absolutePath);
    static QStringList searchRoots();

private:
    static QString normalizeExecutable(const QString& path);
};
