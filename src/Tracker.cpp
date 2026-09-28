// 1. 標準ライブラリと nlohmann/json を最優先でインクルード
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "nlohman/json.hpp"
#include "Tracker.h"
#include "MusicTableReader.h"
#include "logger.h"

// nlohmann::json のエイリアス
using json = nlohmann::json;

// 現在時刻を ISO 8601 形式の文字列（例: "2026-09-27T22:48:30Z"）で取得するヘルパー
std::string getCurrentIsoTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);

    std::tm gmTime{};
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&gmTime, &timeT);
#else
    gmtime_r(&timeT, &gmTime);
#endif

    std::stringstream ss;
    ss << std::put_time(&gmTime, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// 1. JSON 文字列の生成（メインロジック）
std::string Tracker::dumpJsonString(const std::string &infinitasId, const std::string &djName, const std::string &nonce) const
{
    json root;
    root["infinitas_id"] = infinitasId;
    root["dj_name"] = djName;
    root["nonce"] = nonce;
    root["updated_at"] = getCurrentIsoTimestamp();

    json entriesArray = json::array();

    for (const auto &[key, entry] : entries_)
    {
        if (entry.bestClearLamp == 0)
        {
            continue;
        }

        const auto &[songId, difficulty, playType] = key;

        json item;
        item["song_id"] = songId;
        item["difficulty"] = difficulty;
        item["level"] = static_cast<int>(entry.rating);
        item["play_type"] = judgePlayTypeToInt(playType);
        item["clear_lamp"] = static_cast<int>(entry.bestClearLamp);
        item["ex_score"] = entry.bestExScore;
        item["miss_count"] = entry.bestMissCount;

        entriesArray.push_back(item);
    }

    root["entries"] = entriesArray;

    return root.dump(4);
}

std::string Tracker::dumpSingleResultJsonString(const PlayResult &result, const std::string &infinitasId, const std::string &djName, const std::string &nonce) const
{
    nlohmann::json root;
    root["infinitas_id"] = infinitasId;
    root["dj_name"] = djName;
    root["nonce"] = nonce;
    root["updated_at"] = getCurrentIsoTimestamp();

    nlohmann::json item;
    item["song_id"] = result.songId;
    item["difficulty"] = result.difficulty;
    item["level"] = result.rating; // 難易度レベル(1〜12)

    // play_type を整数（0: SP, 1: DP）または定義されたフォーマットで設定
    item["play_type"] = (result.playType == JudgePlayType::P1) ? 0 : 1;

    item["clear_lamp"] = result.clearLamp;
    item["ex_score"] = result.exScore;
    item["miss_count"] = result.missCount;

    // 1曲分の entries 配列を生成
    root["entries"] = nlohmann::json::array({item});

    return root.dump(4);
}

// 2. ファイル書き出し（dumpJsonString を再利用）
bool Tracker::writeJson(const std::string &filePath, const std::string &infinitasId, const std::string &djName, const std::string &nonce) const
{
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);

    if (!file.is_open())
    {
        LOG_ERROR("Failed to open tracker JSON: " + filePath);
        return false;
    }

    file << dumpJsonString(infinitasId, djName, nonce);
    file.close();

    LOG_INFO("Tracker JSON saved successfully: " + filePath);
    return true;
}

bool Tracker::update(const PlayResult &result)
{
    ChartKey key{result.songId, result.difficulty, result.playType};

    auto &entry = entries_[key];

    if (!result.title.empty())
        entry.title = result.title;

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