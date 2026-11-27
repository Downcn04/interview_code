#include <QApplication>
#include <QFont>
#include <QStringList>
#include <QWebSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <cstdio>

#include "config.h"
#include "mainwindow.h"
#include "aliyunasr.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MeetingAI"));
    app.setApplicationDisplayName(QStringLiteral("会议 AI 助手"));

    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(10);
    app.setFont(font);

    const Config cfg = Config::load();

    const QStringList args = QApplication::arguments();
    if (args.contains(QStringLiteral("--test-token"))) {
        QString err;
        const QString token = AliyunToken::fetch(cfg.aliyunAkId, cfg.aliyunAkSecret,
                                                 cfg.aliyunRegion, &err);
        if (token.isEmpty()) {
            std::fprintf(stderr, "TOKEN FAIL: %s\n", err.toUtf8().constData());
            return 1;
        }
        std::printf("TOKEN OK: length=%d\n", token.size());
        return 0;
    }

    if (args.contains(QStringLiteral("--test-asr"))) {
        QString err;
        const QString token = AliyunToken::fetch(cfg.aliyunAkId, cfg.aliyunAkSecret,
                                                 cfg.aliyunRegion, &err);
        if (token.isEmpty()) {
            std::fprintf(stderr, "TOKEN FAIL: %s\n", err.toUtf8().constData());
            return 1;
        }

        QWebSocket ws;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        bool started = false;

        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&ws, &QWebSocket::connected, &loop, [&]() {
            const QJsonObject header{
                {QStringLiteral("appkey"), cfg.aliyunAppKey},
                {QStringLiteral("message_id"),
                 QUuid::createUuid().toString(QUuid::Id128)},
                {QStringLiteral("task_id"),
                 QUuid::createUuid().toString(QUuid::Id128)},
                {QStringLiteral("namespace"), QStringLiteral("SpeechTranscriber")},
                {QStringLiteral("name"), QStringLiteral("StartTranscription")}};
            const QJsonObject payload{{QStringLiteral("format"), QStringLiteral("pcm")},
                                      {QStringLiteral("sample_rate"), 16000}};
            const QJsonObject root{{QStringLiteral("header"), header},
                                   {QStringLiteral("payload"), payload}};
            ws.sendTextMessage(QString::fromUtf8(
                QJsonDocument(root).toJson(QJsonDocument::Compact)));
        });
        QObject::connect(&ws, &QWebSocket::textMessageReceived, &loop,
                         [&](const QString &message) {
            const QJsonObject obj = QJsonDocument::fromJson(message.toUtf8()).object();
            const QString name = obj.value(QStringLiteral("header")).toObject()
                                     .value(QStringLiteral("name")).toString();
            if (name == QStringLiteral("TranscriptionStarted")) {
                started = true;
                loop.quit();
            } else if (name == QStringLiteral("TaskFailed")) {
                std::fprintf(stderr, "ASR FAIL: %s\n", message.toUtf8().constData());
                loop.quit();
            }
        });
        QObject::connect(&ws, &QWebSocket::errorOccurred, &loop,
                         [&](QAbstractSocket::SocketError) {
            std::fprintf(stderr, "WS ERROR: %s\n", ws.errorString().toUtf8().constData());
            loop.quit();
        });

        QUrl url(cfg.aliyunWsUrl);
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("token"), token);
        url.setQuery(q);
        ws.open(url);
        timer.start(10000);
        loop.exec();

        if (started) {
            std::printf("ASR OK: AppKey 与实时服务可用\n");
            return 0;
        }
        return 1;
    }

    MainWindow w(cfg);
    w.show();

    return app.exec();
}
