#pragma once

#include "MemoryReader.h"
#include <cstdint>

struct PlayDataSnapshot
{
    std::int32_t songId = 0;
    std::int32_t rawDifficulty = 0;
    std::int32_t difficulty = 0;
    std::int32_t playType = 0;
    std::int32_t exScore = 0;
    std::int32_t pgreat = 0;
    std::int32_t great = 0;
    std::int32_t missCount = 0;
    std::int32_t clearLamp = 0;
};

class PlayDataReader
{
public:
    explicit PlayDataReader(const MemoryReader& memoryReader);

    // アドレス 0x25D9404 から全リザルトデータを一括取得
    bool read(std::uintptr_t baseAddress, PlayDataSnapshot& snapshot) const;

private:
    const MemoryReader& memoryReader_;
};