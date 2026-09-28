#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include "MemoryReader.h"

class PlayerProfileReader
{
public:
    explicit PlayerProfileReader(const MemoryReader& memoryReader);

    // 指定された絶対アドレスから INFINITAS ID と DJ NAME を読み取る
    bool fetchInfinitasId(std::uintptr_t profileAddress, std::string& outInfinitasId, std::string& outDjName) const;

private:
#pragma pack(push, 1)
    struct InfinitasPlayerHeader
    {
        char versionStr[32];  // 0x00: "P2D:J:B:A:2026080500..." (32 bytes)
        char infinitasId[14]; // 0x20: "C904620745642\0" (14 bytes: 13文字 + null)
        char djName[12];      // 0x2E: "MUSCLE\0..." (12 bytes)
    };
#pragma pack(pop)

    const MemoryReader& memoryReader_;
};