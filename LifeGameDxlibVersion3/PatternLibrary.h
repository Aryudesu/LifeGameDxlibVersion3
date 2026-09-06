#pragma once

#include <cstddef>
#include <span>

struct PatternCell {
    int x;
    int y;
};

enum class PatternCategory {
    Cell,
    StillLife,
    Oscillator,
    Methuselah,
    Spaceship,
    Gun
};

struct LifePattern {
    const char* name;
    PatternCategory category;
    std::span<const PatternCell> cells;
};

namespace PatternLibrary {
std::span<const LifePattern> patterns() noexcept;
const LifePattern& at(std::size_t index) noexcept;
std::size_t size() noexcept;
const char* categoryName(PatternCategory category) noexcept;
} // namespace PatternLibrary
