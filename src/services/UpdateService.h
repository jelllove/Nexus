#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>

class UpdateService : public QObject
{
    Q_OBJECT

public:
    explicit UpdateService(QNetworkAccessManager *networkManager = nullptr, QObject *parent = nullptr);
    static UpdateService& instance();

    void setAutomaticChecksEnabled(bool enabled);
    void checkForUpdate(bool manual = true);
    void downloadAndInstall(const QString &downloadUrl);
    bool isBusy() const;

signals:
    void updateAvailable(const QString &latestVersion, const QString &downloadUrl, const QString &releaseNotes);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString &installerPath);
    void upToDate();
    void error(const QString &message);

private:
    QNetworkAccessManager *m_nam;
    QTimer *m_checkTimer;
    QPointer<QNetworkReply> m_checkReply;
    QPointer<QNetworkReply> m_downloadReply;
    QString m_notifiedVersion;
    QString m_installerUrl;
    QString m_installerPath;
};
