#include "evalrunner.h"

#include "aiclient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QStringList>
#include <QVector>

#include <cstdio>

EvalRunner::EvalRunner(const Config &cfg)
    : m_cfg(cfg)
{
}

QString EvalRunner::projectRoot()
{
    const QStringList dirs = {
        QDir::currentPath(),
        QCoreApplication::applicationDirPath(),
        QCoreApplication::applicationDirPath() + "/..",
        QCoreApplication::applicationDirPath() + "/../..",
    };
    for (const QString &d : dirs) {
        const QString clean = QDir::cleanPath(d);
        if (QFileInfo::exists(clean + "/eval/questions.jsonl"))
            return clean;
    }
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/..");
}

int EvalRunner::run(const QString &questionsPath)
{
    const QString root = projectRoot();
    const QString qPath = questionsPath.isEmpty() ? root + "/eval/questions.jsonl"
                                                  : questionsPath;

    QFile qf(qPath);
    if (!qf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::fprintf(stderr, "Cannot open questions: %s\n", qPath.toUtf8().constData());
        return 0;
    }

    QVector<QJsonObject> questions;
    while (!qf.atEnd()) {
        const QByteArray line = qf.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        if (o.value(QStringLiteral("question")).toString().trimmed().isEmpty())
            continue;
        questions.append(o);
    }
    qf.close();

    if (questions.isEmpty()) {
        std::fprintf(stderr, "No questions found.\n");
        return 0;
    }

    QDir().mkpath(root + "/eval/results");
    const QString runId = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString outPath = root + "/eval/results/" + runId + ".jsonl";

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        std::fprintf(stderr, "Cannot write results: %s\n", outPath.toUtf8().constData());
        return 0;
    }

    std::printf("Run %s: %d questions\n", runId.toUtf8().constData(), questions.size());

    int done = 0;
    for (int i = 0; i < questions.size(); ++i) {
        const QJsonObject q = questions.at(i);
        const QString text = q.value(QStringLiteral("question")).toString();

        AIClient ai(m_cfg.aiBaseUrl, m_cfg.aiApiKey, m_cfg.aiModel,
                    m_cfg.systemPrompt, m_cfg.historyTurns);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

        bool finished = false;
        QString reply;
        QString err;
        QObject::connect(&ai, &AIClient::answered, &loop, [&](const QString &r) {
            reply = r;
            finished = true;
            loop.quit();
        });
        QObject::connect(&ai, &AIClient::failed, &loop, [&](const QString &e) {
            err = e;
            finished = true;
            loop.quit();
        });

        QElapsedTimer elapsed;
        elapsed.start();
        ai.ask(text);
        if (!finished) {
            timer.start(60000);
            loop.exec();
        }
        const qint64 latencyMs = elapsed.elapsed();
        if (!finished)
            err = QStringLiteral("timeout");

        QJsonObject rec;
        rec.insert(QStringLiteral("run"), runId);
        rec.insert(QStringLiteral("ts"), QDateTime::currentDateTime().toString(Qt::ISODate));
        rec.insert(QStringLiteral("model"), m_cfg.aiModel);
        rec.insert(QStringLiteral("id"), q.contains(QStringLiteral("id"))
                                            ? q.value(QStringLiteral("id"))
                                            : QJsonValue(i + 1));
        rec.insert(QStringLiteral("tag"), q.value(QStringLiteral("tag")));
        rec.insert(QStringLiteral("question"), text);
        rec.insert(QStringLiteral("answer"), reply);
        rec.insert(QStringLiteral("error"), err);
        rec.insert(QStringLiteral("latencyMs"), static_cast<double>(latencyMs));
        rec.insert(QStringLiteral("usage"), ai.lastUsage());

        out.write(QJsonDocument(rec).toJson(QJsonDocument::Compact));
        out.write("\n");
        out.flush();
        ++done;

        std::printf("[%d/%d] %lld ms  %s\n", i + 1, questions.size(),
                    static_cast<long long>(latencyMs),
                    err.isEmpty() ? "ok" : err.toUtf8().constData());
    }

    out.close();
    std::printf("Results: %s\n", outPath.toUtf8().constData());

    generateDashboard();
    return done;
}

static QString extractJsonObject(const QString &text)
{
    const int a = text.indexOf(QLatin1Char('{'));
    const int b = text.lastIndexOf(QLatin1Char('}'));
    if (a >= 0 && b > a)
        return text.mid(a, b - a + 1);
    return QString();
}

int EvalRunner::judge(const QString &resultsPath)
{
    const QString root = projectRoot();

    QString path = resultsPath;
    if (path.isEmpty()) {
        const QDir dir(root + "/eval/results");
        const QStringList files = dir.entryList({QStringLiteral("*.jsonl")},
                                                QDir::Files, QDir::Name);
        QStringList base;
        for (const QString &f : files) {
            if (!f.endsWith(QStringLiteral(".judged.jsonl")))
                base.append(f);
        }
        if (base.isEmpty()) {
            std::fprintf(stderr, "No results to judge.\n");
            return 0;
        }
        path = dir.filePath(base.last());
    }

    const QString promptPath = root + "/eval/judge_prompt.txt";
    QFile pf(promptPath);
    if (!pf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::fprintf(stderr, "Cannot open judge prompt: %s\n", promptPath.toUtf8().constData());
        return 0;
    }
    const QString judgePrompt = QString::fromUtf8(pf.readAll()).trimmed();
    pf.close();
    if (judgePrompt.isEmpty()) {
        std::fprintf(stderr, "Judge prompt is empty.\n");
        return 0;
    }

    QFile in(path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::fprintf(stderr, "Cannot open results: %s\n", path.toUtf8().constData());
        return 0;
    }
    QVector<QJsonObject> items;
    while (!in.atEnd()) {
        const QByteArray line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        items.append(QJsonDocument::fromJson(line).object());
    }
    in.close();

    QString outPath = path;
    if (outPath.endsWith(QStringLiteral(".jsonl")))
        outPath.chop(6);
    outPath += QStringLiteral(".judged.jsonl");

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        std::fprintf(stderr, "Cannot write judged results: %s\n", outPath.toUtf8().constData());
        return 0;
    }

    static const char *const keys[5] = {"relevance", "factuality", "spoken_style",
                                        "length", "followup"};
    static const double weights[5] = {0.30, 0.35, 0.15, 0.10, 0.10};

    std::printf("Judge %s: %d items\n", path.toUtf8().constData(), items.size());
    int judged = 0;

    for (int i = 0; i < items.size(); ++i) {
        const QJsonObject it = items.at(i);
        const QString question = it.value(QStringLiteral("question")).toString();
        const QString answer = it.value(QStringLiteral("answer")).toString();
        const QString itemErr = it.value(QStringLiteral("error")).toString();

        QJsonObject rec;
        rec.insert(QStringLiteral("run"), it.value(QStringLiteral("run")));
        rec.insert(QStringLiteral("id"), it.value(QStringLiteral("id")));

        if (!itemErr.isEmpty() || answer.trimmed().isEmpty()) {
            rec.insert(QStringLiteral("judge_error"),
                       QStringLiteral("answer empty or item error"));
            out.write(QJsonDocument(rec).toJson(QJsonDocument::Compact));
            out.write("\n");
            std::printf("[%d/%d] skip (no answer)\n", i + 1, items.size());
            continue;
        }

        QString userContent = QStringLiteral("【面试问题】\n") + question;
        if (it.contains(QStringLiteral("tag")))
            userContent += QStringLiteral("\n\n【标签】") + it.value(QStringLiteral("tag")).toString();
        userContent += QStringLiteral("\n\n【候选回答】\n") + answer;

        QJsonObject judgeObj;
        QString judgeErr;
        for (int attempt = 0; attempt < 2 && judgeObj.isEmpty(); ++attempt) {
            AIClient ai(m_cfg.judgeBaseUrl, m_cfg.judgeApiKey, m_cfg.judgeModel,
                        judgePrompt, 0);
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

            bool finished = false;
            QString reply;
            QString err;
            QObject::connect(&ai, &AIClient::answered, &loop, [&](const QString &r) {
                reply = r;
                finished = true;
                loop.quit();
            });
            QObject::connect(&ai, &AIClient::failed, &loop, [&](const QString &e) {
                err = e;
                finished = true;
                loop.quit();
            });

            ai.ask(userContent);
            if (!finished) {
                timer.start(60000);
                loop.exec();
            }
            if (!finished)
                err = QStringLiteral("timeout");
            if (!err.isEmpty()) {
                judgeErr = err;
                continue;
            }

            const QString json = extractJsonObject(reply);
            if (json.isEmpty()) {
                judgeErr = QStringLiteral("no json");
                continue;
            }
            QJsonParseError pe;
            const QJsonDocument jd = QJsonDocument::fromJson(json.toUtf8(), &pe);
            if (pe.error != QJsonParseError::NoError || !jd.isObject()) {
                judgeErr = QStringLiteral("invalid json");
                continue;
            }
            judgeObj = jd.object();
        }

        if (judgeObj.isEmpty()) {
            rec.insert(QStringLiteral("judge_error"),
                       judgeErr.isEmpty() ? QStringLiteral("unknown") : judgeErr);
            out.write(QJsonDocument(rec).toJson(QJsonDocument::Compact));
            out.write("\n");
            std::printf("[%d/%d] judge error: %s\n", i + 1, items.size(),
                        judgeErr.toUtf8().constData());
            continue;
        }

        QJsonObject scores;
        double sum = 0.0;
        double wsum = 0.0;
        for (int k = 0; k < 5; ++k) {
            const QString key = QString::fromLatin1(keys[k]);
            QJsonObject dim = judgeObj.value(key).toObject();
            const QJsonValue sv = dim.value(QStringLiteral("score"));
            if (!sv.isDouble()) {
                scores.insert(key, QJsonValue());
                continue;
            }
            int s = sv.toInt();
            if (s < 1)
                s = 1;
            if (s > 5)
                s = 5;
            dim.insert(QStringLiteral("score"), s);
            scores.insert(key, dim);
            sum += s * weights[k];
            wsum += weights[k];
        }

        const double overall = wsum > 0.0 ? sum / wsum : 0.0;
        const int fact = scores.value(QStringLiteral("factuality")).toObject()
                             .value(QStringLiteral("score")).toInt();
        const bool pass = (overall >= 3.5 && fact >= 3);

        rec.insert(QStringLiteral("judge_model"), m_cfg.judgeModel);
        rec.insert(QStringLiteral("judged_at"),
                   QDateTime::currentDateTime().toString(Qt::ISODate));
        rec.insert(QStringLiteral("scores"), scores);
        rec.insert(QStringLiteral("overall"), overall);
        rec.insert(QStringLiteral("pass"), pass);
        rec.insert(QStringLiteral("summary"),
                   judgeObj.value(QStringLiteral("summary")).toString());

        out.write(QJsonDocument(rec).toJson(QJsonDocument::Compact));
        out.write("\n");
        out.flush();
        ++judged;

        std::printf("[%d/%d] overall=%.2f %s\n", i + 1, items.size(), overall,
                    pass ? "pass" : "fail");
    }

    out.close();
    std::printf("Judged: %s (%d)\n", outPath.toUtf8().constData(), judged);

    generateDashboard();
    return judged;
}

int EvalRunner::generateDashboard()
{
    const QString root = projectRoot();
    const QString tplPath = root + "/eval/dashboard_template.html";

    QFile tf(tplPath);
    if (!tf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::fprintf(stderr, "Cannot open dashboard template: %s\n", tplPath.toUtf8().constData());
        return 1;
    }
    QString html = QString::fromUtf8(tf.readAll());
    tf.close();

    const QDir resultsDir(root + "/eval/results");
    const QStringList files = resultsDir.entryList({QStringLiteral("*.jsonl")},
                                                   QDir::Files, QDir::Name);

    QMap<QString, QJsonObject> runMap;
    QStringList order;
    for (const QString &fn : files) {
        if (fn.endsWith(QStringLiteral(".judged.jsonl")))
            continue;
        QFile f(resultsDir.filePath(fn));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!f.atEnd()) {
            const QByteArray line = f.readLine().trimmed();
            if (line.isEmpty())
                continue;
            const QJsonObject o = QJsonDocument::fromJson(line).object();
            const QString runId = o.value(QStringLiteral("run")).toString(fn);
            if (!runMap.contains(runId)) {
                QJsonObject r;
                r.insert(QStringLiteral("run"), runId);
                r.insert(QStringLiteral("model"), o.value(QStringLiteral("model")));
                r.insert(QStringLiteral("items"), QJsonArray());
                runMap.insert(runId, r);
                order.append(runId);
            }
            QJsonObject r = runMap.value(runId);
            QJsonArray items = r.value(QStringLiteral("items")).toArray();
            items.append(o);
            r.insert(QStringLiteral("items"), items);
            runMap.insert(runId, r);
        }
        f.close();
    }

    const QStringList judgedFiles = resultsDir.entryList(
        {QStringLiteral("*.judged.jsonl")}, QDir::Files, QDir::Name);
    for (const QString &fn : judgedFiles) {
        QFile jf(resultsDir.filePath(fn));
        if (!jf.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!jf.atEnd()) {
            const QByteArray line = jf.readLine().trimmed();
            if (line.isEmpty())
                continue;
            const QJsonObject j = QJsonDocument::fromJson(line).object();
            const QString runId = j.value(QStringLiteral("run")).toString();
            if (!runMap.contains(runId))
                continue;
            QJsonObject r = runMap.value(runId);
            QJsonArray items = r.value(QStringLiteral("items")).toArray();
            for (int k = 0; k < items.size(); ++k) {
                QJsonObject it = items.at(k).toObject();
                if (it.value(QStringLiteral("id")) == j.value(QStringLiteral("id"))) {
                    it.insert(QStringLiteral("judge"), j);
                    items.replace(k, it);
                    break;
                }
            }
            r.insert(QStringLiteral("items"), items);
            runMap.insert(runId, r);
        }
        jf.close();
    }

    QJsonArray runs;
    for (int i = order.size() - 1; i >= 0; --i)
        runs.append(runMap.value(order.at(i)));

    const QString json = QString::fromUtf8(QJsonDocument(runs).toJson(QJsonDocument::Compact))
                             .replace(QStringLiteral("</"), QStringLiteral("<\\/"));
    html.replace(QStringLiteral("__RUNS_JSON__"), json);

    const QString outPath = root + "/eval/dashboard.html";
    QFile of(outPath);
    if (!of.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        std::fprintf(stderr, "Cannot write dashboard: %s\n", outPath.toUtf8().constData());
        return 1;
    }
    of.write(html.toUtf8());
    of.close();

    std::printf("Dashboard: %s\n", outPath.toUtf8().constData());
    return 0;
}
