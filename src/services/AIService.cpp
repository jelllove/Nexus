#include "AIService.h"
#include "db/DatabaseManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
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

QNetworkRequest AIService::buildRequest(const QString &endpoint, const QString &apiKey)
{
    QUrl url(endpoint);
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Auto-detect Azure OpenAI: endpoint contains ".openai.azure.com"
    if (endpoint.contains(".openai.azure.com", Qt::CaseInsensitive)) {
        // Azure OpenAI uses api-key header
        // Ensure api-version query param exists
        if (!endpoint.contains("api-version")) {
            QUrlQuery query(url);
            query.addQueryItem("api-version", "2024-02-01");
            url.setQuery(query);
        }
        request.setUrl(url);
        request.setRawHeader("api-key", apiKey.toUtf8());
    } else {
        // Standard OpenAI-compatible: Bearer token
        request.setUrl(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    }

    return request;
}

void AIService::sendChatRequest(const QString &systemPrompt, const QString &userPrompt,
                                int maxTokens, double temperature,
                                std::function<void(const QString &)> onSuccess,
                                std::function<void(const QString &)> onFailure)
{
    auto fail = [this, onFailure](const QString &message) {
        if (onFailure) {
            onFailure(message);
        } else {
            emit error(message);
        }
    };

    auto &db = DatabaseManager::instance();
    QString endpoint = db.getSetting("ai_endpoint");
    QString apiKey = db.getSetting("ai_api_key");
    QString model = db.getSetting("ai_model", "gpt-4o-mini");

    if (endpoint.isEmpty() || apiKey.isEmpty()) {
        fail("AI endpoint or API key not configured. Please check Settings.");
        return;
    }

    QJsonArray messages;

    if (!systemPrompt.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
        messages.append(sysMsg);
    }

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userPrompt;
    messages.append(userMsg);

    QJsonObject requestBody;
    requestBody["model"] = model;
    requestBody["messages"] = messages;
    requestBody["max_tokens"] = maxTokens;
    requestBody["temperature"] = temperature;

    QNetworkRequest request = buildRequest(endpoint, apiKey);
    QByteArray postData = QJsonDocument(requestBody).toJson();
    QNetworkReply *reply = m_networkManager->post(request, postData);

    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess, onFailure]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (onFailure) {
                onFailure(QString("AI request failed: %1").arg(reply->errorString()));
            } else {
                emit error(QString("AI request failed: %1").arg(reply->errorString()));
            }
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject obj = doc.object();

        QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices[0].toObject();
            QJsonObject messageObj = firstChoice["message"].toObject();
            QString result = messageObj["content"].toString().trimmed();
            if (!result.isEmpty()) {
                onSuccess(result);
                return;
            }
        }

        if (onFailure) {
            onFailure("Failed to parse AI response.");
        } else {
            emit error("Failed to parse AI response.");
        }
    });
}

void AIService::generateTitle(const QString &content)
{
    QString plainContent = content;
    plainContent.remove(QRegularExpression("<[^>]*>"));
    plainContent = plainContent.trimmed();

    if (plainContent.isEmpty()) {
        emit error("No content to generate title from.");
        return;
    }

    if (plainContent.length() > 2000) {
        plainContent = plainContent.left(2000) + "...";
    }

    sendChatRequest(
        QString(),
        QString("Generate a concise, descriptive title (max 80 characters) for the following task content. "
                "Return ONLY the title text, nothing else.\n\n%1").arg(plainContent),
        100, 0.3,
        [this](const QString &result) {
            QString title = result;
            if (title.startsWith('"') && title.endsWith('"')) {
                title = title.mid(1, title.length() - 2);
            }
            emit titleGenerated(title);
        }
    );
}

void AIService::summarizeContent(const QString &content)
{
    QString plainContent = content;
    plainContent.remove(QRegularExpression("<[^>]*>"));
    plainContent = plainContent.trimmed();

    if (plainContent.isEmpty()) {
        emit error("No content to summarize.");
        return;
    }

    if (plainContent.length() > 4000) {
        plainContent = plainContent.left(4000) + "...";
    }

    sendChatRequest(
        "You are a helpful assistant that summarizes task content concisely. "
        "Output a clear, structured summary in the same language as the input. "
        "Use bullet points if appropriate. Keep it under 200 words.",
        plainContent,
        500, 0.3,
        [this](const QString &result) {
            emit summaryGenerated(result);
        }
    );
}

void AIService::summarizeOneLineForExport(
    const QString &content,
    std::function<void(const QString &summary)> onSuccess,
    std::function<void(const QString &errorMessage)> onFailure)
{
    QString plainContent = content;
    plainContent.remove(QRegularExpression("<[^>]*>"));
    plainContent = plainContent.trimmed();

    if (plainContent.isEmpty()) {
        if (onSuccess) {
            onSuccess("📝 (empty)");
        }
        return;
    }

    if (plainContent.length() > 4000) {
        plainContent = plainContent.left(4000) + "...";
    }

    sendChatRequest(
        "Summarize the task in exactly one concise sentence in the same language as the input. "
        "Return plain text only, no bullets.",
        plainContent,
        120, 0.2,
        [onSuccess](const QString &result) {
            QString summary = result.simplified();
            if (summary.startsWith('"') && summary.endsWith('"') && summary.length() > 1) {
                summary = summary.mid(1, summary.length() - 2);
            }
            if (onSuccess) {
                onSuccess(summary);
            }
        },
        [onFailure](const QString &message) {
            if (onFailure) {
                onFailure(message);
            }
        }
    );
}

bool AIService::isConfigured() const
{
    auto &db = DatabaseManager::instance();
    return !db.getSetting("ai_endpoint").trimmed().isEmpty()
        && !db.getSetting("ai_api_key").trimmed().isEmpty();
}

void AIService::verifyConnection(const QString &endpoint, const QString &apiKey, const QString &model)
{
    if (endpoint.isEmpty() || apiKey.isEmpty()) {
        emit verifyResult(false, "Endpoint or API key is empty.");
        return;
    }

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = "Hello";
    messages.append(userMsg);

    QJsonObject requestBody;
    requestBody["model"] = model.isEmpty() ? "gpt-4o-mini" : model;
    requestBody["messages"] = messages;
    requestBody["max_tokens"] = 5;
    requestBody["temperature"] = 0.0;

    QNetworkRequest request = buildRequest(endpoint, apiKey);
    QByteArray postData = QJsonDocument(requestBody).toJson();
    QNetworkReply *reply = m_networkManager->post(request, postData);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit verifyResult(false, QString("Connection failed: %1").arg(reply->errorString()));
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject obj = doc.object();

        if (obj.contains("choices")) {
            emit verifyResult(true, "Connection successful!");
        } else if (obj.contains("error")) {
            QString errMsg = obj["error"].toObject()["message"].toString();
            emit verifyResult(false, QString("API error: %1").arg(errMsg));
        } else {
            emit verifyResult(false, "Unexpected response format.");
        }
    });
}
