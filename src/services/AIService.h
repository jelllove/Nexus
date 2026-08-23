#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>

class AIService : public QObject
{
    Q_OBJECT

public:
    static AIService& instance();

    void generateTitle(const QString &content);
    void summarizeContent(const QString &content);
    void summarizeOneLineForExport(
        const QString &content,
        std::function<void(const QString &summary)> onSuccess,
        std::function<void(const QString &errorMessage)> onFailure);
    bool isConfigured() const;
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
                         std::function<void(const QString &)> onSuccess,
                         std::function<void(const QString &)> onFailure = {});
    QNetworkRequest buildRequest(const QString &endpoint, const QString &apiKey);

    QNetworkAccessManager *m_networkManager;
};
