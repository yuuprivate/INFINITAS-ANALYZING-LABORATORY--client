#include "OffsetManager.h"
#include "Logger.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    bool parseOffsetName(
        const std::string &name,
        OffsetType &type)
    {
        std::string normalized = name;

        for (char &character : normalized)
        {
            character = static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(character)));
        }

        if (normalized == "songlist")
        {
            type = OffsetType::SongList;
            return true;
        }

        if (normalized == "unlockdata")
        {
            type = OffsetType::UnlockData;
            return true;
        }

        if (normalized == "playsettings")
        {
            type = OffsetType::PlaySettings;
            return true;
        }

        if (normalized == "playdata")
        {
            type = OffsetType::PlayData;
            return true;
        }

        if (normalized == "currentsong")
        {
            type = OffsetType::CurrentSong;
            return true;
        }

        if (normalized == "judgedata")
        {
            type = OffsetType::JudgeData;
            return true;
        }

        if (normalized == "datamap")
        {
            type = OffsetType::DataMap;
            return true;
        }
        if (normalized == "playerprofile")
        {
            type = OffsetType::PlayerProfile;
            return true;
        }
        if (normalized == "songstate")
        {
            type = OffsetType::SongState;
            return true;
        }

        return false;
    }
}

std::string OffsetManager::trim(
    const std::string &value)
{
    const auto first =
        value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
    {
        return {};
    }

    const auto last =
        value.find_last_not_of(" \t\r\n");

    return value.substr(
        first,
        last - first + 1);
}

bool OffsetManager::parseHex(
    const std::string &text,
    std::uintptr_t &value)
{
    try
    {
        std::string normalized = trim(text);

        if (
            normalized.size() >= 2 &&
            normalized[0] == '0' &&
            (normalized[1] == 'x' ||
             normalized[1] == 'X'))
        {
            normalized = normalized.substr(2);
        }

        if (normalized.empty())
        {
            return false;
        }

        value = std::stoull(
            normalized,
            nullptr,
            16);

        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool OffsetManager::load(
    const std::string &filePath,
    std::uintptr_t moduleBaseAddress)
{
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        LOG_ERROR(
            "offsets.txt を開けませんでした。");

        LOG_ERROR(
            "Failed to open offsets.txt.");

        return false;
    }

    OffsetCollection loadedOffsets;

    std::string line;
    bool firstLine = true;

    constexpr std::uintptr_t ImageBase =
        0x140000000ULL;

    while (std::getline(file, line))
    {
        line = trim(line);

        if (line.empty())
        {
            continue;
        }

        if (firstLine)
        {
            loadedOffsets.version = line;
            firstLine = false;
            continue;
        }

        const auto separator =
            line.find('=');

        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string name =
            trim(line.substr(0, separator));

        const std::string valueText =
            trim(line.substr(separator + 1));

        OffsetType type;

        if (!parseOffsetName(name, type))
        {
            continue;
        }

        std::uintptr_t absoluteAddress = 0;

        if (!parseHex(valueText, absoluteAddress))
        {
            LOG_ERROR(
                std::string("オフセットの解析に失敗しました: ") +
                name);

            continue;
        }

        if (absoluteAddress < ImageBase)
        {
            LOG_ERROR(
                std::string("不正なアドレスです: ") +
                name);

            continue;
        }

        const std::uintptr_t rva =
            absoluteAddress - ImageBase;

        switch (type)
        {
        case OffsetType::SongList:
            loadedOffsets.songList = rva;
            break;

        case OffsetType::DataMap:
            loadedOffsets.dataMap = rva;
            break;

        case OffsetType::JudgeData:
            loadedOffsets.judgeData = rva;
            break;

        case OffsetType::PlayData:
            loadedOffsets.playData = rva;
            break;

        case OffsetType::PlaySettings:
            loadedOffsets.playSettings = rva;
            break;

        case OffsetType::UnlockData:
            loadedOffsets.unlockData = rva;
            break;

        case OffsetType::CurrentSong:
            loadedOffsets.currentSong = rva;
            break;

        case OffsetType::PlayerProfile:
            loadedOffsets.playerProfile = rva;
            break;
        
            case OffsetType::SongState:
            loadedOffsets.songState = rva;
            break;
        }
    }

    file.close();

    offsets_ = loadedOffsets;
    loaded_ = true;

    LOG_INFO(
        std::string("オフセットを読み込みました。Version: ") +
        offsets_.version);

    LOG_INFO(
        std::string("Offsets loaded. Version: ") +
        offsets_.version);

    return true;
}

bool OffsetManager::isLoaded() const
{
    return loaded_;
}

const std::string &OffsetManager::version() const
{
    return offsets_.version;
}

std::uintptr_t OffsetManager::get(
    OffsetType type) const
{
    switch (type)
    {
    case OffsetType::SongList:
        return offsets_.songList;

    case OffsetType::DataMap:
        return offsets_.dataMap;

    case OffsetType::JudgeData:
        return offsets_.judgeData;

    case OffsetType::PlayData:
        return offsets_.playData;

    case OffsetType::PlaySettings:
        return offsets_.playSettings;

    case OffsetType::UnlockData:
        return offsets_.unlockData;

    case OffsetType::CurrentSong:
        return offsets_.currentSong;

    case OffsetType::PlayerProfile:
        return offsets_.playerProfile;

    case OffsetType::SongState:
        return offsets_.songState;
    }

    return 0;
}

std::uintptr_t OffsetManager::getAddress(
    OffsetType type,
    std::uintptr_t moduleBaseAddress) const
{
    return moduleBaseAddress + get(type);
}

bool OffsetManager::validate() const
{
    if (!loaded_)
    {
        return false;
    }

    const bool valid =
        !offsets_.version.empty() &&
        offsets_.songList != 0 &&
        offsets_.dataMap != 0 &&
        offsets_.judgeData != 0 &&
        offsets_.playData != 0 &&
        offsets_.playSettings != 0 &&
        offsets_.unlockData != 0 &&
        offsets_.currentSong != 0 &&
        offsets_.playerProfile != 0 &&
        offsets_.songState != 0;

    if (!valid)
    {
        LOG_ERROR(
            "オフセット定義に不足または不正な値があります。");

        LOG_ERROR(
            "Offset definition is incomplete or invalid.");

        return false;
    }

    LOG_INFO(
        "オフセット定義の検証に成功しました。");

    LOG_INFO(
        "Offset definition validated successfully.");

    return true;
}