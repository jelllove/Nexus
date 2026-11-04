#pragma once

#include <QPointer>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>

class FakeAiServer : public QTcpServer
{
public:
    QByteArray body;
    QByteArray authorization;
    QString response = R"({"choices":[{"message":{"content":"# Weekly summary\nSynthetic result"}}]})";
    bool holdResponse = false;
    int statusCode = 200;
    int requests = 0;
    QPointer<QTcpSocket> pendingSocket;

    FakeAiServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            auto *socket = nextPendingConnection();
            auto data = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket, data]() {
                data->append(socket->readAll());
                const int boundary = data->indexOf("\r\n\r\n");
                if (boundary < 0 || socket->property("handled").toBool()) return;
                const auto headers = data->left(boundary);
                const auto match = QRegularExpression("Content-Length: (\\d+)", QRegularExpression::CaseInsensitiveOption)
                                       .match(QString::fromUtf8(headers));
                if (!match.hasMatch() || data->size() < boundary + 4 + match.captured(1).toInt()) return;
                socket->setProperty("handled", true);
                ++requests;
                for (const auto &line : headers.split('\n'))
                    if (line.trimmed().toLower().startsWith("authorization:")) authorization = line.trimmed();
                body = data->mid(boundary + 4);
                pendingSocket = socket;
                if (!holdResponse) sendResponse();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
    }

    void sendResponse()
    {
        if (!pendingSocket || pendingSocket->state() != QAbstractSocket::ConnectedState) return;
        const auto bytes = response.toUtf8();
        pendingSocket->write("HTTP/1.1 " + QByteArray::number(statusCode)
                             + " Test response\r\nContent-Type: application/json\r\nContent-Length: "
                             + QByteArray::number(bytes.size()) + "\r\nConnection: close\r\n\r\n" + bytes);
        pendingSocket->disconnectFromHost();
    }
};
