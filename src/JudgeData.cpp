#include "JudgeData.h"
#include "Logger.h"

#include <array>
#include <cstdint>

JudgeDataReader::JudgeDataReader(
    const MemoryReader &memoryReader)
    : memoryReader_(memoryReader)
{
    LOG_INFO(
        "JudgeDataReader を初期化しました。");

    LOG_INFO(
        "JudgeDataReader initialized successfully.");
}

bool JudgeDataReader::read(
    std::uintptr_t address,
    JudgeSnapshot &snapshot) const
{
    snapshot = {};

    std::array<std::int32_t, 18> values{};

    if (!memoryReader_.read(
            address,
            values.data(),
            sizeof(values)))
    {
        LOG_ERROR(
            "JudgeDataを読み取れませんでした。");

        LOG_ERROR(
            "Failed to read JudgeData.");

        return false;
    }

    snapshot.p1Pgreat = values[0];
    snapshot.p1Great = values[1];
    snapshot.p1Good = values[2];
    snapshot.p1Bad = values[3];
    snapshot.p1Poor = values[4];

    snapshot.p2Pgreat = values[5];
    snapshot.p2Great = values[6];
    snapshot.p2Good = values[7];
    snapshot.p2Bad = values[8];
    snapshot.p2Poor = values[9];

    snapshot.p1ComboBreak = values[10];
    snapshot.p2ComboBreak = values[11];

    snapshot.p1Fast = values[12];
    snapshot.p2Fast = values[13];

    snapshot.p1Slow = values[14];
    snapshot.p2Slow = values[15];

    snapshot.p1MeasureEnd = values[16];
    snapshot.p2MeasureEnd = values[17];

    const bool p1HasJudge =
        snapshot.p1Pgreat +
            snapshot.p1Great +
            snapshot.p1Good +
            snapshot.p1Bad +
            snapshot.p1Poor >
        0;

    const bool p2HasJudge =
        snapshot.p2Pgreat +
            snapshot.p2Great +
            snapshot.p2Good +
            snapshot.p2Bad +
            snapshot.p2Poor >
        0;

    if (!p1HasJudge)
    {
        snapshot.playType =
            JudgePlayType::P2;
    }
    else if (p2HasJudge)
    {
        snapshot.playType =
            JudgePlayType::DP;
    }
    else
    {
        snapshot.playType =
            JudgePlayType::P1;
    }

    return true;
}

bool hasJudgeResult(const JudgeSnapshot &snapshot)
{
    return snapshot.p1Pgreat != 0 ||
           snapshot.p1Great != 0 ||
           snapshot.p1Good != 0 ||
           snapshot.p1Bad != 0 ||
           snapshot.p1Poor != 0 ||

           snapshot.p2Pgreat != 0 ||
           snapshot.p2Great != 0 ||
           snapshot.p2Good != 0 ||
           snapshot.p2Bad != 0 ||
           snapshot.p2Poor != 0 ||

           snapshot.p1ComboBreak != 0 ||
           snapshot.p2ComboBreak != 0 ||

           snapshot.p1Fast != 0 ||
           snapshot.p2Fast != 0 ||

           snapshot.p1Slow != 0 ||
           snapshot.p2Slow != 0;
}

JudgePlayType detectJudgePlayType(const JudgeSnapshot &snapshot)
{
    const bool p1HasData =
        snapshot.p1Pgreat != 0 ||
        snapshot.p1Great != 0 ||
        snapshot.p1Good != 0 ||
        snapshot.p1Bad != 0 ||
        snapshot.p1Poor != 0 ||
        snapshot.p1ComboBreak != 0 ||
        snapshot.p1Fast != 0 ||
        snapshot.p1Slow != 0;

    const bool p2HasData =
        snapshot.p2Pgreat != 0 ||
        snapshot.p2Great != 0 ||
        snapshot.p2Good != 0 ||
        snapshot.p2Bad != 0 ||
        snapshot.p2Poor != 0 ||
        snapshot.p2ComboBreak != 0 ||
        snapshot.p2Fast != 0 ||
        snapshot.p2Slow != 0;

    if (p1HasData && p2HasData)
    {
        return JudgePlayType::DP;
    }

    if (p2HasData)
    {
        return JudgePlayType::P2;
    }

    return JudgePlayType::P1;
}