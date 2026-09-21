#pragma once

#include "MemoryReader.h"
#include "ChartNotes.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class MusicTableReader
{
public:
    MusicTableReader(const MemoryReader &memoryReader, std::uintptr_t baseAddress, std::size_t moduleSize);

    bool scanAndBuildMusicMap(std::uintptr_t searchStartRva = 0x3000000, std::size_t scanSize = 0x1000000);

    std::string getTitle(std::int32_t songId) const;
    std::string getGenre(std::int32_t songId) const;
    std::string getArtist(std::int32_t songId) const;

    const std::unordered_map<std::int32_t, std::string> &getMusicMap() const { return musicMap_; }

    bool exportToTsv(const std::string &filePath) const;

    ChartNotes getChartNotes(std::int32_t songId) const;

    ChartRatings getChartRatings(std::int32_t songId) const
    {
        auto it = ratingsMap_.find(songId);
        if (it != ratingsMap_.end())
            return it->second;
        return ChartRatings{};
    }

    // 指定した Song ID の構造体内部を丸ごとダンプしてオフセットを検証するデバッグ関数
    void debugInspectSong(std::int32_t targetSongId) const;    

private:
    std::vector<std::uint8_t> cachedBuffer_;
    const MemoryReader &memoryReader_;
    std::uintptr_t baseAddress_;
    std::size_t moduleSize_;

    static constexpr std::intptr_t kSongIdOffsetFromTitle = 1200;

    std::unordered_map<std::int32_t, std::string> titleMap_;
    std::unordered_map<std::int32_t, std::string> genreMap_;
    std::unordered_map<std::int32_t, std::string> artistMap_;

    std::unordered_map<std::int32_t, std::string> musicMap_;
    std::unordered_map<std::int32_t, ChartNotes> notesMap_;
    std::unordered_map<std::int32_t, ChartRatings> ratingsMap_;
};