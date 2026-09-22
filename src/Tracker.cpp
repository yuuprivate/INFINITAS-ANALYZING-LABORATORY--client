#include "Tracker.h"
#include "MusicTableReader.h"
#include "logger.h"

#include <fstream>
#include <iomanip>
#include <sstream>


bool Tracker::writeTsv(const std::string &filePath) const
{
    // バイナリモードまたはテキストモードでオープン
    std::ofstream file(filePath, std::ios::out | std::ios::binary | std::ios::trunc);

    if (!file.is_open())
    {
        LOG_ERROR("Failed to open tracker TSV: " + filePath);
        return false;
    }

    const unsigned char utf8Bom[] = {0xEF, 0xBB, 0xBF};
    file.write(reinterpret_cast<const char *>(utf8Bom), sizeof(utf8Bom));

    file
        << "song_id" << '\t'
//        << "title" << '\t'
        << "difficulty" << '\t'
        << "level" << '\t'
        << "play_type" << '\t'
        << "best_clear_lamp" << '\t'
        << "best_ex_score" << '\t'
        << "notes" << '\t'
        << "best_miss_count" << '\t'
        << '\n';

    for (const auto &[key, entry] : entries_)
    {
        const auto &[songId, difficulty, playType] = key;

        file
            << songId << '\t'
        //  << entry.title << '\t'
            << difficulty << '\t'
            << static_cast<int>(entry.rating) << '\t'
            << judgePlayTypeToInt(playType) << '\t'
            << static_cast<int>(entry.bestClearLamp) << '\t'
            << entry.bestExScore << '\t'
            << entry.notes << '\t'
            << '\n';
    }

    file.close();

    LOG_INFO("Tracker TSV (UTF-8) saved successfully: " + filePath);
    return true;
}

bool Tracker::update(const PlayResult &result)
{
    ChartKey key{result.songId, result.difficulty, result.playType};

    auto &entry = entries_[key];

    if (!result.title.empty())  entry.title = result.title;

    // ★ rating (難易度レベル: 1〜12) の更新
    if (result.rating > 0)
    {
        entry.rating = result.rating;
    }

    // ノーツ数更新
    if (result.notes > 0)
    {
        entry.notes = result.notes;
    }

    // クリアランプ更新 (自己ベスト更新時)
    if (result.clearLamp > entry.bestClearLamp)
    {
        entry.bestClearLamp = result.clearLamp;
    }

    // EXスコア更新
    if (result.exScore > entry.bestExScore)
    {
        entry.bestExScore = result.exScore;
    }

    // ミスカウント更新 (有効かつ最小値更新時)
    if (result.missCountValid)
    {
        if (!entry.bestMissCountValid || result.missCount < entry.bestMissCount)
        {
            entry.bestMissCount = result.missCount;
            entry.bestMissCountValid = true;
        }
    }

    entry.lastPlayType = result.playType;
    entry.lastPlayedAt = result.timestamp;

    return true;
}