#include "mainwindow.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QMouseEvent>
#include <QShowEvent>
#include <QTimer>
#include <QApplication>

#include "asrbackend.h"
#include "aliyunasr.h"
#include "turnassembler.h"
#include "aiclient.h"
#include "config.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(const Config &cfg, QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(540, 500);

    buildUi();

    m_asr = new AliyunAsr(cfg.aliyunAppKey, cfg.aliyunAkId, cfg.aliyunAkSecret,
                          cfg.aliyunRegion, cfg.aliyunToken, cfg.aliyunWsUrl, this);
    m_ai = new AIClient(cfg.aiBaseUrl, cfg.aiApiKey, cfg.aiModel, cfg.systemPrompt,
                        cfg.historyTurns, this);
    m_assembler = new TurnAssembler(cfg.turnDebounceMs, cfg.turnMaxWaitMs, this);

    connect(m_asr, &AsrBackend::textRecognized, this, [this](const QString &text) {
        appendLog(QStringLiteral("[识别] %1").arg(text), QStringLiteral("#7fd1ff"));
        m_assembler->push(text);
    });
    connect(m_assembler, &TurnAssembler::questionReady, this, [this](const QString &q) {
        appendLog(QStringLiteral("[提问] %1").arg(q), QStringLiteral("#c9b6ff"));
        m_ai->ask(q);
    });
    connect(m_asr, &AsrBackend::errorOccurred, this, [this](const QString &err) {
        appendLog(QStringLiteral("[错误] %1").arg(err), QStringLiteral("#ff6b6b"));
        setStatus(QStringLiteral("错误"), QStringLiteral("#ff6b6b"));
        if (m_listening) {
            m_listening = false;
            m_btn->setText(QStringLiteral("开始监听"));
        }
    });
    connect(m_ai, &AIClient::answered, this, [this](const QString &reply) {
        appendLog(QStringLiteral("[AI] %1").arg(reply), QStringLiteral("#a6f6a6"));
    });
    connect(m_ai, &AIClient::failed, this, [this](const QString &err) {
        appendLog(QStringLiteral("[AI 错误] %1").arg(err), QStringLiteral("#ff6b6b"));
    });
}

void MainWindow::buildUi()
{
    setStyleSheet(QStringLiteral(R"(
        #root {
            background-color: rgba(12, 12, 18, 70);
            border: 1px solid rgba(150, 150, 180, 38);
            border-radius: 16px;
        }
        QLabel#title { color: rgba(235, 235, 245, 210); font-size: 17px; font-weight: 600; }
        QLabel#subtitle { color: rgba(180, 180, 200, 115); font-size: 12px; }
        QPushButton#closeBtn {
            color: rgba(180, 180, 200, 120); background: transparent; border: none;
            font-size: 20px; font-weight: 500;
        }
        QPushButton#closeBtn:hover { color: #ff8080; background: rgba(255, 255, 255, 20); border-radius: 8px; }
        QPushButton#minBtn {
            color: rgba(180, 180, 200, 120); background: transparent; border: none;
            font-size: 18px; font-weight: 500;
        }
        QPushButton#minBtn:hover { color: rgba(235, 235, 245, 220); background: rgba(255, 255, 255, 20); border-radius: 8px; }
        QLabel#status { color: rgba(210, 210, 225, 175); font-size: 13px; }
        QPushButton#primary {
            color: rgba(255, 255, 255, 220);
            background: rgba(124, 92, 255, 100);
            border: 1px solid rgba(160, 140, 255, 70);
            border-radius: 10px; padding: 12px; font-size: 15px; font-weight: 600;
        }
        QPushButton#primary:hover { background: rgba(124, 92, 255, 140); }
        QPushButton#primary:pressed { background: rgba(124, 92, 255, 75); }
        QTextEdit#log {
            background: rgba(0, 0, 0, 45);
            border: 1px solid rgba(150, 150, 180, 30);
            border-radius: 10px;
            color: rgba(215, 215, 230, 190);
            font-size: 13px; padding: 8px;
        }
    )"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *root = new QFrame(this);
    root->setObjectName(QStringLiteral("root"));
    outer->addWidget(root);

    auto *lay = new QVBoxLayout(root);
    lay->setContentsMargins(18, 16, 18, 18);
    lay->setSpacing(12);

    auto *header = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("会议 AI 助手"), root);
    title->setObjectName(QStringLiteral("title"));
    m_minBtn = new QPushButton(QStringLiteral("\u2013"), root);
    m_minBtn->setObjectName(QStringLiteral("minBtn"));
    m_minBtn->setFixedSize(30, 30);
    m_minBtn->setCursor(Qt::PointingHandCursor);
    m_minBtn->setToolTip(QStringLiteral("最小化"));
    auto *closeBtn = new QPushButton(QStringLiteral("\u00d7"), root);
    closeBtn->setObjectName(QStringLiteral("closeBtn"));
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_minBtn);
    header->addWidget(closeBtn);
    lay->addLayout(header);

    connect(m_minBtn, &QPushButton::clicked, this, &MainWindow::toggleMinimize);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

    m_subtitle = new QLabel(QStringLiteral("共享屏幕时对观众不可见 · 仅本机可见"), root);
    m_subtitle->setObjectName(QStringLiteral("subtitle"));
    lay->addWidget(m_subtitle);

    m_status = new QLabel(root);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setTextFormat(Qt::RichText);
    lay->addWidget(m_status);
    setStatus(QStringLiteral("就绪"), QStringLiteral("#8b8b9e"));

    m_btn = new QPushButton(QStringLiteral("开始监听"), root);
    m_btn->setObjectName(QStringLiteral("primary"));
    m_btn->setCursor(Qt::PointingHandCursor);
    lay->addWidget(m_btn);
    connect(m_btn, &QPushButton::clicked, this, &MainWindow::toggleListening);

    m_log = new QTextEdit(root);
    m_log->setObjectName(QStringLiteral("log"));
    m_log->setReadOnly(true);
    lay->addWidget(m_log, 1);
}

void MainWindow::toggleListening()
{
    if (!m_listening) {
        if (m_asr->start()) {
            m_listening = true;
            m_btn->setText(QStringLiteral("停止监听"));
            setStatus(QStringLiteral("监听中…"), QStringLiteral("#5ad469"));
            appendLog(QStringLiteral("[系统] 已开始监听"), QStringLiteral("#8b8b9e"));
        }
    } else {
        m_asr->stop();
        m_listening = false;
        m_btn->setText(QStringLiteral("开始监听"));
        setStatus(QStringLiteral("已停止"), QStringLiteral("#8b8b9e"));
        appendLog(QStringLiteral("[系统] 已停止监听"), QStringLiteral("#8b8b9e"));
    }
}

void MainWindow::toggleMinimize()
{
    if (!m_collapsed) {
        m_expandedSize = size();
        m_subtitle->hide();
        m_status->hide();
        m_btn->hide();
        m_log->hide();
        m_collapsed = true;
        m_minBtn->setText(QStringLiteral("\u25A1"));
        m_minBtn->setToolTip(QStringLiteral("还原"));
        setFixedHeight(64);
    } else {
        m_subtitle->show();
        m_status->show();
        m_btn->show();
        m_log->show();
        m_collapsed = false;
        m_minBtn->setText(QStringLiteral("\u2013"));
        m_minBtn->setToolTip(QStringLiteral("最小化"));
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        resize(m_expandedSize);
    }
}

void MainWindow::appendLog(const QString &text, const QString &color)
{
    const QString safe = text.toHtmlEscaped();
    if (color.isEmpty())
        m_log->append(safe);
    else
        m_log->append(QStringLiteral("<span style='color:%1'>%2</span>").arg(color, safe));
}

void MainWindow::setStatus(const QString &text, const QString &color)
{
    m_status->setText(QStringLiteral("<span style='color:%1'>\u25cf</span> %2")
                          .arg(color, text.toHtmlEscaped()));
}

void MainWindow::applyUnshareable()
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowDisplayAffinity(hwnd, 0x00000011);
#endif
}

void MainWindow::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    applyUnshareable();
    QTimer::singleShot(0, this, [this]() { applyUnshareable(); });
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
    QWidget::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton)
        move(e->globalPosition().toPoint() - m_dragOffset);
    QWidget::mouseMoveEvent(e);
}
