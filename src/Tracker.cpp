#include "Tracker.h"
#include "Logger.h"
#include "MusicTableReader.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
    std::string formatTimestamp(
        const std::chrono::system_clock::time_point &timePoint)
    {
        const std::time_t time =
            std::chrono::system_clock::to_time_t(
                timePoint);

        std::tm localTime{};

        localtime_s(
            &localTime,
            &time);

        std::ostringstream stream;

        stream
            << std::put_time(
                   &localTime,
                   "%Y/%m/%d %H:%M:%S");

        return stream.str();
    }
}

bool Tracker::update(
    const PlayResult &result)
{
    if (result.songId <= 0)
    {
        LOG_ERROR(
            "Tracker update failed: invalid song ID.");

        return false;
    }

    if (
        result.difficulty < 0 ||
        result.difficulty > 9)
    {
        LOG_ERROR(
            "Tracker update failed: invalid difficulty.");

        return false;
    }

    const ChartKey key{
        result.songId,
        result.difficulty,
        result.playType};

    auto [iterator, inserted] = entries_.try_emplace(key);
    TrackerEntry &entry = iterator->second;

    if (inserted)
    {
        entry.bestClearLamp = result.clearLamp;
        entry.bestExScore = result.exScore;
        entry.lastPlayType = result.playType; // 初期化

        if (result.missCountValid)
        {
            entry.bestMissCount = result.missCount;
            entry.bestMissCountValid = true;
        }
    }
    else
    {
        /*
         * Refluxと同様、
         * Lampは数値の大きい方を残す。
         */
        if (
            result.clearLamp >
            entry.bestClearLamp)
        {
            entry.bestClearLamp =
                result.clearLamp;
        }

        /*
         * EXScoreは最高値を残す。
         */
        if (
            result.exScore >
            entry.bestExScore)
        {
            entry.bestExScore =
                result.exScore;
        }

        /*
         * Miss Countは最小値を残す。
         *
         * ただし有効なデータだけを対象にする。
         */
        if (result.missCountValid)
        {
            if (
                !entry.bestMissCountValid ||
                result.missCount <
                    entry.bestMissCount)
            {
                entry.bestMissCount =
                    result.missCount;

                entry.bestMissCountValid =
                    true;
            }
        }
    }

    entry.lastPlayType =
        result.playType;

    entry.lastPlayedAt =
        result.timestamp;

    return true;
}

bool Tracker::writeTsv(
    const std::string &filePath) const
{
    std::ofstream file(
        filePath,
        std::ios::out |
            std::ios::trunc);

    if (!file.is_open())
    {
        LOG_ERROR(
            "Tracker TSVを開けませんでした。");

        LOG_ERROR(
            "Failed to open tracker TSV.");

        return false;
    }

    /*
     * 修正後のC++版Tracker TSV（difficultyの後ろに notes を追加）。
     */
    file
        << "song_id"
        << '\t'
        << "difficulty"
        << '\t'
        << "notes" // difficulty の後ろに notes カラムを追加
        << '\t'
        << "play_type" // 1 <= difficulty <= 5 :SP, 6 <= difficulty <= 9 :DP (プロジェクトの仕様に準拠)
        << '\t'
        << "best_clear_lamp"
        << '\t'
        << "best_ex_score"
        << '\t'
        << "best_miss_count";

    for (const auto &[key, entry] : entries_)
    {
        const auto &[songId, difficulty, playType] = key;

        int notes = getNotesForChart(songId, difficulty, playType);

        file
            << songId
            << '\t'
            << difficulty
            << '\t'
            << (notes > 0 ? std::to_string(notes) : "-") // ノーツ数が出力できない場合は '-' や '0'
            << '\t'
            << judgePlayTypeToString(playType) // key の playType を出力
            << '\t'
            << entry.bestClearLamp
            << '\t'
            << clearLampToString(entry.bestClearLamp)
            << '\t'
            << entry.bestExScore
            << '\t';

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
            << '\t';

        if (entry.lastPlayedAt.time_since_epoch().count() != 0)
        {
            file << formatTimestamp(entry.lastPlayedAt);
        }

        file << '\n';
    }

    file.close();

    LOG_INFO(
        "Tracker TSV saved successfully.");

    LOG_INFO(
        "Tracker TSV: " +
        filePath);

    return true;
}

bool Tracker::appendPlayResultTsv(
    const std::string &filePath,
    const PlayResult &result,
    bool writeHeader) const
{
    std::ofstream file(
        filePath,
        std::ios::out |
            (writeHeader
                 ? std::ios::trunc
                 : std::ios::app));

    if (!file.is_open())
    {
        LOG_ERROR(
            "Play result TSVを開けませんでした.");

        return false;
    }

    if (writeHeader)
    {
        file
            << "song_id"
            << '\t'
            << "difficulty"
            << '\t'
            << "play_type"
            << '\t'
            << "clear_lamp"
            << '\t'
            << "clear_lamp_name"
            << '\t'
            << "ex_score"
            << '\t'
            << "miss_count"
            << '\t'
            << "miss_count_valid"
            << '\t'
            << "pgreat"
            << '\t'
            << "great"
            << '\t'
            << "good"
            << '\t'
            << "bad"
            << '\t'
            << "poor"
            << '\t'
            << "combo_break"
            << '\t'
            << "fast"
            << '\t'
            << "slow"
            << '\t'
            << "premature_end"
            << '\t'
            << "timestamp"
            << '\n';
    }

    file
        << result.songId
        << '\t'
        << result.difficulty
        << '\t'
        << judgePlayTypeToString(
               result.playType)
        << '\t'
        << result.clearLamp
        << '\t'
        << clearLampToString(
               result.clearLamp)
        << '\t'
        << result.exScore
        << '\t'
        << result.missCount
        << '\t'
        << (result.missCountValid
                ? "true"
                : "false")
        << '\t'
        << result.pGreat
        << '\t'
        << result.great
        << '\t'
        << result.good
        << '\t'
        << result.bad
        << '\t'
        << result.poor
        << '\t'
        << result.comboBreak
        << '\t'
        << result.fast
        << '\t'
        << result.slow
        << '\t'
        << (result.prematureEnd
                ? "true"
                : "false")
        << '\t'
        << formatTimestamp(
               result.timestamp)
        << '\n';

    file.close();

    LOG_INFO(
        "Play result TSV saved successfully.");

    return true;
}
// 難易度やプレイタイプの定義に合わせて調整してください
// 例: playType が SP / DP、difficulty が 0〜4 などのインデックスや種別の場合
int getNotesForChart(int songId, int difficulty, int playType)
{
    // 1. メモリ上のバッファや、事前に構築した MusicTableReader のマップから
    //    該当 songId が存在するメモリ上の基準アドレス（song_id が格納されている位置）を取得します。
    const std::uint8_t *songIdPtr = MusicTableReader::getInstance().findSongIdPointer(songId);
    if (!songIdPtr)
    {
        return 0; // 該当楽曲が見つからない場合
    }

    std::ptrdiff_t base = reinterpret_cast<const std::ptrdiff_t>(songIdPtr);
    std::ptrdiff_t targetOffset = 0;

    // 2. プレイタイプと難易度（または内部の譜面種別）に応じたオフセットを決定
    // playType が SP か DP か、以及び difficulty の値（例: 0=Beginner, 1=Normal, 2=Hyper, 3=Another, 4=Leggendaria 等）に応じた分岐

    // ※プロジェクト内の difficulty の数値定義に合わせて調整してください
    // ここでは一般的なナンバリングやインデックスを想定したマッピング例です
    bool isDP = (playType == 2); // 例: DPの場合（プロジェクトの judgePlayTypeToString の定義に合わせる）

    if (!isDP)
    {
        // SP 側のオフセット (song_id 基準)
        switch (difficulty)
        {
        case 0:
            targetOffset = -576;
            break; // SP Beginner
        case 1:
            targetOffset = -572;
            break; // SP Normal
        case 2:
            targetOffset = -568;
            break; // SP Hyper
        case 3:
            targetOffset = -564;
            break; // SP Another
        case 4:
            targetOffset = -560;
            break; // SP Leggendaria
        default:
            return 0;
        }
    }
    else
    {
        // DP 側のオフセット (song_id 基準)
        // ※DP Beginner は存在しないため除外
        switch (difficulty)
        {
        case 1:
            targetOffset = -552;
            break; // DP Normal
        case 2:
            targetOffset = -548;
            break; // DP Hyper
        case 3:
            targetOffset = -544;
            break; // DP Another
        case 4:
            targetOffset = -540;
            break; // DP Leggendaria
        default:
            return 0;
        }
    }

    // 3. 該当オフセットから 4 バイトの整数（ノーツ数）を安全に読み取る
    std::int32_t notes = 0;
    std::memcpy(&notes, songIdPtr + targetOffset, 4);

    // バリデーション（異常値やゴミデータの除外）
    if (notes < 0 || notes > 5000)
    {
        return 0;
    }

    return static_cast<int>(notes);
}