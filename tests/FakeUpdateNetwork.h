#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
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
        if (isFinished()) return;
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

inline QString installerUrl(const QString &version = "v1.0.8")
{
    return QString("https://github.com/jelllove/Nexus/releases/download/%1/Nexus-Setup-%1-x64.exe")
        .arg(version);
}

inline QByteArray releaseData(const QString &version)
{
    return QJsonDocument(QJsonObject{
        {"tag_name", version},
        {"body", "Test release notes"},
        {"assets", QJsonArray{QJsonObject{
            {"name", "Nexus-Setup-" + version + "-x64.exe"},
            {"browser_download_url", installerUrl(version)}
        }}}
    }).toJson();
}
