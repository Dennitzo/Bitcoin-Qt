#pragma once

#include <QString>
#include <QStringList>

class RuntimePaths final {
public:
    static QString runtimeRoot();
    static QString executable(const QString& relativePath);
    static bool isExecutableAvailable(const QString& absolutePath);
    static QStringList searchRoots();

private:
    static QString normalizeExecutable(const QString& path);
};
