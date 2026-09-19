#pragma once

#include <cstddef>
#include <cstdint>

#include "Process.h"

class MemoryReader
{
public:
    explicit MemoryReader(const Process& process);

    bool isReadable(
        std::uintptr_t address,
        std::size_t size
    ) const;

    bool read(
        std::uintptr_t address,
        void* buffer,
        std::size_t size
    ) const;

    bool readReadable(
        std::uintptr_t address,
        void* buffer,
        std::size_t size
    ) const;

    template<typename T>
    bool read(
        std::uintptr_t address,
        T& value
    ) const;

    
private:
    const Process& process_;

    static bool isReadableProtection(
        DWORD protection
    );
};

template<typename T>
bool MemoryReader::read(
    std::uintptr_t address,
    T& value
) const
{
    return read(
        address,
        &value,
        sizeof(T)
    );
}