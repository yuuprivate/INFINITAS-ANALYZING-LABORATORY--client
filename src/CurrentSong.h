#pragma once

#include "MemoryReader.h"

#include <cstdint>

struct CurrentSongSnapshot
{
    std::int32_t songId{0};
    std::int32_t difficulty{0};
    std::int32_t playType{0};
    std::int32_t exScore{0};
    std::int32_t missCount{0};
    std::int32_t clearLamp{0};

    bool operator==(const CurrentSongSnapshot &other) const
    {
        return songId == other.songId &&
               difficulty == other.difficulty &&
               playType == other.playType &&
               exScore == other.exScore &&
               missCount == other.missCount &&
               clearLamp == other.clearLamp;
    }

    bool operator!=(const CurrentSongSnapshot &other) const
    {
        return !(*this == other);
    }
};

class CurrentSongReader
{
public:
    explicit CurrentSongReader(const MemoryReader &memoryReader);

    bool read(std::uintptr_t address, CurrentSongSnapshot &snapshot) const;

    // 周辺メモリを細かくスキャンしてログ出力するデバッグ関数
    void debugDumpMemory(std::uintptr_t baseAddress, std::size_t rangeSize = 64) const;

private:
    const MemoryReader &memoryReader_;
};