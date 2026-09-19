#pragma once

#include "JudgeData.h"
#include "OffsetTypes.h"
#include "PatternScanner.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct OffsetSearchResult
{
    OffsetType type;
    std::size_t rva = 0;
};

class OffsetSearcher
{
public:
    explicit OffsetSearcher(
        const PatternScanner &patternScanner);

    bool searchSongList(
        const Module &module,
        OffsetSearchResult &result) const;

    bool searchUnlockData(
        const Module &module,
        OffsetSearchResult &result) const;

    bool searchDataMap(
        const Module &module,
        OffsetSearchResult &result) const;

    bool searchJudgeData(
        const Module &module,
        const JudgeSnapshot &snapshot,
        OffsetSearchResult &result) const;

private:
    static Pattern createSongListPattern();
    static Pattern createUnlockDataPattern();
    static Pattern createDataMapPattern();

    static Pattern createJudgeP1Pattern(
        const JudgeSnapshot &snapshot);

    static Pattern createJudgeP2Pattern(
        const JudgeSnapshot &snapshot);

    const PatternScanner &patternScanner_;
};