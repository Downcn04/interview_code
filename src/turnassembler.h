#pragma once
#include <QObject>
#include <QString>
#include <QTimer>

class TurnAssembler : public QObject {
    Q_OBJECT
public:
    TurnAssembler(int debounceMs, int maxWaitMs, QObject *parent = nullptr);

    void push(const QString &finalText);
    void flush();

signals:
    void questionReady(const QString &text);

private:
    void onDebounce();
    void emitAndClear();

    int m_debounceMs;
    int m_maxWaitMs;
    QTimer m_debounce;
    QTimer m_maxWait;
    QString m_buffer;
};
