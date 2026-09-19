#include "Logger.h"
#include "MemoryReader.h"
#include "Module.h"
#include "OffsetManager.h"
#include "PatternScanner.h"
#include "OffsetSearcher.h"
#include "Process.h"
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

static bool waitForOneJudgePlay(
    const JudgeDataReader &judgeReader,
    std::uintptr_t judgeAddress,
    JudgeSnapshot &resultSnapshot)
{
    LOG_INFO(
        "次のJudgeDataプレイを待機します。");

    LOG_INFO(
        "Waiting for the next JudgeData play.");

    JudgeSnapshot snapshot;

    // --------------------------------------------------
    // 1. まず全0になるまで待つ
    // --------------------------------------------------

    while (true)
    {
        if (!judgeReader.read(
                judgeAddress,
                snapshot))
        {
            LOG_ERROR(
                "JudgeDataの読み取りに失敗しました。");

            LOG_ERROR(
                "Failed to read JudgeData.");

            return false;
        }

        if (isJudgeSnapshotZero(snapshot))
        {
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(20));
    }

    LOG_INFO(
        "JudgeDataが全0になりました。");

    LOG_INFO(
        "JudgeData has been reset to zero.");

    // --------------------------------------------------
    // 2. 新しいプレイ開始を待つ
    // --------------------------------------------------

    LOG_INFO(
        "新しいプレイの開始を待機します。");

    LOG_INFO(
        "Waiting for the new play to start.");

    while (true)
    {
        if (!judgeReader.read(
                judgeAddress,
                snapshot))
        {
            LOG_ERROR(
                "JudgeDataの読み取りに失敗しました。");

            LOG_ERROR(
                "Failed to read JudgeData.");

            return false;
        }

        if (!isJudgeSnapshotZero(snapshot))
        {
            LOG_INFO(
                "新しいJudgeDataプレイを検出しました。");

            LOG_INFO(
                "New JudgeData play detected.");

            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(20));
    }

    // 最初の非0 Snapshot
    resultSnapshot = snapshot;

    // --------------------------------------------------
    // 3. プレイ中の最新Snapshotを監視
    // --------------------------------------------------

    LOG_INFO(
        "JudgeDataプレイを監視しています。");

    LOG_INFO(
        "Monitoring JudgeData play.");

    while (true)
    {
        if (!judgeReader.read(
                judgeAddress,
                snapshot))
        {
            LOG_ERROR(
                "JudgeDataの読み取りに失敗しました。");

            LOG_ERROR(
                "Failed to read JudgeData.");

            return false;
        }

        // 非0なら現在のプレイデータとして更新
        if (!isJudgeSnapshotZero(snapshot))
        {
            resultSnapshot = snapshot;
        }
        else
        {
            // --------------------------------------------------
            // 4. 全0に戻った
            //    → 1プレイ終了
            // --------------------------------------------------

            LOG_INFO(
                "JudgeDataが再び全0になりました。");

            LOG_INFO(
                "JudgeData has reset to zero again.");

            LOG_INFO(
                "1プレイ分のJudgeDataを確定します。");

            LOG_INFO(
                "Finalizing JudgeData for one play.");

            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(20));
    }
}

static void logJudgeSnapshot(
    const JudgeSnapshot &snapshot)
{
    LOG_INFO(
        "----- JudgeData Snapshot -----");

    LOG_INFO(
        "P1 PGreat: " +
        std::to_string(snapshot.p1Pgreat));

    LOG_INFO(
        "P1 Great: " +
        std::to_string(snapshot.p1Great));

    LOG_INFO(
        "P1 Good: " +
        std::to_string(snapshot.p1Good));

    LOG_INFO(
        "P1 Bad: " +
        std::to_string(snapshot.p1Bad));

    LOG_INFO(
        "P1 Poor: " +
        std::to_string(snapshot.p1Poor));

    LOG_INFO(
        "P1 ComboBreak: " +
        std::to_string(snapshot.p1ComboBreak));

    LOG_INFO(
        "P1 Fast: " +
        std::to_string(snapshot.p1Fast));

    LOG_INFO(
        "P1 Slow: " +
        std::to_string(snapshot.p1Slow));

    LOG_INFO(
        "P1 MeasureEnd: " +
        std::to_string(snapshot.p1MeasureEnd));

    LOG_INFO(
        "P2 PGreat: " +
        std::to_string(snapshot.p2Pgreat));

    LOG_INFO(
        "P2 Great: " +
        std::to_string(snapshot.p2Great));

    LOG_INFO(
        "P2 Good: " +
        std::to_string(snapshot.p2Good));

    LOG_INFO(
        "P2 Bad: " +
        std::to_string(snapshot.p2Bad));

    LOG_INFO(
        "P2 Poor: " +
        std::to_string(snapshot.p2Poor));

    LOG_INFO(
        "P2 ComboBreak: " +
        std::to_string(snapshot.p2ComboBreak));

    LOG_INFO(
        "P2 Fast: " +
        std::to_string(snapshot.p2Fast));

    LOG_INFO(
        "P2 Slow: " +
        std::to_string(snapshot.p2Slow));

    LOG_INFO(
        "P2 MeasureEnd: " +
        std::to_string(snapshot.p2MeasureEnd));

    LOG_INFO(
        "------------------------------");
}

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

    LOG_INFO("現在選曲中: song_id = " + std::to_string(songSnapshot.songId) +
             " / difficulty = " + std::to_string(songSnapshot.difficulty));

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

    Logger::initialize();

    LOG_INFO(
        "INFINITAS Client を開始します。");

    LOG_INFO(
        "Starting INFINITAS Client.");

    /*
     * ------------------------------------------------------------
     * 1. INFINITASプロセスを検出
     * ------------------------------------------------------------
     */
    const DWORD processId =
        ProcessFinder::waitForProcess();

    Process process;

    if (!process.open(processId))
    {
        LOG_ERROR(
            "プロセスへの接続に失敗したため終了します。");

        LOG_ERROR(
            "Exiting because the process could not be opened.");

        return 1;
    }

    /*
     * ------------------------------------------------------------
     * 2. メインモジュール情報を取得
     * ------------------------------------------------------------
     */
    Module module(process);

    if (!module.loadMainModule())
    {
        LOG_ERROR(
            "メインモジュール情報の取得に失敗しました。");

        LOG_ERROR(
            "Failed to load the main module information.");

        return 1;
    }

    const std::uintptr_t baseAddress =
        module.baseAddress();

    LOG_INFO(
        "Module name: " +
        module.name());

    LOG_INFO(
        std::string(
            "Main module base address: 0x") +
        toHex(
            module.baseAddress()));

    LOG_INFO(
        std::string(
            "Main module image size: 0x") +
        toHex(
            static_cast<std::uintptr_t>(
                module.imageSize())));

    LOG_INFO(
        std::string(
            "Main module end address: 0x") +
        toHex(
            module.endAddress()));

    /*
     * ------------------------------------------------------------
     * 3. MemoryReader初期化
     * ------------------------------------------------------------
     */
    MemoryReader memoryReader(
        process);

    /*
     * ------------------------------------------------------------
     * 4. Offsetファイルを読み込む
     * ------------------------------------------------------------
     */
    OffsetManager offsetManager;

    if (!offsetManager.load(
            "offsets.txt",
            baseAddress))
    {
        LOG_ERROR(
            "オフセットの読み込みに失敗しました。");

        LOG_ERROR(
            "Failed to load offsets.");

        return 1;
    }

    if (!offsetManager.validate())
    {
        LOG_ERROR(
            "オフセットの検証に失敗しました。");

        LOG_ERROR(
            "Offset validation failed.");

        return 1;
    }

    /*
     * ------------------------------------------------------------
     * 5. INFINITASのVersion候補をすべて検出
     * ------------------------------------------------------------
     */
    VersionDetector versionDetector(
        memoryReader);

    std::vector<VersionCandidate>
        versionCandidates;

    if (!versionDetector.findCandidates(
            module,
            versionCandidates))
    {
        LOG_ERROR(
            "INFINITASのバージョン候補を検出できないため終了します。");

        LOG_ERROR(
            "Exiting because no INFINITAS version candidates could be detected.");

        return 1;
    }

    /*
     * ------------------------------------------------------------
     * 6. 検出VersionとOffset Versionから
     *    使用するVersionを解決
     * ------------------------------------------------------------
     */
    VersionResolver versionResolver;

    VersionCandidate resolvedVersion;

    if (!versionResolver.resolve(
            versionCandidates,
            offsetManager,
            resolvedVersion))
    {
        LOG_ERROR(
            "対応するVersionとOffsetを解決できないため終了します。");

        LOG_ERROR(
            "Exiting because the INFINITAS version could not be resolved to a supported offset set.");

        return 1;
    }

    LOG_INFO(
        "Detected INFINITAS version: " +
        resolvedVersion.version);

    LOG_INFO(
        "Resolved INFINITAS version: " +
        resolvedVersion.version);

    LOG_INFO(
        "Offset file version: " +
        offsetManager.version());

    LOG_INFO(
        "INFINITASのバージョンとOffsetのバージョンが一致しました。");

    LOG_INFO(
        "INFINITAS version matches the offset version.");

    LOG_INFO(
        "対応Offset version: " +
        offsetManager.version());

    LOG_INFO(
        "Supported offset version: " +
        offsetManager.version());

    LOG_INFO(
        std::string(
            "Resolved version RVA: 0x") +
        toHex(
            static_cast<std::uintptr_t>(
                resolvedVersion.rva)));

    /*
     * ------------------------------------------------------------
     * 7. SongListの絶対アドレスを計算
     * ------------------------------------------------------------
     */
    const std::uintptr_t songListAddress =
        offsetManager.getAddress(
            OffsetType::SongList,
            baseAddress);

    LOG_INFO(
        std::string(
            "SongList absolute address: 0x") +
        toHex(
            songListAddress));

    /*
     * ------------------------------------------------------------
     * 8. SongListのロード完了を待機
     * ------------------------------------------------------------
     */
    constexpr char expectedPrefix[] =
        "5.1.1.";

    LOG_INFO(
        "SongList のロード完了を待機しています...");

    LOG_INFO(
        "Waiting for SongList to finish loading...");

    while (true)
    {
        if (WaitForSingleObject(
                process.handle(),
                0) == WAIT_OBJECT_0)
        {
            LOG_ERROR(
                "INFINITASのプロセスが終了しました。");

            LOG_ERROR(
                "The INFINITAS process has exited.");

            return 1;
        }

        std::array<char, 64> buffer{};

        if (!memoryReader.read(
                songListAddress,
                buffer.data(),
                buffer.size()))
        {
            LOG_INFO(
                "SongListをまだ読み取れません。再試行します...");

            LOG_INFO(
                "SongList is not readable yet. Retrying...");

            std::this_thread::sleep_for(
                std::chrono::seconds(2));

            continue;
        }

        const std::string songListData =
            bytesToAscii(buffer);

        LOG_INFO(
            "SongList先頭データ: " +
            songListData);

        LOG_INFO(
            "SongList first data: " +
            songListData);

        if (songListData.rfind(
                expectedPrefix,
                0) == 0)
        {
            LOG_INFO(
                "SongListの5.1.1.シグネチャを確認しました。");

            LOG_INFO(
                "The SongList 5.1.1. signature was verified.");

            break;
        }

        LOG_INFO(
            "SongListはまだ完全にロードされていません。");

        LOG_INFO(
            "SongList is not fully loaded yet.");

        std::this_thread::sleep_for(
            std::chrono::seconds(2));
    }

    /*
     * ------------------------------------------------------------
     * 9. UnlockDataのロード完了を待機
     * ------------------------------------------------------------
     *
     * RefluxではUnlockDataの先頭songIDが1000になった
     * ことをロード完了判定の一つとして使用している。
     */
    const std::uintptr_t unlockDataAddress =
        offsetManager.getAddress(
            OffsetType::UnlockData,
            baseAddress);

    LOG_INFO(
        std::string(
            "UnlockData absolute address: 0x") +
        toHex(
            unlockDataAddress));

    LOG_INFO(
        "UnlockData のロード完了を待機しています...");

    LOG_INFO(
        "Waiting for UnlockData to finish loading...");

    while (true)
    {
        if (WaitForSingleObject(
                process.handle(),
                0) == WAIT_OBJECT_0)
        {
            LOG_ERROR(
                "INFINITASのプロセスが終了しました。");

            LOG_ERROR(
                "The INFINITAS process has exited.");

            return 1;
        }

        std::array<std::uint8_t, 4>
            songIdBuffer{};

        if (!memoryReader.read(
                unlockDataAddress,
                songIdBuffer.data(),
                songIdBuffer.size()))
        {
            LOG_INFO(
                "UnlockDataをまだ読み取れません。再試行します...");

            LOG_INFO(
                "UnlockData is not readable yet. Retrying...");

            std::this_thread::sleep_for(
                std::chrono::seconds(2));

            continue;
        }

        const std::uint32_t songId =
            bytesToUInt32(
                songIdBuffer.data());

        LOG_INFO(
            "UnlockData first songID: " +
            std::to_string(songId));

        if (songId == 1000)
        {
            LOG_INFO(
                "UnlockDataのロード完了を確認しました。");

            LOG_INFO(
                "UnlockData has finished loading.");

            break;
        }

        LOG_INFO(
            "UnlockDataはまだ完全にロードされていません。");

        LOG_INFO(
            "UnlockData is not fully loaded yet.");

        std::this_thread::sleep_for(
            std::chrono::seconds(2));
    }

    /*
     * ------------------------------------------------------------
     * 10. PatternScanner / OffsetSearcher初期化
     * ------------------------------------------------------------
     */
    PatternScanner patternScanner(
        memoryReader);

    OffsetSearcher offsetSearcher(
        patternScanner);

    /*
     * ------------------------------------------------------------
     * 11. SongListを実メモリから探索
     * ------------------------------------------------------------
     */
    OffsetSearchResult searchedSongList;

    if (!offsetSearcher.searchSongList(
            module,
            searchedSongList))
    {
        LOG_ERROR(
            "SongList Offsetの探索に失敗しました。");

        LOG_ERROR(
            "Failed to search for the SongList offset.");

        return 1;
    }

    const std::uintptr_t configuredSongListRva =
        offsetManager.get(
            OffsetType::SongList);

    LOG_INFO(
        std::string(
            "Configured SongList RVA: 0x") +
        toHex(
            configuredSongListRva));

    LOG_INFO(
        std::string(
            "Searched SongList RVA: 0x") +
        toHex(
            static_cast<std::uintptr_t>(
                searchedSongList.rva)));

    if (
        configuredSongListRva ==
        searchedSongList.rva)
    {
        LOG_INFO(
            "SongList Offsetの探索結果が既存Offsetと一致しました。");

        LOG_INFO(
            "Searched SongList offset matches the configured offset.");
    }
    else
    {
        LOG_ERROR(
            "SongList Offsetの探索結果と既存Offsetが一致しません。");

        LOG_ERROR(
            "Searched SongList offset does not match the configured offset.");

        return 1;
    }

    /*
     * ------------------------------------------------------------
     * 12. UnlockDataを実メモリから探索
     * ------------------------------------------------------------
     */
    OffsetSearchResult searchedUnlockData;

    if (!offsetSearcher.searchUnlockData(
            module,
            searchedUnlockData))
    {
        LOG_ERROR(
            "UnlockData Offsetの探索に失敗しました。");

        LOG_ERROR(
            "Failed to search for the UnlockData offset.");

        return 1;
    }

    const std::uintptr_t configuredUnlockDataRva =
        offsetManager.get(
            OffsetType::UnlockData);

    LOG_INFO(
        std::string(
            "Configured UnlockData RVA: 0x") +
        toHex(
            configuredUnlockDataRva));

    LOG_INFO(
        std::string(
            "Searched UnlockData RVA: 0x") +
        toHex(
            static_cast<std::uintptr_t>(
                searchedUnlockData.rva)));

    if (
        configuredUnlockDataRva ==
        searchedUnlockData.rva)
    {
        LOG_INFO(
            "UnlockData Offsetの探索結果が既存Offsetと一致しました。");

        LOG_INFO(
            "Searched UnlockData offset matches the configured offset.");
    }
    else
    {
        LOG_ERROR(
            "UnlockData Offsetの探索結果と既存Offsetが一致しません。");

        LOG_ERROR(
            "Searched UnlockData offset does not match the configured offset.");

        return 1;
    }

    /*
     * ------------------------------------------------------------
     * 13. DataMapを実メモリから探索
     * ------------------------------------------------------------
     */
    OffsetSearchResult searchedDataMap;

    if (!offsetSearcher.searchDataMap(
            module,
            searchedDataMap))
    {
        LOG_ERROR(
            "DataMap Offsetの探索に失敗しました。");

        LOG_ERROR(
            "Failed to search for the DataMap offset.");

        return 1;
    }

    const std::uintptr_t configuredDataMapRva =
        offsetManager.get(
            OffsetType::DataMap);

    LOG_INFO(
        std::string(
            "Configured DataMap RVA: 0x") +
        toHex(
            configuredDataMapRva));

    LOG_INFO(
        std::string(
            "Searched DataMap RVA: 0x") +
        toHex(
            static_cast<std::uintptr_t>(
                searchedDataMap.rva)));

    if (
        configuredDataMapRva ==
        searchedDataMap.rva)
    {
        LOG_INFO(
            "DataMap Offsetの探索結果が既存Offsetと一致しました。");

        LOG_INFO(
            "Searched DataMap offset matches the configured offset.");
    }
    else
    {
        LOG_ERROR(
            "DataMap Offsetの探索結果と既存Offsetが一致しません。");

        LOG_ERROR(
            "Searched DataMap offset does not match the configured offset.");

        return 1;
    }

    LOG_INFO("######## SCORE MAP TEST START ########");

    ScoreMapReader scoreMapReader(
        memoryReader,
        module);

    LOG_INFO("ScoreMapReader object created.");

    if (!scoreMapReader.dumpScoreMapStart(
            searchedDataMap.rva))
    {
        LOG_ERROR(
            "ScoreMap START raw dump failed.");

        return 1;
    }

    LOG_INFO("ScoreMap START raw dump returned TRUE.");

    LOG_INFO("######## SCORE MAP TEST END ########");

    LOG_INFO(
        "######## SCORE MAP ENTRY RAW TEST START ########");

    ScoreMapReader scoreMapEntryReader(
        memoryReader,
        module);

    for (
        std::size_t bucketIndex = 0;
        bucketIndex < 4;
        ++bucketIndex)
    {
        if (!scoreMapEntryReader.dumpScoreMapEntry(
                searchedDataMap.rva,
                bucketIndex))
        {
            LOG_ERROR(
                std::string(
                    "ScoreMap entry raw dump failed. bucket=") +
                std::to_string(bucketIndex));

            return 1;
        }
    }

    LOG_INFO(
        "######## SCORE MAP ENTRY RAW TEST END ########");

    LOG_INFO(
        "######## SCORE MAP ENTRY CANDIDATE START ########");

    ScoreMapReader scoreMapEntryCandidates(
        memoryReader,
        module);

    LOG_INFO(
        "ScoreMapEntryCandidates object created.");

    if (!scoreMapEntryCandidates.dumpScoreMapEntryCandidates(
            searchedDataMap.rva))
    {
        LOG_ERROR(
            "ScoreMap entry candidate search failed.");

        return 1;
    }

    LOG_INFO(
        "ScoreMap entry candidate search returned TRUE.");

    LOG_INFO(
        "######## SCORE MAP ENTRY CANDIDATE END ########");

    LOG_INFO(
        "######## SCORE MAP TARGET SEARCH START ########");

    LOG_INFO(
        "######## SCORE MAP TARGET SEARCH END ########");

    LOG_INFO("======== SCORE MAP FULL SCAN START ========");

    if (!scoreMapReader.scanAllScoreRecords(searchedDataMap.rva))
    {
        LOG_ERROR("scanAllScoreRecords failed.");
        return 1;
    }

    LOG_INFO("======== SCORE MAP FULL SCAN END ========");

    LOG_INFO("######## DETAILED RECORD DUMP TEST START ########");

    // 確定している 0x998B5CE0 (30080, diff=0) の詳細ダンプ
    scoreMapReader.dumpScoreRecordDetailed(0x998B5CE0);

    LOG_INFO("######## DETAILED RECORD DUMP TEST END ########");

    LOG_INFO("######## POINTER TARGET DUMP TEST START ########");

    // 先ほど特定した 30080 (diff=0) のレコードアドレスを指定
    scoreMapReader.dumpPointerTargets(0x998B5CE0);

    LOG_INFO("######## POINTER TARGET DUMP TEST END ########");

    LOG_INFO("######## WIDE SCAN FOR 19002 START ########");

    // 19002 (旧曲) のレコードがメモリのどこかに存在するかモジュール全域をスキャン
    std::uintptr_t hitAddr = scoreMapReader.scanMemoryForSongId(19002);

    if (hitAddr != 0)
    {
        LOG_INFO("Target 19002 was found in memory at: " + toHex(hitAddr));
    }
    else
    {
        LOG_ERROR("Target 19002 was NOT found in module memory.");
    }

    LOG_INFO("######## WIDE SCAN FOR 19002 END ########");

    LOG_INFO("######## EXPORT TO TRACKER TSV START ########");

    Tracker tracker;
    std::string tsvFileName = "tracker.tsv";

    if (scoreMapReader.dumpAllRecordsToTracker(searchedDataMap.rva, tracker, tsvFileName))
    {
        LOG_INFO("Successfully generated " + tsvFileName);
    }
    else
    {
        LOG_ERROR("Failed to generate TSV file.");
    }

    LOG_INFO("######## EXPORT TO TRACKER TSV END ########");

    // 1. MusicTableReader の作成とマップ構築
    MusicTableReader musicReader(memoryReader, module.baseAddress(), 74874880);

    // BroGamer (RVA 0x33AA790) の周辺メモリ領域を一括スキャン
    if (musicReader.scanAndBuildMusicMap(0x2000000, 0x2000000))
    {
        // 検証用: 取得できた曲名マップを TSV に出力
        musicReader.exportToTsv("music_map.tsv");

        // 動作確認テスト
        LOG_INFO("Song 24080: " + musicReader.getSongTitle(24080)); // BroGamer
        LOG_INFO("Song 28079: " + musicReader.getSongTitle(28079)); // Chewingood!!!
        LOG_INFO("Song 80001: " + musicReader.getSongTitle(80001)); // 3y3s(Long ver.)
    }

    return 0;

    /*
     * ------------------------------------------------------------
     * 13. JudgeData 1プレイ取得テスト
     * ------------------------------------------------------------
     */
    const std::uintptr_t knownJudgeDataAddress =
        offsetManager.getAddress(
            OffsetType::JudgeData,
            baseAddress);

    LOG_INFO(
        std::string(
            "Known JudgeData absolute address: 0x") +
        toHex(
            knownJudgeDataAddress));

    JudgeDataReader judgeReader(
        memoryReader);

    LOG_INFO(
        "========================================");

    LOG_INFO(
        "JudgeData 1プレイ取得テスト");

    LOG_INFO(
        "JudgeData single-play capture test");

    LOG_INFO(
        "========================================");

    LOG_INFO(
        "次のプレイを開始してください。");

    LOG_INFO(
        "Start the next play.");

    JudgeSnapshot playSnapshot;

    if (!waitForOneJudgePlay(
            judgeReader,
            knownJudgeDataAddress,
            playSnapshot))
    {
        LOG_ERROR(
            "1プレイ分のJudgeData取得に失敗しました。");

        LOG_ERROR(
            "Failed to capture one JudgeData play.");

        return 1;
    }

    LOG_INFO(
        "========================================");

    LOG_INFO(
        "1プレイ分のJudgeDataを取得しました。");

    LOG_INFO(
        "Captured JudgeData for one play.");

    LOG_INFO(
        "========================================");

    logJudgeSnapshot(
        playSnapshot);

    /*
     * ------------------------------------------------------------
     * 14. Offset探索確認完了
     * ------------------------------------------------------------
     */

    LOG_INFO(
        "Offset探索テストに成功しました。");

    LOG_INFO(
        "Offset search test completed successfully.");

    return 0;
}