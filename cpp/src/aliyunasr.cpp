#include "aliyunasr.h"

#include <QAudioSource>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>
#include <QUuid>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>

#include <cmath>
#include <cstring>

namespace {

QString gmtTimestamp()
{
    return QDateTime::currentDateTimeUtc()
        .toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss'Z'"));
}

QByteArray percentEncode(const QString &s)
{
    return QUrl::toPercentEncoding(s);
}

QString uuid()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

QJsonObject transcriptionHeader(const QString &appKey, const QString &name,
                                const QString &taskId)
{
    QJsonObject header;
    header.insert(QStringLiteral("appkey"), appKey);
    header.insert(QStringLiteral("message_id"), uuid());
    header.insert(QStringLiteral("task_id"), taskId);
    header.insert(QStringLiteral("namespace"), QStringLiteral("SpeechTranscriber"));
    header.insert(QStringLiteral("name"), name);
    return header;
}

} // namespace

QString AliyunToken::fetch(const QString &akId, const QString &akSecret,
                           const QString &region, QString *error, int timeoutMs)
{
    if (error)
        error->clear();
    if (akId.isEmpty() || akSecret.isEmpty()) {
        if (error)
            *error = QStringLiteral("缺少 AccessKey ID 或 Secret");
        return QString();
    }

    QMap<QString, QString> params;
    params.insert(QStringLiteral("AccessKeyId"), akId);
    params.insert(QStringLiteral("Action"), QStringLiteral("CreateToken"));
    params.insert(QStringLiteral("Format"), QStringLiteral("JSON"));
    params.insert(QStringLiteral("RegionId"), region.isEmpty() ? QStringLiteral("cn-shanghai") : region);
    params.insert(QStringLiteral("SignatureMethod"), QStringLiteral("HMAC-SHA1"));
    params.insert(QStringLiteral("SignatureVersion"), QStringLiteral("1.0"));
    params.insert(QStringLiteral("SignatureNonce"), uuid());
    params.insert(QStringLiteral("Timestamp"), gmtTimestamp());
    params.insert(QStringLiteral("Version"), QStringLiteral("2019-02-28"));

    QByteArray canonical;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (!canonical.isEmpty())
            canonical += '&';
        canonical += percentEncode(it.key());
        canonical += '=';
        canonical += percentEncode(it.value());
    }

    const QByteArray stringToSign = "GET&%2F&" + percentEncode(QString::fromLatin1(canonical));
    const QByteArray key = (akSecret + QStringLiteral("&")).toUtf8();
    const QByteArray signature =
        QMessageAuthenticationCode::hash(stringToSign, key, QCryptographicHash::Sha1).toBase64();

    QByteArray query = canonical;
    query += "&Signature=";
    query += percentEncode(QString::fromLatin1(signature));

    const QString url = QStringLiteral("https://nls-meta.cn-shanghai.aliyuncs.com/?")
                        + QString::fromLatin1(query);

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    QNetworkReply *reply = nam.get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (error)
            *error = QStringLiteral("获取 Token 超时");
        reply->deleteLater();
        return QString();
    }

    const QByteArray body = reply->readAll();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError && body.isEmpty()) {
        if (error)
            *error = netErrStr;
        return QString();
    }

    const QJsonObject obj = QJsonDocument::fromJson(body).object();
    QString token;
    if (obj.contains(QStringLiteral("Token"))) {
        token = obj.value(QStringLiteral("Token")).toObject()
                    .value(QStringLiteral("Id")).toString();
    } else {
        const QString code = obj.value(QStringLiteral("Code")).toString();
        const QString msg = obj.value(QStringLiteral("Message")).toString();
        if (error)
            *error = (code + QStringLiteral(" ") + msg).trimmed();
    }

    if (token.isEmpty() && error && error->isEmpty())
        *error = QStringLiteral("响应无效: ") + QString::fromUtf8(body.left(200));

    return token;
}

AliyunAsr::AliyunAsr(const QString &appKey,
                     const QString &akId,
                     const QString &akSecret,
                     const QString &region,
                     const QString &token,
                     const QString &wsUrl,
                     QObject *parent)
    : AsrBackend(parent)
    , m_appKey(appKey)
    , m_akId(akId)
    , m_akSecret(akSecret)
    , m_region(region.isEmpty() ? QStringLiteral("cn-shanghai") : region)
    , m_token(token)
    , m_wsUrl(wsUrl.isEmpty()
                  ? QStringLiteral("wss://nls-gateway-cn-shanghai.aliyuncs.com/ws/v1")
                  : wsUrl)
{
}

AliyunAsr::~AliyunAsr()
{
    stop();
}

bool AliyunAsr::start()
{
    if (m_running)
        return true;

    if (m_appKey.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("未配置 ALIYUN_APPKEY"));
        return false;
    }

    QString token = m_token;
    if (!m_akId.isEmpty() && !m_akSecret.isEmpty()) {
        QString err;
        const QString fetched = AliyunToken::fetch(m_akId, m_akSecret, m_region, &err);
        if (!fetched.isEmpty()) {
            token = fetched;
        } else if (token.isEmpty()) {
            emit errorOccurred(QStringLiteral("获取阿里云 Token 失败：%1").arg(err));
            return false;
        }
    }
    if (token.isEmpty()) {
        emit errorOccurred(QStringLiteral("未配置 ALIYUN_TOKEN 或 AccessKey"));
        return false;
    }

    const QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (dev.isNull()) {
        emit errorOccurred(QStringLiteral("未找到麦克风输入设备"));
        return false;
    }

    QAudioFormat fmt;
    fmt.setSampleRate(16000);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    if (!dev.isFormatSupported(fmt))
        fmt = dev.preferredFormat();
    if (fmt.sampleFormat() != QAudioFormat::Int16) {
        emit errorOccurred(QStringLiteral("当前录音设备不支持 16bit PCM 格式"));
        return false;
    }

    m_sourceRate = fmt.sampleRate();
    m_sourceChannels = fmt.channelCount();
    m_mono.clear();
    m_resamplePos = 0.0;
    m_pending.clear();
    m_taskId = uuid();

    m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    QUrl url(m_wsUrl);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("token"), token);
    url.setQuery(q);

    connect(m_ws, &QWebSocket::connected, this, &AliyunAsr::onConnected);
    connect(m_ws, &QWebSocket::textMessageReceived, this, &AliyunAsr::onTextMessage);
    connect(m_ws, &QWebSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                emit errorOccurred(QStringLiteral("阿里云连接错误：%1")
                                       .arg(m_ws ? m_ws->errorString() : QString()));
            });

    m_source = new QAudioSource(dev, fmt, this);
    m_io = m_source->start();
    if (!m_io) {
        emit errorOccurred(QStringLiteral("无法启动音频采集"));
        m_ws->deleteLater();
        m_ws = nullptr;
        m_source->deleteLater();
        m_source = nullptr;
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &AliyunAsr::handleAudio);

    m_running = true;
    m_ws->open(url);
    return true;
}

void AliyunAsr::stop()
{
    if (m_ws) {
        if (m_started)
            sendJson(QJsonObject{{QStringLiteral("header"),
                                  transcriptionHeader(m_appKey, QStringLiteral("StopTranscription"), m_taskId)},
                                 {QStringLiteral("payload"), QJsonObject{}}});
        m_ws->close();
        m_ws->deleteLater();
        m_ws = nullptr;
    }
    if (m_io) {
        disconnect(m_io, &QIODevice::readyRead, this, &AliyunAsr::handleAudio);
        m_io = nullptr;
    }
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
    m_running = false;
    m_started = false;
    m_pending.clear();
}

void AliyunAsr::sendJson(const QJsonObject &obj)
{
    if (m_ws && m_ws->isValid())
        m_ws->sendTextMessage(QString::fromUtf8(
            QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void AliyunAsr::onConnected()
{
    QJsonObject payload;
    payload.insert(QStringLiteral("format"), QStringLiteral("pcm"));
    payload.insert(QStringLiteral("sample_rate"), 16000);
    payload.insert(QStringLiteral("enable_intermediate_result"), true);
    payload.insert(QStringLiteral("enable_punctuation_prediction"), true);
    payload.insert(QStringLiteral("enable_inverse_text_normalization"), true);

    sendJson(QJsonObject{
        {QStringLiteral("header"), transcriptionHeader(m_appKey, QStringLiteral("StartTranscription"), m_taskId)},
        {QStringLiteral("payload"), payload}});
}

void AliyunAsr::onTextMessage(const QString &message)
{
    const QJsonObject obj = QJsonDocument::fromJson(message.toUtf8()).object();
    const QJsonObject header = obj.value(QStringLiteral("header")).toObject();
    const QJsonObject payload = obj.value(QStringLiteral("payload")).toObject();
    const QString name = header.value(QStringLiteral("name")).toString();

    if (name == QStringLiteral("TranscriptionStarted")) {
        m_started = true;
        if (!m_pending.isEmpty()) {
            const QByteArray buffered = m_pending;
            m_pending.clear();
            sendAudio(buffered);
        }
    } else if (name == QStringLiteral("SentenceEnd")
               || name == QStringLiteral("TranscriptionResultChanged")) {
        const QString text = payload.value(QStringLiteral("result")).toString().trimmed();
        if (!text.isEmpty() && name == QStringLiteral("SentenceEnd"))
            emit textRecognized(text);
    } else if (name == QStringLiteral("TaskFailed")) {
        const QString msg = payload.value(QStringLiteral("message")).toString();
        emit errorOccurred(QStringLiteral("阿里云识别失败：%1").arg(msg));
    } else if (name == QStringLiteral("TranscriptionCompleted")) {
        m_started = false;
    }
}

void AliyunAsr::handleAudio()
{
    if (!m_io)
        return;

    const QByteArray data = m_io->readAll();
    if (data.isEmpty())
        return;

    const int bytesPerSample = 2;
    const int frameBytes = bytesPerSample * m_sourceChannels;
    const int frames = data.size() / frameBytes;

    for (int i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < m_sourceChannels; ++c) {
            qint16 s = 0;
            std::memcpy(&s, data.constData() + i * frameBytes + c * bytesPerSample, 2);
            sum += static_cast<float>(s) / 32768.0f;
        }
        m_mono.append(sum / static_cast<float>(m_sourceChannels));
    }

    QByteArray out;
    auto pushSample = [&out](float f) {
        const float clamped = qBound(-1.0f, f, 1.0f);
        const qint16 v = static_cast<qint16>(std::lround(clamped * 32767.0f));
        out.append(reinterpret_cast<const char *>(&v), 2);
    };

    if (m_sourceRate == 16000) {
        for (float f : m_mono)
            pushSample(f);
        m_mono.clear();
    } else {
        const double ratio = 16000.0 / static_cast<double>(m_sourceRate);
        while (m_resamplePos + 1.0 < static_cast<double>(m_mono.size())) {
            const int i0 = static_cast<int>(std::floor(m_resamplePos));
            const double frac = m_resamplePos - i0;
            const float s = m_mono[i0] * (1.0f - static_cast<float>(frac))
                            + m_mono[i0 + 1] * static_cast<float>(frac);
            pushSample(s);
            m_resamplePos += ratio;
        }
        const int drop = static_cast<int>(std::floor(m_resamplePos));
        if (drop > 0) {
            m_mono.remove(0, drop);
            m_resamplePos -= drop;
        }
    }

    if (out.isEmpty())
        return;

    if (!m_started) {
        m_pending.append(out);
        const int cap = 16000 * 2 * 10;
        if (m_pending.size() > cap)
            m_pending = m_pending.right(cap);
    } else {
        sendAudio(out);
    }
}

void AliyunAsr::sendAudio(const QByteArray &pcm)
{
    if (!m_ws || !m_started || pcm.isEmpty())
        return;
    const int chunk = 3200; // 约 100ms @ 16k/16bit
    for (int i = 0; i < pcm.size(); i += chunk)
        m_ws->sendBinaryMessage(pcm.mid(i, chunk));
}
