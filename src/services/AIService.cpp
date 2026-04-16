#include "AIService.h"
#include "db/DatabaseManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QRegularExpression>

AIService::AIService()
    : QObject(nullptr)
{
    m_networkManager = new QNetworkAccessManager(this);
}

AIService& AIService::instance()
{
    static AIService inst;
    return inst;
}

void AIService::generateTitle(const QString &content)
{
    auto &db = DatabaseManager::instance();
    QString endpoint = db.getSetting("ai_endpoint");
    QString apiKey = db.getSetting("ai_api_key");
    QString model = db.getSetting("ai_model", "gpt-4o-mini");

    if (endpoint.isEmpty() || apiKey.isEmpty()) {
        emit error("AI endpoint or API key not configured. Please check Settings.");
        return;
    }

    // Strip HTML for the prompt
    QString plainContent = content;
    plainContent.remove(QRegularExpression("<[^>]*>"));
    plainContent = plainContent.trimmed();

    if (plainContent.isEmpty()) {
        emit error("No content to generate title from.");
        return;
    }

    // Truncate if too long
    if (plainContent.length() > 2000) {
        plainContent = plainContent.left(2000) + "...";
    }

    // Build OpenAI-compatible request
    QJsonObject message;
    message["role"] = "user";
    message["content"] = QString(
        "Generate a concise, descriptive title (max 80 characters) for the following task content. "
        "Return ONLY the title text, nothing else.\n\n%1").arg(plainContent);

    QJsonArray messages;
    messages.append(message);

    QJsonObject requestBody;
    requestBody["model"] = model;
    requestBody["messages"] = messages;
    requestBody["max_tokens"] = 100;
    requestBody["temperature"] = 0.3;

    QUrl url(endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    QByteArray postData = QJsonDocument(requestBody).toJson();
    QNetworkReply *reply = m_networkManager->post(request, postData);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit error(QString("AI request failed: %1").arg(reply->errorString()));
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject obj = doc.object();

        // Parse OpenAI-compatible response
        QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices[0].toObject();
            QJsonObject messageObj = firstChoice["message"].toObject();
            QString title = messageObj["content"].toString().trimmed();
            // Remove quotes if the AI wrapped the title in them
            if (title.startsWith('"') && title.endsWith('"')) {
                title = title.mid(1, title.length() - 2);
            }
            if (!title.isEmpty()) {
                emit titleGenerated(title);
                return;
            }
        }

        emit error("Failed to parse AI response.");
    });
}
