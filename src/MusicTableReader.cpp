#include "MusicTableReader.h"
#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <array>

namespace
{
    template <typename T>
    std::string toHex(T value)
    {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << static_cast<std::uint64_t>(value);
        return oss.str();
    }
}

MusicTableReader::MusicTableReader(const MemoryReader &memoryReader, std::uintptr_t baseAddress, std::size_t moduleSize)
    : memoryReader_(memoryReader), baseAddress_(baseAddress), moduleSize_(moduleSize)
{
}

bool MusicTableReader::scanAndBuildMusicMap(std::uintptr_t searchStartRva, std::size_t scanSize)
{
    LOG_INFO("======== MUSIC TABLE BUILDER START (DEBUG MODE) ========");

    if (baseAddress_ == 0 || moduleSize_ == 0)
    {
        LOG_ERROR("Invalid module information.");
        return false;
    }

    const std::size_t safeScanSize = std::min(scanSize, moduleSize_);

    cachedBuffer_.resize(safeScanSize);
    if (!memoryReader_.read(baseAddress_, cachedBuffer_.data(), safeScanSize))
    {
        LOG_ERROR("Failed to read module memory.");
        return false;
    }

    musicMap_.clear();
    notesMap_.clear();
    titleMap_.clear();

    const std::size_t ENTRY_SIZE = 1840; // 0x730
    const std::size_t TITLE_OFFSET = 0;
    const std::size_t NOTES_OFFSET = 624;
    const std::size_t ID_OFFSET = 1200;

    const std::size_t anchorRva = 0x3340E80;
    LOG_INFO("Anchor RVA: 0x" + toHex(anchorRva));

    // 安全なテーブル開始位置の特定（遡りすぎ防止の安全弁を追加）
    std::size_t tableStartRva = anchorRva;
    int backwardCount = 0;
    while (tableStartRva >= ENTRY_SIZE && backwardCount < 5000) // 最大5000曲分まで安全に遡る
    {
        std::size_t prevRva = tableStartRva - ENTRY_SIZE;
        if (prevRva >= safeScanSize)
            break;

        char firstChar = static_cast<char>(cachedBuffer_[prevRva + TITLE_OFFSET]);
        std::int32_t prevSongId = 0;
        std::memcpy(&prevSongId, &cachedBuffer_[prevRva + ID_OFFSET], sizeof(std::int32_t));

        // 厳しすぎる条件を少し緩和: タイトルが完全にゴミデータでなければ遡る
        if (prevSongId > 0 && prevSongId < 200000)
        {
            tableStartRva = prevRva;
            backwardCount++;
        }
        else
        {
            break;
        }
    }

    LOG_INFO("Table Start RVA identified at: 0x" + toHex(tableStartRva) + " (Backtracked " + std::to_string(backwardCount) + " entries)");

    std::size_t currentRva = tableStartRva;
    int loadedCount = 0;

    while (currentRva + ENTRY_SIZE <= safeScanSize)
    {
        const char *titlePtr = reinterpret_cast<const char *>(&cachedBuffer_[currentRva + TITLE_OFFSET]);

        std::string title(titlePtr, strnlen(titlePtr, 63));

        std::int32_t songId = 0;
        std::memcpy(&songId, &cachedBuffer_[currentRva + ID_OFFSET], sizeof(std::int32_t));

        // タイトルが空、またはIDが異常値なら終了
        if (songId <= 0 || songId > 200000)
        {
            // 途中に空きスロットがある可能性も考慮して、少し先を見るかここで終了するか
            // 基本は連続しているため終了
            break;
        }

        // ノーツ取得
        ChartNotes notes{};
        auto readNote = [&](std::size_t relBytes) -> int
        {
            std::int32_t val = 0;
            std::size_t pos = currentRva + NOTES_OFFSET + relBytes;
            if (pos + sizeof(std::int32_t) <= safeScanSize)
            {
                std::memcpy(&val, &cachedBuffer_[pos], sizeof(std::int32_t));
            }
            return (val >= 0 && val < 10000) ? static_cast<int>(val) : 0;
        };

        notes.sp_beginner = readNote(0);
        notes.sp_normal = readNote(4);
        notes.sp_hyper = readNote(8);
        notes.sp_another = readNote(12);
        notes.sp_leggendaria = readNote(16);
        notes.dp_normal = readNote(24);
        notes.dp_hyper = readNote(28);
        notes.dp_another = readNote(32);
        notes.dp_leggendaria = readNote(36);

        // 各マップへ登録
        musicMap_[songId] = title;
        notesMap_[songId] = notes;
        titleMap_[songId] = title;

        loadedCount++;
        currentRva += ENTRY_SIZE;
    }

    LOG_INFO("Successfully loaded " + std::to_string(loadedCount) + " songs.");
    LOG_INFO("======== MUSIC TABLE BUILDER END ========");

    return !musicMap_.empty();
}

ChartNotes MusicTableReader::getChartNotes(std::int32_t songId) const
{
    // 1. まず通常のマップから検索
    auto it = notesMap_.find(songId);
    if (it != notesMap_.end())
    {
        return it->second;
    }

    if (cachedBuffer_.empty())
    {
        return ChartNotes{};
    }

    const std::size_t ENTRY_SIZE = 1840;
    const std::size_t ID_OFFSET = 1200;

    // 2. バッファから該当する Song ID を持つ構造体の位置（rva）を探す
    for (std::size_t rva = 0; rva + ENTRY_SIZE <= cachedBuffer_.size(); rva += 4)
    {
        std::int32_t currentId = 0;
        std::size_t idPos = rva + ID_OFFSET;
        if (idPos + sizeof(std::int32_t) <= cachedBuffer_.size())
        {
            std::memcpy(&currentId, &cachedBuffer_[idPos], sizeof(std::int32_t));
            if (currentId == songId)
            {
                ChartNotes fallbackNotes{};

                // 指定オフセットから int32 を安全に読み込むヘルパー
                auto readRawInt32 = [&](std::size_t pos) -> int
                {
                    std::int32_t val = 0;
                    if (pos + sizeof(std::int32_t) <= cachedBuffer_.size())
                    {
                        std::memcpy(&val, &cachedBuffer_[pos], sizeof(std::int32_t));
                    }
                    return (val > 0 && val < 10000) ? static_cast<int>(val) : 0;
                };

                // まずは基本位置 (+624) で試す
                fallbackNotes.sp_beginner = readRawInt32(rva + 624 + 0);
                fallbackNotes.sp_normal = readRawInt32(rva + 624 + 4);
                fallbackNotes.sp_hyper = readRawInt32(rva + 624 + 8);
                fallbackNotes.sp_another = readRawInt32(rva + 624 + 12);
                fallbackNotes.sp_leggendaria = readRawInt32(rva + 624 + 16);
                fallbackNotes.dp_normal = readRawInt32(rva + 624 + 24);
                fallbackNotes.dp_hyper = readRawInt32(rva + 624 + 28);
                fallbackNotes.dp_another = readRawInt32(rva + 624 + 32);
                fallbackNotes.dp_leggendaria = readRawInt32(rva + 624 + 36);

                // もし基本位置で有効なノーツが取れなかった場合（初期曲などのオフセット違い対策）
                if (fallbackNotes.sp_normal == 0 && fallbackNotes.sp_hyper == 0 && fallbackNotes.sp_another == 0)
                {
                    // 構造体内部 (+550 ～ +700) を直接スキャンして、連続する有効なノーツ数を探す
                    for (std::size_t offset = 550; offset <= 700; offset += 4)
                    {
                        int n1 = readRawInt32(rva + offset + 4);  // SPN想定
                        int n2 = readRawInt32(rva + offset + 8);  // SPH想定
                        int n3 = readRawInt32(rva + offset + 12); // SPA想定

                        // 妥当なノーツ数（例: 20〜4000）が並んでいればそれを採用
                        if (n1 >= 20 && n2 >= 20 && n3 >= 20)
                        {
                            fallbackNotes.sp_normal = n1;
                            fallbackNotes.sp_hyper = n2;
                            fallbackNotes.sp_another = n3;
                            fallbackNotes.dp_normal = readRawInt32(rva + offset + 24);
                            fallbackNotes.dp_hyper = readRawInt32(rva + offset + 28);
                            fallbackNotes.dp_another = readRawInt32(rva + offset + 32);
                            break;
                        }
                    }
                }

                LOG_ERROR("Fallback resolved Song ID " + std::to_string(songId) +
                          " -> SPN:" + std::to_string(fallbackNotes.sp_normal) +
                          " SPH:" + std::to_string(fallbackNotes.sp_hyper) +
                          " SPA:" + std::to_string(fallbackNotes.sp_another));

                return fallbackNotes;
            }
        }
    }

    return ChartNotes{};
}

bool MusicTableReader::exportToTsv(const std::string &filePath) const
{
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        LOG_ERROR("Failed to open file for writing music map TSV: " + filePath);
        return false;
    }

    file << "song_id\ttitle\n";
    for (const auto &[songId, title] : musicMap_)
    {
        file << songId << '\t' << title << '\n';
    }

    file.close();
    LOG_INFO("Exported music map to: " + filePath);
    return true;
}

std::string MusicTableReader::getTitle(std::int32_t songId) const
{
    auto it = titleMap_.find(songId);
    if (it != titleMap_.end())
        return it->second;

    return "";
}

void MusicTableReader::debugInspectSong(std::int32_t targetSongId) const
{
    LOG_INFO("======== SCAN UTF-16 STRINGS IN RECORD FOR 'SMALLEST' ========");

    std::string keyword = "SMALLEST";
    for (std::size_t rva = 0; rva + keyword.size() <= cachedBuffer_.size(); ++rva)
    {
        if (std::memcmp(&cachedBuffer_[rva], keyword.data(), keyword.size()) == 0)
        {
            LOG_INFO("True Start RVA: 0x" + toHex(rva));

            // 0 から 1200 の間を 2バイト刻みで走査し、UTF-16LE文字列を抽出する
            for (std::size_t offset = 0; offset < 1200; offset += 2)
            {
                const wchar_t* wptr = reinterpret_cast<const wchar_t*>(&cachedBuffer_[rva + offset]);
                
                // 妥当な長さのワイド文字列をチェック (例: 2文字以上、50文字以下)
                std::size_t wlen = 0;
                while (wlen < 60 && offset + (wlen + 1) * sizeof(wchar_t) <= 1200)
                {
                    wchar_t wc = wptr[wlen];
                    if (wc == L'\0') break;
                    // 制御文字などを除外
                    if (wc < 32 && wc != 9 && wc != 10 && wc != 13) break;
                    wlen++;
                }

                if (wlen >= 2)
                {
                    // std::wstring から std::string への簡易変換（英数字やASCII中心の確認用）
                    std::string asciiFallback;
                    for (std::size_t i = 0; i < wlen; ++i)
                    {
                        wchar_t wc = wptr[i];
                        if (wc < 128) asciiFallback.push_back(static_cast<char>(wc));
                        else asciiFallback.push_back('?');
                    }

                    LOG_INFO("  [+ " + std::to_string(offset) + "] (UTF-16 len=" + std::to_string(wlen) + ") -> ASCII preview: \"" + asciiFallback + "\"");
                }
            }
            break;
        }
    }
    LOG_INFO("===============================================================");
}