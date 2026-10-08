#include "UpdateService.h"
#include "platform/UpdatePackage.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QTemporaryFile>
#include <QVersionNumber>
#include <QDebug>

UpdateService::UpdateService(QNetworkAccessManager *networkManager, QObject *parent)
    : QObject(parent)
    , m_nam(networkManager ? networkManager : new QNetworkAccessManager(this))
    , m_checkTimer(new QTimer(this))
{
    m_checkTimer->setObjectName("updateCheckTimer");
    m_checkTimer->setInterval(6 * 60 * 60 * 1000);
    m_checkTimer->setTimerType(Qt::PreciseTimer);
    connect(m_checkTimer, &QTimer::timeout, this, [this]() {
        checkForUpdate(false);
    });
    connect(this, &UpdateService::error, this, [](const QString &message) {
        qWarning().noquote() << "Update:" << message;
    });
}

UpdateService& UpdateService::instance()
{
    static UpdateService inst;
    return inst;
}

void UpdateService::setAutomaticChecksEnabled(bool enabled)
{
    if (enabled == m_checkTimer->isActive()) {
        return;
    }
    if (!enabled) {
        m_checkTimer->stop();
        return;
    }

    m_checkTimer->start();
    QTimer::singleShot(0, this, [this]() { checkForUpdate(false); });
}

bool UpdateService::isBusy() const
{
    return m_checkReply || m_downloadReply;
}

void UpdateService::checkForUpdate(bool manual)
{
    if (isBusy() || (!manual && !m_checkTimer->isActive())) {
        return;
    }
    QNetworkRequest request(QUrl("https://api.github.com/repos/jelllove/Nexus/releases/latest"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Nexus-Updater");
    request.setTransferTimeout(30000);

    QNetworkReply *reply = m_nam->get(request);
    m_checkReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual]() {
        m_checkReply.clear();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit error("Update check failed: " + reply->errorString());
            return;
        }
        if (!manual && !m_checkTimer->isActive()) {
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            emit error("Invalid response from GitHub");
            return;
        }

        QJsonObject release = doc.object();
        QString tagName = release["tag_name"].toString();       // e.g. "v1.1.0"
        QString releaseNotes = release["body"].toString();

        // Strip leading 'v' for version comparison
        QString remoteVersion = tagName;
        if (remoteVersion.startsWith('v') || remoteVersion.startsWith('V'))
            remoteVersion = remoteVersion.mid(1);

        QString localVersion = qApp->applicationVersion();

        QVersionNumber remote = QVersionNumber::fromString(remoteVersion);
        QVersionNumber local = QVersionNumber::fromString(localVersion);

        if (remote.isNull() || local.isNull()) {
            emit error("Invalid release or application version");
            return;
        }
        if (release["draft"].toBool() || release["prerelease"].toBool() || remote <= local) {
            if (manual) emit upToDate();
            return;
        }

        QString downloadUrl;
        QJsonArray assets = release["assets"].toArray();
        for (const QJsonValue &val : assets) {
            QJsonObject asset = val.toObject();
            QString name = asset["name"].toString();
            if (UpdatePackage::matchesAsset(name, UpdatePackage::currentPlatform(),
                                            UpdatePackage::currentArchitecture())) {
                downloadUrl = asset["browser_download_url"].toString();
                break;
            }
        }

        if (downloadUrl.isEmpty()) {
            emit error("No installer found in release " + tagName);
            return;
        }

        emit updateAvailable(tagName, downloadUrl, releaseNotes, manual);
    });
}

void UpdateService::downloadAndInstall(const QString &downloadUrl)
{
    if (isBusy()) {
        return;
    }
    if (downloadUrl == m_installerUrl && QFile::exists(m_installerPath)) {
        emit downloadFinished(m_installerPath);
        return;
    }
    QUrl url(downloadUrl);
    if (!url.isValid() || url.scheme() != "https" || url.host() != "github.com"
        || !url.path().startsWith("/jelllove/Nexus/releases/download/")
        || !UpdatePackage::matchesAsset(url.fileName(), UpdatePackage::currentPlatform(),
                                        UpdatePackage::currentArchitecture())) {
        emit error("Invalid Nexus installer download URL");
        return;
    }
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Nexus-Updater");
    request.setTransferTimeout(60000);
    // GitHub redirects asset downloads; follow redirects
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(request);
    m_downloadReply = reply;
    m_downloadCancelled = false;

    connect(reply, &QNetworkReply::downloadProgress,
            this, &UpdateService::downloadProgress);

    connect(reply, &QNetworkReply::finished, this, [this, reply, downloadUrl]() {
        m_downloadReply.clear();
        reply->deleteLater();

        if (m_downloadCancelled) {
            emit downloadCanceled();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit error("Download failed: " + reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();
        if (data.isEmpty()) {
            emit error("Downloaded installer is empty");
            return;
        }
        const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QTemporaryFile file(QDir(tempDir).filePath(
            "Nexus-Update-XXXXXX" + UpdatePackage::extension(UpdatePackage::currentPlatform())));
        if (!file.open()) {
            emit error("Failed to save installer: " + file.errorString());
            return;
        }
        if (file.write(data) != data.size() || !file.flush()) {
            emit error("Failed to write installer: " + file.errorString());
            return;
        }
        const QString installerPath = file.fileName();
        file.close();
        file.setAutoRemove(false);
        m_installerUrl = downloadUrl;
        m_installerPath = installerPath;
        emit downloadFinished(installerPath);
    });
}

void UpdateService::cancelDownload()
{
    if (m_downloadReply) {
        m_downloadCancelled = true;
        m_downloadReply->abort();
    }
}
