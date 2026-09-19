#include "OffsetSearcher.h"
#include "Logger.h"

#include <cstdint>
#include <sstream>
#include <vector>

namespace
{
    void appendInt32(
        Pattern &pattern,
        std::int32_t value)
    {
        const auto *bytes =
            reinterpret_cast<const std::uint8_t *>(
                &value);

        for (
            std::size_t i = 0;
            i < sizeof(value);
            ++i)
        {
            pattern.bytes.push_back(
                bytes[i]);

            pattern.mask.push_back(
                true);
        }
    }

    void appendUInt64(
        Pattern &pattern,
        std::uint64_t value)
    {
        for (std::size_t i = 0; i < sizeof(value); ++i)
        {
            pattern.bytes.push_back(
                static_cast<std::uint8_t>(
                    (value >> (i * 8)) & 0xFF));

            pattern.mask.push_back(
                true);
        }
    }

    void appendAscii(
        Pattern &pattern,
        const char *text)
    {
        while (*text != '\0')
        {
            pattern.bytes.push_back(
                static_cast<std::uint8_t>(
                    *text));

            pattern.mask.push_back(
                true);

            ++text;
        }
    }
}

OffsetSearcher::OffsetSearcher(
    const PatternScanner &patternScanner)
    : patternScanner_(patternScanner)
{
    LOG_INFO(
        "OffsetSearcher を初期化しました。");

    LOG_INFO(
        "OffsetSearcher initialized successfully.");
}

Pattern OffsetSearcher::createSongListPattern()
{
    Pattern pattern;

    appendAscii(
        pattern,
        "5.1.1.");

    return pattern;
}

Pattern OffsetSearcher::createUnlockDataPattern()
{
    Pattern pattern;

    appendInt32(
        pattern,
        1000);

    appendInt32(
        pattern,
        1);

    appendInt32(
        pattern,
        462);

    return pattern;
}

Pattern OffsetSearcher::createDataMapPattern()
{
    Pattern pattern;

    /*
     * 現在のINFINITAS 2026080500で確認した
     * DataMap周辺の実データ。
     *
     * DataMap + 0x18:
     *   QWORD[0] = 0x7FFF
     *   QWORD[1] = 0x8000
     *   QWORD[2] = 0x0000
     */
    appendUInt64(
        pattern,
        0x7FFF);

    appendUInt64(
        pattern,
        0x8000);

    appendUInt64(
        pattern,
        0);

    return pattern;
}

Pattern OffsetSearcher::createJudgeP1Pattern(
    const JudgeSnapshot &snapshot)
{
    Pattern pattern;

    appendInt32(
        pattern,
        snapshot.p1Pgreat);

    appendInt32(
        pattern,
        snapshot.p1Great);

    appendInt32(
        pattern,
        snapshot.p1Good);

    appendInt32(
        pattern,
        snapshot.p1Bad);

    appendInt32(
        pattern,
        snapshot.p1Poor);

    /*
     * P2側は0。
     */
    appendInt32(pattern, 0);
    appendInt32(pattern, 0);
    appendInt32(pattern, 0);
    appendInt32(pattern, 0);
    appendInt32(pattern, 0);

    appendInt32(
        pattern,
        snapshot.p1ComboBreak);

    appendInt32(
        pattern,
        0);

    appendInt32(
        pattern,
        snapshot.p1Fast);

    appendInt32(
        pattern,
        0);

    appendInt32(
        pattern,
        snapshot.p1Slow);

    appendInt32(
        pattern,
        0);

    return pattern;
}

Pattern OffsetSearcher::createJudgeP2Pattern(
    const JudgeSnapshot &snapshot)
{
    Pattern pattern;

    /*
     * P1側は0。
     */
    appendInt32(pattern, 0);
    appendInt32(pattern, 0);
    appendInt32(pattern, 0);
    appendInt32(pattern, 0);
    appendInt32(pattern, 0);

    appendInt32(
        pattern,
        snapshot.p2Pgreat);

    appendInt32(
        pattern,
        snapshot.p2Great);

    appendInt32(
        pattern,
        snapshot.p2Good);

    appendInt32(
        pattern,
        snapshot.p2Bad);

    appendInt32(
        pattern,
        snapshot.p2Poor);

    appendInt32(
        pattern,
        0);

    appendInt32(
        pattern,
        snapshot.p2ComboBreak);

    appendInt32(
        pattern,
        0);

    appendInt32(
        pattern,
        snapshot.p2Fast);

    appendInt32(
        pattern,
        0);

    appendInt32(
        pattern,
        snapshot.p2Slow);

    return pattern;
}

bool OffsetSearcher::searchSongList(
    const Module &module,
    OffsetSearchResult &result) const
{
    result = {};

    const Pattern pattern =
        createSongListPattern();

    std::vector<PatternMatch> matches;

    if (!patternScanner_.findAll(
            module,
            pattern,
            matches))
    {
        LOG_ERROR(
            "SongListパターンが見つかりませんでした。");

        LOG_ERROR(
            "SongList pattern was not found.");

        return false;
    }

    LOG_INFO(
        "SongList pattern matches: " +
        std::to_string(
            matches.size()));

    for (const auto &match : matches)
    {
        LOG_INFO(
            [&]()
            {
                std::ostringstream stream;

                stream
                    << "SongList candidate RVA: 0x"
                    << std::hex
                    << std::uppercase
                    << match.rva;

                return stream.str();
            }());
    }

    /*
     * 現時点では最初の候補を採用。
     *
     * 今後、候補の周辺データを検証して
     * 正しい候補を絞り込む。
     */
    result.type =
        OffsetType::SongList;

    result.rva =
        matches.front().rva;

    LOG_INFO(
        [&]()
        {
            std::ostringstream stream;

            stream
                << "SongList offset resolved: 0x"
                << std::hex
                << std::uppercase
                << result.rva;

            return stream.str();
        }());

    return true;
}

bool OffsetSearcher::searchUnlockData(
    const Module &module,
    OffsetSearchResult &result) const
{
    result = {};

    const Pattern pattern =
        createUnlockDataPattern();

    std::vector<PatternMatch> matches;

    if (!patternScanner_.findAll(
            module,
            pattern,
            matches))
    {
        LOG_ERROR(
            "UnlockDataパターンが見つかりませんでした。");

        LOG_ERROR(
            "UnlockData pattern was not found.");

        return false;
    }

    LOG_INFO(
        "UnlockData pattern matches: " +
        std::to_string(
            matches.size()));

    for (const auto &match : matches)
    {
        LOG_INFO(
            [&]()
            {
                std::ostringstream stream;

                stream
                    << "UnlockData candidate RVA: 0x"
                    << std::hex
                    << std::uppercase
                    << match.rva;

                return stream.str();
            }());
    }

    /*
     * 現時点では最初の候補を採用。
     *
     * 今後、Refluxと同様に候補周辺のデータを
     * 検証して正しい候補に絞り込む。
     */
    result.type =
        OffsetType::UnlockData;

    result.rva =
        matches.front().rva;

    LOG_INFO(
        [&]()
        {
            std::ostringstream stream;

            stream
                << "UnlockData offset resolved: 0x"
                << std::hex
                << std::uppercase
                << result.rva;

            return stream.str();
        }());

    return true;
}

bool OffsetSearcher::searchDataMap(
    const Module &module,
    OffsetSearchResult &result) const
{
    result = {};

    const Pattern pattern =
        createDataMapPattern();

    /*
     * 現在の既知DataMap Offset。
     *
     * offsets.txt:
     * DataMap = 0x35BAB88
     *
     * TODO:
     * 将来的にはOffsetManagerから取得する。
     */
    constexpr std::size_t knownOffset =
        0x035BAB88;

    /*
     * Refluxの探索方式に合わせて、
     * 初期2MBから開始し、必要なら倍々で拡大する。
     */
    constexpr std::size_t initialSearchSpace =
        2ULL * 1024ULL * 1024ULL;

    constexpr std::size_t maxSearchSpace =
        300ULL * 1024ULL * 1024ULL;

    constexpr std::size_t backOffset =
        3 * sizeof(std::uint64_t);

    std::size_t searchSpace =
        initialSearchSpace;

    while (searchSpace <= maxSearchSpace)
    {
        LOG_INFO(
            [&]()
            {
                std::ostringstream stream;

                stream
                    << "Searching DataMap around RVA 0x"
                    << std::hex
                    << std::uppercase
                    << knownOffset
                    << ", range ±0x"
                    << searchSpace;

                return stream.str();
            }());

        std::vector<PatternMatch> matches;

        const bool found =
            patternScanner_.findAllInRange(
                module,
                pattern,
                knownOffset,
                searchSpace,
                matches);

        if (found)
        {
            LOG_INFO(
                "DataMap pattern matches in range: " +
                std::to_string(
                    matches.size()));

            /*
             * 候補を確認する。
             */
            for (const auto &match : matches)
            {
                /*
                 * DataMapは8-byte単位の構造を前提としているため、
                 * パターン開始位置も8-byte境界にある候補だけを
                 * 検証対象とする。
                 */
                if (
                    match.rva %
                        sizeof(std::uint64_t) !=
                    0)
                {
                    LOG_INFO(
                        [&]()
                        {
                            std::ostringstream stream;

                            stream
                                << "Ignoring unaligned DataMap candidate RVA: 0x"
                                << std::hex
                                << std::uppercase
                                << match.rva;

                            return stream.str();
                        }());

                    continue;
                }

                LOG_INFO(
                    [&]()
                    {
                        std::ostringstream stream;

                        stream
                            << "DataMap pattern candidate RVA: 0x"
                            << std::hex
                            << std::uppercase
                            << match.rva;

                        return stream.str();
                    }());

                if (match.rva < backOffset)
                {
                    continue;
                }

                const std::size_t dataMapRva =
                    match.rva -
                    backOffset;

                LOG_INFO(
                    [&]()
                    {
                        std::ostringstream stream;

                        stream
                            << "DataMap resolved candidate RVA: 0x"
                            << std::hex
                            << std::uppercase
                            << dataMapRva;

                        return stream.str();
                    }());

                /*
                 * 現段階では、まず候補のうち
                 * 8-byte境界にあるものを採用する。
                 *
                 * 次の段階で周辺構造を検証して、
                 * 候補を一意に絞り込む。
                 */
                result.type =
                    OffsetType::DataMap;

                result.rva =
                    dataMapRva;

                LOG_INFO(
                    [&]()
                    {
                        std::ostringstream stream;

                        stream
                            << "DataMap offset resolved: 0x"
                            << std::hex
                            << std::uppercase
                            << result.rva;

                        return stream.str();
                    }());

                return true;
            }
        }

        /*
         * 次の探索範囲へ拡大。
         */
        if (searchSpace >
            maxSearchSpace / 2)
        {
            break;
        }

        searchSpace *= 2;
    }

    LOG_ERROR(
        "DataMapの有効な候補が見つかりませんでした。");

    LOG_ERROR(
        "No valid DataMap candidate was found.");

    return false;
}

bool OffsetSearcher::searchJudgeData(
    const Module &module,
    const JudgeSnapshot &snapshot,
    OffsetSearchResult &result) const
{
    result = {};

    const Pattern p1Pattern =
        createJudgeP1Pattern(
            snapshot);

    const Pattern p2Pattern =
        createJudgeP2Pattern(
            snapshot);

    /*
     * 現在の既知JudgeData Offset。
     *
     * 2026080500:
     * RVA = 0x288618C
     */
    constexpr std::size_t knownOffset =
        0x0288618C;

    constexpr std::size_t initialSearchSpace =
        2ULL * 1024ULL * 1024ULL;

    constexpr std::size_t maxSearchSpace =
        300ULL * 1024ULL * 1024ULL;

    std::size_t searchSpace =
        initialSearchSpace;

    while (searchSpace <= maxSearchSpace)
    {
        LOG_INFO(
            [&]()
            {
                std::ostringstream stream;

                stream
                    << "Searching JudgeData around RVA 0x"
                    << std::hex
                    << std::uppercase
                    << knownOffset
                    << ", range ±0x"
                    << searchSpace;

                return stream.str();
            }());

        /*
         * まずP1パターンを探索。
         */
        std::vector<PatternMatch> p1Matches;

        if (patternScanner_.findAllInRange(
                module,
                p1Pattern,
                knownOffset,
                searchSpace,
                p1Matches))
        {
            LOG_INFO(
                "JudgeData P1 pattern matches: " +
                std::to_string(
                    p1Matches.size()));

            if (!p1Matches.empty())
            {
                result.type =
                    OffsetType::JudgeData;

                result.rva =
                    p1Matches.front().rva;

                LOG_INFO(
                    [&]()
                    {
                        std::ostringstream stream;

                        stream
                            << "JudgeData P1 offset resolved: 0x"
                            << std::hex
                            << std::uppercase
                            << result.rva;

                        return stream.str();
                    }());

                return true;
            }
        }

        /*
         * 次にP2パターンを探索。
         */
        std::vector<PatternMatch> p2Matches;

        if (patternScanner_.findAllInRange(
                module,
                p2Pattern,
                knownOffset,
                searchSpace,
                p2Matches))
        {
            LOG_INFO(
                "JudgeData P2 pattern matches: " +
                std::to_string(
                    p2Matches.size()));

            if (!p2Matches.empty())
            {
                result.type =
                    OffsetType::JudgeData;

                result.rva =
                    p2Matches.front().rva;

                LOG_INFO(
                    [&]()
                    {
                        std::ostringstream stream;

                        stream
                            << "JudgeData P2 offset resolved: 0x"
                            << std::hex
                            << std::uppercase
                            << result.rva;

                        return stream.str();
                    }());

                return true;
            }
        }

        if (
            searchSpace >
            maxSearchSpace / 2)
        {
            break;
        }

        searchSpace *= 2;
    }

    LOG_ERROR(
        "JudgeDataパターンが見つかりませんでした。");

    LOG_ERROR(
        "JudgeData pattern was not found.");

    return false;
}