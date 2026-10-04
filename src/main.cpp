#include "Logger.h"
#include "MemoryReader.h"
#include "Module.h"
#include "OffsetManager.h"
#include "PatternScanner.h"
#include "OffsetSearcher.h"
#include "Process.h"
#include "PlayData.h"
#include "ProcessFinder.h"
#include "VersionDetector.h"
#include "VersionResolver.h"
#include "JudgeData.h"
#include "ScoreMapReader.h"
#include "MusicTableReader.h"
#include "PlayerProfileReader.h"
#include "HttpClient.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>
#include <unordered_map>

#include <iostream>

namespace
{
    std::string toHex(
        std::uintptr_t value)
    {
        std::ostringstream stream;

        stream
            << std::hex
            << std::uppercase
            << value;

        return stream.str();
    }

    std::string bytesToAscii(
        const std::array<char, 64> &buffer)
    {
        std::string result;

        for (char character : buffer)
        {
            if (character == '\0')
            {
                break;
            }

            result.push_back(character);
        }

        return result;
    }

    std::uint32_t bytesToUInt32(
        const std::uint8_t *data)
    {
        return static_cast<std::uint32_t>(data[0]) |
               (static_cast<std::uint32_t>(data[1]) << 8) |
               (static_cast<std::uint32_t>(data[2]) << 16) |
               (static_cast<std::uint32_t>(data[3]) << 24);
    }
    // 現在日時を "YYYY/MM/DD HH:MM:SS" 形式の文字列で返す関数
    std::string getCurrentTimeString()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t timeT = std::chrono::system_clock::to_time_t(now);

        std::tm tmBuffer{};
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tmBuffer, &timeT); // Windows (MSVC) 用のスレッドセーフな関数
#else
        localtime_r(&timeT, &tmBuffer); // POSIX (Linux/macOS) 用
#endif

        std::ostringstream oss;
        oss << std::put_time(&tmBuffer, "-- %Y/%m/%d %H:%M:%S");
        return oss.str();
    }
}

// シンプルな Key=Value 設定ファイル読み込み関数
std::unordered_map<std::string, std::string> loadEnvFile(const std::string &filePath)
{
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(filePath);
    std::string line;

    while (std::getline(file, line))
    {
        // 空行やコメント行(#)をスキップ
        if (line.empty() || line[0] == '#')
            continue;

        auto delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos)
        {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            config[key] = value;
        }
    }
    return config;
}
std::string difficultyResolver(std::int32_t difficulty)
{
    switch (difficulty)
    {
    case 0:
        return "BEGINNER";
    case 1:
        return "NORMAL";
    case 2:
        return "HYPER";
    case 3:
        return "ANOTHER";
    case 4:
        return "LEGGENDARIA";
    default:
        return "UNKNOWN";
    }
}
std::string clearlampResolver(std::int32_t clearlamp)
{
    switch (clearlamp)
    {
    case 0:
        return "NO PLAY";
    case 1:
        return "FAILED";
    case 2:
        return "ASSISTED EASY";
    case 3:
        return "EASY CLEAR";
    case 4:
        return "CLEAR";
    case 5:
        return "HARD CLEAR";
    case 6:
        return "EX-HARD CLEAR";
    case 7:
        return "FULL COMBO";
    default:
        return "UNKNOWN";
    }
}

int main()
{
    while (true)
    {

        Logger::initialize();

        LOG_INFO("INFINITAS Client を開始します。");

        LOG_INFO("Starting INFINITAS Client.");

        // 1.bm2dx.exe検出
        const DWORD processId = ProcessFinder::waitForProcess();

        Process process;

        if (!process.open(processId))
        {
            LOG_ERROR("プロセスへの接続に失敗したため終了します。");

            return 1;
        }

        // 2.メインモジュール情報取得
        Module module(process);

        if (!module.loadMainModule())
        {
            LOG_ERROR("メインモジュール情報の取得に失敗しました。");

            return 1;
        }

        const std::uintptr_t baseAddress = module.baseAddress();

        LOG_INFO("Module name: " + module.name());

        LOG_INFO(std::string("メインモジュールベースアドレス: 0x") + toHex(module.baseAddress()));

        LOG_INFO(std::string("メインモジュールサイズ: 0x") + toHex(static_cast<std::uintptr_t>(module.imageSize())));

        // 3.MemoryReader初期化
        MemoryReader memoryReader(process);

        // 4.Offsetファイルを読み込む

        OffsetManager offsetManager;

        if (!offsetManager.load("offsets.txt", baseAddress))
        {
            LOG_ERROR("オフセットの読み込みに失敗しました。");

            return 1;
        }

        if (!offsetManager.validate())
        {
            LOG_ERROR("オフセットの検証に失敗しました。");

            return 1;
        }

        // 5. INFINITASのVersion候補をすべて検出
        VersionDetector versionDetector(memoryReader);

        std::vector<VersionCandidate> versionCandidates;

        if (!versionDetector.findCandidates(module, versionCandidates))
        {
            LOG_ERROR("INFINITASのバージョン候補を検出できないため終了します。");

            return 1;
        }

        // 6. 検出VersionとOffset Versionから使用するVersionを解決
        VersionResolver versionResolver;

        VersionCandidate resolvedVersion;

        if (!versionResolver.resolve(versionCandidates, offsetManager, resolvedVersion))
        {
            LOG_ERROR("対応するVersionとOffsetを解決できないため終了します。");

            return 1;
        }

        LOG_INFO("使用するINFINITASバージョン: " + resolvedVersion.version);

        LOG_INFO("使用するオフセットバージョン: " + offsetManager.version());

        LOG_INFO("INFINITASのバージョンとオフセットのバージョンが一致しました。");

        LOG_INFO("対応オフセットバージョン: " + offsetManager.version());

        LOG_INFO(std::string("使用するバージョンのRVA: 0x") + toHex(static_cast<std::uintptr_t>(resolvedVersion.rva)));

        // 7. SongListの絶対アドレスを計算
        const std::uintptr_t songListAddress = offsetManager.getAddress(OffsetType::SongList, baseAddress);

        LOG_INFO(std::string("楽曲リストの絶対アドレス: 0x") + toHex(songListAddress));

        // 8. SongListのロード完了を待機
        constexpr char expectedPrefix[] = "5.1.1.";

        LOG_INFO("楽曲リストのロード完了を待機しています...");

        while (true)
        {
            if (WaitForSingleObject(process.handle(), 0) == WAIT_OBJECT_0)
            {
                LOG_ERROR("bm2dx.exeが終了しました。");

                return 1;
            }

            std::array<char, 64> buffer{};

            if (!memoryReader.read(songListAddress, buffer.data(), buffer.size()))
            {
                LOG_INFO("楽曲リストをまだ読み取れません。再試行します...");

                std::this_thread::sleep_for(std::chrono::seconds(20));

                continue;
            }

            const std::string songListData = bytesToAscii(buffer);

            LOG_INFO("楽曲リスト先頭データ: " + songListData);

            if (songListData.rfind(expectedPrefix, 0) == 0)
            {
                LOG_INFO("楽曲リストの5.1.1.シグネチャを確認しました。");

                break;
            }

            LOG_INFO("楽曲リストはまだ完全にロードされていません。");

            std::this_thread::sleep_for(std::chrono::seconds(20));
        }

        // 9. UnlockDataのロード完了を待機
        const std::uintptr_t unlockDataAddress = offsetManager.getAddress(OffsetType::UnlockData, baseAddress);

        LOG_INFO(std::string("未解禁データの絶対アドレス: 0x") + toHex(unlockDataAddress));

        LOG_INFO("未解禁データのロード完了を待機しています...");

        while (true)
        {
            if (WaitForSingleObject(process.handle(), 0) == WAIT_OBJECT_0)
            {
                LOG_ERROR("bm2dx.exeが終了しました。");

                return 1;
            }

            std::array<std::uint8_t, 4>
                songIdBuffer{};

            if (!memoryReader.read(unlockDataAddress, songIdBuffer.data(), songIdBuffer.size()))
            {
                LOG_INFO("未解禁データをまだ読み取れません。再試行します...");

                std::this_thread::sleep_for(std::chrono::seconds(20));

                continue;
            }

            const std::uint32_t songId = bytesToUInt32(songIdBuffer.data());

            LOG_INFO("UnlockData first songID: " + std::to_string(songId));

            if (songId == 1000)
            {
                LOG_INFO("未解禁データのロード完了を確認しました。");

                break;
            }

            LOG_INFO("未解禁データはまだ完全にロードされていません。");

            std::this_thread::sleep_for(std::chrono::seconds(30));
        }

        // 10. PatternScanner / OffsetSearcher初期化
        PatternScanner patternScanner(memoryReader);

        OffsetSearcher offsetSearcher(patternScanner);

        // 11. SongListを実メモリから探索
        OffsetSearchResult searchedSongList;

        if (!offsetSearcher.searchSongList(module, searchedSongList))
        {
            LOG_ERROR("SongList Offsetの探索に失敗しました。");

            return 1;
        }

        const std::uintptr_t configuredSongListRva = offsetManager.get(OffsetType::SongList);

        LOG_INFO(std::string("Configured SongList RVA: 0x") + toHex(configuredSongListRva));

        LOG_INFO(std::string("Searched SongList RVA: 0x") + toHex(static_cast<std::uintptr_t>(searchedSongList.rva)));

        if (configuredSongListRva == searchedSongList.rva)
        {
            LOG_INFO("楽曲リストのオフセット探索結果が既存Offsetと一致しました。");
        }
        else
        {
            LOG_ERROR("楽曲リストのオフセット探索結果と既存Offsetが一致しません。");

            return 1;
        }

        // 12. UnlockDataを実メモリから探索
        OffsetSearchResult searchedUnlockData;

        if (!offsetSearcher.searchUnlockData(module, searchedUnlockData))
        {
            LOG_ERROR("未解禁データオフセットの探索に失敗しました。");

            return 1;
        }

        const std::uintptr_t configuredUnlockDataRva = offsetManager.get(OffsetType::UnlockData);

        LOG_INFO(std::string("Configured UnlockData RVA: 0x") + toHex(configuredUnlockDataRva));

        LOG_INFO(std::string("Searched UnlockData RVA: 0x") + toHex(static_cast<std::uintptr_t>(searchedUnlockData.rva)));

        if (configuredUnlockDataRva == searchedUnlockData.rva)
        {
            LOG_INFO("未解禁データオフセットの探索結果が既存Offsetと一致しました。");
        }
        else
        {
            LOG_ERROR("未解禁データオフセットの探索結果と既存Offsetが一致しません。");

            return 1;
        }

        // 13. DataMapを実メモリから探索
        OffsetSearchResult searchedDataMap;

        if (!offsetSearcher.searchDataMap(module, searchedDataMap))
        {
            LOG_ERROR("データマップオフセットの探索に失敗しました。");

            return 1;
        }

        const std::uintptr_t configuredDataMapRva = offsetManager.get(OffsetType::DataMap);

        LOG_INFO(std::string("Configured DataMap RVA: 0x") + toHex(configuredDataMapRva));

        LOG_INFO(std::string("Searched DataMap RVA: 0x") + toHex(static_cast<std::uintptr_t>(searchedDataMap.rva)));

        if (configuredDataMapRva == searchedDataMap.rva)
        {
            LOG_INFO("データマップオフセットの探索結果が既存Offsetと一致しました。");
        }
        else
        {
            LOG_ERROR("データマップオフセットの探索結果と既存Offsetが一致しません。");

            return 1;
        }

        LOG_INFO("######## SCORE MAP TEST START ########");

        ScoreMapReader scoreMapReader(memoryReader, module);

        if (!scoreMapReader.dumpScoreMapStart(searchedDataMap.rva))
        {
            LOG_ERROR("スコアマップが生データのダンプに失敗しました");

            return 1;
        }

        LOG_INFO("スコアマップが生データのダンプに成功しました");

        LOG_INFO("######## SCORE MAP TEST END ########");

        LOG_INFO("######## SCORE MAP ENTRY RAW TEST START ########");

        ScoreMapReader scoreMapEntryReader(memoryReader, module);

        for (std::size_t bucketIndex = 0; bucketIndex < 4; ++bucketIndex)
        {
            if (!scoreMapEntryReader.dumpScoreMapEntry(searchedDataMap.rva, bucketIndex))
            {
                LOG_ERROR(std::string("スコアマップ入力プロセスが生データの吐き出しに失敗しました bucket=") + std::to_string(bucketIndex));

                return 1;
            }
        }

        LOG_INFO("######## PLAYER PROFILE READ START ########");

        // const std::uintptr_t playerProfileBase = offsetManager.get(OffsetType::PlayerProfile);
        const std::uintptr_t playerProfileAddress = offsetManager.getAddress(OffsetType::PlayerProfile, baseAddress);

        LOG_INFO(std::string("PlayerProfile の絶対アドレス: 0x") + toHex(playerProfileAddress));

        // main.cpp

        // 2. PlayerProfileReader で ID を読み出し
        PlayerProfileReader profileReader(memoryReader);
        std::string infinitasId;
        std::string djName;

        if (!profileReader.fetchInfinitasId(playerProfileAddress, infinitasId, djName))
        {
            LOG_ERROR("INFINITAS ID の取得に失敗しました。デフォルト値を使用するか処理を中断します。");
            return 1;
        }

        LOG_INFO("取得成功 INFINITAS ID: " + infinitasId);
        LOG_INFO("######## PLAYER PROFILE READ END ########");

        LOG_INFO("######## SCORE MAP ENTRY CANDIDATE START ########");

        ScoreMapReader scoreMapEntryCandidates(memoryReader, module);
        LOG_INFO("スコアマップ入力プロセスが候補を作成しました");

        if (!scoreMapEntryCandidates.dumpScoreMapEntryCandidates(searchedDataMap.rva))
        {
            LOG_ERROR("スコアマップ入力プロセスが候補の作成に失敗しました");
            return 1;
        }

        LOG_INFO("スコアマップ入力プロセスが候補の作成に成功しました");
        LOG_INFO("######## SCORE MAP ENTRY CANDIDATE END ########");

        LOG_INFO("======== SCORE MAP FULL SCAN START ========");

        if (!scoreMapReader.scanAllScoreRecords(searchedDataMap.rva))
        {
            LOG_ERROR("全譜面のベストスコア検出に失敗しました");
            return 1;
        }

        LOG_INFO("======== SCORE MAP FULL SCAN END ========");

        LOG_INFO("######## EXPORT TO TRACKER START ########");

        Tracker tracker;

        MusicTableReader musicReader(memoryReader, module.baseAddress(), 74874880);

        LOG_INFO("楽曲マップのビルドをしています...");
        if (!musicReader.scanAndBuildMusicMap(0, 74874880))
        {
            LOG_ERROR("楽曲マップの検出とビルドに失敗しました。ノーツ数と楽曲名は無効になります");
        }

        scoreMapReader.dumpAllRecordsToTracker(configuredDataMapRva, tracker, musicReader);
        // 1. ローカルファイル (tracker.json) への書き出し
        if (tracker.writeJson("tracker.json", infinitasId, djName))
        {
            LOG_INFO("tracker.json successfully updated!");
        }
        else
        {
            LOG_ERROR("Failed to write tracker.json.");
        }
        // 2. HTTP POST 送信（ペロード送信）
        auto config = loadEnvFile("config.txt");
        std::string targetUrl = config.count("API_ENDPOINT") && !config["API_ENDPOINT"].empty() ? config["API_ENDPOINT"] : "";

        // もし API_ENDPOINT が空で、GIST_FALLBACK_URL があればそちらを初期URLとして使う
        std::string initialUrl = !targetUrl.empty() ? targetUrl : (config.count("GIST_FALLBACK_URL") ? config["GIST_FALLBACK_URL"] : "");

        LOG_INFO("Sending score data to server...");
        std::string jsonPayload = tracker.dumpJsonString(infinitasId, djName);

        HttpClient httpClient(initialUrl);
        bool success = httpClient.postJsonWithFallback(jsonPayload);

        if (success)
        {
            LOG_INFO("Server payload successfully transmitted!");
        }
        else
        {
            LOG_ERROR("Failed to transmit payload to server.");
        }
        LOG_INFO("######## EXPORT TO TRACKER END ########");

        // 15. JudgeData / リザルト監視ループ
        PlayDataReader playDataReader(memoryReader);
        JudgeDataReader judgeReader(memoryReader);

        // 各種アドレス設定
        const std::uintptr_t songStateAddr = offsetManager.getAddress(OffsetType::SongState, baseAddress);
        const std::uintptr_t playDataBaseAddr = offsetManager.getAddress(OffsetType::PlayData, baseAddress);
        const std::uintptr_t judgeDataBaseAddr = offsetManager.getAddress(OffsetType::JudgeData, baseAddress);

        int32_t prevSongState = 0;
        JudgeSnapshot activeJudgeSnap{};

        LOG_INFO("Result Watcher Loop Started...");

        while (true)
        {
            //  プロセス終了判定 作成予定

            int32_t currentSongState = 0;
            memoryReader.read(songStateAddr, &currentSongState, sizeof(currentSongState));

            // 2. リザルト画面に入った瞬間
            if (currentSongState > 0 && prevSongState == 0)
            {
                LOG_INFO("==================================================");
                LOG_INFO(" [RESULT DETECTED] Song State ID: " + std::to_string(currentSongState) + " " + getCurrentTimeString());
                LOG_INFO("==================================================");

                PlayDataSnapshot playData{};

                if (playDataReader.read(playDataBaseAddr, playData))
                {
                    // 1. playData と judgeSnap から正確な PlayResult を構築
                    PlayResult result = createPlayResult(activeJudgeSnap, playData);

                    // LOG_INFO("Song ID     : " + std::to_string(result.songId));

                    std::string pt = static_cast<int>(result.playType) == 1 ? "DP" : "SP";
                    std::string diff = difficultyResolver(result.difficulty);
                    LOG_INFO("Difficulty  : " + (pt + " " + diff));
                    LOG_INFO("EX Score    : " + std::to_string(result.exScore));
                    LOG_INFO("Clear Lamp  : " + clearlampResolver(result.clearLamp));

                    // 2. ローカルの全曲トラッカーの記録を更新
                    tracker.update(result);

                    // INFINITAS ID の取得と処理
                    std::string currentInfinitasId = infinitasId;
                    if (currentInfinitasId.empty())
                    {
                        profileReader.fetchInfinitasId(playerProfileAddress, currentInfinitasId, djName);
                    }

                    if (!currentInfinitasId.empty())
                    {
                        // 4. 正確な result から 1曲分 JSON を作成して送信
                        const std::string resultPayload = tracker.dumpSingleResultJsonString(result, currentInfinitasId, djName);

                        LOG_INFO("Sending single score result to server...");

                        if (httpClient.postJsonWithFallback(resultPayload))
                        {
                            LOG_INFO("Server payload successfully transmitted!");
                        }
                        else
                        {
                            LOG_ERROR("Failed to transmit payload to server.");
                        }
                    }
                }
                else
                {
                    LOG_ERROR("Failed to read PlayDataSnapshot.");
                }
                LOG_INFO("--------------------------------------------------");
            }
            // 3. 選曲画面等に戻った瞬間
            else if (currentSongState == 0 && prevSongState > 0)
            {
                LOG_INFO(">>> Returned to Select Screen (State Cleared) " + getCurrentTimeString());
                activeJudgeSnap = {};
            }

            prevSongState = currentSongState;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        LOG_INFO("ソフト終了します");

        return 0;
    }
}