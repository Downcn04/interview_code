#pragma once
#include <QObject>
#include <QString>

class AsrBackend : public QObject {
    Q_OBJECT
public:
    explicit AsrBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~AsrBackend() override = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool running() const = 0;

signals:
    void textRecognized(const QString &text);
    void errorOccurred(const QString &error);
};
