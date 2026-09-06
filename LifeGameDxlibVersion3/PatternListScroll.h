#pragma once

#include "PatternLibrary.h"

#include <array>
#include <cstddef>

class PatternListScroll {
public:
    static constexpr int VisibleRows = 14;

    int offset(PatternCategory category) const noexcept;
    int maxOffset(PatternCategory category) const noexcept;
    int count(PatternCategory category) const noexcept;
    void scroll(PatternCategory category, int wheelDelta) noexcept;
    void ensurePatternVisible(std::size_t patternIndex) noexcept;

private:
    static constexpr std::size_t CategoryCount = static_cast<std::size_t>(PatternCategory::Gun) + 1;
    static std::size_t categoryIndex(PatternCategory category) noexcept;
    static int rowInCategory(std::size_t patternIndex) noexcept;
    static int patternCount(PatternCategory category) noexcept;

    std::array<int, CategoryCount> offsets_{};
};
