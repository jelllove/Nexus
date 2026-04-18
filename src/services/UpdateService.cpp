#include "UpdateService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QVersionNumber>

UpdateService::UpdateService()
    : QObject(nullptr)
    , m_nam(new QNetworkAccessManager(this))
{
}

UpdateService& UpdateService::instance()
{
    static UpdateService inst;
    return inst;
}

void UpdateService::checkForUpdate()
{
    QNetworkRequest request(QUrl("https://api.github.com/repos/jelllove/Nexus/releases/latest"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Nexus-Updater");

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit error("Update check failed: " + reply->errorString());
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

        if (remote <= local) {
            // Already up to date
            return;
        }

        // Find the installer asset (Nexus-Setup-*.exe)
        QString downloadUrl;
        QJsonArray assets = release["assets"].toArray();
        for (const QJsonValue &val : assets) {
            QJsonObject asset = val.toObject();
            QString name = asset["name"].toString();
            if (name.contains("Nexus-Setup") && name.endsWith(".exe")) {
                downloadUrl = asset["browser_download_url"].toString();
                break;
            }
        }

        if (downloadUrl.isEmpty()) {
            emit error("No installer found in release " + tagName);
            return;
        }

        emit updateAvailable(tagName, downloadUrl, releaseNotes);
    });
}

void UpdateService::downloadAndInstall(const QString &downloadUrl)
{
    QUrl url(downloadUrl);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Nexus-Updater");
    // GitHub redirects asset downloads; follow redirects
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::downloadProgress,
            this, &UpdateService::downloadProgress);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit error("Download failed: " + reply->errorString());
            return;
        }

        // Save to temp directory
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString installerPath = tempDir + "/Nexus-Setup.exe";

        QFile file(installerPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit error("Failed to save installer: " + file.errorString());
            return;
        }

        file.write(reply->readAll());
        file.close();

        emit downloadFinished(installerPath);
    });
}
