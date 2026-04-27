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
    void summarizeContent(const QString &content);
    void verifyConnection(const QString &endpoint, const QString &apiKey, const QString &model);

signals:
    void titleGenerated(const QString &title);
    void summaryGenerated(const QString &summary);
    void verifyResult(bool success, const QString &message);
    void error(const QString &message);

private:
    AIService();
    void sendChatRequest(const QString &systemPrompt, const QString &userPrompt,
                         int maxTokens, double temperature,
                         std::function<void(const QString &)> onSuccess);
    QNetworkRequest buildRequest(const QString &endpoint, const QString &apiKey);

    QNetworkAccessManager *m_networkManager;
};
