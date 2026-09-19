#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class MemoryReader;
class Module;

struct Pattern
{
    std::vector<std::uint8_t> bytes;
    std::vector<bool> mask;

    bool isValid() const;
};

struct PatternMatch
{
    std::size_t rva = 0;
};

class PatternScanner
{
public:
    explicit PatternScanner(
        const MemoryReader &memoryReader);

    bool findFirst(
        const Module &module,
        const Pattern &pattern,
        PatternMatch &match) const;

    bool findAll(
        const Module &module,
        const Pattern &pattern,
        std::vector<PatternMatch> &matches) const;

    bool findAllInRange(
        const Module &module,
        const Pattern &pattern,
        std::size_t centerRva,
        std::size_t rangeSize,
        std::vector<PatternMatch> &matches) const;

private:
    static bool matchesAt(
        const std::uint8_t *data,
        const Pattern &pattern);

    const MemoryReader &memoryReader_;
};