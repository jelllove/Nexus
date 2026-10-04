#pragma once

#include <QString>

namespace UpdatePackage {
QString currentPlatform();
QString currentArchitecture();
QString extension(const QString &platform);
bool matchesAsset(const QString &name, const QString &platform, const QString &architecture);
}
