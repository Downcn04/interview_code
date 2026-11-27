#pragma once
#include <QWidget>
#include <QPoint>

class QLabel;
class QTextEdit;
class QPushButton;
class AsrBackend;
class AIClient;
class TurnAssembler;
struct Config;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(const Config &cfg, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;

private:
    void buildUi();
    void toggleListening();
    void toggleMinimize();
    void appendLog(const QString &text, const QString &color = QString());
    void setStatus(const QString &text, const QString &color);
    void applyUnshareable();

    AsrBackend *m_asr = nullptr;
    AIClient *m_ai = nullptr;
    TurnAssembler *m_assembler = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_status = nullptr;
    QTextEdit *m_log = nullptr;
    QPushButton *m_btn = nullptr;
    QPushButton *m_minBtn = nullptr;
    bool m_listening = false;
    bool m_collapsed = false;
    QSize m_expandedSize;
    QPoint m_dragOffset;
};
