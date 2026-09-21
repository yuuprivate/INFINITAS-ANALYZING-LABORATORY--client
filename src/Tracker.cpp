#include "Tracker.h"
#include "MusicTableReader.h"
#include "logger.h"

#include <fstream>
#include <iomanip>
#include <sstream>

// // ヘルパー関数: ChartNotes から該当難易度のノーツ数を取得
// static int getNotesFromChartNotes(const ChartNotes &notes, int difficulty, JudgePlayType playType)
// {
//     // 1 <= difficulty <= 5 : SP, 6 <= difficulty <= 9 : DP
//     const bool isDP = (difficulty >= 6) ||
//                       (playType == JudgePlayType::DP) ||
//                       (static_cast<int>(playType) == 2);

//     if (!isDP)
//     {
//         switch (difficulty)
//         {
//         case 1:
//             return notes.sp_beginner;
//         case 2:
//             return notes.sp_normal;
//         case 3:
//             return notes.sp_hyper;
//         case 4:
//             return notes.sp_another;
//         case 5:
//             return notes.sp_leggendaria;
//         default:
//             return 0;
//         }
//     }
//     else
//     {
//         switch (difficulty)
//         {
//         case 6:
//             return notes.dp_normal;
//         case 7:
//             return notes.dp_hyper;
//         case 8:
//             return notes.dp_another;
//         case 9:
//             return notes.dp_leggendaria;
//         default:
//             return 0;
//         }
//     }
// }

bool Tracker::writeTsv(const std::string &filePath) const
{
    // バイナリモードまたはテキストモードでオープン
    std::ofstream file(filePath, std::ios::out | std::ios::binary | std::ios::trunc);

    if (!file.is_open())
    {
        LOG_ERROR("Failed to open tracker TSV: " + filePath);
        return false;
    }

    // ★ Excel等の環境で文字化けしないよう、UTF-8のBOMを書き込む
    const unsigned char utf8Bom[] = {0xEF, 0xBB, 0xBF};
    file.write(reinterpret_cast<const char *>(utf8Bom), sizeof(utf8Bom));

    // ヘッダー出力 (title, genre, artist を追加)
    file
        << "song_id" << '\t'
        << "title" << '\t'
        << "genre" << '\t'
        << "artist" << '\t'
        << "difficulty" << '\t'
        << "level" << '\t'
        << "play_type" << '\t'
        << "best_clear_lamp" << '\t'
        << "best_clear_lamp_name" << '\t'
        << "best_ex_score" << '\t'
        << "notes" << '\t'
        << "best_miss_count" << '\t'
        << "best_miss_count_valid" << '\n';

    for (const auto &[key, entry] : entries_)
    {
        const auto &[songId, difficulty, playType] = key;

        file
            << songId << '\t'
            << entry.title << '\t'  // 保持している曲名
            << entry.genre << '\t'  // 保持しているジャンル
            << entry.artist << '\t' // 保持しているアーティスト
            << difficulty << '\t'
            << static_cast<int>(entry.rating) << '\t'
            << judgePlayTypeToString(playType) << '\t'
            << static_cast<int>(entry.bestClearLamp) << '\t'
            << clearLampToString(entry.bestClearLamp) << '\t'
            << entry.bestExScore << '\t'
            << entry.notes << '\t';

        if (entry.bestMissCountValid)
        {
            file << entry.bestMissCount;
        }
        else
        {
            file << '-';
        }

        file
            << '\t'
            << (entry.bestMissCountValid ? "true" : "false")
            << '\n';
    }

    file.close();

    LOG_INFO("Tracker TSV (UTF-8) saved successfully: " + filePath);
    return true;
}

// bool Tracker::appendPlayResultTsv(
//     const std::string &filePath,
//     const PlayResult &result,
//     bool writeHeader) const
// {
//     std::ofstream file(
//         filePath,
//         std::ios::out | (writeHeader ? std::ios::trunc : std::ios::app));

//     if (!file.is_open())
//     {
//         LOG_ERROR("Play result TSVを開けませんでした.");
//         return false;
//     }

//     if (writeHeader)
//     {
//         file

//             << '\n'
//             << "song_id"
//             << '\t'
//             << "difficulty"
//             << '\t'
//             << "play_type"
//             << '\t'
//             << "clear_lamp"
//             << '\t'
//             << "clear_lamp_name"
//             << '\t'
//             << "ex_score"
//             << '\t'
//             << "miss_count"
//             << '\t'
//             << "miss_count_valid"
//             << '\t'
//             << "notes"
//             << "\t"
//             << "premature_end";
//     }

//     file
//         << '\n'
//         << result.songId
//         << '\t'
//         << result.difficulty
//         << '\t'
//         << judgePlayTypeToString(result.playType)
//         << '\t'
//         << result.clearLamp
//         << '\t'
//         << clearLampToString(result.clearLamp)
//         << '\t'
//         << result.exScore
//         << '\t'
//         << result.missCount
//         << '\t'
//         << (result.missCountValid ? "true" : "false")
//         << '\t'
//         << result.notes
//         << '\t'
//         << (result.prematureEnd ? "true" : "false");

//     file.close();

//     LOG_INFO("Play result TSV saved successfully.");

//     return true;
// }

bool Tracker::update(const PlayResult &result)
{
    // ChartKey: std::tuple<std::int32_t, std::int32_t, JudgePlayType>
    ChartKey key{result.songId, result.difficulty, result.playType};

    auto &entry = entries_[key];

    if (!result.title.empty())  entry.title = result.title;
    if (!result.genre.empty())  entry.genre = result.genre;
    if (!result.artist.empty()) entry.artist = result.artist;

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