#include "VersionDetector.h"
#include "Logger.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr char versionPrefix[] =
        "P2D:J:B:A:";

    constexpr std::size_t versionSuffixLength =
        10;

    constexpr std::size_t chunkSize =
        1024ULL * 1024ULL;

    constexpr std::size_t overlapSize =
        64;

    constexpr std::size_t prefixLength =
        sizeof(versionPrefix) - 1;

    constexpr std::size_t fullVersionLength =
        prefixLength +
        versionSuffixLength;
}

VersionDetector::VersionDetector(
    const MemoryReader& memoryReader
)
    : memoryReader_(memoryReader)
{
}

bool VersionDetector::isValidVersionSuffix(
    const std::string& suffix
)
{
    if (
        suffix.size() !=
        versionSuffixLength
    )
    {
        return false;
    }

    return std::all_of(
        suffix.begin(),
        suffix.end(),
        [](char character)
        {
            return std::isdigit(
                static_cast<unsigned char>(
                    character
                )
            ) != 0;
        }
    );
}

std::string VersionDetector::extractVersionAt(
    const std::uint8_t* data,
    std::size_t size
)
{
    if (
        data == nullptr ||
        size < fullVersionLength
    )
    {
        return {};
    }

    /*
     * Prefixの完全一致を確認。
     */
    for (
        std::size_t i = 0;
        i < prefixLength;
        ++i
    )
    {
        if (
            static_cast<char>(
                data[i]
            ) != versionPrefix[i]
        )
        {
            return {};
        }
    }

    /*
     * 10桁のVersion番号を取得。
     */
    std::string suffix;

    suffix.reserve(
        versionSuffixLength
    );

    for (
        std::size_t i = 0;
        i < versionSuffixLength;
        ++i
    )
    {
        suffix.push_back(
            static_cast<char>(
                data[
                    prefixLength + i
                ]
            )
        );
    }

    if (!isValidVersionSuffix(suffix))
    {
        return {};
    }

    return std::string(
        versionPrefix
    ) + suffix;
}

bool VersionDetector::findCandidates(
    const Module& module,
    std::vector<VersionCandidate>& candidates
) const
{
    candidates.clear();

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

    const std::uintptr_t moduleBaseAddress =
        module.baseAddress();

    const std::size_t searchSize =
        module.imageSize();

    LOG_INFO(
        "実際のモジュールサイズを検索範囲として使用します。"
    );

    LOG_INFO(
        "Using the actual module image size as the search range."
    );

    LOG_INFO(
        std::string(
            "Version search size: 0x"
        ) +
        [&]()
        {
            std::ostringstream stream;

            stream
                << std::hex
                << std::uppercase
                << searchSize;

            return stream.str();
        }()
    );

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

        const bool readResult =
            memoryReader_.readReadable(
                currentAddress,
                chunkBuffer.data(),
                readSize
            );

        if (!readResult)
        {
            LOG_INFO(
                "このチャンクから読み取り可能なデータを取得できませんでした。"
            );

            LOG_INFO(
                "No readable data was obtained from this chunk."
            );
        }

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
            fullVersionLength
        )
        {
            for (
                std::size_t i = 0;
                i + fullVersionLength <=
                    searchBuffer.size();
                ++i
            )
            {
                const std::string candidate =
                    extractVersionAt(
                        searchBuffer.data() + i,
                        fullVersionLength
                    );

                if (candidate.empty())
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

                VersionCandidate versionCandidate{};

                versionCandidate.version =
                    candidate;

                versionCandidate.rva =
                    static_cast<std::size_t>(
                        signedOffset
                    );

                /*
                 * 同一Version・同一RVAの重複を防ぐ。
                 */
                const bool alreadyExists =
                    std::any_of(
                        candidates.begin(),
                        candidates.end(),
                        [&](const VersionCandidate& existing)
                        {
                            return
                                existing.version ==
                                    versionCandidate.version &&
                                existing.rva ==
                                    versionCandidate.rva;
                        }
                    );

                if (alreadyExists)
                {
                    continue;
                }

                candidates.push_back(
                    versionCandidate
                );

                LOG_INFO(
                    "Version候補を検出しました: " +
                    candidate
                );

                LOG_INFO(
                    "Detected version candidate: " +
                    candidate
                );

                LOG_INFO(
                    [&]()
                    {
                        std::ostringstream stream;

                        stream
                            << "Version RVA: 0x"
                            << std::hex
                            << std::uppercase
                            << versionCandidate.rva;

                        return stream.str();
                    }()
                );
            }
        }

        /*
         * 次のチャンクへ渡す末尾64byte。
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

    if (candidates.empty())
    {
        LOG_ERROR(
            "INFINITASのVersion文字列を検出できませんでした。"
        );

        LOG_ERROR(
            "Failed to detect the INFINITAS version string."
        );

        return false;
    }

    LOG_INFO(
        "検出したVersion候補数: " +
        std::to_string(
            candidates.size()
        )
    );

    LOG_INFO(
        "Number of detected version candidates: " +
        std::to_string(
            candidates.size()
        )
    );

    return true;
}

bool VersionDetector::detect(
    const Module& module,
    std::string& version
) const
{
    version.clear();

    std::vector<VersionCandidate> candidates;

    if (!findCandidates(
            module,
            candidates
        ))
    {
        return false;
    }

    const VersionCandidate& selected =
        candidates.back();

    version =
        selected.version;

    LOG_INFO(
        "採用するINFINITASバージョン: " +
        selected.version
    );

    LOG_INFO(
        "Selected INFINITAS version: " +
        selected.version
    );

    LOG_INFO(
        [&]()
        {
            std::ostringstream stream;

            stream
                << "Selected version RVA: 0x"
                << std::hex
                << std::uppercase
                << selected.rva;

            return stream.str();
        }()
    );

    return true;
}