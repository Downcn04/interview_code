#include "config.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QStringList>
#include <QProcessEnvironment>
#include <QCoreApplication>

static QMap<QString, QString> readDotEnv(const QString &path)
{
    QMap<QString, QString> m;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return m;
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        int eq = line.indexOf('=');
        if (eq <= 0)
            continue;
        QString k = line.left(eq).trimmed();
        QString v = line.mid(eq + 1).trimmed();
        if (v.size() >= 2 &&
            ((v.startsWith('"') && v.endsWith('"')) || (v.startsWith('\'') && v.endsWith('\''))))
            v = v.mid(1, v.size() - 2);
        m.insert(k, v);
    }
    return m;
}

static QString pick(const QProcessEnvironment &env,
                    const QMap<QString, QString> &dot,
                    const QString &key,
                    const QString &def)
{
    if (env.contains(key) && !env.value(key).isEmpty())
        return env.value(key);
    if (dot.contains(key) && !dot.value(key).isEmpty())
        return dot.value(key);
    return def;
}

Config Config::load()
{
    Config c;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/.env",
        appDir + "/../.env",
        appDir + "/../../.env",
        appDir + "/../../../.env",
        QDir::currentPath() + "/.env",
    };

    QMap<QString, QString> dot;
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p)) {
            dot = readDotEnv(p);
            if (!dot.isEmpty())
                break;
        }
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    c.aiBaseUrl = pick(env, dot, "AI_BASE_URL", "https://api.openai.com/v1/chat/completions");
    c.aiApiKey = pick(env, dot, "AI_API_KEY", "");
    c.aiModel = pick(env, dot, "AI_MODEL", "gpt-3.5-turbo");
    c.asrEngine = pick(env, dot, "ASR_ENGINE", "vosk");
    c.asrLanguage = pick(env, dot, "ASR_LANGUAGE", "zh-CN");
    c.voskModelPath = pick(env, dot, "VOSK_MODEL_PATH", "");
    c.aliyunAppKey = pick(env, dot, "ALIYUN_APPKEY", "");
    c.aliyunAkId = pick(env, dot, "ALIYUN_ACCESS_KEY_ID", "");
    c.aliyunAkSecret = pick(env, dot, "ALIYUN_ACCESS_KEY_SECRET", "");
    c.aliyunRegion = pick(env, dot, "ALIYUN_REGION", "cn-shanghai");
    c.aliyunToken = pick(env, dot, "ALIYUN_TOKEN", "");
    c.aliyunWsUrl = pick(env, dot, "ALIYUN_WS_URL",
                         "wss://nls-gateway-cn-shanghai.aliyuncs.com/ws/v1");
    c.systemPrompt = pick(env, dot, "SYSTEM_PROMPT",
                          "你是一个会议实时助手，请用简洁中文回答，直接给出要点。");

    const QStringList dirs = {
        appDir,
        appDir + "/..",
        appDir + "/../..",
        appDir + "/../../..",
        QDir::currentPath(),
    };
    auto findFile = [&dirs](const QStringList &names) -> QString {
        for (const QString &d : dirs) {
            for (const QString &name : names) {
                const QString p = QDir::cleanPath(d + "/" + name);
                if (QFileInfo::exists(p))
                    return p;
            }
        }
        return QString();
    };
    auto readText = [](const QString &p) -> QString {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        return QString::fromUtf8(f.readAll()).trimmed();
    };

    const QString promptPath = findFile({QStringLiteral("system_prompt.txt")});
    if (!promptPath.isEmpty()) {
        const QString text = readText(promptPath);
        if (!text.isEmpty())
            c.systemPrompt = text;
    }

    const QString resumePath = findFile({QStringLiteral("resume.md"),
                                         QStringLiteral("resume.txt")});
    if (!resumePath.isEmpty()) {
        const QString text = readText(resumePath);
        if (!text.isEmpty())
            c.systemPrompt += QStringLiteral(
                                  "\n\n【我的简历（事实依据，回答须与之一致，不得编造）】\n")
                              + text;
    }

    const QString jdPath = findFile({QStringLiteral("jd.md"), QStringLiteral("jd.txt")});
    if (!jdPath.isEmpty()) {
        const QString text = readText(jdPath);
        if (!text.isEmpty())
            c.systemPrompt += QStringLiteral("\n\n【目标岗位 JD（回答尽量贴合）】\n") + text;
    }

    c.historyTurns = pick(env, dot, "HISTORY_TURNS", "8").toInt();
    if (c.historyTurns < 0)
        c.historyTurns = 0;
    if (c.historyTurns > 50)
        c.historyTurns = 50;

    return c;
}
