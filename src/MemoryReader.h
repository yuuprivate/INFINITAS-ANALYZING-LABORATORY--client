#pragma once

#include <cstddef>
#include <cstdint>
#include <windows.h>

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

    /**
     * @brief 指定したアドレスが実行コード領域（.text セクション等）にあるか確認します。
     */
    bool isExecutableAddress(
        std::uintptr_t address
    ) const;

    template<typename T>
    bool read(
        std::uintptr_t address,
        T& value
    ) const
    {
        return read(
            address,
            static_cast<void*>(&value),
            sizeof(T)
        );
    }

private:
    const Process& process_;

    static bool isReadableProtection(
        DWORD protection
    );
};