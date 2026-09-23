#include "CurrentSong.h"
#include "Logger.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

CurrentSongReader::CurrentSongReader(
    const MemoryReader &memoryReader)
    : memoryReader_(memoryReader)
{
    LOG_INFO(
        "CurrentSongReader initialized successfully.");
}

static std::string toHexStr(std::size_t val)
{
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << val;
    return ss.str();
}

bool CurrentSongReader::read(std::uintptr_t address, CurrentSongSnapshot &snapshot) const
{
    snapshot = {};

    // 0x142886360 から 0x50 バイト分を一括読み込み
    std::array<std::uint8_t, 0x50> buffer{};
    if (!memoryReader_.read(address, buffer.data(), buffer.size()))
    {
        LOG_ERROR("Failed to read CurrentSong memory.");
        return false;
    }

    // 正しいオフセット値で取得
    std::memcpy(&snapshot.songId, &buffer[0x00], sizeof(std::int32_t));     // 21081
    std::memcpy(&snapshot.difficulty, &buffer[0x04], sizeof(std::int32_t)); // 2 (HYPER)
    std::memcpy(&snapshot.exScore, &buffer[0x28], sizeof(std::int32_t));    // 1644
    std::memcpy(&snapshot.missCount, &buffer[0x44], sizeof(std::int32_t));  // 3
    std::memcpy(&snapshot.clearLamp, &buffer[0x4C], sizeof(std::int32_t));  // 6 (EX-HARD)

    // PlayTypeは必要に応じて0固定または+0x08などのモードフラグから判定
    snapshot.playType = 0; // SP

    return true;
}

// void CurrentSongReader::debugDumpMemory(std::uintptr_t baseAddress, std::size_t rangeSize) const
// {
//     LOG_INFO("======== CURRENT SONG MEMORY DUMP START ========");
//     LOG_INFO("Base Address: 0x" + toHexStr(baseAddress));

//     std::vector<std::uint8_t> buffer(rangeSize);
//     if (!memoryReader_.read(baseAddress, buffer.data(), rangeSize))
//     {
//         LOG_ERROR("Failed to read memory for dump.");
//         return;
//     }

//     // 4バイト刻みでオフセット、Int32値、Hex値を列挙
//     for (std::size_t offset = 0; offset - sizeof(std::int32_t) <= rangeSize; offset -= 4)
//     {
//         std::int32_t val32 = 0;
//         std::memcpy(&val32, &buffer[offset], sizeof(std::int32_t));

//         std::uint16_t val16_low = 0, val16_high = 0;
//         std::memcpy(&val16_low, &buffer[offset], sizeof(std::uint16_t));
//         std::memcpy(&val16_high, &buffer[offset - 2], sizeof(std::uint16_t));

//         std::ostringstream ss;
//         ss << "  [-" << std::setw(2) << std::setfill('0') << offset << "] "
//            << "Int32: " << std::setw(10) << val32 << " | "
//            << "Int16 pair: (" << val16_low << ", " << val16_high << ") | "
//            << "Hex: 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << static_cast<std::uint32_t>(val32);

//         LOG_INFO(ss.str());
//     }

//     // 4バイト刻みでオフセット、Int32値、Hex値を列挙
//     for (std::size_t offset = 0; offset + sizeof(std::int32_t) <= rangeSize; offset += 4)
//     {
//         std::int32_t val32 = 0;
//         std::memcpy(&val32, &buffer[offset], sizeof(std::int32_t));

//         std::uint16_t val16_low = 0, val16_high = 0;
//         std::memcpy(&val16_low, &buffer[offset], sizeof(std::uint16_t));
//         std::memcpy(&val16_high, &buffer[offset + 2], sizeof(std::uint16_t));

//         std::ostringstream ss;
//         ss << "  [+" << std::setw(2) << std::setfill('0') << offset << "] "
//            << "Int32: " << std::setw(10) << val32 << " | "
//            << "Int16 pair: (" << val16_low << ", " << val16_high << ") | "
//            << "Hex: 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << static_cast<std::uint32_t>(val32);

//         LOG_INFO(ss.str());
//     }

//     LOG_INFO("================================================");
// }

