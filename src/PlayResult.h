#pragma once

#include "JudgeData.h"
#include "PlayData.h"

#include <chrono>
#include <cstdint>
#include <string>

struct PlayResult
{
    std::int32_t songId = 0;
    std::int32_t difficulty = 0;
    std::int32_t clearLamp = 0;

    JudgePlayType playType =
        JudgePlayType::P1;

    std::int32_t pGreat = 0;
    std::int32_t great = 0;
    std::int32_t good = 0;
    std::int32_t bad = 0;
    std::int32_t poor = 0;

    std::int32_t comboBreak = 0;
    std::int32_t fast = 0;
    std::int32_t slow = 0;

    std::int32_t exScore = 0;
    std::int32_t missCount = 0;

    bool missCountValid = false;
    bool prematureEnd = false;

    std::chrono::system_clock::time_point timestamp{};
};

PlayResult createPlayResult(
    const JudgeSnapshot &judge,
    const PlayDataSnapshot &playData);

std::string judgePlayTypeToString(
    JudgePlayType playType);

std::string clearLampToString(
    std::int32_t clearLamp);