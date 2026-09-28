#pragma once

#include <cstdint>
#include <string>

enum class OffsetType
{
    SongList,
    DataMap,
    JudgeData,
    PlayData,
    PlaySettings,
    UnlockData,
    CurrentSong,
    PlayerProfile
};

struct OffsetCollection
{
    std::string version;

    std::uintptr_t songList = 0;
    std::uintptr_t dataMap = 0;
    std::uintptr_t judgeData = 0;
    std::uintptr_t playData = 0;
    std::uintptr_t playSettings = 0;
    std::uintptr_t unlockData = 0;
    std::uintptr_t currentSong = 0;
    std::uintptr_t playerProfile = 0;
};