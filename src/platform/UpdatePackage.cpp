#include "UpdatePackage.h"
#include <QSysInfo>

QString UpdatePackage::currentPlatform()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macOS");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("Linux");
#else
    return QStringLiteral("Unsupported");
#endif
}

QString UpdatePackage::currentArchitecture()
{
    const QString architecture = QSysInfo::currentCpuArchitecture();
    if (architecture == "x86_64") return QStringLiteral("x64");
    if (architecture == "aarch64" || architecture == "arm64") return QStringLiteral("arm64");
    return architecture;
}

QString UpdatePackage::extension(const QString &platform)
{
    if (platform == "Windows") return QStringLiteral(".exe");
    if (platform == "macOS") return QStringLiteral(".dmg");
    if (platform == "Linux") return QStringLiteral(".tar.gz");
    return {};
}

bool UpdatePackage::matchesAsset(const QString &name, const QString &platform,
                                const QString &architecture)
{
    if (architecture != "x64" && architecture != "arm64") return false;
    if (platform == "Windows")
        return name.startsWith("Nexus-Setup-") && name.endsWith("-" + architecture + ".exe");
    if (platform == "macOS")
        return name.startsWith("Nexus-")
            && (name.endsWith("-macOS-" + architecture + ".dmg")
                || name.endsWith("-macOS-universal.dmg"));
    if (platform == "Linux")
        return name.startsWith("Nexus-") && name.endsWith("-Linux-" + architecture + ".tar.gz");
    return false;
}
