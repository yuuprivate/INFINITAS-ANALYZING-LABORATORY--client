#include "PlayData.h"
#include <array>
#include <cstring>

PlayDataReader::PlayDataReader(const MemoryReader& memoryReader)
    : memoryReader_(memoryReader)
{
}

bool PlayDataReader::read(std::uintptr_t baseAddress, PlayDataSnapshot& snapshot) const
{
    snapshot = {};

    // -0x04 〜 +0x1C までの 0x24 (36) バイトを一括で読み込み
    constexpr std::size_t READ_SIZE = 0x24;
    std::array<std::uint8_t, READ_SIZE> buffer{};

    // 0x25D9404 - 0x04 (= 0x25D9400) から読み込み開始
    if (!memoryReader_.read(baseAddress - 0x04, buffer.data(), buffer.size()))
    {
        return false;
    }

    // オフセットに基づくマッピング (-0x04 起点のためインデックス補正)
    std::memcpy(&snapshot.playType,      &buffer[0x00], sizeof(std::int32_t)); // 原点-0x04
    std::memcpy(&snapshot.songId,        &buffer[0x04], sizeof(std::int32_t)); // 原点+0x00
    std::memcpy(&snapshot.rawDifficulty, &buffer[0x08], sizeof(std::int32_t)); // 原点+0x04
    std::memcpy(&snapshot.exScore,       &buffer[0x0C], sizeof(std::int32_t)); // 原点+0x08
    std::memcpy(&snapshot.missCount,     &buffer[0x10], sizeof(std::int32_t)); // 原点+0x0C
    std::memcpy(&snapshot.pgreat,        &buffer[0x14], sizeof(std::int32_t)); // 原点+0x10
    std::memcpy(&snapshot.great,         &buffer[0x18], sizeof(std::int32_t)); // 原点+0x14
    std::memcpy(&snapshot.clearLamp,     &buffer[0x1C], sizeof(std::int32_t)); // 原点+0x18

    // 難易度を 0~4 の範囲に正規化 (DP難易度のオフセット対応)
    snapshot.difficulty = snapshot.rawDifficulty % 5;

    return true;
}