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
    QString systemPrompt;
    int historyTurns = 8;

    static Config load();
};
