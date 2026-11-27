#pragma once
#include <QString>

struct Config {
    QString aiBaseUrl;
    QString aiApiKey;
    QString aiModel;
    QString aliyunAppKey;
    QString aliyunAkId;
    QString aliyunAkSecret;
    QString aliyunRegion;
    QString aliyunToken;
    QString aliyunWsUrl;
    QString judgeBaseUrl;
    QString judgeApiKey;
    QString judgeModel;
    QString systemPrompt;
    int historyTurns = 8;
    int turnDebounceMs = 800;
    int turnMaxWaitMs = 6000;

    static Config load();
};
