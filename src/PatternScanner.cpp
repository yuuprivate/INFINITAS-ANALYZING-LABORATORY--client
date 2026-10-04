#include "PatternScanner.h"
#include "Logger.h"

#include "MemoryReader.h"
#include "Module.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <vector>

namespace
{
    constexpr std::size_t chunkSize =
        1024ULL * 1024ULL;
}

bool Pattern::isValid() const
{
    if (bytes.empty())
    {
        return false;
    }

    /*
     * maskを省略した場合は、
     * 全byteを比較対象として扱う。
     */
    if (mask.empty())
    {
        return true;
    }

    return mask.size() == bytes.size();
}

PatternScanner::PatternScanner(
    const MemoryReader& memoryReader
)
    : memoryReader_(memoryReader)
{
    LOG_INFO(
        "PatternScanner を初期化しました。"
    );

    LOG_INFO(
        "PatternScanner initialized successfully."
    );
}

bool PatternScanner::matchesAt(
    const std::uint8_t* data,
    const Pattern& pattern
)
{
    if (data == nullptr ||
        !pattern.isValid())
    {
        return false;
    }

    for (
        std::size_t i = 0;
        i < pattern.bytes.size();
        ++i
    )
    {
        const bool compareByte =
            pattern.mask.empty()
                ? true
                : pattern.mask[i];

        if (!compareByte)
        {
            continue;
        }

        if (data[i] != pattern.bytes[i])
        {
            return false;
        }
    }

    return true;
}

bool PatternScanner::findFirst(
    const Module& module,
    const Pattern& pattern,
    PatternMatch& match
) const
{
    match = {};

    std::vector<PatternMatch> matches;

    if (!findAll(
            module,
            pattern,
            matches
        ))
    {
        return false;
    }

    match = matches.front();

    return true;
}

bool PatternScanner::findAll(
    const Module& module,
    const Pattern& pattern,
    std::vector<PatternMatch>& matches
) const
{
    matches.clear();

    if (!module.isLoaded())
    {
        LOG_ERROR(
            "モジュールがロードされていません。"
        );

        LOG_ERROR(
            "The module has not been loaded."
        );

        return false;
    }

    if (!pattern.isValid())
    {
        LOG_ERROR(
            "無効なPatternが指定されました。"
        );

        LOG_ERROR(
            "An invalid pattern was specified."
        );

        return false;
    }

    const std::size_t patternSize =
        pattern.bytes.size();

    const std::size_t searchSize =
        module.imageSize();

    const std::uintptr_t moduleBaseAddress =
        module.baseAddress();

    if (searchSize < patternSize)
    {
        return true;
    }

    /*
     * Patternがチャンク境界を跨ぐ可能性に対応するため、
     * patternSize - 1 byteをoverlapさせる。
     */
    const std::size_t overlapSize =
        patternSize > 1
            ? patternSize - 1
            : 0;

    std::vector<std::uint8_t> chunkBuffer(
        chunkSize
    );

    std::vector<std::uint8_t> previousTail;

    for (
        std::size_t chunkOffset = 0;
        chunkOffset < searchSize;
        chunkOffset += chunkSize
    )
    {
        const std::size_t remaining =
            searchSize - chunkOffset;

        const std::size_t readSize =
            std::min(
                chunkSize,
                remaining
            );

        std::fill(
            chunkBuffer.begin(),
            chunkBuffer.end(),
            0
        );

        const std::uintptr_t currentAddress =
            moduleBaseAddress +
            static_cast<std::uintptr_t>(
                chunkOffset
            );

        memoryReader_.readReadable(
            currentAddress,
            chunkBuffer.data(),
            readSize
        );

        /*
         * [前チャンク末尾][現在チャンク]
         */
        std::vector<std::uint8_t> searchBuffer;

        searchBuffer.reserve(
            previousTail.size() +
            readSize
        );

        searchBuffer.insert(
            searchBuffer.end(),
            previousTail.begin(),
            previousTail.end()
        );

        searchBuffer.insert(
            searchBuffer.end(),
            chunkBuffer.begin(),
            chunkBuffer.begin() +
                static_cast<std::ptrdiff_t>(
                    readSize
                )
        );

        if (
            searchBuffer.size() >=
            patternSize
        )
        {
            for (
                std::size_t i = 0;
                i + patternSize <=
                    searchBuffer.size();
                ++i
            )
            {
                if (!matchesAt(
                        searchBuffer.data() + i,
                        pattern
                    ))
                {
                    continue;
                }

                const std::ptrdiff_t signedOffset =
                    static_cast<std::ptrdiff_t>(
                        chunkOffset
                    ) -
                    static_cast<std::ptrdiff_t>(
                        previousTail.size()
                    ) +
                    static_cast<std::ptrdiff_t>(
                        i
                    );

                if (signedOffset < 0)
                {
                    continue;
                }

                PatternMatch result{};

                result.rva =
                    static_cast<std::size_t>(
                        signedOffset
                    );

                /*
                 * overlapによる同一matchの重複を防ぐ。
                 */
                const bool alreadyExists =
                    std::any_of(
                        matches.begin(),
                        matches.end(),
                        [&](const PatternMatch& existing)
                        {
                            return existing.rva ==
                                result.rva;
                        }
                    );

                if (alreadyExists)
                {
                    continue;
                }

                matches.push_back(result);
            }
        }

        /*
         * 次のチャンクへ渡す末尾。
         */
        previousTail.clear();

        const std::size_t tailSize =
            std::min(
                overlapSize,
                readSize
            );

        if (tailSize > 0)
        {
            previousTail.insert(
                previousTail.end(),
                chunkBuffer.begin() +
                    static_cast<std::ptrdiff_t>(
                        readSize - tailSize
                    ),
                chunkBuffer.begin() +
                    static_cast<std::ptrdiff_t>(
                        readSize
                    )
            );
        }
    }

    LOG_INFO(
        "Pattern search completed. Matches: " +
        std::to_string(
            matches.size()
        )
    );

    return !matches.empty();
}

bool PatternScanner::findAllInRange(
    const Module& module,
    const Pattern& pattern,
    std::size_t centerRva,
    std::size_t rangeSize,
    std::vector<PatternMatch>& matches
) const
{
    matches.clear();

    if (!module.isLoaded())
    {
        LOG_ERROR(
            "モジュールがロードされていません。"
        );

        LOG_ERROR(
            "The module has not been loaded."
        );

        return false;
    }

    if (!pattern.isValid())
    {
        LOG_ERROR(
            "無効なPatternが指定されました。"
        );

        LOG_ERROR(
            "An invalid pattern was specified."
        );

        return false;
    }

    const std::size_t moduleSize =
        module.imageSize();

    const std::size_t patternSize =
        pattern.bytes.size();

    if (moduleSize < patternSize)
    {
        return false;
    }

    /*
     * center - rangeSize が0未満にならないようにする。
     */
    const std::size_t startRva =
        centerRva > rangeSize
            ? centerRva - rangeSize
            : 0;

    /*
     * center + rangeSize が
     * moduleSizeを超えないようにする。
     */
    const std::size_t requestedEnd =
        centerRva + rangeSize;

    const std::size_t endRva =
        requestedEnd < moduleSize
            ? requestedEnd
            : moduleSize;

    if (startRva >= endRva)
    {
        return false;
    }

    const std::size_t searchSize =
        endRva - startRva;

    /*
     * patternがチャンク境界を跨げるように
     * patternSize - 1 byteをOverlap。
     */
    const std::size_t overlapSize =
        patternSize > 1
            ? patternSize - 1
            : 0;

    constexpr std::size_t chunkSize =
        1024ULL * 1024ULL;

    std::vector<std::uint8_t> chunkBuffer(
        chunkSize
    );

    std::vector<std::uint8_t> previousTail;

    std::size_t processedOffset = 0;

    while (processedOffset < searchSize)
    {
        const std::size_t remaining =
            searchSize - processedOffset;

        const std::size_t readSize =
            std::min(
                chunkSize,
                remaining
            );

        std::fill(
            chunkBuffer.begin(),
            chunkBuffer.end(),
            0
        );

        const std::size_t chunkRva =
            startRva + processedOffset;

        const std::uintptr_t address =
            module.baseAddress() +
            static_cast<std::uintptr_t>(
                chunkRva
            );

        memoryReader_.readReadable(
            address,
            chunkBuffer.data(),
            readSize
        );

        std::vector<std::uint8_t> searchBuffer;

        searchBuffer.reserve(
            previousTail.size() +
            readSize
        );

        searchBuffer.insert(
            searchBuffer.end(),
            previousTail.begin(),
            previousTail.end()
        );

        searchBuffer.insert(
            searchBuffer.end(),
            chunkBuffer.begin(),
            chunkBuffer.begin() +
                static_cast<std::ptrdiff_t>(
                    readSize
                )
        );

        if (
            searchBuffer.size() >=
            patternSize
        )
        {
            for (
                std::size_t i = 0;
                i + patternSize <=
                    searchBuffer.size();
                ++i
            )
            {
                if (!matchesAt(
                        searchBuffer.data() + i,
                        pattern
                    ))
                {
                    continue;
                }

                const std::ptrdiff_t signedRva =
                    static_cast<std::ptrdiff_t>(
                        chunkRva
                    ) -
                    static_cast<std::ptrdiff_t>(
                        previousTail.size()
                    ) +
                    static_cast<std::ptrdiff_t>(
                        i
                    );

                if (signedRva < 0)
                {
                    continue;
                }

                const std::size_t matchRva =
                    static_cast<std::size_t>(
                        signedRva
                    );

                if (matchRva < startRva ||
                    matchRva + patternSize > endRva)
                {
                    continue;
                }

                const bool alreadyExists =
                    std::any_of(
                        matches.begin(),
                        matches.end(),
                        [&](const PatternMatch& existing)
                        {
                            return existing.rva ==
                                matchRva;
                        }
                    );

                if (alreadyExists)
                {
                    continue;
                }

                PatternMatch match{};

                match.rva =
                    matchRva;

                matches.push_back(
                    match
                );
            }
        }

        previousTail.clear();

        const std::size_t tailSize =
            std::min(
                overlapSize,
                readSize
            );

        if (tailSize > 0)
        {
            previousTail.insert(
                previousTail.end(),
                chunkBuffer.begin() +
                    static_cast<std::ptrdiff_t>(
                        readSize - tailSize
                    ),
                chunkBuffer.begin() +
                    static_cast<std::ptrdiff_t>(
                        readSize
                    )
            );
        }

        processedOffset +=
            readSize;
    }

    LOG_INFO(
        "Range pattern search completed. Matches: " +
        std::to_string(
            matches.size()
        )
    );

    return !matches.empty();
}