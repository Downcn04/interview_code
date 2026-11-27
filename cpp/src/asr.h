#pragma once
#include "asrbackend.h"

#include <QString>
#include <QVector>
#include <QByteArray>

class QAudioSource;
class QIODevice;

class AsrEngine : public AsrBackend {
    Q_OBJECT
public:
    explicit AsrEngine(const QString &modelPath, QObject *parent = nullptr);
    ~AsrEngine() override;

    bool start() override;
    void stop() override;
    bool running() const override { return m_running; }

private:
    void handleAudio();
    void emitResult(bool final);

    QString m_modelPath;
    void *m_model = nullptr;
    void *m_recognizer = nullptr;
    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    bool m_running = false;

    int m_sourceRate = 16000;
    int m_sourceChannels = 1;
    QVector<float> m_mono;
    double m_resamplePos = 0.0;
};
