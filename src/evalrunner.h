#pragma once
#include <QString>

#include "config.h"

class EvalRunner {
public:
    explicit EvalRunner(const Config &cfg);

    int run(const QString &questionsPath = QString());
    int judge(const QString &resultsPath = QString());

    static int generateDashboard();
    static QString projectRoot();

private:
    Config m_cfg;
};
