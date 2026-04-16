#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class AIService : public QObject
{
    Q_OBJECT

public:
    static AIService& instance();

    void generateTitle(const QString &content);

signals:
    void titleGenerated(const QString &title);
    void error(const QString &message);

private:
    AIService();
    QNetworkAccessManager *m_networkManager;
};
