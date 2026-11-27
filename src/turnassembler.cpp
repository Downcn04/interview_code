#include "turnassembler.h"

TurnAssembler::TurnAssembler(int debounceMs, int maxWaitMs, QObject *parent)
    : QObject(parent)
    , m_debounceMs(debounceMs)
    , m_maxWaitMs(maxWaitMs)
{
    m_debounce.setSingleShot(true);
    m_maxWait.setSingleShot(true);
    connect(&m_debounce, &QTimer::timeout, this, &TurnAssembler::onDebounce);
    connect(&m_maxWait, &QTimer::timeout, this, &TurnAssembler::emitAndClear);
}

void TurnAssembler::push(const QString &finalText)
{
    const QString t = finalText.trimmed();
    if (t.isEmpty())
        return;

    if (m_buffer.isEmpty()) {
        m_buffer = t;
        if (m_maxWaitMs > 0)
            m_maxWait.start(m_maxWaitMs);
    } else {
        m_buffer += QStringLiteral("，") + t;
    }

    if (m_debounceMs > 0)
        m_debounce.start(m_debounceMs);
    else
        onDebounce();
}

void TurnAssembler::flush()
{
    emitAndClear();
}

void TurnAssembler::onDebounce()
{
    emitAndClear();
}

void TurnAssembler::emitAndClear()
{
    m_debounce.stop();
    m_maxWait.stop();

    const QString text = m_buffer.trimmed();
    m_buffer.clear();
    if (!text.isEmpty())
        emit questionReady(text);
}
