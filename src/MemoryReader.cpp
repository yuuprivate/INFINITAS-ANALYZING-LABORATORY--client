#include "MemoryReader.h"
#include "Logger.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <limits>

MemoryReader::MemoryReader(
    const Process& process
)
    : process_(process)
{
    LOG_INFO(
        "MemoryReader を初期化しました。"
    );
}

bool MemoryReader::isReadableProtection(
    DWORD protection
)
{
    if ((protection & PAGE_GUARD) != 0)
    {
        return false;
    }

    const DWORD basicProtection =
        protection & 0xFF;

    switch (basicProtection)
    {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;

    default:
        return false;
    }
}

bool MemoryReader::isReadable(
    std::uintptr_t address,
    std::size_t size
) const
{
    if (!process_.isOpen())
    {
        return false;
    }

    if (size == 0)
    {
        return true;
    }

    std::uintptr_t currentAddress = address;
    std::size_t remainingSize = size;

    while (remainingSize > 0)
    {
        MEMORY_BASIC_INFORMATION memoryInfo{};

        const SIZE_T queryResult =
            VirtualQueryEx(
                process_.handle(),
                reinterpret_cast<LPCVOID>(currentAddress),
                &memoryInfo,
                sizeof(memoryInfo)
            );

        if (queryResult == 0)
        {
            return false;
        }

        if (memoryInfo.State != MEM_COMMIT)
        {
            return false;
        }

        if (!isReadableProtection(memoryInfo.Protect))
        {
            return false;
        }

        const auto regionBase =
            reinterpret_cast<std::uintptr_t>(memoryInfo.BaseAddress);

        const auto regionSize =
            static_cast<std::uintptr_t>(memoryInfo.RegionSize);

        if (regionSize == 0 || currentAddress < regionBase)
        {
            return false;
        }

        const std::uintptr_t offsetInRegion = currentAddress - regionBase;

        if (offsetInRegion >= regionSize)
        {
            return false;
        }

        const std::uintptr_t availableSize = regionSize - offsetInRegion;
        const std::uintptr_t remaining = static_cast<std::uintptr_t>(remainingSize);
        const std::uintptr_t advance = std::min(availableSize, remaining);

        if (advance == 0)
        {
            return false;
        }

        currentAddress += advance;
        remainingSize -= static_cast<std::size_t>(advance);
    }

    return true;
}

bool MemoryReader::read(
    std::uintptr_t address,
    void* buffer,
    std::size_t size
) const
{
    if (!process_.isOpen())
    {
        return false;
    }

    if (buffer == nullptr || size == 0)
    {
        return false;
    }

    if (!isReadable(address, size))
    {
        return false;
    }

    SIZE_T bytesRead = 0;

    const BOOL result =
        ReadProcessMemory(
            process_.handle(),
            reinterpret_cast<LPCVOID>(address),
            buffer,
            size,
            &bytesRead
        );

    if (result == FALSE)
    {
        return false;
    }

    return bytesRead == size;
}

bool MemoryReader::readReadable(
    std::uintptr_t address,
    void* buffer,
    std::size_t size
) const
{
    if (!process_.isOpen())
    {
        return false;
    }

    if (buffer == nullptr || size == 0)
    {
        return false;
    }

    auto* output = static_cast<std::uint8_t*>(buffer);
    std::uintptr_t currentAddress = address;
    std::size_t remainingSize = size;
    bool readSomething = false;

    while (remainingSize > 0)
    {
        MEMORY_BASIC_INFORMATION memoryInfo{};

        const SIZE_T queryResult =
            VirtualQueryEx(
                process_.handle(),
                reinterpret_cast<LPCVOID>(currentAddress),
                &memoryInfo,
                sizeof(memoryInfo)
            );

        if (queryResult == 0)
        {
            std::fill(output, output + remainingSize, 0);
            break;
        }

        const auto regionBase =
            reinterpret_cast<std::uintptr_t>(memoryInfo.BaseAddress);

        const auto regionSize =
            static_cast<std::uintptr_t>(memoryInfo.RegionSize);

        if (regionSize == 0 || currentAddress < regionBase)
        {
            std::fill(output, output + remainingSize, 0);
            break;
        }

        const std::uintptr_t offsetInRegion = currentAddress - regionBase;

        if (offsetInRegion >= regionSize)
        {
            std::fill(output, output + remainingSize, 0);
            break;
        }

        const std::uintptr_t availableSize = regionSize - offsetInRegion;
        const std::size_t requestedSize =
            std::min(remainingSize, static_cast<std::size_t>(availableSize));

        bool regionReadSuccessfully = false;

        if (memoryInfo.State == MEM_COMMIT &&
            isReadableProtection(memoryInfo.Protect))
        {
            SIZE_T bytesRead = 0;

            const BOOL result =
                ReadProcessMemory(
                    process_.handle(),
                    reinterpret_cast<LPCVOID>(currentAddress),
                    output,
                    requestedSize,
                    &bytesRead
                );

            if (result != FALSE && bytesRead == requestedSize)
            {
                regionReadSuccessfully = true;
                readSomething = true;
            }
        }

        if (!regionReadSuccessfully)
        {
            std::fill(output, output + requestedSize, 0);
        }

        output += requestedSize;
        currentAddress += static_cast<std::uintptr_t>(requestedSize);
        remainingSize -= requestedSize;
    }

    return readSomething;
}