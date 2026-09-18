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
#include "CurrentSong.h"
#include "PlayData.h"

#include <windows.h>

#include <iostream>
#include <limits>
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

            // LOG_INFO(
            //     "JudgeDataが再び全0になりました。");

            // LOG_INFO(
            //     "JudgeData has reset to zero again.");

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

// static bool waitForJudgeDataMeasureEndChange(
//     const JudgeDataReader &judgeReader,
//     std::uintptr_t judgeAddress,
//     int timeoutSeconds = 300)
// {
//     LOG_INFO(
//         "========================================");

//     LOG_INFO(
//         "JudgeData MeasureEnd変化監視テスト");

//     LOG_INFO(
//         "========================================");

//     LOG_INFO(
//         "1曲プレイしてください。");

//     LOG_INFO(
//         "Please play one song.");

//     JudgeSnapshot previousSnapshot;

//     if (!judgeReader.read(
//             judgeAddress,
//             previousSnapshot))
//     {
//         LOG_ERROR(
//             "JudgeDataの初期値を取得できませんでした。");

//         LOG_ERROR(
//             "Failed to acquire initial JudgeData.");

//         return false;
//     }

//     LOG_INFO(
//         "初期JudgeDataを取得しました。");

//     logJudgeSnapshot(
//         previousSnapshot);

//     const auto startTime =
//         std::chrono::steady_clock::now();

//     while (true)
//     {
//         JudgeSnapshot currentSnapshot;

//         if (!judgeReader.read(
//                 judgeAddress,
//                 currentSnapshot))
//         {
//             LOG_ERROR(
//                 "JudgeDataの読み取りに失敗しました。");

//             LOG_ERROR(
//                 "Failed to read JudgeData.");

//             return false;
//         }

//         bool p1Changed =
//             currentSnapshot.p1Pgreat !=
//                 previousSnapshot.p1Pgreat ||
//             currentSnapshot.p1Great !=
//                 previousSnapshot.p1Great ||
//             currentSnapshot.p1Good !=
//                 previousSnapshot.p1Good ||
//             currentSnapshot.p1Bad !=
//                 previousSnapshot.p1Bad ||
//             currentSnapshot.p1Poor !=
//                 previousSnapshot.p1Poor ||
//             currentSnapshot.p1ComboBreak !=
//                 previousSnapshot.p1ComboBreak ||
//             currentSnapshot.p1Fast !=
//                 previousSnapshot.p1Fast ||
//             currentSnapshot.p1Slow !=
//                 previousSnapshot.p1Slow ||
//             currentSnapshot.p1MeasureEnd !=
//                 previousSnapshot.p1MeasureEnd;

//         bool p2Changed =
//             currentSnapshot.p2Pgreat !=
//                 previousSnapshot.p2Pgreat ||
//             currentSnapshot.p2Great !=
//                 previousSnapshot.p2Great ||
//             currentSnapshot.p2Good !=
//                 previousSnapshot.p2Good ||
//             currentSnapshot.p2Bad !=
//                 previousSnapshot.p2Bad ||
//             currentSnapshot.p2Poor !=
//                 previousSnapshot.p2Poor ||
//             currentSnapshot.p2ComboBreak !=
//                 previousSnapshot.p2ComboBreak ||
//             currentSnapshot.p2Fast !=
//                 previousSnapshot.p2Fast ||
//             currentSnapshot.p2Slow !=
//                 previousSnapshot.p2Slow ||
//             currentSnapshot.p2MeasureEnd !=
//                 previousSnapshot.p2MeasureEnd;

//         if (p1Changed || p2Changed)
//         {
//             LOG_INFO(
//                 "JudgeDataの変化を検出しました。");

//             LOG_INFO(
//                 "JudgeData change detected.");

//             logJudgeSnapshot(
//                 currentSnapshot);
//         }

//         if (currentSnapshot.p1MeasureEnd !=
//             previousSnapshot.p1MeasureEnd)
//         {
//             LOG_INFO(
//                 "P1 MeasureEnd が変化しました: " +
//                 std::to_string(
//                     previousSnapshot.p1MeasureEnd) +
//                 " -> " +
//                 std::to_string(
//                     currentSnapshot.p1MeasureEnd));
//         }

//         if (currentSnapshot.p2MeasureEnd !=
//             previousSnapshot.p2MeasureEnd)
//         {
//             LOG_INFO(
//                 "P2 MeasureEnd が変化しました: " +
//                 std::to_string(
//                     previousSnapshot.p2MeasureEnd) +
//                 " -> " +
//                 std::to_string(
//                     currentSnapshot.p2MeasureEnd));
//         }

//         previousSnapshot =
//             currentSnapshot;

//         const auto now =
//             std::chrono::steady_clock::now();

//         const auto elapsed =
//             std::chrono::duration_cast<
//                 std::chrono::seconds>(
//                 now - startTime);

//         if (elapsed.count() >=
//             timeoutSeconds)
//         {
//             LOG_INFO(
//                 "JudgeData MeasureEnd変化監視がタイムアウトしました。");

//             LOG_INFO(
//                 "JudgeData MeasureEnd monitoring timed out.");

//             return false;
//         }

//         std::this_thread::sleep_for(
//             std::chrono::milliseconds(200));
//     }
// }

// static bool hasP1JudgeDataChanged(
//     const JudgeSnapshot &baseline,
//     const JudgeSnapshot &current)
// {
//     return baseline.p1Pgreat != current.p1Pgreat ||
//            baseline.p1Great != current.p1Great ||
//            baseline.p1Good != current.p1Good ||
//            baseline.p1Bad != current.p1Bad ||
//            baseline.p1Poor != current.p1Poor ||
//            baseline.p1ComboBreak != current.p1ComboBreak ||
//            baseline.p1Fast != current.p1Fast ||
//            baseline.p1Slow != current.p1Slow;
// }

// static bool hasP2JudgeDataChanged(
//     const JudgeSnapshot &baseline,
//     const JudgeSnapshot &current)
// {
//     return baseline.p2Pgreat != current.p2Pgreat ||
//            baseline.p2Great != current.p2Great ||
//            baseline.p2Good != current.p2Good ||
//            baseline.p2Bad != current.p2Bad ||
//            baseline.p2Poor != current.p2Poor ||
//            baseline.p2ComboBreak != current.p2ComboBreak ||
//            baseline.p2Fast != current.p2Fast ||
//            baseline.p2Slow != current.p2Slow;
// }

// static int totalJudgeCount(const JudgeSnapshot &snapshot)
// {
//     return snapshot.p1Pgreat +
//            snapshot.p1Great +
//            snapshot.p1Good +
//            snapshot.p1Bad +
//            snapshot.p1Poor +
//            snapshot.p2Pgreat +
//            snapshot.p2Great +
//            snapshot.p2Good +
//            snapshot.p2Bad +
//            snapshot.p2Poor;
// }

// static bool waitForJudgeDataChange(
//     const JudgeDataReader &judgeReader,
//     std::uintptr_t judgeAddress,
//     const JudgeSnapshot &baseline,
//     JudgeSnapshot &resultSnapshot,
//     int minimumJudgeCount = 10,
//     int timeoutSeconds = 300)
// {
//     LOG_INFO("JudgeDataの変化を待機します。");
//     LOG_INFO("Waiting for JudgeData changes...");

//     const auto startTime =
//         std::chrono::steady_clock::now();

//     const int baselineJudgeCount =
//         totalJudgeCount(baseline);

//     while (true)
//     {
//         JudgeSnapshot snapshot;

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

//         const int currentJudgeCount =
//             totalJudgeCount(snapshot);

//         if (currentJudgeCount >=
//             baselineJudgeCount +
//                 minimumJudgeCount)
//         {
//             resultSnapshot = snapshot;

//             LOG_INFO(
//                 "JudgeDataに十分な実プレイ結果が入りました。");

//             LOG_INFO(
//                 "Sufficient JudgeData play results detected.");

//             const bool p1Changed =
//                 hasP1JudgeDataChanged(
//                     baseline,
//                     snapshot);

//             const bool p2Changed =
//                 hasP2JudgeDataChanged(
//                     baseline,
//                     snapshot);

//             LOG_INFO(
//                 "P1側JudgeData変化: " +
//                 std::string(
//                     p1Changed ? "あり" : "なし"));

//             LOG_INFO(
//                 "P2側JudgeData変化: " +
//                 std::string(
//                     p2Changed ? "あり" : "なし"));

//             if (p1Changed && p2Changed)
//             {
//                 LOG_INFO(
//                     "変化したプレイデータ: P1/P2両側");
//             }
//             else if (p1Changed)
//             {
//                 LOG_INFO(
//                     "変化したプレイデータ: P1側");
//             }
//             else if (p2Changed)
//             {
//                 LOG_INFO(
//                     "変化したプレイデータ: P2側");
//             }
//             else
//             {
//                 LOG_ERROR(
//                     "判定数は増加しましたが、P1/P2の変化を特定できませんでした。");

//                 LOG_ERROR(
//                     "Judge count increased, but the changed side could not be identified.");

//                 return false;
//             }

//             return true;
//         }

//         const auto now =
//             std::chrono::steady_clock::now();

//         const auto elapsed =
//             std::chrono::duration_cast<
//                 std::chrono::seconds>(
//                 now - startTime);

//         if (elapsed.count() >=
//             timeoutSeconds)
//         {
//             LOG_ERROR(
//                 "JudgeDataの実プレイ結果待機がタイムアウトしました。");

//             LOG_ERROR(
//                 "Timed out while waiting for JudgeData play result.");

//             return false;
//         }

//         std::this_thread::sleep_for(
//             std::chrono::milliseconds(200));
//     }
// }

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

    const std::uintptr_t configuredPlayDataRva =
        offsetManager.get(
            OffsetType::PlayData);

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
    // ///////////////////////////////////////////

    // /*
    //  * ------------------------------------------------------------
    //  * DataMap構造の直接確認
    //  * ------------------------------------------------------------
    //  */
    // const std::uintptr_t dataMapAddress =
    //     offsetManager.getAddress(
    //         OffsetType::DataMap,
    //         baseAddress);

    // const std::uintptr_t dataMapPatternAddress =
    //     dataMapAddress + 0x18;

    // LOG_INFO(
    //     std::string(
    //         "Known DataMap absolute address: 0x") +
    //     toHex(
    //         dataMapAddress));

    // LOG_INFO(
    //     std::string(
    //         "Expected DataMap pattern address: 0x") +
    //     toHex(
    //         dataMapPatternAddress));

    // std::array<std::uint64_t, 8> dataMapValues{};

    // if (memoryReader.read(
    //         dataMapPatternAddress,
    //         dataMapValues.data(),
    //         sizeof(dataMapValues)))
    // {
    //     for (
    //         std::size_t i = 0;
    //         i < dataMapValues.size();
    //         ++i)
    //     {
    //         LOG_INFO(
    //             [&]()
    //             {
    //                 std::ostringstream stream;

    //                 stream
    //                     << "DataMap QWORD["
    //                     << i
    //                     << "] = 0x"
    //                     << std::hex
    //                     << std::uppercase
    //                     << dataMapValues[i];

    //                 return stream.str();
    //             }());
    //     }
    // }
    // else
    // {
    //     LOG_ERROR(
    //         "既知のDataMapパターン位置を読み取れませんでした。");

    //     LOG_ERROR(
    //         "Failed to read the expected DataMap pattern address.");
    // }

    // ///////////////////////////////////////////

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

// #if 0 // JudgeData/PlayDataの1プレイ取得テストは一時的に無効化
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
     *
     * 14. PlayData
     *
     */

    LOG_INFO("=== PlayData Search Test ===");

    constexpr std::size_t knownPlayDataRva =
        0x025D9404;

    const std::uintptr_t knownPlayDataAddress =
        module.baseAddress() +
        knownPlayDataRva;

    std::int32_t songId = 0;
    std::int32_t playType = 0;

    if (!memoryReader.read(
            knownPlayDataAddress + 0x00,
            songId))
    {
        LOG_ERROR(
            "Failed to read PlayData Song ID.");

        return 1;
    }

    if (!memoryReader.read(
            knownPlayDataAddress + 0x04,
            playType))
    {
        LOG_ERROR(
            "Failed to read PlayData PlayType.");

        return 1;
    }

    LOG_INFO(
        "PlayData Song ID: " +
        std::to_string(songId));

    LOG_INFO(
        "PlayData PlayType: " +
        std::to_string(playType));

    const std::int32_t exScore =
        playSnapshot.p1Pgreat * 2 +
        playSnapshot.p1Great +
        playSnapshot.p2Pgreat * 2 +
        playSnapshot.p2Great;

    LOG_INFO(
        "PlayData EXScore: " +
        std::to_string(exScore));

    OffsetSearchResult playDataResult;

    if (!offsetSearcher.searchPlayData(
            module,
            static_cast<std::uint32_t>(songId),
            playType,
            exScore,
            knownPlayDataRva,
            playDataResult))
    {
        LOG_ERROR(
            "PlayData search failed.");

        return 1;
    }

    LOG_INFO(
        "PlayData search succeeded.");

    LOG_INFO(
        "Resolved PlayData RVA: 0x" +
        toHex(
            static_cast<std::uintptr_t>(
                playDataResult.rva)));

// #endif

// #if 0
    /*
     *
     * 15. PlaySettings
     *
     */

    LOG_INFO("=== PlaySettings Search Test ===");

    LOG_INFO(
        "INFINITASのプレイ設定を "
        "RANDOM / EXHARD / OFF / SUDDEN+ "
        "に設定してください。");

    LOG_INFO(
        "設定が完了したらEnterを押してください。");

    // std::cin.ignore(
    //     std::numeric_limits<std::streamsize>::max(),
    //     '\n');

    std::cin.get();

    constexpr std::size_t knownPlaySettingsRva =
        0x025D9154;

    std::size_t playSettingsCandidateRva = 0;

    if (!offsetSearcher.searchPlaySettingsPattern1(
            module,
            knownPlaySettingsRva,
            playSettingsCandidateRva))
    {
        LOG_ERROR(
            "PlaySettings pattern 1 search failed.");

        return 1;
    }

    LOG_INFO(
        "次にINFINITASのプレイ設定を "
        "MIRROR / EASY / AUTO-SCRATCH / HIDDEN+ "
        "に変更してください。");

    LOG_INFO(
        "設定が完了したらEnterを押してください。");

    std::cin.get();

    if (!offsetSearcher.validatePlaySettingsPattern2(
            module,
            playSettingsCandidateRva))
    {
        LOG_ERROR(
            "PlaySettings pattern 2 validation failed.");

        return 1;
    }

    constexpr std::size_t p2PlaySettingsOffset =
        sizeof(std::int32_t) * 18;

    std::size_t resolvedPlaySettingsRva =
        playSettingsCandidateRva;

    // JudgeDataテストを無効化しているため、現在確認済みのP2側レイアウトを使用する。
    const bool isP2PlaySettingsLayout = playSnapshot.playType == JudgePlayType::P2;

    if (isP2PlaySettingsLayout)
    {
        if (playSettingsCandidateRva < p2PlaySettingsOffset)
        {
            LOG_ERROR(
                "PlaySettings P2 offset correction failed.");

            return 1;
        }

        LOG_INFO(
            "PlaySettings P2 candidate detected. "
            "Applying -0x48 correction.");

        resolvedPlaySettingsRva -=
            p2PlaySettingsOffset;
    }

    OffsetSearchResult playSettingsResult{
        OffsetType::PlaySettings,
        resolvedPlaySettingsRva};

    LOG_INFO(
        "PlaySettings search succeeded.");

    LOG_INFO(
        "Resolved PlaySettings RVA: 0x" +
        toHex(
            static_cast<std::uintptr_t>(
                playSettingsResult.rva)));

    // /*
    //  * ------------------------------------------------------------
    //  * 13. JudgeData終了検出調査
    //  * ------------------------------------------------------------
    //  */

    // const std::uintptr_t knownJudgeDataAddress =
    //     offsetManager.getAddress(
    //         OffsetType::JudgeData,
    //         baseAddress);

    // LOG_INFO(
    //     std::string(
    //         "Known JudgeData absolute address: 0x") +
    //     toHex(
    //         knownJudgeDataAddress));

    // JudgeDataReader judgeReader(
    //     memoryReader);

    // if (!waitForJudgeDataMeasureEndChange(
    //         judgeReader,
    //         knownJudgeDataAddress))
    // {
    //     LOG_ERROR(
    //         "JudgeData終了検出調査に失敗しました。");

    //     LOG_ERROR(
    //         "JudgeData end detection analysis failed.");

    //     return 1;
    // }

    // LOG_INFO(
    //     "JudgeData終了検出調査が完了しました。");

    // LOG_INFO(
    //     "JudgeData end detection analysis completed.");

    // return 0;

    // /*
    //  * ------------------------------------------------------------
    //  * 13. JudgeData実プレイテスト
    //  * ------------------------------------------------------------
    //  */
    // const std::uintptr_t knownJudgeDataAddress =
    //     offsetManager.getAddress(
    //         OffsetType::JudgeData,
    //         baseAddress);

    // LOG_INFO(
    //     std::string(
    //         "Known JudgeData absolute address: 0x") +
    //     toHex(
    //         knownJudgeDataAddress));

    // JudgeDataReader judgeReader(
    //     memoryReader);

    // LOG_INFO(
    //     "========================================");

    // LOG_INFO(
    //     "JudgeData P1/P2/DP 実プレイテスト");

    // LOG_INFO(
    //     "========================================");

    // // --------------------------------------------------
    // // 単側プレイ #1
    // // --------------------------------------------------

    // LOG_INFO(
    //     "JudgeData 単側プレイテスト #1");

    // LOG_INFO(
    //     "JudgeData single-side play test #1");

    // LOG_INFO(
    //     "どちらか一方のコントローラーでプレイしてください。");

    // LOG_INFO(
    //     "Play using only one controller.");

    // JudgeSnapshot baselineSnapshot;

    // if (!judgeReader.read(
    //         knownJudgeDataAddress,
    //         baselineSnapshot))
    // {
    //     LOG_ERROR(
    //         "JudgeDataの基準値を取得できませんでした。");

    //     LOG_ERROR(
    //         "Failed to acquire JudgeData baseline.");

    //     return 1;
    // }

    // JudgeSnapshot test1Snapshot;

    // if (!waitForJudgeDataChange(
    //         judgeReader,
    //         knownJudgeDataAddress,
    //         baselineSnapshot,
    //         test1Snapshot))
    // {
    //     return 1;
    // }

    // // --------------------------------------------------
    // // 単側プレイ #2
    // // --------------------------------------------------

    // JudgeSnapshot baselineSnapshot2 =
    //     test1Snapshot;

    // LOG_INFO(
    //     "JudgeData 単側プレイテスト #2");

    // LOG_INFO(
    //     "JudgeData single-side play test #2");

    // LOG_INFO(
    //     "もう一方のコントローラーでプレイしてください。");

    // LOG_INFO(
    //     "Play using the other controller.");

    // JudgeSnapshot test2Snapshot;

    // if (!waitForJudgeDataChange(
    //         judgeReader,
    //         knownJudgeDataAddress,
    //         baselineSnapshot2,
    //         test2Snapshot))
    // {
    //     return 1;
    // }

    // // --------------------------------------------------
    // // DP
    // // --------------------------------------------------

    // LOG_INFO(
    //     "========================================");

    // LOG_INFO(
    //     "DPテストを開始します。");

    // LOG_INFO(
    //     "DPテスト開始。両方のコントローラーでプレイしてください。");

    // LOG_INFO(
    //     "Start DP test. Play using both controllers.");

    // JudgeSnapshot baselineDpSnapshot =
    //     test2Snapshot;

    // JudgeSnapshot dpSnapshot;

    // if (!waitForJudgeDataChange(
    //         judgeReader,
    //         knownJudgeDataAddress,
    //         baselineDpSnapshot,
    //         dpSnapshot))
    // {
    //     LOG_ERROR(
    //         "DPテストに失敗しました。");

    //     LOG_ERROR(
    //         "DP test failed.");

    //     return 1;
    // }

    // const bool p1Changed =
    //     hasP1JudgeDataChanged(
    //         baselineDpSnapshot,
    //         dpSnapshot);

    // const bool p2Changed =
    //     hasP2JudgeDataChanged(
    //         baselineDpSnapshot,
    //         dpSnapshot);

    // if (p1Changed && p2Changed)
    // {
    //     LOG_INFO(
    //         "DPテスト成功。P1/P2両方のJudgeDataが変化しました。");

    //     LOG_INFO(
    //         "DP test succeeded. Both P1 and P2 JudgeData changed.");
    // }
    // else if (p1Changed)
    // {
    //     LOG_ERROR(
    //         "DPテスト失敗。P1側のJudgeDataのみ変化しました。");

    //     LOG_ERROR(
    //         "DP test failed. Only P1 JudgeData changed.");

    //     return 1;
    // }
    // else if (p2Changed)
    // {
    //     LOG_ERROR(
    //         "DPテスト失敗。P2側のJudgeDataのみ変化しました。");

    //     LOG_ERROR(
    //         "DP test failed. Only P2 JudgeData changed.");

    //     return 1;
    // }
    // else
    // {
    //     LOG_ERROR(
    //         "DPテスト失敗。P1/P2どちらのJudgeDataも変化しませんでした。");

    //     LOG_ERROR(
    //         "DP test failed. Neither P1 nor P2 JudgeData changed.");

    //     return 1;
    // }

    // LOG_INFO(
    //     "========================================");

    // LOG_INFO(
    //     "JudgeData P1/P2/DP テスト完了");

    // LOG_INFO(
    //     "========================================");

// // #endif

// #if 0

    /*
     * ------------------------------------------------------------
     * 16. CurrentSongを選曲画面から探索
     * ------------------------------------------------------------
     */
    LOG_INFO("=== CurrentSong Search Test ===");

    LOG_INFO(
        "CurrentSongを更新するため、任意の譜面を1曲プレイしてください。");

    LOG_INFO(
        "プレイ終了後、選曲画面に戻ってからEnterを押してください。");

    std::cin.get();

    constexpr std::size_t knownCurrentSongRva =
        0x02886370;

    // constexpr std::size_t knownPlayDataRva =
    //     0x025D9404;

    const std::uintptr_t knownCurrentSongAddress =
        module.baseAddress() +
        knownCurrentSongRva;

    std::int32_t currentSongId = 0;
    std::int32_t currentDifficulty = 0;

    if (!memoryReader.read(
            knownCurrentSongAddress + 0x00,
            currentSongId) ||
        !memoryReader.read(
            knownCurrentSongAddress + 0x04,
            currentDifficulty))
    {
        LOG_ERROR(
            "Failed to read CurrentSong ID or difficulty.");

        return 1;
    }

    if (currentSongId <= 0)
    {
        LOG_ERROR(
            "CurrentSong ID is not populated. Select a chart and retry.");

        return 1;
    }

    LOG_INFO(
        "CurrentSong ID: " +
        std::to_string(currentSongId));

    LOG_INFO(
        "CurrentSong difficulty: " +
        std::to_string(currentDifficulty));

    OffsetSearchResult currentSongResult;

    if (!offsetSearcher.searchCurrentSong(
            module,
            static_cast<std::uint32_t>(currentSongId),
            currentDifficulty,
            knownCurrentSongRva,
            knownPlayDataRva,
            currentSongResult))
    {
        LOG_ERROR(
            "CurrentSong search failed.");

        return 1;
    }

    const std::size_t configuredCurrentSongRva =
        offsetManager.get(
            OffsetType::CurrentSong);

    LOG_INFO(
        "Configured CurrentSong RVA: 0x" +
        toHex(
            static_cast<std::uintptr_t>(
                configuredCurrentSongRva)));

    LOG_INFO(
        "Searched CurrentSong RVA: 0x" +
        toHex(
            static_cast<std::uintptr_t>(
                currentSongResult.rva)));

    if (configuredCurrentSongRva != currentSongResult.rva)
    {
        LOG_ERROR(
            "Searched CurrentSong offset does not match the configured offset.");

        return 1;
    }

    LOG_INFO(
        "Searched CurrentSong offset matches the configured offset.");

    /*
     * 探索済みRVAからCurrentSongを再読取して検証する。
     */
    const std::uintptr_t resolvedCurrentSongAddress =
        module.baseAddress() +
        static_cast<std::uintptr_t>(
            currentSongResult.rva);

    CurrentSongReader currentSongReader(
        memoryReader);

    CurrentSongSnapshot currentSongSnapshot;

    if (!currentSongReader.read(
            resolvedCurrentSongAddress,
            currentSongSnapshot))
    {
        LOG_ERROR(
            "CurrentSong read test failed.");

        return 1;
    }

    LOG_INFO(
        "CurrentSong read succeeded.");

    LOG_INFO(
        "Read CurrentSong ID: " +
        std::to_string(currentSongSnapshot.songId));

    LOG_INFO(
        "Read CurrentSong difficulty: " +
        std::to_string(currentSongSnapshot.difficulty));

    if (currentSongSnapshot.songId != currentSongId ||
        currentSongSnapshot.difficulty != currentDifficulty)
    {
        LOG_ERROR(
            "CurrentSong read values do not match the search values.");

        return 1;
    }

    LOG_INFO(
        "CurrentSong read values match the search values.");
// #endif

    /*
     *
     *  PlayData
     *
     */

    PlayDataReader playDataReader(memoryReader);

    PlayDataSnapshot playData{};

    const std::uintptr_t playDataRva =
        offsetManager.get(OffsetType::PlayData);

    const auto playDataAddress =
        module.baseAddress() + playDataRva;

    if (playDataReader.read(
            playDataAddress,
            playData))
    {
        LOG_INFO("PlayDataReader succeeded.");
        LOG_INFO("songId: " + std::to_string(playData.songId));
        LOG_INFO("difficulty: " + std::to_string(playData.difficulty));
        LOG_INFO("clearLamp: " + std::to_string(playData.clearLamp));
    }
    else
    {
        LOG_ERROR("PlayDataReader failed.");
    }
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