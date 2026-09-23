#include "PlayResult.h"

#include <chrono>

namespace
{
    std::int32_t calculateExScore(
        const JudgeSnapshot &judge)
    {
        const std::int32_t pGreat =
            judge.p1Pgreat +
            judge.p2Pgreat;

        const std::int32_t great =
            judge.p1Great +
            judge.p2Great;

        return
            pGreat * 2 +
            great;
    }

    std::int32_t calculateMissCount(
        const JudgeSnapshot &judge)
    {
        return
            judge.p1Bad +
            judge.p1Poor +
            judge.p2Bad +
            judge.p2Poor;
    }

    bool isPrematureEnd(
        const JudgeSnapshot &judge)
    {
        return
            judge.p1MeasureEnd != 0 ||
            judge.p2MeasureEnd != 0;
    }

    std::int32_t normalizeClearLamp(
        const JudgeSnapshot &judge,
        std::int32_t clearLamp)
    {
        constexpr std::int32_t fullComboLamp = 7;
        constexpr std::int32_t perfectFullComboLamp = 8;

        const bool pfc =
            judge.p1Good +
                judge.p2Good +
                judge.p1Bad +
                judge.p2Bad +
                judge.p1Poor +
                judge.p2Poor ==
            0;

        if (
            clearLamp == fullComboLamp &&
            pfc)
        {
            return perfectFullComboLamp;
        }

        return clearLamp;
    }
}

PlayResult createPlayResult(
    const JudgeSnapshot &judge,
    const PlayDataSnapshot &playData)
{
    PlayResult result;

    result.songId =
        playData.songId;

    result.difficulty =
        playData.difficulty;

    result.clearLamp =
        normalizeClearLamp(
            judge,
            playData.clearLamp);

    result.playType =
        judge.playType;

    result.pGreat =
        judge.p1Pgreat +
        judge.p2Pgreat;

    result.great =
        judge.p1Great +
        judge.p2Great;

    result.good =
        judge.p1Good +
        judge.p2Good;

    result.bad =
        judge.p1Bad +
        judge.p2Bad;

    result.poor =
        judge.p1Poor +
        judge.p2Poor;

    result.comboBreak =
        judge.p1ComboBreak +
        judge.p2ComboBreak;

    result.fast =
        judge.p1Fast +
        judge.p2Fast;

    result.slow =
        judge.p1Slow +
        judge.p2Slow;

    result.exScore =
        calculateExScore(judge);

    result.missCount =
        calculateMissCount(judge);

    result.prematureEnd =
        isPrematureEnd(judge);

    result.missCountValid =
        !result.prematureEnd;

    result.timestamp =
        std::chrono::system_clock::now();

    return result;
}

std::string judgePlayTypeToInt(
    JudgePlayType playType)
{
    switch (playType)
    {
    case JudgePlayType::P1:
        return "0";

    case JudgePlayType::P2:
        return "0";

    case JudgePlayType::DP:
        return "1";
    }

    return "UNKNOWN";
}