#include "PatternListScroll.h"

#include <algorithm>

std::size_t PatternListScroll::categoryIndex(PatternCategory category) noexcept {
    return static_cast<std::size_t>(category);
}

int PatternListScroll::patternCount(PatternCategory category) noexcept {
    int count = 0;
    for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {
        if (PatternLibrary::at(i).category == category) ++count;
    }
    return count;
}

int PatternListScroll::rowInCategory(std::size_t patternIndex) noexcept {
    if (patternIndex == 0 || patternIndex >= PatternLibrary::size()) return -1;
    const PatternCategory category = PatternLibrary::at(patternIndex).category;
    int row = 0;
    for (std::size_t i = 1; i < patternIndex; ++i) {
        if (PatternLibrary::at(i).category == category) ++row;
    }
    return row;
}

int PatternListScroll::offset(PatternCategory category) const noexcept {
    return offsets_[categoryIndex(category)];
}

int PatternListScroll::maxOffset(PatternCategory category) const noexcept {
    return std::max(0, patternCount(category) - VisibleRows);
}

int PatternListScroll::count(PatternCategory category) const noexcept {
    return patternCount(category);
}

void PatternListScroll::scroll(PatternCategory category, int wheelDelta) noexcept {
    if (wheelDelta == 0) return;
    int& current = offsets_[categoryIndex(category)];
    current = std::clamp(current - wheelDelta, 0, maxOffset(category));
}

void PatternListScroll::ensurePatternVisible(std::size_t patternIndex) noexcept {
    if (patternIndex == 0 || patternIndex >= PatternLibrary::size()) return;
    const PatternCategory category = PatternLibrary::at(patternIndex).category;
    const int row = rowInCategory(patternIndex);
    if (row < 0) return;

    int& current = offsets_[categoryIndex(category)];
    if (row < current) current = row;
    else if (row >= current + VisibleRows) current = row - VisibleRows + 1;
    current = std::clamp(current, 0, maxOffset(category));
}
