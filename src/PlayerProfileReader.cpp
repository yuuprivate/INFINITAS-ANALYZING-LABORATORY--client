#include "PlayerProfileReader.h"
#include "Logger.h"

PlayerProfileReader::PlayerProfileReader(const MemoryReader& memoryReader)
    : memoryReader_(memoryReader)
{
}

bool PlayerProfileReader::fetchInfinitasId(std::uintptr_t profileAddress, std::string& outInfinitasId, std::string& outDjName) const
{
    InfinitasPlayerHeader header{};

    if (!memoryReader_.read(profileAddress, &header, sizeof(header)))
    {
        LOG_ERROR("PlayerProfile 領域のメモリ読み取りに失敗しました。");
        return false;
    }

    // 13桁の文字として取得
    std::string rawId(header.infinitasId, 13);

    // バリデーション: 先頭が 'C' かつ 13 桁であること
    if (rawId.length() == 13 && rawId[0] == 'C')
    {
        outInfinitasId = rawId;
        outDjName = std::string(header.djName, strnlen(header.djName, sizeof(header.djName)));
        return true;
    }

    LOG_ERROR("取得した INFINITAS ID のフォーマットが無効です: " + rawId);
    return false;
}