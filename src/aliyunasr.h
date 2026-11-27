#pragma once
#include "asrbackend.h"

#include <QString>
#include <QVector>
#include <QByteArray>

class QAudioSource;
class QIODevice;
class QWebSocket;
class QJsonObject;

namespace AliyunToken {
QString fetch(const QString &akId, const QString &akSecret,
              const QString &region, QString *error, int timeoutMs = 8000);
}

class AliyunAsr : public AsrBackend {
    Q_OBJECT
public:
    AliyunAsr(const QString &appKey,
              const QString &akId,
              const QString &akSecret,
              const QString &region,
              const QString &token,
              const QString &wsUrl,
              QObject *parent = nullptr);
    ~AliyunAsr() override;

    bool start() override;
    void stop() override;
    bool running() const override { return m_running; }

private:
    void onConnected();
    void onTextMessage(const QString &message);
    void handleAudio();
    void sendAudio(const QByteArray &pcm);
    void sendJson(const QJsonObject &obj);

    QString m_appKey;
    QString m_akId;
    QString m_akSecret;
    QString m_region;
    QString m_token;
    QString m_wsUrl;

    QWebSocket *m_ws = nullptr;
    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;

    bool m_running = false;
    bool m_started = false;
    QString m_taskId;

    QByteArray m_pending;
    int m_sourceRate = 16000;
    int m_sourceChannels = 1;
    QVector<float> m_mono;
    double m_resamplePos = 0.0;
};
