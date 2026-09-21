#pragma once

#include "MemoryReader.h"
#include "Module.h"
#include "Tracker.h"

#include <cstdint>

class ScoreMapReader
{
public:
    ScoreMapReader(
        const MemoryReader &memoryReader,
        const Module &module);

    bool dumpDataMapNeighborhood(
        std::uintptr_t dataMapRva) const;

    bool dumpScoreMapStart(
        std::uintptr_t dataMapRva) const;

    bool dumpScoreMapEntryCandidates(
        std::uintptr_t dataMapRva) const;

    bool dumpScoreMapEntry(
        std::uintptr_t dataMapRva,
        std::size_t bucketIndex) const;

    bool dumpScoreRecord32(
        std::uintptr_t recordAddress) const;

    bool findScoreRecord(
        std::uintptr_t dataMapRva,
        int targetSongId,
        int targetDifficulty) const;

    bool scanAllScoreRecords(
        std::uintptr_t dataMapRva) const;

    bool dumpScoreRecordDetailed(
        std::uintptr_t recordAddress) const;

    bool dumpPointerTargets(
        std::uintptr_t recordAddress) const;

    bool scanMemoryForSongId(
        std::uint32_t targetSongId) const;

    bool dumpAllRecordsToTracker(
        std::uintptr_t dataMapRva,
        Tracker &tracker,
        const std::string &tsvFilePath,
    const MusicTableReader &musicTableReader) const;

    bool inspectScoreRecord(
        std::uintptr_t dataMapRva,
        std::uint32_t targetSongId,
        std::uint32_t targetDifficulty) const;

    void findAndDumpRecord(std::uintptr_t dataMapRva, std::int32_t targetSongId) const;

    bool findMusicTableCandidateToTsv(const std::string &outputTsvPath = "music_table_candidates.tsv") const;

private:
    const MemoryReader &memoryReader_;
    const Module &module_;
};