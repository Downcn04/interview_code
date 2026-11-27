#include "asr.h"

#include <QAudioSource>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

#include <cmath>
#include <cstring>

#include "vosk_api.h"

AsrEngine::AsrEngine(const QString &modelPath, QObject *parent)
    : AsrBackend(parent)
    , m_modelPath(modelPath)
{
}

AsrEngine::~AsrEngine()
{
    stop();
    if (m_model) {
        vosk_model_free(static_cast<VoskModel *>(m_model));
        m_model = nullptr;
    }
}

bool AsrEngine::start()
{
    if (m_running)
        return true;

    if (m_modelPath.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("未配置 VOSK_MODEL_PATH，无法进行离线识别"));
        return false;
    }

    if (!m_model) {
        m_model = vosk_model_new(m_modelPath.toUtf8().constData());
        if (!m_model) {
            emit errorOccurred(QStringLiteral("加载 Vosk 模型失败：%1").arg(m_modelPath));
            return false;
        }
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

    m_recognizer = vosk_recognizer_new(static_cast<VoskModel *>(m_model),
                                       static_cast<float>(16000));
    if (!m_recognizer) {
        emit errorOccurred(QStringLiteral("初始化识别器失败"));
        return false;
    }

    m_source = new QAudioSource(dev, fmt, this);
    m_io = m_source->start();
    if (!m_io) {
        emit errorOccurred(QStringLiteral("无法启动音频采集"));
        vosk_recognizer_free(static_cast<VoskRecognizer *>(m_recognizer));
        m_recognizer = nullptr;
        delete m_source;
        m_source = nullptr;
        return false;
    }

    connect(m_io, &QIODevice::readyRead, this, &AsrEngine::handleAudio);
    m_running = true;
    return true;
}

void AsrEngine::stop()
{
    if (m_io) {
        disconnect(m_io, &QIODevice::readyRead, this, &AsrEngine::handleAudio);
        m_io = nullptr;
    }
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
    if (m_recognizer) {
        emitResult(true);
        vosk_recognizer_free(static_cast<VoskRecognizer *>(m_recognizer));
        m_recognizer = nullptr;
    }
    m_running = false;
}

void AsrEngine::handleAudio()
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

    if (out.isEmpty() || !m_recognizer)
        return;

    if (vosk_recognizer_accept_waveform(static_cast<VoskRecognizer *>(m_recognizer),
                                        out.constData(), out.size())) {
        emitResult(false);
    }
}

void AsrEngine::emitResult(bool final)
{
    if (!m_recognizer)
        return;

    const char *json = final
        ? vosk_recognizer_final_result(static_cast<VoskRecognizer *>(m_recognizer))
        : vosk_recognizer_result(static_cast<VoskRecognizer *>(m_recognizer));

    if (!json)
        return;

    const QJsonObject obj = QJsonDocument::fromJson(QByteArray(json)).object();
    const QString text = obj.value("text").toString().trimmed();
    if (!text.isEmpty())
        emit textRecognized(text);
}
