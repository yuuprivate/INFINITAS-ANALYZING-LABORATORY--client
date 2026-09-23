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
#include "CurrentSong.h"
#include "MusicTableReader.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

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

void dumpMemoryRegion(const MemoryReader &reader, std::uintptr_t baseAddr, const std::string &label)
{
    // 対象アドレスの前後 0x50 バイト (計 0x100 バイト = 256 バイト) を読み込み
    constexpr size_t DUMP_SIZE = 0x100;
    std::uintptr_t startAddr = baseAddr - 0x50;
    std::vector<std::uint8_t> buffer(DUMP_SIZE);

    if (!reader.read(startAddr, buffer.data(), buffer.size()))
    {
        LOG_ERROR("Failed to dump memory for: " + label);
        return;
    }

    LOG_INFO("==================================================");
    LOG_INFO(" MEMORY DUMP [" + label + "] Base: 0x" + [](uintptr_t v)
             {
        std::ostringstream ss; ss << std::hex << v; return ss.str(); }(baseAddr));
    LOG_INFO(" Range: 0x" + [](uintptr_t v)
             {
        std::ostringstream ss; ss << std::hex << v; return ss.str(); }(startAddr) + " - 0x" + [](uintptr_t v)
             {
        std::ostringstream ss; ss << std::hex << v; return ss.str(); }(startAddr + DUMP_SIZE));
    LOG_INFO("==================================================");

    // 16バイトごとにヘキサダンプを出力
    for (size_t offset = 0; offset < DUMP_SIZE; offset += 16)
    {
        std::ostringstream line;
        uintptr_t currentAddr = startAddr + offset;

        // アドレス
        line << std::hex << std::setw(8) << std::setfill('0') << currentAddr << ": ";

        // 16バイトのHEX表示
        for (size_t i = 0; i < 16; ++i)
        {
            line << std::hex << std::setw(2) << std::setfill('0')
                 << static_cast<int>(buffer[offset + i]) << " ";
        }

        LOG_INFO(line.str());
    }
    LOG_INFO("--------------------------------------------------");
}
static bool isJudgeSnapshotZero(
    const JudgeSnapshot &snapshot)
{
    return snapshot.p1Pgreat == 0 &&
           snapshot.p1Great == 0 &&
           snapshot.p1Good == 0 &&
           snapshot.p1Bad == 0 &&
           snapshot.p1Poor == 0 &&
           snapshot.p2Pgreat == 0 &&
           snapshot.p2Great == 0 &&
           snapshot.p2Good == 0 &&
           snapshot.p2Bad == 0 &&
           snapshot.p2Poor == 0 &&
           snapshot.p1ComboBreak == 0 &&
           snapshot.p2ComboBreak == 0 &&
           snapshot.p1Fast == 0 &&
           snapshot.p2Fast == 0 &&
           snapshot.p1Slow == 0 &&
           snapshot.p2Slow == 0 &&
           snapshot.p1MeasureEnd == 0 &&
           snapshot.p2MeasureEnd == 0;
}

// static bool waitForOneJudgePlay(
//     const JudgeDataReader &judgeReader,
//     std::uintptr_t judgeAddress,
//     JudgeSnapshot &resultSnapshot)
// {
//     LOG_INFO("次のJudgeDataプレイを待機します。");

//     LOG_INFO("Waiting for the next JudgeData play.");

//     JudgeSnapshot snapshot;

//     // 1. まず全0になるまで待つ

//     while (true)
//     {
//         if (!judgeReader.read(
//                 judgeAddress,
//                 snapshot))
//         {
//             LOG_ERROR("JudgeDataの読み取りに失敗しました。");

//             LOG_ERROR("Failed to read JudgeData.");

//             return false;
//         }

//         if (isJudgeSnapshotZero(snapshot))
//         {
//             break;
//         }

//         std::this_thread::sleep_for(
//             std::chrono::milliseconds(200));
//     }

//     LOG_INFO("JudgeDataが全0になりました。");

//     LOG_INFO("JudgeData has been reset to zero.");

//     // 2. 新しいプレイ開始を待つ

//     LOG_INFO("新しいプレイの開始を待機します。");

//     LOG_INFO("Waiting for the new play to start.");

//     while (true)
//     {
//         if (!judgeReader.read(
//                 judgeAddress,
//                 snapshot))
//         {
//             LOG_ERROR(
//                 "JudgeDataの読み取りに失敗しました。");

//             LOG_ERROR(
//                 "Failed to read JudgeData.");

//             return false;
//         }

//         if (!isJudgeSnapshotZero(snapshot))
//         {
//             LOG_INFO(
//                 "新しいJudgeDataプレイを検出しました。");

//             LOG_INFO(
//                 "New JudgeData play detected.");

//             break;
//         }

//         std::this_thread::sleep_for(
//             std::chrono::milliseconds(200));
//     }

//     // 最初の非0 Snapshot
//     resultSnapshot = snapshot;

//     // 3. プレイ中の最新Snapshotを監視

//     LOG_INFO(
//         "JudgeDataプレイを監視しています。");

//     LOG_INFO(
//         "Monitoring JudgeData play.");

//     while (true)
//     {
//         if (!judgeReader.read(
//                 judgeAddress,
//                 snapshot))
//         {
//             LOG_ERROR(
//                 "JudgeDataの読み取りに失敗しました。");

//             LOG_ERROR(
//                 "Failed to read JudgeData.");

//             return false;
//         }

//         // 非0なら現在のプレイデータとして更新
//         if (!isJudgeSnapshotZero(snapshot))
//         {
//             resultSnapshot = snapshot;
//         }
//         else
//         {
//             // 4. 全0に戻った → 1プレイ終了

//             LOG_INFO("JudgeDataが再び全0になりました。");

//             LOG_INFO("1プレイ分のJudgeDataを確定します。");

//             return true;
//         }

//         std::this_thread::sleep_for(std::chrono::milliseconds(200));
//     }
// }

// static void logJudgeSnapshot(
//     const JudgeSnapshot &snapshot)
// {
//     LOG_INFO("----- JudgeData Snapshot -----");

//     LOG_INFO("P1 PGreat: " + std::to_string(snapshot.p1Pgreat));

//     LOG_INFO("P1 Great: " + std::to_string(snapshot.p1Great));

//     LOG_INFO("P1 Good: " + std::to_string(snapshot.p1Good));

//     LOG_INFO("P1 Bad: " + std::to_string(snapshot.p1Bad));

//     LOG_INFO("P1 Poor: " + std::to_string(snapshot.p1Poor));

//     LOG_INFO("P1 ComboBreak: " + std::to_string(snapshot.p1ComboBreak));

//     LOG_INFO("P1 Fast: " + std::to_string(snapshot.p1Fast));

//     LOG_INFO("P1 Slow: " + std::to_string(snapshot.p1Slow));

//     LOG_INFO("P1 MeasureEnd: " + std::to_string(snapshot.p1MeasureEnd));

//     LOG_INFO("P2 PGreat: " + std::to_string(snapshot.p2Pgreat));

//     LOG_INFO("P2 Great: " + std::to_string(snapshot.p2Great));

//     LOG_INFO("P2 Good: " + std::to_string(snapshot.p2Good));

//     LOG_INFO("P2 Bad: " + std::to_string(snapshot.p2Bad));

//     LOG_INFO("P2 Poor: " + std::to_string(snapshot.p2Poor));

//     LOG_INFO("P2 ComboBreak: " + std::to_string(snapshot.p2ComboBreak));

//     LOG_INFO("P2 Fast: " + std::to_string(snapshot.p2Fast));

//     LOG_INFO("P2 Slow: " + std::to_string(snapshot.p2Slow));

//     LOG_INFO("P2 MeasureEnd: " + std::to_string(snapshot.p2MeasureEnd));

//     LOG_INFO("------------------------------");
// }

// 選曲中の楽曲情報を読み取り、該当スコアレコードの HEX Dump を出力する処理
void inspectCurrentSongRecord(
    const MemoryReader &memoryReader,
    const ScoreMapReader &scoreMapReader,
    std::uintptr_t currentSongAddress,
    std::uintptr_t dataMapRva)
{
    LOG_INFO("==================================================");
    LOG_INFO("選曲中（CurrentSong）のスコアレコード詳細解析");

    CurrentSongReader currentSongReader(memoryReader);
    CurrentSongSnapshot songSnapshot;

    if (!currentSongReader.read(currentSongAddress, songSnapshot))
    {
        LOG_ERROR("CurrentSong の取得に失敗しました。");
        return;
    }

    LOG_INFO("現在選曲中: song_id = " + std::to_string(songSnapshot.songId) + " / difficulty = " + std::to_string(songSnapshot.difficulty));

    if (songSnapshot.songId == 0)
    {
        LOG_INFO("song_id が 0 です。選曲画面で楽曲を選択した状態でお試しください。");
        return;
    }

    // 取得した songId と difficulty を使ってスコアレコードを Raw Dump
    scoreMapReader.inspectScoreRecord(dataMapRva, songSnapshot.songId, songSnapshot.difficulty);
    LOG_INFO("==================================================");
}

int main()
{
    while(true)
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

        LOG_INFO("######## SCORE MAP ENTRY RAW TEST END ########");

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

        LOG_INFO("######## EXPORT TO TRACKER TSV START ########");

        Tracker tracker;
        std::string tsvFileName = "tracker.tsv";

        MusicTableReader musicReader(memoryReader, module.baseAddress(), 74874880);

        LOG_INFO("楽曲マップのビルドをしています...");
        if (!musicReader.scanAndBuildMusicMap(0, 74874880))
        {
            LOG_ERROR("楽曲マップの検出とビルドに失敗しました。ノーツ数と楽曲名は無効になります");
        }

        // 14. スコアマップのダンプ実行
        if (scoreMapReader.dumpAllRecordsToTracker(searchedDataMap.rva, tracker, tsvFileName, musicReader))
        {
            LOG_INFO("生成に成功しました : " + tsvFileName);
        }
        else
        {
            LOG_ERROR("TSVファイルの作成に失敗しました");
        }

        LOG_INFO("######## EXPORT TO TRACKER TSV END ########");

        // 15. JudgeData 1プレイ取得テスト
        const std::uintptr_t knownJudgeDataAddress = offsetManager.getAddress(OffsetType::JudgeData, baseAddress);

        LOG_INFO(std::string("既知の判定データの絶対アドレス: 0x") + toHex(knownJudgeDataAddress));

        // MemoryReader memoryReader;
        // (※ 必要なプロセスオープン処理等を行う)

        PlayDataReader playDataReader(memoryReader);

        CurrentSongReader currentSongReader(memoryReader);

        JudgeDataReader judgeReader(memoryReader);

        // =========================================================
        // リザルト監視メインループ
        // =========================================================

        // 画面状態（選曲中:0 / プレイ中・リザルト:>0）を監視するアドレス
        const std::uintptr_t songStateAddr = baseAddress + 0x31ACF5C;

        // 本命リザルトデータ一括取得用アドレス (0x025D9404)
        const std::uintptr_t playDataBaseAddr = baseAddress + 0x025D9404;

        // 判定データのアドレス
        const std::uintptr_t judgeDataBaseAddr = offsetManager.getAddress(OffsetType::JudgeData, baseAddress);

        int32_t prevSongState = 0;
        JudgeSnapshot activeJudgeSnap{}; // プレイ中の判定データを一時保持するスナップショット

        LOG_INFO("Result Watcher Loop Started...");

        while (true)
        {
            int32_t currentSongState = 0;
            // 状態判定用の Song ID を読み取る (0x31ACF5C)
            memoryReader.read(songStateAddr, &currentSongState, sizeof(currentSongState));

            // 2. リザルト遷移の瞬間（State が 0 から 正の数 に変化した初回時）
            if (currentSongState > 0 && prevSongState == 0)
            {
                LOG_INFO("==================================================");
                LOG_INFO(" [RESULT DETECTED] Song State ID: " + std::to_string(currentSongState) + " " + getCurrentTimeString());
                LOG_INFO("==================================================");

                PlayDataSnapshot playData{};

                // 本命アドレス (0x25D9404) から全リザルトデータを一括読み込み
                if (playDataReader.read(playDataBaseAddr, playData))
                {
                    // 判定データと合成して PlayResult を作成
                    PlayResult result = createPlayResult(activeJudgeSnap, playData);

                    if (playDataReader.read(playDataBaseAddr, playData))
                    {
                        // 判定データと合成して PlayResult を作成
                        PlayResult result = createPlayResult(activeJudgeSnap, playData);

                        LOG_INFO("Song ID     : " + std::to_string(playData.songId));
                        LOG_INFO("Difficulty  : " + std::to_string(playData.difficulty) + " (Raw: " + std::to_string(playData.rawDifficulty) + ")");
                        LOG_INFO("Play Type   : " + std::to_string(playData.playType) + " (" + (playData.playType == 0 ? "SP" : "DP") + ")");
                        LOG_INFO("EX Score    : " + std::to_string(playData.exScore));
                        LOG_INFO("Miss Count  : " + std::to_string(playData.missCount));
                        LOG_INFO("Clear Lamp  : " + std::to_string(playData.clearLamp));
                    }
                }
                else
                {
                    LOG_ERROR("Failed to read PlayDataSnapshot from 0x25D9404.");
                }
                LOG_INFO("--------------------------------------------------");
            }
            // 3. 選曲画面に戻った瞬間（State が 0 に戻った時） -> 状態リセット
            else if (currentSongState == 0 && prevSongState > 0)
            {
                LOG_INFO(">>> Returned to Select Screen (State Cleared) " + getCurrentTimeString());
                activeJudgeSnap = {}; // 判定スナップショットをクリア
            }

            prevSongState = currentSongState;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    LOG_INFO("ソフト終了します");

    return 0;
}
