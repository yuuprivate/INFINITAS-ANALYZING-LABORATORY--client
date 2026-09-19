#pragma once

#include "MemoryReader.h"

#include <cstdint>
#include <string>
#include <unordered_map>

struct ChartNotes {
    int sp_beginner = 0;
    int sp_normal = 0;
    int sp_hyper = 0;
    int sp_another = 0;
    int sp_leggendaria = 0;
    int dp_normal = 0;
    int dp_hyper = 0;
    int dp_another = 0;
    int dp_leggendaria = 0;
};

// 楽曲マップとノーツ数を保持・提供するインターフェース
// (実装状況に合わせて適宜調整してください)

class MusicTableReader
{
public:
    // ベースアドレスとサイズを直接受け取るコンストラクタ
    MusicTableReader(const MemoryReader &memoryReader, std::uintptr_t baseAddress, std::size_t moduleSize);

    // メモリ上の楽曲情報領域をスキャンして song_id -> 曲名 のマップを一括構築する
    bool scanAndBuildMusicMap(std::uintptr_t searchStartRva = 0x3000000, std::size_t scanSize = 0x1000000);

    // IDから曲名を取得する（見つからない場合は ID:XXXX を返す）
    std::string getSongTitle(std::int32_t songId) const;

    // 構築されたマップを取得する
    const std::unordered_map<std::int32_t, std::string> &getMusicMap() const { return musicMap_; }

    // 読み込んだ楽曲マップを TSV ファイルにエクスポートする
    bool exportToTsv(const std::string &filePath) const;

private:
    const MemoryReader &memoryReader_;
    std::uintptr_t baseAddress_;
    std::size_t moduleSize_;

    // song_id -> 曲名文字列
    std::unordered_map<std::int32_t, std::string> musicMap_;

    // 曲名から song_id へのオフセット (-1200 bytes = -0x4B0)
    static constexpr std::intptr_t kSongIdOffsetFromTitle = 1200;
};