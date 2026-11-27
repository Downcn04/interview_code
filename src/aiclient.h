#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QNetworkAccessManager;

class AIClient : public QObject {
    Q_OBJECT
public:
    AIClient(const QString &baseUrl, const QString &apiKey, const QString &model,
             const QString &systemPrompt, int historyTurns = 8,
             QObject *parent = nullptr);

    void ask(const QString &text);

signals:
    void answered(const QString &reply);
    void failed(const QString &error);

private:
    struct Turn {
        QString user;
        QString assistant;
    };

    void send(const QString &text);
    void startNext();
    void finishTurn(const QString &user, const QString &assistant);

    QNetworkAccessManager *m_nam;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
    QString m_systemPrompt;
    int m_maxTurns = 8;

    QVector<Turn> m_history;
    QStringList m_queue;
    bool m_busy = false;
};
