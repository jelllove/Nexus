#include "platform/UpdatePackage.h"
#include <QtTest>

class UpdatePackageTest : public QObject
{
    Q_OBJECT

private slots:
    void matchingAssets_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("platform");
        QTest::addColumn<QString>("architecture");
        QTest::addColumn<bool>("matches");
        QTest::newRow("windows") << "Nexus-Setup-v1.0.8-x64.exe" << "Windows" << "x64" << true;
        QTest::newRow("windows-wrong-cpu") << "Nexus-Setup-v1.0.8-arm64.exe" << "Windows" << "x64" << false;
        QTest::newRow("mac-intel") << "Nexus-1.0.8-macOS-x64.dmg" << "macOS" << "x64" << true;
        QTest::newRow("mac-arm") << "Nexus-1.0.8-macOS-arm64.dmg" << "macOS" << "arm64" << true;
        QTest::newRow("mac-universal") << "Nexus-1.0.8-macOS-universal.dmg" << "macOS" << "arm64" << true;
        QTest::newRow("mac-wrong-cpu") << "Nexus-1.0.8-macOS-x64.dmg" << "macOS" << "arm64" << false;
        QTest::newRow("linux") << "Nexus-1.0.8-Linux-x64.tar.gz" << "Linux" << "x64" << true;
        QTest::newRow("linux-arm") << "Nexus-1.0.8-Linux-arm64.tar.gz" << "Linux" << "arm64" << true;
        QTest::newRow("linux-wrong-cpu") << "Nexus-1.0.8-Linux-arm64.tar.gz" << "Linux" << "x64" << false;
        QTest::newRow("wrong-os") << "Nexus-Setup-v1.0.8-x64.exe" << "Linux" << "x64" << false;
        QTest::newRow("wrong-extension") << "Nexus-1.0.8-macOS-arm64.exe" << "macOS" << "arm64" << false;
        QTest::newRow("wrong-product") << "Other-1.0.8-Linux-x64.tar.gz" << "Linux" << "x64" << false;
        QTest::newRow("unknown-os") << "Nexus-1.0.8-Linux-x64.tar.gz" << "Other" << "x64" << false;
        QTest::newRow("unknown-cpu") << "Nexus-1.0.8-Linux-other.tar.gz" << "Linux" << "other" << false;
    }

    void matchingAssets()
    {
        QFETCH(QString, name);
        QFETCH(QString, platform);
        QFETCH(QString, architecture);
        QFETCH(bool, matches);
        QCOMPARE(UpdatePackage::matchesAsset(name, platform, architecture), matches);
    }

    void extensions()
    {
        QCOMPARE(UpdatePackage::extension("Windows"), ".exe");
        QCOMPARE(UpdatePackage::extension("macOS"), ".dmg");
        QCOMPARE(UpdatePackage::extension("Linux"), ".tar.gz");
        QVERIFY(UpdatePackage::extension("Other").isEmpty());
    }

    void currentPlatformMatchesBuild()
    {
#ifdef Q_OS_WIN
        QCOMPARE(UpdatePackage::currentPlatform(), "Windows");
#elif defined(Q_OS_MACOS)
        QCOMPARE(UpdatePackage::currentPlatform(), "macOS");
#else
        QCOMPARE(UpdatePackage::currentPlatform(), "Linux");
#endif
        QVERIFY(!UpdatePackage::currentArchitecture().isEmpty());
    }
};

QTEST_GUILESS_MAIN(UpdatePackageTest)
#include "UpdatePackageTest.moc"
