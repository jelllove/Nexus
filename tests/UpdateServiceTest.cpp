#include "services/UpdateService.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSysInfo>
#include <QTimer>
#include <QtTest>
#include <cstring>

class TestReply : public QNetworkReply
{
public:
    TestReply(const QNetworkRequest &request, QObject *parent)
        : QNetworkReply(parent)
    {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly);
    }

    void abort() override { finish({}, OperationCanceledError); }

    void finish(const QByteArray &body, NetworkError failure = NoError)
    {
        m_body = body;
        if (failure != NoError) setError(failure, "simulated failure");
        setFinished(true);
        emit readyRead();
        emit finished();
    }

    qint64 bytesAvailable() const override
    {
        return m_body.size() - m_offset + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 count = qMin(maxSize, m_body.size() - m_offset);
        if (count <= 0) return -1;
        std::memcpy(data, m_body.constData() + m_offset, static_cast<size_t>(count));
        m_offset += count;
        return count;
    }

private:
    QByteArray m_body;
    qint64 m_offset = 0;
};

class TestNetwork : public QNetworkAccessManager
{
public:
    QList<QNetworkRequest> requests;
    QPointer<TestReply> reply;

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        requests.append(request);
        reply = new TestReply(request, this);
        return reply;
    }
};

static QString installerUrl(const QString &version = "v1.0.8")
{
    QString architecture = QSysInfo::currentCpuArchitecture();
    if (architecture == "x86_64") architecture = "x64";
    if (architecture == "aarch64") architecture = "arm64";
#ifdef Q_OS_WIN
    const QString name = "Nexus-Setup-" + version + "-" + architecture + ".exe";
#elif defined(Q_OS_MACOS)
    const QString name = "Nexus-" + version + "-macOS-" + architecture + ".dmg";
#else
    const QString name = "Nexus-" + version + "-Linux-" + architecture + ".tar.gz";
#endif
    return QString("https://github.com/jelllove/Nexus/releases/download/%1/%2")
        .arg(version, name);
}

static QByteArray releaseData(const QString &version)
{
    return QJsonDocument(QJsonObject{
        {"tag_name", version},
        {"body", "Test release notes"},
        {"assets", QJsonArray{QJsonObject{
            {"name", QUrl(installerUrl(version)).fileName()},
            {"browser_download_url", installerUrl(version)}
        }}}
    }).toJson();
}

class UpdateServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationVersion("1.0.7");
    }

    void automaticChecksUseTwoHourTimer()
    {
        auto &service = UpdateService::instance();
        auto *timer = service.findChild<QTimer *>("updateCheckTimer");
        QVERIFY2(timer, "UpdateService must own a repeating update timer");
        QCOMPARE(timer->interval(), 2 * 60 * 60 * 1000);
        QVERIFY(!timer->isSingleShot());
        QCOMPARE(timer->timerType(), Qt::PreciseTimer);
    }

    void enablingChecksStartsOnceAndTimerRepeats()
    {
        TestNetwork network;
        UpdateService service(&network);
        auto *timer = service.findChild<QTimer *>("updateCheckTimer");
        QVERIFY(timer);
        QVERIFY(!timer->isActive());
        service.setAutomaticChecksEnabled(true);
        QTRY_COMPARE(network.requests.size(), 1);
        QCOMPARE(network.requests.first().transferTimeout(), 30000);
        network.reply->finish(releaseData("v1.0.7"));
        service.setAutomaticChecksEnabled(true);
        QCoreApplication::processEvents();
        QCOMPARE(network.requests.size(), 1);
        timer->setInterval(20);
        QTRY_COMPARE(network.requests.size(), 2);
        network.reply->finish(releaseData("v1.0.7"));
        QTRY_COMPARE(network.requests.size(), 3);
        service.setAutomaticChecksEnabled(false);
        QVERIFY(!timer->isActive());
        network.reply->finish(releaseData("v1.0.7"));
        QTest::qWait(60);
        QCOMPARE(network.requests.size(), 3);
    }

    void disablingDuringCheckSuppressesAutomaticPromptButManualStillWorks()
    {
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy available(&service, &UpdateService::updateAvailable);
        service.setAutomaticChecksEnabled(true);
        QTRY_COMPARE(network.requests.size(), 1);
        service.setAutomaticChecksEnabled(false);
        network.reply->finish(releaseData("v1.0.8"));
        QCOMPARE(available.count(), 0);
        service.checkForUpdate();
        QCOMPARE(network.requests.size(), 2);
        network.reply->finish(releaseData("v1.0.8"));
        QCOMPARE(available.count(), 1);
    }

    void cancellingStartupBeforeEventLoopPreventsRequest()
    {
        TestNetwork network;
        UpdateService service(&network);
        service.setAutomaticChecksEnabled(true);
        service.setAutomaticChecksEnabled(false);
        QCoreApplication::processEvents();
        QVERIFY(network.requests.isEmpty());
    }

    void duplicateChecksAreCoalesced()
    {
        TestNetwork network;
        UpdateService service(&network);
        service.checkForUpdate();
        QVERIFY(service.isBusy());
        service.checkForUpdate();
        service.downloadAndInstall(installerUrl());
        QCOMPARE(network.requests.size(), 1);
        network.reply->finish(releaseData("v1.0.7"));
        QVERIFY(!service.isBusy());
        service.checkForUpdate();
        QCOMPARE(network.requests.size(), 2);
    }

    void failureReleasesGuardAndNextTimerRetries()
    {
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy errors(&service, &UpdateService::error);
        service.setAutomaticChecksEnabled(true);
        QTRY_COMPARE(network.requests.size(), 1);
        QTest::ignoreMessage(QtWarningMsg, "Update: Update check failed: simulated failure");
        network.reply->finish({}, QNetworkReply::TimeoutError);
        QCOMPARE(errors.count(), 1);
        QVERIFY(!service.isBusy());
        auto *timer = service.findChild<QTimer *>("updateCheckTimer");
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout"));
        QCOMPARE(network.requests.size(), 2);
    }

    void backgroundPromptsOncePerVersionButManualCanRetry()
    {
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy available(&service, &UpdateService::updateAvailable);
        service.setAutomaticChecksEnabled(true);
        QTRY_COMPARE(network.requests.size(), 1);
        network.reply->finish(releaseData("v1.0.8"));
        QCOMPARE(available.count(), 1);
        service.checkForUpdate(false);
        network.reply->finish(releaseData("v1.0.8"));
        QCOMPARE(available.count(), 1);
        service.checkForUpdate();
        network.reply->finish(releaseData("v1.0.8"));
        QCOMPARE(available.count(), 2);
        service.checkForUpdate(false);
        network.reply->finish(releaseData("v1.0.9"));
        QCOMPARE(available.count(), 3);
    }

    void upToDateFeedbackOnlyForManualCheck()
    {
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy latest(&service, &UpdateService::upToDate);
        QSignalSpy available(&service, &UpdateService::updateAvailable);
        service.checkForUpdate();
        network.reply->finish(releaseData("v1.0.6"));
        QCOMPARE(latest.count(), 1);
        QCOMPARE(available.count(), 0);
        service.setAutomaticChecksEnabled(true);
        QTRY_COMPARE(network.requests.size(), 2);
        network.reply->finish(releaseData("v1.0.7"));
        QCOMPARE(latest.count(), 1);
    }

    void invalidResponseRecovers_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QString>("message");
        QTest::newRow("bad-json") << QByteArray("not json") << "Invalid response from GitHub";
        QTest::newRow("bad-version") << QByteArray("{\"tag_name\":\"oops\"}")
                                    << "Invalid release or application version";
        QTest::newRow("missing-installer") << QByteArray("{\"tag_name\":\"v1.0.8\"}")
                                          << "No installer found in release v1.0.8";
    }

    void invalidResponseRecovers()
    {
        QFETCH(QByteArray, data);
        QFETCH(QString, message);
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy errors(&service, &UpdateService::error);
        service.checkForUpdate();
        QTest::ignoreMessage(QtWarningMsg, qPrintable("Update: " + message));
        network.reply->finish(data);
        QCOMPARE(errors.count(), 1);
        QVERIFY(!service.isBusy());
        service.checkForUpdate();
        QCOMPARE(network.requests.size(), 2);
    }

    void downloadIsExclusiveAndCompletedFileIsReused()
    {
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy downloaded(&service, &UpdateService::downloadFinished);
        service.downloadAndInstall(installerUrl());
        QVERIFY(service.isBusy());
        QCOMPARE(network.requests.first().transferTimeout(), 60000);
        service.downloadAndInstall(installerUrl());
        service.checkForUpdate();
        QCOMPARE(network.requests.size(), 1);
        const QByteArray payload("test installer payload - never executed");
        network.reply->finish(payload);
        QVERIFY(!service.isBusy());
        QCOMPARE(downloaded.count(), 1);
        const QString path = downloaded.first().first().toString();
#ifdef Q_OS_WIN
        QVERIFY(path.endsWith(".exe"));
#elif defined(Q_OS_MACOS)
        QVERIFY(path.endsWith(".dmg"));
#else
        QVERIFY(path.endsWith(".tar.gz"));
#endif
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), payload);
        file.close();
        service.downloadAndInstall(installerUrl());
        QCOMPARE(downloaded.count(), 2);
        QCOMPARE(network.requests.size(), 1);
        QVERIFY(QFile::remove(path));
        service.downloadAndInstall(installerUrl());
        QCOMPARE(network.requests.size(), 2);
    }

    void failedDownloadAllowsBackgroundRetry_data()
    {
        QTest::addColumn<bool>("networkFailure");
        QTest::newRow("network") << true;
        QTest::newRow("empty") << false;
    }

    void failedDownloadAllowsBackgroundRetry()
    {
        QFETCH(bool, networkFailure);
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy available(&service, &UpdateService::updateAvailable);
        QSignalSpy downloaded(&service, &UpdateService::downloadFinished);
        service.setAutomaticChecksEnabled(true);
        QTRY_COMPARE(network.requests.size(), 1);
        network.reply->finish(releaseData("v1.0.8"));
        service.downloadAndInstall(installerUrl());
        QTest::ignoreMessage(QtWarningMsg, networkFailure
            ? "Update: Download failed: simulated failure" : "Update: Downloaded installer is empty");
        network.reply->finish({}, networkFailure ? QNetworkReply::TimeoutError : QNetworkReply::NoError);
        QCOMPARE(downloaded.count(), 0);
        QVERIFY(!service.isBusy());
        service.checkForUpdate(false);
        network.reply->finish(releaseData("v1.0.8"));
        QCOMPARE(available.count(), 2);
    }

    void refusesNonReleaseDownload()
    {
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy errors(&service, &UpdateService::error);
        QTest::ignoreMessage(QtWarningMsg, "Update: Invalid Nexus installer download URL");
        service.downloadAndInstall("http://example.com/setup.exe");
        QCOMPARE(errors.count(), 1);
        QVERIFY(network.requests.isEmpty());
    }

    void refusesWrongPlatformOrArchitecture()
    {
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy available(&service, &UpdateService::updateAvailable);
        QSignalSpy errors(&service, &UpdateService::error);
        QJsonObject release = QJsonDocument::fromJson(releaseData("v1.0.8")).object();
        release["assets"] = QJsonArray{
            QJsonObject{{"name", "Nexus-Setup-v1.0.8-other.exe"},
                        {"browser_download_url", "https://github.com/jelllove/Nexus/releases/download/v1.0.8/Nexus-Setup-v1.0.8-other.exe"}}
        };
        service.checkForUpdate();
        QTest::ignoreMessage(QtWarningMsg, "Update: No installer found in release v1.0.8");
        network.reply->finish(QJsonDocument(release).toJson());
        QCOMPARE(available.count(), 0);
        QCOMPARE(errors.count(), 1);
    }

    void refusesWrongPackageBeforeDownloading()
    {
        TestNetwork network;
        UpdateService service(&network);
        QSignalSpy errors(&service, &UpdateService::error);
        QTest::ignoreMessage(QtWarningMsg, "Update: Invalid Nexus installer download URL");
        service.downloadAndInstall(
            "https://github.com/jelllove/Nexus/releases/download/v1.0.8/Nexus-Setup-v1.0.8-other.exe");
        QCOMPARE(errors.count(), 1);
        QVERIFY(network.requests.isEmpty());
    }
};

QTEST_GUILESS_MAIN(UpdateServiceTest)
#include "UpdateServiceTest.moc"
