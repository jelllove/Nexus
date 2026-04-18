#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class UpdateService : public QObject
{
    Q_OBJECT

public:
    static UpdateService& instance();

    void checkForUpdate();
    void downloadAndInstall(const QString &downloadUrl);

signals:
    void updateAvailable(const QString &latestVersion, const QString &downloadUrl, const QString &releaseNotes);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString &installerPath);
    void error(const QString &message);

private:
    UpdateService();
    QNetworkAccessManager *m_nam;
};
