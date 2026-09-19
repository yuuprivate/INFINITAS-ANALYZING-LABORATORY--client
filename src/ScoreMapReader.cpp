#include "ScoreMapReader.h"
#include "Logger.h"

#include <string>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <stack>
#include <unordered_set>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <set>

#include <cstring>
#include <fstream>

namespace
{
    std::string toHex(std::uintptr_t value)
    {
        std::ostringstream stream;

        stream
            << "0x"
            << std::uppercase
            << std::hex
            << value;

        return stream.str();
    }

    std::string toHex64(std::uint64_t value)
    {
        std::ostringstream stream;

        stream
            << "0x"
            << std::uppercase
            << std::hex
            << std::setw(16)
            << std::setfill('0')
            << value;

        return stream.str();
    }
}

ScoreMapReader::ScoreMapReader(
    const MemoryReader &memoryReader,
    const Module &module)
    : memoryReader_(memoryReader),
      module_(module)
{
}

bool ScoreMapReader::dumpDataMapNeighborhood(
    std::uintptr_t dataMapRva) const
{
    LOG_INFO("ScoreMapReader::dumpDataMapNeighborhood() ENTER");

    constexpr std::ptrdiff_t beginOffset = -0x40;
    constexpr std::ptrdiff_t endOffset = 0x80;

    const std::uintptr_t dataMapAddress =
        module_.baseAddress() + dataMapRva;

    LOG_INFO(
        std::string("DataMap RVA: ") +
        toHex(dataMapRva));

    LOG_INFO(
        std::string("DataMap address: ") +
        toHex(dataMapAddress));

    for (
        std::ptrdiff_t offset = beginOffset;
        offset <= endOffset;
        offset += sizeof(std::uint64_t))
    {
        const std::uintptr_t address =
            static_cast<std::uintptr_t>(
                static_cast<std::intptr_t>(dataMapAddress) +
                offset);

        std::uint64_t value = 0;

        if (!memoryReader_.read(address, value))
        {
            LOG_ERROR(
                std::string("DataMap周辺の読み取りに失敗しました: ") +
                toHex(address));

            continue;
        }

        std::ostringstream message;

        message
            << "DataMap "
            << (offset >= 0 ? "+" : "")
            << "0x"
            << std::uppercase
            << std::hex
            << offset
            << " : "
            << toHex64(value);

        LOG_INFO(message.str());
    }

    return true;
}

bool ScoreMapReader::dumpScoreMapStart(
    std::uintptr_t dataMapRva) const
{
    constexpr std::uintptr_t startOffset = 0x00;
    constexpr std::size_t dumpSize = 0x200;

    const std::uintptr_t dataMapAddress =
        module_.baseAddress() + dataMapRva;

    std::uint64_t startAddress = 0;

    if (!memoryReader_.read(
            dataMapAddress + startOffset,
            startAddress))
    {
        LOG_ERROR(
            "ScoreMap START候補の読み取りに失敗しました。");

        return false;
    }

    LOG_INFO(
        std::string("ScoreMap START candidate: ") +
        toHex64(startAddress));

    LOG_INFO(
        std::string("ScoreMap START candidate decimal: ") +
        std::to_string(startAddress));

    if (startAddress == 0)
    {
        LOG_ERROR(
            "ScoreMap START candidate is NULL.");

        return false;
    }

    LOG_INFO(
        "===== ScoreMap START raw dump =====");

    for (
        std::size_t offset = 0;
        offset < dumpSize;
        offset += sizeof(std::uint64_t))
    {
        std::uint64_t value = 0;

        if (!memoryReader_.read(
                startAddress + offset,
                value))
        {
            LOG_ERROR(
                std::string(
                    "ScoreMap領域の読み取りに失敗しました: ") +
                toHex(startAddress + offset));

            continue;
        }

        std::ostringstream message;

        // message
        //     << "ScoreMap "
        //     << "+0x"
        //     << std::uppercase
        //     << std::hex
        //     << std::setw(3)
        //     << std::setfill('0')
        //     << offset
        //     << " : "
        //     << toHex64(value);

        LOG_INFO(message.str());
    }

    LOG_INFO(
        "===== ScoreMap START raw dump end =====");

    return true;
}

bool ScoreMapReader::dumpScoreMapEntryCandidates(
    std::uintptr_t dataMapRva) const
{
    const std::uintptr_t dataMapAddress =
        module_.baseAddress() + dataMapRva;

    std::uint64_t startAddress = 0;

    if (!memoryReader_.read(
            dataMapAddress,
            startAddress))
    {
        LOG_ERROR(
            "ScoreMap START candidateの読み取りに失敗しました。");

        return false;
    }

    std::uint64_t sentinel = 0;

    if (!memoryReader_.read(
            startAddress + 0x20,
            sentinel))
    {
        LOG_ERROR(
            "ScoreMap sentinel candidateの読み取りに失敗しました。");

        return false;
    }

    // LOG_INFO(
    //     std::string("ScoreMap START: ") +
    //     toHex64(startAddress));

    // LOG_INFO(
    //     std::string("ScoreMap sentinel candidate: ") +
    //     toHex64(sentinel));

    LOG_INFO(
        "===== ScoreMap Entry Candidates =====");

    constexpr std::size_t bucketCount = 0x10000;

    std::size_t candidateCount = 0;

    for (
        std::size_t bucket = 0;
        bucket < bucketCount;
        ++bucket)
    {
        const std::uintptr_t bucketAddress =
            static_cast<std::uintptr_t>(
                startAddress +
                bucket * sizeof(std::uint64_t));

        std::uint64_t entryAddress = 0;

        if (!memoryReader_.read(
                bucketAddress,
                entryAddress))
        {
            LOG_ERROR(
                std::string(
                    "ScoreMap bucketの読み取りに失敗しました: ") +
                toHex(bucketAddress));

            continue;
        }

        if (entryAddress == 0 ||
            entryAddress == sentinel)
        {
            continue;
        }

        ++candidateCount;

        LOG_INFO(
            std::string("bucket[") +
            std::to_string(bucket) +
            "] -> " +
            toHex64(entryAddress));

        if (candidateCount >= 20)
        {
            LOG_INFO(
                "最初の20件の候補のみ表示します。");

            break;
        }
    }

    LOG_INFO(
        std::string(
            "ScoreMap entry candidate count shown: ") +
        std::to_string(candidateCount));

    LOG_INFO(
        "===== ScoreMap Entry Candidates End =====");

    return true;
}

bool ScoreMapReader::dumpScoreMapEntry(
    std::uintptr_t dataMapRva,
    std::size_t bucketIndex) const
{
    const std::uintptr_t dataMapAddress =
        module_.baseAddress() + dataMapRva;

    std::uint64_t startAddress = 0;

    if (!memoryReader_.read(
            dataMapAddress,
            startAddress))
    {
        LOG_ERROR(
            "ScoreMap STARTの読み取りに失敗しました。");

        return false;
    }

    const std::uintptr_t bucketAddress =
        startAddress +
        bucketIndex * sizeof(std::uint64_t);

    std::uint64_t entryAddress = 0;

    if (!memoryReader_.read(
            bucketAddress,
            entryAddress))
    {
        LOG_ERROR(
            std::string(
                "ScoreMap entry pointerの読み取りに失敗しました: ") +
            toHex(bucketAddress));

        return false;
    }

    LOG_INFO(
        std::string("bucket[") +
        std::to_string(bucketIndex) +
        "]");

    LOG_INFO(
        std::string("  bucket address: ") +
        toHex(bucketAddress));

    LOG_INFO(
        std::string("  entry address: ") +
        toHex64(entryAddress));

    if (entryAddress == 0)
    {
        LOG_INFO("  entry is NULL.");
        return true;
    }

    constexpr std::size_t dumpSize = 0x100;

    LOG_INFO(
        "===== ScoreMap Entry Raw Dump =====");

    for (
        std::size_t offset = 0;
        offset < dumpSize;
        offset += sizeof(std::uint64_t))
    {
        std::uint64_t value = 0;

        if (!memoryReader_.read(
                entryAddress + offset,
                value))
        {
            LOG_ERROR(
                std::string(
                    "ScoreMap entryの読み取りに失敗しました: ") +
                toHex(entryAddress + offset));

            continue;
        }

        std::ostringstream message;

        message
            << "Entry +0x"
            << std::uppercase
            << std::hex
            << std::setw(3)
            << std::setfill('0')
            << offset
            << " : "
            << toHex64(value);

        LOG_INFO(message.str());
    }

    LOG_INFO(
        "===== ScoreMap Entry Raw Dump End =====");

    return true;
}
bool ScoreMapReader::dumpScoreRecord32(
    std::uintptr_t recordAddress) const
{
    LOG_INFO(
        "===== ScoreMap Record 32-bit Dump =====");

    LOG_INFO(
        "Record address: " +
        toHex(recordAddress));

    constexpr std::size_t recordSize = 0x40;

    for (
        std::size_t offset = 0;
        offset < recordSize;
        offset += sizeof(std::uint32_t))
    {
        std::uint32_t value = 0;

        if (!memoryReader_.read(
                recordAddress + offset,
                value))
        {
            LOG_ERROR(
                "Failed to read record at offset " +
                toHex(offset));

            return false;
        }

        std::ostringstream oss;

        oss << "Record +"
            << std::uppercase
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << offset
            << " : 0x"
            << std::setw(8)
            << std::setfill('0')
            << value
            << " (" << std::dec
            << value
            << ")";

        LOG_INFO(oss.str());
    }

    LOG_INFO(
        "===== ScoreMap Record 32-bit Dump End =====");

    return true;
}
bool ScoreMapReader::findScoreRecord(
    std::uintptr_t dataMapRva,
    int targetSongId,
    int targetDifficulty) const
{
    const std::uintptr_t dataMapAddress =
        module_.baseAddress() + dataMapRva;

    std::uintptr_t scoreMapStart = 0;

    if (!memoryReader_.read(
            dataMapAddress,
            scoreMapStart))
    {
        LOG_ERROR(
            "Failed to read ScoreMap START.");

        return false;
    }

    std::uintptr_t sentinel = 0;

    if (!memoryReader_.read(
            scoreMapStart + 0x20,
            sentinel))
    {
        LOG_ERROR(
            "Failed to read ScoreMap sentinel.");

        return false;
    }

    LOG_INFO(
        "ScoreMap START: " +
        toHex(scoreMapStart));

    LOG_INFO(
        "ScoreMap sentinel: " +
        toHex(sentinel));

    LOG_INFO(
        "Searching target:"
        " song_id=" +
        std::to_string(targetSongId) +
        " difficulty=" +
        std::to_string(targetDifficulty));

    constexpr std::size_t bucketPointerSize = 0x08;
    constexpr std::size_t recordSize = 0x40;
    constexpr std::size_t recordSongDifficultyOffset = 0x10;

    for (std::size_t bucketIndex = 0;
         bucketIndex <= 131;
         ++bucketIndex)
    {
        if ((bucketIndex % 32) == 0)
        {
            LOG_INFO(
                "Searching bucket " +
                std::to_string(bucketIndex));
        }

        const std::uintptr_t bucketAddress =
            scoreMapStart +
            bucketIndex * bucketPointerSize;

        std::uintptr_t entryAddress = 0;

        if (!memoryReader_.read(
                bucketAddress,
                entryAddress))
        {
            continue;
        }

        // NULL / sentinel はスキップ
        if (entryAddress == 0 ||
            entryAddress == sentinel)
        {
            continue;
        }

        // 現時点では各 entry の先頭4 recordだけ調査
        constexpr std::size_t maxRecordsPerEntry = 16;

        for (std::size_t recordIndex = 0;
             recordIndex < maxRecordsPerEntry;
             ++recordIndex)
        {
            const std::uintptr_t recordAddress =
                entryAddress +
                recordIndex * recordSize;

            std::uint64_t packedSongDifficulty = 0;

            if (!memoryReader_.read(
                    recordAddress +
                        recordSongDifficultyOffset,
                    packedSongDifficulty))
            {
                break;
            }

            const std::uint32_t difficulty =
                static_cast<std::uint32_t>(
                    packedSongDifficulty &
                    0xFFFFFFFFULL);

            const std::uint32_t songId =
                static_cast<std::uint32_t>(
                    packedSongDifficulty >> 32);

            LOG_INFO(
                "bucket=" +
                std::to_string(bucketIndex) +
                " record=" +
                std::to_string(recordIndex) +
                " address=" +
                toHex(recordAddress) +
                " packed=" +
                toHex64(packedSongDifficulty) +
                " song_id=" +
                std::to_string(songId) +
                " difficulty=" +
                std::to_string(difficulty));

            if (
                songId ==
                    static_cast<std::uint32_t>(
                        targetSongId) &&
                difficulty ==
                    static_cast<std::uint32_t>(
                        targetDifficulty))
            {
                LOG_INFO(
                    "===== TARGET SCORE RECORD FOUND =====");

                LOG_INFO(
                    "bucket index: " +
                    std::to_string(bucketIndex));

                LOG_INFO(
                    "entry address: " +
                    toHex(entryAddress));

                LOG_INFO(
                    "record index: " +
                    std::to_string(recordIndex));

                LOG_INFO(
                    "record address: " +
                    toHex(recordAddress));

                LOG_INFO(
                    "song_id: " +
                    std::to_string(songId));

                LOG_INFO(
                    "difficulty: " +
                    std::to_string(difficulty));

                LOG_INFO(
                    "packed: " +
                    toHex64(packedSongDifficulty));

                return dumpScoreRecord32(
                    recordAddress);
            }
        }
        LOG_INFO("===== ScoreMap Bucket Table =====");

        for (std::size_t bucketIndex = 0;
             bucketIndex <= 131;
             ++bucketIndex)
        {
            const std::uintptr_t bucketAddress =
                scoreMapStart +
                bucketIndex * 0x08;

            std::uintptr_t entryAddress = 0;

            if (!memoryReader_.read(
                    bucketAddress,
                    entryAddress))
            {
                LOG_ERROR(
                    "Failed to read bucket " +
                    std::to_string(bucketIndex));

                continue;
            }

            LOG_INFO(
                "bucket=" +
                std::to_string(bucketIndex) +
                " bucketAddress=" +
                toHex(bucketAddress) +
                " entryAddress=" +
                toHex(entryAddress));
        }

        LOG_INFO("===== ScoreMap Bucket Table End =====");
    }

    LOG_INFO(
        "Target score record was not found.");

    return false;
}

bool ScoreMapReader::scanAllScoreRecords(std::uintptr_t dataMapRva) const
{
    const std::uintptr_t dataMapAddress = module_.baseAddress() + dataMapRva;
    std::uintptr_t scoreMapStart = 0;

    if (!memoryReader_.read(dataMapAddress, scoreMapStart))
        return false;

    std::uintptr_t sentinel = 0;
    if (!memoryReader_.read(scoreMapStart + 0x20, sentinel))
        return false;

    // 1. 重複しない Unique な Entry アドレスだけを収集
    std::vector<std::uintptr_t> uniqueEntries;
    for (std::size_t i = 0; i < 132; ++i)
    {
        std::uintptr_t entryAddress = 0;
        if (memoryReader_.read(scoreMapStart + (i * sizeof(std::uintptr_t)), entryAddress))
        {
            if (entryAddress != 0 && entryAddress != sentinel)
            {
                // まだ登録されていないアドレスのみ保持
                if (std::find(uniqueEntries.begin(), uniqueEntries.end(), entryAddress) == uniqueEntries.end())
                {
                    uniqueEntries.push_back(entryAddress);
                }
            }
        }
    }

    LOG_INFO("Unique Entry Count: " + std::to_string(uniqueEntries.size()));

    // 2. 全 Entry 内の Record を走査して ID 範囲を調査
    std::uint32_t minSongId = 0xFFFFFFFF;
    std::uint32_t maxSongId = 0;
    std::size_t totalRecordsFound = 0;

    constexpr std::size_t recordSize = 0x40;
    constexpr std::size_t maxRecordsPerEntry = 64; // 安全限界値

    for (std::uintptr_t entry : uniqueEntries)
    {
        for (std::size_t recIdx = 0; recIdx < maxRecordsPerEntry; ++recIdx)
        {
            const std::uintptr_t recAddr = entry + (recIdx * recordSize);

            std::uint64_t packed = 0;
            if (!memoryReader_.read(recAddr + 0x10, packed) || packed == 0)
            {
                break; // 読み取り失敗または終端で次の Entry へ
            }

            const std::uint32_t diff = static_cast<std::uint32_t>(packed & 0xFFFFFFFFULL);
            const std::uint32_t songId = static_cast<std::uint32_t>(packed >> 32);

            // difficulty が 0~4 の範囲外ならレコード終端と判断
            if (diff > 4 || songId == 0)
            {
                break;
            }

            minSongId = std::min(minSongId, songId);
            maxSongId = std::max(maxSongId, songId);
            totalRecordsFound++;

            // 特定の検索対象があればログ表示
            if (songId == 19002)
            {
                LOG_INFO("[MATCH!] Found song_id 19002 at " + toHex(recAddr));
            }
        }
    }

    LOG_INFO("======== ScoreMap Record Dump Summary ========");
    LOG_INFO("Total Valid Records Scanned: " + std::to_string(totalRecordsFound));
    LOG_INFO("Detected Song ID Range     : " + std::to_string(minSongId) + " ~ " + std::to_string(maxSongId));
    LOG_INFO("==============================================");

    return true;
}

bool ScoreMapReader::dumpScoreRecordDetailed(std::uintptr_t recordAddress) const
{
    LOG_INFO("===== Detailed ScoreMap Record Dump =====");
    LOG_INFO("Record Address: " + toHex(recordAddress));

    constexpr std::size_t recordSize = 0x40;
    std::vector<std::uint8_t> buffer(recordSize);

    if (!memoryReader_.read(recordAddress, buffer.data(), recordSize))
    {
        LOG_ERROR("Failed to read record raw bytes.");
        return false;
    }

    // 16バイトごとに HEX & ASCII / 数値表現で出力
    for (std::size_t offset = 0; offset < recordSize; offset += 16)
    {
        std::ostringstream oss;
        oss << "+" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << offset << " | ";

        // 8-bit HEX 表示
        for (std::size_t i = 0; i < 16; ++i)
        {
            oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                << static_cast<int>(buffer[offset + i]) << " ";
        }

        LOG_INFO(oss.str());
    }

    // 主要な 16-bit 整数値のデコード（スコア等の検出用）
    LOG_INFO("--- 16-bit Integer Interpretation ---");
    for (std::size_t offset = 0x18; offset < 0x38; offset += 2)
    {
        std::uint16_t val16 = *reinterpret_cast<std::uint16_t *>(&buffer[offset]);
        std::ostringstream oss;
        oss << "Offset +" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << offset
            << " (16-bit): " << std::dec << val16 << " (0x" << std::hex << val16 << ")";
        LOG_INFO(oss.str());
    }

    LOG_INFO("=========================================");
    return true;
}

bool ScoreMapReader::dumpPointerTargets(std::uintptr_t recordAddress) const
{
    LOG_INFO("===== Dump Pointer Targets =====");

    std::uintptr_t ptr0 = 0;
    std::uintptr_t ptr8 = 0;

    if (!memoryReader_.read(recordAddress + 0x00, ptr0) ||
        !memoryReader_.read(recordAddress + 0x08, ptr8))
    {
        LOG_ERROR("Failed to read record pointers.");
        return false;
    }

    LOG_INFO("Record +0x00 Pointer Target: " + toHex(ptr0));
    if (ptr0 != 0)
    {
        std::vector<std::uint8_t> buf(0x20);
        if (memoryReader_.read(ptr0, buf.data(), buf.size()))
        {
            std::ostringstream oss;
            for (auto b : buf)
                oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
            LOG_INFO("  Data: " + oss.str());
        }
    }

    LOG_INFO("Record +0x08 Pointer Target: " + toHex(ptr8));
    if (ptr8 != 0)
    {
        std::vector<std::uint8_t> buf(0x20);
        if (memoryReader_.read(ptr8, buf.data(), buf.size()))
        {
            std::ostringstream oss;
            for (auto b : buf)
                oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
            LOG_INFO("  Data: " + oss.str());
        }
    }

    LOG_INFO("================================");
    return true;
}

bool ScoreMapReader::scanMemoryForSongId(std::uint32_t targetSongId) const
{
    LOG_INFO("======== WIDE MEMORY SCAN START ========");
    LOG_INFO("Searching for song_id: " + std::to_string(targetSongId) + " across module memory...");

    const std::uintptr_t baseAddr = module_.baseAddress();
    const std::size_t moduleSize = module_.imageSize(); // Moduleクラスにsize()が無い場合は固定値(例: 0x10000000)等に指定

    constexpr std::size_t step = 0x10; // アライメント考慮（16バイト単位）
    constexpr std::size_t recordSize = 0x40;

    // 大きめのバッファでチャンク読み込み（高速化のため 4MB 単位）
    constexpr std::size_t chunkSize = 0x400000;
    std::vector<std::uint8_t> buffer(chunkSize);

    std::size_t matchCount = 0;
    std::uintptr_t firstMatchAddr = 0;

    for (std::size_t offset = 0; offset < moduleSize; offset += chunkSize - recordSize)
    {
        const std::uintptr_t currentAddr = baseAddr + offset;
        const std::size_t bytesToRead = std::min(chunkSize, moduleSize - offset);

        if (!memoryReader_.read(currentAddr, buffer.data(), bytesToRead))
        {
            continue; // 読み込めない保護領域はスキップ
        }

        for (std::size_t i = 0; i <= bytesToRead - recordSize; i += step)
        {
            const std::uint8_t *pRecord = &buffer[i];

            // +0x10 (diff) と +0x14 (song_id) のチェック
            std::uint32_t diff = *reinterpret_cast<const std::uint32_t *>(pRecord + 0x10);
            std::uint32_t songId = *reinterpret_cast<const std::uint32_t *>(pRecord + 0x14);

            if (songId == targetSongId && diff <= 4)
            {
                const std::uintptr_t hitAddress = currentAddr + i;
                LOG_INFO("[HIT!] Found target record at: " + toHex(hitAddress) +
                         " | song_id=" + std::to_string(songId) +
                         " diff=" + std::to_string(diff));

                if (firstMatchAddr == 0)
                {
                    firstMatchAddr = hitAddress;
                }
                matchCount++;

                // 見つかったレコードの詳細を即座にダンプ
                dumpScoreRecordDetailed(hitAddress);
            }
        }
    }

    LOG_INFO("Scan complete. Total hits for song_id " + std::to_string(targetSongId) + ": " + std::to_string(matchCount));
    LOG_INFO("======== WIDE MEMORY SCAN END ========");

    return firstMatchAddr;
}

bool ScoreMapReader::dumpAllRecordsToTracker(
    std::uintptr_t dataMapRva,
    Tracker &tracker,
    const std::string &tsvFilePath) const
{
    LOG_INFO("======== DUMP ALL RECORDS TO TRACKER START ========");

    const std::uintptr_t dataMapAddress = module_.baseAddress() + dataMapRva;
    std::uintptr_t rootAddress = 0;

    // findAndDumpRecord と同じく DataMap からルートノードアドレスを取得
    if (!memoryReader_.read(dataMapAddress, &rootAddress, sizeof(rootAddress)) || rootAddress == 0)
    {
        LOG_ERROR("Failed to read rootAddress from DataMap.");
        return false;
    }

    constexpr std::size_t recordSize = 0x40;
    std::set<std::uintptr_t> visited;
    std::vector<std::uintptr_t> stack;
    std::size_t exportedCount = 0;

    std::uintptr_t current = rootAddress;

    // findAndDumpRecord と全く同じ順序（中順走査 / In-order traversal）で二分木を全走査
    while (current != 0 || !stack.empty())
    {
        while (current != 0 && visited.find(current) == visited.end())
        {
            visited.insert(current);
            stack.push_back(current);

            std::uintptr_t leftChild = 0;
            if (!memoryReader_.read(current + 0x00, &leftChild, sizeof(leftChild)))
            {
                leftChild = 0;
            }
            current = leftChild;
        }

        if (stack.empty())
            break;

        current = stack.back();
        stack.pop_back();

        // 64バイトを一括読み込み
        std::vector<std::uint8_t> buf(recordSize);
        if (memoryReader_.read(current, buf.data(), recordSize))
        {
            // フィールドデータの解析
            std::uint32_t diff      = *reinterpret_cast<const std::uint32_t *>(&buf[0x10]);
            std::uint32_t songId    = *reinterpret_cast<const std::uint32_t *>(&buf[0x14]);
            std::uint32_t playType  = *reinterpret_cast<const std::uint32_t *>(&buf[0x18]); // 0: SP, 1: DP
            std::uint32_t exScore   = *reinterpret_cast<const std::uint32_t *>(&buf[0x20]);
            std::uint32_t missCount = *reinterpret_cast<const std::uint32_t *>(&buf[0x24]);
            std::uint32_t clearLamp = *reinterpret_cast<const std::uint32_t *>(&buf[0x30]);

            // 有効な楽曲レコードか判定
            if (songId != 0 && diff <= 5)
            {
                // 0 -> P1(SP), 1 -> DP
                JudgePlayType pType = (playType == 1) ? JudgePlayType::DP : JudgePlayType::P1;

                PlayResult result{};
                result.songId = static_cast<std::int32_t>(songId);
                result.difficulty = static_cast<std::int32_t>(diff);
                result.playType = pType;
                result.clearLamp = static_cast<std::uint8_t>(clearLamp);
                result.exScore = static_cast<std::int32_t>(exScore);

                if (missCount != 0xFFFFFFFF && missCount < 99999)
                {
                    result.missCount = static_cast<std::int32_t>(missCount);
                    result.missCountValid = true;
                }
                else
                {
                    result.missCountValid = false;
                }

                result.timestamp = std::chrono::system_clock::now();
                tracker.update(result);
                exportedCount++;
            }
        }

        // 右の子ノードへ移動
        std::uintptr_t rightChild = 0;
        if (!memoryReader_.read(current + 0x08, &rightChild, sizeof(rightChild)))
        {
            rightChild = 0;
        }
        current = rightChild;
    }

    LOG_INFO("Exported " + std::to_string(exportedCount) + " total valid records to Tracker.");

    bool success = tracker.writeTsv(tsvFilePath);

    LOG_INFO("======== DUMP ALL RECORDS TO TRACKER END ========");
    return success;
}

bool ScoreMapReader::inspectScoreRecord(
    std::uintptr_t dataMapRva,
    std::uint32_t targetSongId,
    std::uint32_t targetDifficulty) const
{
    LOG_INFO("==================================================");
    LOG_INFO(" [INSPECT RECORD] Target: song_id=" + std::to_string(targetSongId) +
             " difficulty=" + std::to_string(targetDifficulty));

    const std::uintptr_t dataMapAddress = module_.baseAddress() + dataMapRva;
    std::uintptr_t scoreMapStart = 0;

    if (!memoryReader_.read(dataMapAddress, scoreMapStart))
    {
        LOG_ERROR("Failed to read scoreMapStart from DataMap.");
        return false;
    }

    std::uintptr_t sentinel = 0;
    if (!memoryReader_.read(scoreMapStart + 0x20, sentinel))
        return false;

    // 1. ユニークエントリの収集
    std::vector<std::uintptr_t> uniqueEntries;
    for (std::size_t i = 0; i < 132; ++i)
    {
        std::uintptr_t entryAddress = 0;
        if (memoryReader_.read(scoreMapStart + (i * sizeof(std::uintptr_t)), entryAddress))
        {
            if (entryAddress != 0 && entryAddress != sentinel)
            {
                if (std::find(uniqueEntries.begin(), uniqueEntries.end(), entryAddress) == uniqueEntries.end())
                {
                    uniqueEntries.push_back(entryAddress);
                }
            }
        }
    }

    // 2. 確定した 64 レコード / エントリの全走査
    constexpr std::size_t recordSize = 0x40;
    constexpr std::size_t recordsPerEntry = 64;
    std::uintptr_t targetRecordAddr = 0;

    for (std::uintptr_t entry : uniqueEntries)
    {
        for (std::size_t recIdx = 0; recIdx < recordsPerEntry; ++recIdx)
        {
            const std::uintptr_t recAddr = entry + (recIdx * recordSize);

            std::uint32_t diff = 0;
            std::uint32_t songId = 0;

            if (!memoryReader_.read(recAddr + 0x10, diff))
                continue;
            if (!memoryReader_.read(recAddr + 0x14, songId))
                continue;

            if (songId == targetSongId && diff == targetDifficulty)
            {
                targetRecordAddr = recAddr;
                break;
            }
        }
        if (targetRecordAddr != 0)
            break;
    }

    if (targetRecordAddr == 0)
    {
        LOG_ERROR("Record not found in current ScoreMap.");
        LOG_INFO("==================================================");
        return false;
    }

    LOG_INFO("Record Match Found at Address: " + toHex(targetRecordAddr));

    // 3. 64バイトの生データを一括取得
    std::vector<std::uint8_t> buf(recordSize);
    if (!memoryReader_.read(targetRecordAddr, buf.data(), recordSize))
    {
        LOG_ERROR("Failed to read record raw bytes.");
        return false;
    }

    // 4. フィールド仮説に基づく詳細デコード表示
    std::uintptr_t prevNode = *reinterpret_cast<std::uintptr_t *>(&buf[0x00]);
    std::uintptr_t nextNode = *reinterpret_cast<std::uintptr_t *>(&buf[0x08]);
    std::uint32_t recDiff = *reinterpret_cast<std::uint32_t *>(&buf[0x10]);
    std::uint32_t recSong = *reinterpret_cast<std::uint32_t *>(&buf[0x14]);
    std::uint32_t flags = *reinterpret_cast<std::uint32_t *>(&buf[0x18]);

    std::uint8_t clearType = buf[0x1C];
    std::uint8_t rankType = buf[0x1D];
    std::uint16_t exScore = *reinterpret_cast<std::uint16_t *>(&buf[0x1E]);
    std::uint16_t bpOrVal1 = *reinterpret_cast<std::uint16_t *>(&buf[0x20]);
    std::uint16_t val2 = *reinterpret_cast<std::uint16_t *>(&buf[0x22]);
    std::int32_t targetVal = *reinterpret_cast<std::int32_t *>(&buf[0x24]);
    std::uint64_t timestamp = *reinterpret_cast<std::uint64_t *>(&buf[0x38]);

    LOG_INFO("----------- Field Analysis ----------");
    LOG_INFO(" [+0x00] Prev Node Ptr : " + toHex(prevNode));
    LOG_INFO(" [+0x08] Next Node Ptr : " + toHex(nextNode));
    LOG_INFO(" [+0x10] Difficulty    : " + std::to_string(recDiff));
    LOG_INFO(" [+0x14] Song ID       : " + std::to_string(recSong));
    LOG_INFO(" [+0x18] Flags/Status  : 0x" + toHex(flags));
    LOG_INFO(" [+0x1C] Clear Lamp    : " + std::to_string(static_cast<int>(clearType)) + " (0x" + toHex(clearType) + ")");
    LOG_INFO(" [+0x1D] DJ Level/Rank : " + std::to_string(static_cast<int>(rankType)) + " (0x" + toHex(rankType) + ")");
    LOG_INFO(" [+0x1E] EX SCORE      : " + std::to_string(exScore) + " (0x" + toHex(exScore) + ")");
    LOG_INFO(" [+0x20] BP / MissCount: " + std::to_string(bpOrVal1));
    LOG_INFO(" [+0x22] Unknown 16bit : " + std::to_string(val2));
    LOG_INFO(" [+0x24] Target Compare: " + std::to_string(targetVal));
    LOG_INFO(" [+0x38] Timestamp Raw : 0x" + toHex(timestamp));
    LOG_INFO("-------------------------------------");

    // 5. 生データ 16バイトごとの HEX 出力
    LOG_INFO("----------- Raw HEX Dump -----------");
    for (std::size_t offset = 0; offset < recordSize; offset += 16)
    {
        std::ostringstream oss;
        oss << "+" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << offset << " | ";
        for (std::size_t i = 0; i < 16; ++i)
        {
            oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                << static_cast<int>(buf[offset + i]) << " ";
        }
        LOG_INFO(oss.str());
    }
    LOG_INFO("==================================================");

    return true;
}

void ScoreMapReader::findAndDumpRecord(std::uintptr_t dataMapRva, std::int32_t targetSongId) const
{
    LOG_INFO("==================================================");
    LOG_INFO("[FIND RECORD] Target song_id = " + std::to_string(targetSongId) + " の検索を開始します。");

    std::uintptr_t rootAddress = 0;
    if (!memoryReader_.read(module_.baseAddress() + dataMapRva, &rootAddress, sizeof(rootAddress)) || rootAddress == 0)
    {
        LOG_ERROR("DataMap のルートノードアドレス取得に失敗しました。");
        return;
    }

    std::vector<std::uintptr_t> stack;
    std::set<std::uintptr_t> visited; // 無限ループ防止用
    std::uintptr_t current = rootAddress;
    bool found = false;

    while (current != 0 || !stack.empty())
    {
        // 訪問済みまたはNULLでなければ左の子を探索
        while (current != 0 && visited.find(current) == visited.end())
        {
            visited.insert(current);
            stack.push_back(current);

            std::uintptr_t leftChild = 0;
            if (!memoryReader_.read(current + 0x00, &leftChild, sizeof(leftChild)))
            {
                leftChild = 0;
            }
            current = leftChild;
        }

        if (stack.empty())
            break;

        current = stack.back();
        stack.pop_back();

        // キー情報取得 (+0x10: difficulty, +0x14: songId)
        std::int32_t difficulty = 0;
        std::int32_t songId = 0;
        memoryReader_.read(current + 0x10, &difficulty, sizeof(difficulty));
        memoryReader_.read(current + 0x14, &songId, sizeof(songId));

        if (songId == targetSongId)
        {
            found = true;
            LOG_INFO("--------------------------------------------------");
            LOG_INFO("Match Found! Node Address: 0x" + toHex(current));
            LOG_INFO("Song ID: " + std::to_string(songId) + " / Difficulty: " + std::to_string(difficulty));

            // 64バイト (0x40) の Raw HEX Dump を出力
            std::array<std::uint8_t, 64> rawBytes{};
            if (memoryReader_.read(current, rawBytes.data(), rawBytes.size()))
            {
                LOG_INFO("----------- Raw HEX Dump (64 bytes) -----------");
                for (size_t i = 0; i < rawBytes.size(); i += 16)
                {
                    std::string line = "+" + toHex(static_cast<uint8_t>(i)) + " | ";
                    for (size_t j = 0; j < 16; ++j)
                    {
                        char buf[4];
                        snprintf(buf, sizeof(buf), "%02X ", rawBytes[i + j]);
                        line += buf;
                    }
                    LOG_INFO(line);
                }
            }
            LOG_INFO("--------------------------------------------------");
        }

        std::uintptr_t rightChild = 0;
        if (!memoryReader_.read(current + 0x08, &rightChild, sizeof(rightChild)))
        {
            rightChild = 0;
        }
        current = rightChild;
    }

    if (!found)
    {
        LOG_ERROR("song_id=" + std::to_string(targetSongId) + " のレコードは見つかりませんでした。");
    }

    LOG_INFO("==================================================");
}

bool ScoreMapReader::findMusicTableCandidateToTsv(const std::string &outputTsvPath) const
{
    LOG_INFO("======== MUSIC TABLE SEARCH START ========");

    const std::string targetTitle = "BroGamer";
    const std::int32_t targetSongId = 24080;

    const std::uintptr_t base = module_.baseAddress();
    const std::size_t size = module_.imageSize();

    if (base == 0 || size == 0)
    {
        LOG_ERROR("Module baseAddress or size is invalid.");
        return false;
    }

    std::vector<std::uint8_t> buffer(size);
    if (!memoryReader_.read(base, buffer.data(), size))
    {
        LOG_ERROR("Failed to read module memory for scanning.");
        return false;
    }

    // 1. "BroGamer" 文字列のアドレスを検索
    std::vector<std::uintptr_t> foundStringAddrs;
    for (std::size_t i = 0; i <= size - targetTitle.length(); ++i)
    {
        if (std::memcmp(&buffer[i], targetTitle.c_str(), targetTitle.length()) == 0)
        {
            foundStringAddrs.push_back(base + i);
        }
    }

    if (foundStringAddrs.empty())
    {
        LOG_ERROR("String 'BroGamer' was not found in module memory.");
        return false;
    }

    std::ofstream file(outputTsvPath, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        LOG_ERROR("Failed to open TSV file for writing: " + outputTsvPath);
        return false;
    }

    // TSV ヘッダー
    file << "match_type\ttitle_addr\ttitle_rva\tsong_id_addr\tsong_id_rva\toffset_from_title_bytes\n";

    std::size_t candidateCount = 0;

    for (std::uintptr_t strAddr : foundStringAddrs)
    {
        const std::size_t strRva = strAddr - base;

        // 【Pattern A: 直接埋め込み検証】
        // "BroGamer" 文字列自体の前後 ±0x1000 (4096バイト) の範囲に 24080 (0x5E10) が直接配置されているか？
        const std::size_t searchStartA = (strRva >= 0x1000) ? (strRva - 0x1000) : 0;
        const std::size_t searchEndA = std::min(strRva + 0x1000, size - sizeof(std::uint32_t));

        for (std::size_t j = searchStartA; j <= searchEndA; j += 4)
        {
            std::uint32_t val = 0;
            std::memcpy(&val, &buffer[j], sizeof(std::uint32_t));

            if (val == static_cast<std::uint32_t>(targetSongId))
            {
                std::uintptr_t songIdAddr = base + j;
                std::intptr_t offset = static_cast<std::intptr_t>(j) - static_cast<std::intptr_t>(strRva);

                file << "DIRECT_EMBEDDED\t"
                     << "0x" << toHex(strAddr) << '\t'
                     << "0x" << toHex(strRva) << '\t'
                     << "0x" << toHex(songIdAddr) << '\t'
                     << "0x" << toHex(j) << '\t'
                     << (offset >= 0 ? "+" : "") << offset << '\n';

                candidateCount++;
            }
        }

        // 【Pattern B: ポインタ参照 + 広範囲検索】
        // "BroGamer" を指しているポインタを探し、そのポインタの前後 ±0x1000 バイト内を探す
        for (std::size_t i = 0; i <= size - sizeof(std::uintptr_t); i += alignof(std::uintptr_t))
        {
            std::uintptr_t ptrVal = 0;
            std::memcpy(&ptrVal, &buffer[i], sizeof(std::uintptr_t));

            if (ptrVal == strAddr)
            {
                const std::size_t ptrRva = i;
                const std::size_t searchStartB = (ptrRva >= 0x1000) ? (ptrRva - 0x1000) : 0;
                const std::size_t searchEndB = std::min(ptrRva + 0x1000, size - sizeof(std::uint32_t));

                for (std::size_t j = searchStartB; j <= searchEndB; j += 4)
                {
                    std::uint32_t val = 0;
                    std::memcpy(&val, &buffer[j], sizeof(std::uint32_t));

                    if (val == static_cast<std::uint32_t>(targetSongId))
                    {
                        std::uintptr_t songIdAddr = base + j;
                        std::intptr_t offset = static_cast<std::intptr_t>(j) - static_cast<std::intptr_t>(ptrRva);

                        file << "POINTER_REFERENCED\t"
                             << "0x" << toHex(strAddr) << '\t'
                             << "0x" << toHex(strRva) << '\t'
                             << "0x" << toHex(songIdAddr) << '\t'
                             << "0x" << toHex(j) << '\t'
                             << (offset >= 0 ? "+" : "") << offset << '\n';

                        candidateCount++;
                    }
                }
            }
        }
    }

    file.close();
    LOG_INFO("Found " + std::to_string(candidateCount) + " candidate record(s). Saved to: " + outputTsvPath);
    LOG_INFO("======== MUSIC TABLE SEARCH END ========");

    return true;
}