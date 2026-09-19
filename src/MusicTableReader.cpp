#include "MusicTableReader.h"
#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
    template <typename T>
    std::string toHex(T value)
    {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << static_cast<std::uint64_t>(value);
        return oss.str();
    }
}

MusicTableReader::MusicTableReader(const MemoryReader &memoryReader, std::uintptr_t baseAddress, std::size_t moduleSize)
    : memoryReader_(memoryReader), baseAddress_(baseAddress), moduleSize_(moduleSize)
{
}

bool MusicTableReader::scanAndBuildMusicMap(std::uintptr_t searchStartRva, std::size_t scanSize)
{
    LOG_INFO("======== MUSIC TABLE MAP BUILDING START ========");

    if (baseAddress_ == 0 || moduleSize_ == 0)
    {
        LOG_ERROR("Invalid module information.");
        return false;
    }

    if (searchStartRva >= moduleSize_)
    {
        searchStartRva = 0;
    }
    const std::size_t actualScanSize = std::min(scanSize, moduleSize_ - searchStartRva);

    LOG_INFO("Reading memory for music map scan (RVA: 0x" + toHex(searchStartRva) +
             ", Size: " + std::to_string(actualScanSize) + " bytes)...");

    std::vector<std::uint8_t> buffer(actualScanSize);
    if (!memoryReader_.read(baseAddress_ + searchStartRva, buffer.data(), actualScanSize))
    {
        LOG_ERROR("Failed to read memory for music map scan.");
        return false;
    }

    musicMap_.clear();

    for (std::size_t i = kSongIdOffsetFromTitle; i <= actualScanSize - sizeof(std::int32_t); i += 4)
    {
        std::int32_t songId = 0;
        std::memcpy(&songId, &buffer[i], sizeof(std::int32_t));

        if (songId >= 1000 && songId <= 40000)
        {
            if (songId == 24080)
            {
                LOG_INFO("=== WIDE SURROUNDING DUMP FOR SONG_ID 24080 (Notes & Levels) ===");
                
                std::ptrdiff_t base = static_cast<std::ptrdiff_t>(i);
                std::ptrdiff_t startOffset = -650;
                std::ptrdiff_t endOffset = -480;
                
                if (base + startOffset >= 0 && base + endOffset <= static_cast<std::ptrdiff_t>(actualScanSize))
                {
                    std::ostringstream oss;
                    oss << "\n--- Offset range [-650 to -480] from song_id ---\n";
                    
                    for (std::ptrdiff_t off = startOffset; off <= endOffset; off += 2) // 2バイト刻みで細かく走査
                    {
                        std::int16_t val16 = 0;
                        std::int32_t val32 = 0;
                        
                        std::memcpy(&val16, &buffer[base + off], 2);
                        if (base + off + 4 <= static_cast<std::ptrdiff_t>(actualScanSize))
                        {
                            std::memcpy(&val32, &buffer[base + off], 4);
                        }
                        
                        // レベルの範囲（1〜12）に合致する値があれば目印をつける
                        bool isLevelRange = (val16 >= 1 && val16 <= 12);
                        
                        oss << "Rel [" << (off >= 0 ? "+" : "") << off << "] "
                            << "I16: " << std::setw(3) << val16 << (isLevelRange ? " *" : "  ")
                            << " | I32: " << std::setw(6) << val32 << "\n";
                    }
                    LOG_INFO(oss.str());
                }
            }

            const std::size_t titleIndex = i - kSongIdOffsetFromTitle;
            // ... (以降の処理はそのまま)
            const char *titlePtr = reinterpret_cast<const char *>(&buffer[titleIndex]);

            if (titlePtr[0] != '\0' && static_cast<unsigned char>(titlePtr[0]) >= 0x20)
            {
                std::size_t strLen = 0;
                bool isValidString = true;

                // 曲名の最大長を 48文字 に制限（隣のデータへのみこみを防止）
                while (strLen < 48 && titleIndex + strLen < actualScanSize)
                {
                    unsigned char c = static_cast<unsigned char>(titlePtr[strLen]);
                    if (c == '\0')
                        break;

                    // 制御文字、タブ、バックスラッシュ、その他の不審なバイナリが含まれている場合は即座に無効とする
                    if (c < 0x20 || c == '\t' || c == '\\' || c == 0x7F)
                    {
                        isValidString = false;
                        break;
                    }
                    ++strLen;
                }

                // 文字列長が 2文字以上 48文字未満 の場合のみ採用
                if (isValidString && strLen >= 2 && strLen < 48)
                {
                    std::string title(titlePtr, strLen);

                    if (musicMap_.find(songId) == musicMap_.end())
                    {
                        musicMap_[songId] = title;
                    }
                }
            }
        }
    }

    LOG_INFO("Successfully loaded " + std::to_string(musicMap_.size()) + " valid song title entries.");
    LOG_INFO("======== MUSIC TABLE MAP BUILDING END ========");

    return !musicMap_.empty();
}

std::string MusicTableReader::getSongTitle(std::int32_t songId) const
{
    auto it = musicMap_.find(songId);
    if (it != musicMap_.end())
    {
        return it->second;
    }
    return "ID:" + std::to_string(songId);
}

bool MusicTableReader::exportToTsv(const std::string &filePath) const
{
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        LOG_ERROR("Failed to open file for writing music map TSV: " + filePath);
        return false;
    }

    file << "song_id\ttitle\n";
    for (const auto &[songId, title] : musicMap_)
    {
        file << songId << '\t' << title << '\n';
    }

    file.close();
    LOG_INFO("Exported music map to: " + filePath);
    return true;
}

