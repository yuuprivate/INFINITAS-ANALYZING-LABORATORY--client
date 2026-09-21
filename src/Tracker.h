#pragma once

#include "PlayResult.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

class MusicTableReader;

struct TrackerEntry
{
    std::string title;
    std::string genre;
    std::string artist;
    
    std::int32_t bestClearLamp = 0;
    std::int32_t bestExScore = 0;

    std::int32_t rating = 0;

    std::int32_t bestMissCount = 0;
    bool bestMissCountValid = false;

    JudgePlayType lastPlayType =
        JudgePlayType::P1;

    std::int32_t notes = 0;
    
    std::chrono::system_clock::time_point lastPlayedAt{};
};

class Tracker
{
public:
    bool update(
        const PlayResult &result);

    bool writeTsv(
        const std::string &filePath) const;

    bool appendPlayResultTsv(
        const std::string &filePath,
        const PlayResult &result,
        bool writeHeader) const;

private:
    using ChartKey = std::tuple<std::int32_t, std::int32_t, JudgePlayType>;

    std::map<
        ChartKey, TrackerEntry>
        entries_;
};
