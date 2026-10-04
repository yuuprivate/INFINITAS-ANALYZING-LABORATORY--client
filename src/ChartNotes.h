#pragma once
#include <cstdint>
#include <string>

#pragma pack(push, 1) // ★ 構造体のメンバ間パディングを無効化
struct ChartNotes {
    std::uint32_t sp_beginner    = 0;
    std::uint32_t sp_normal      = 0;
    std::uint32_t sp_hyper       = 0;
    std::uint32_t sp_another     = 0;
    std::uint32_t sp_leggendaria = 0;

    std::uint32_t dp_normal      = 0;
    std::uint32_t dp_hyper       = 0;
    std::uint32_t dp_another     = 0;
    std::uint32_t dp_leggendaria = 0;
};

struct ChartRatings {
    std::uint8_t sp_beginner    = 0;
    std::uint8_t sp_normal      = 0;
    std::uint8_t sp_hyper       = 0;
    std::uint8_t sp_another     = 0;
    std::uint8_t sp_leggendaria = 0;

    std::uint8_t dp_normal      = 0;
    std::uint8_t dp_hyper       = 0;
    std::uint8_t dp_another     = 0;
    std::uint8_t dp_leggendaria = 0;
};
#pragma pack(pop)

struct MusicInfo {
    std::int32_t songId = 0;
    ChartNotes notes;
    ChartRatings ratings;
};