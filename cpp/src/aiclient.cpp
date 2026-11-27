#include "aiclient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

AIClient::AIClient(const QString &baseUrl, const QString &apiKey, const QString &model,
                   const QString &systemPrompt, int historyTurns, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_baseUrl(baseUrl)
    , m_apiKey(apiKey)
    , m_model(model)
    , m_systemPrompt(systemPrompt)
    , m_maxTurns(qBound(0, historyTurns, 50))
{
}

void AIClient::ask(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return;

    if (m_busy) {
        m_queue.append(t);
        while (m_queue.size() > 8)
            m_queue.removeFirst();
        return;
    }

    m_busy = true;
    send(t);
}

void AIClient::startNext()
{
    if (m_queue.isEmpty())
        return;
    const QString next = m_queue.takeFirst();
    m_busy = true;
    send(next);
}

void AIClient::finishTurn(const QString &user, const QString &assistant)
{
    if (m_maxTurns <= 0)
        return;
    m_history.append(Turn{user, assistant});
    while (m_history.size() > m_maxTurns)
        m_history.removeFirst();
}

void AIClient::send(const QString &text)
{
    if (m_apiKey.trimmed().isEmpty()) {
        const QString reply =
            QStringLiteral("（未配置 API Key，模拟回答）你刚才说：「%1」。").arg(text);
        finishTurn(text, reply);
        emit answered(reply);
        m_busy = false;
        startNext();
        return;
    }

    QJsonArray msgs;

    QJsonObject sys;
    sys.insert("role", "system");
    sys.insert("content", m_systemPrompt);
    msgs.append(sys);

    for (const Turn &turn : m_history) {
        QJsonObject u;
        u.insert("role", "user");
        u.insert("content", turn.user);
        msgs.append(u);

        QJsonObject a;
        a.insert("role", "assistant");
        a.insert("content", turn.assistant);
        msgs.append(a);
    }

    QJsonObject usr;
    usr.insert("role", "user");
    usr.insert("content", text);
    msgs.append(usr);

    QJsonObject body;
    body.insert("model", m_model);
    body.insert("messages", msgs);
    body.insert("temperature", 0.7);

    QNetworkRequest req{QUrl(m_baseUrl)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, text]() {
        reply->deleteLater();
        m_busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            emit failed(reply->errorString());
            startNext();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray choices = doc.object().value("choices").toArray();
        if (choices.isEmpty()) {
            emit failed(QStringLiteral("AI 返回为空"));
            startNext();
            return;
        }
        const QString content = choices.at(0).toObject()
                                    .value("message").toObject()
                                    .value("content").toString();
        finishTurn(text, content);
        emit answered(content);
        startNext();
    });
}
