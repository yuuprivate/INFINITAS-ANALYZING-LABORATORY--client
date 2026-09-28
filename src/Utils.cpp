#include "Utils.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace Utils
{
    std::string generateNonce()
    {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<std::uint64_t> dis;

        std::uint64_t ab = dis(gen);
        std::uint64_t cd = dis(gen);

        // UUID v4 の規格に合わせたビット調整
        ab = (ab & 0xFFFFFFFFFFFF0000ULL) | (ab & 0xFFFFULL);
        ab = (ab & ~(0xF000ULL)) | 0x4000ULL; // version 4
        cd = (cd & ~(0xC000000000000000ULL)) | 0x8000000000000000ULL; // variant 1

        std::stringstream ss;
        ss << std::hex << std::setfill('0')
           << std::setw(8) << (ab >> 32) << "-"
           << std::setw(4) << ((ab >> 16) & 0xFFFF) << "-"
           << std::setw(4) << (ab & 0xFFFF) << "-"
           << std::setw(4) << (cd >> 48) << "-"
           << std::setw(12) << (cd & 0xFFFFFFFFFFFFULL);

        return ss.str();
    }
}