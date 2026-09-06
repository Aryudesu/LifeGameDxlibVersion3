#pragma once

#include "DxLib.h"
#include "InfiniteCamera.h"
#include "InfiniteLifeBoard.h"
#include "PatternLibrary.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace PatternPlacementPreview {

inline std::pair<int, int> rotatedCell(PatternCell cell, int rotation) noexcept {
    switch (rotation & 3) {
    case 1: return {-cell.y, cell.x};
    case 2: return {-cell.x, -cell.y};
    case 3: return {cell.y, -cell.x};
    default: return {cell.x, cell.y};
    }
}

inline std::pair<int, int> rotationOffset(const LifePattern& pattern, int rotation) noexcept {
    int minX = 0;
    int minY = 0;
    for (const PatternCell cell : pattern.cells) {
        const auto [x, y] = rotatedCell(cell, rotation);
        minX = std::min(minX, x);
        minY = std::min(minY, y);
    }
    return {-minX, -minY};
}

inline bool checkedAdd(InfiniteLifeBoard::Coord origin, int delta,
                       InfiniteLifeBoard::Coord& result) noexcept {
    using Coord = InfiniteLifeBoard::Coord;
    if (delta > 0 && origin > std::numeric_limits<Coord>::max() - delta) return false;
    if (delta < 0 && origin < std::numeric_limits<Coord>::min() - delta) return false;
    result = origin + delta;
    return true;
}

inline bool canPlace(const LifePattern& pattern,
                     InfiniteLifeBoard::Coord originX,
                     InfiniteLifeBoard::Coord originY,
                     int rotation) noexcept {
    const auto [offsetX, offsetY] = rotationOffset(pattern, rotation);
    for (const PatternCell cell : pattern.cells) {
        const auto [rx, ry] = rotatedCell(cell, rotation);
        InfiniteLifeBoard::Coord x = 0;
        InfiniteLifeBoard::Coord y = 0;
        if (!checkedAdd(originX, rx + offsetX, x) ||
            !checkedAdd(originY, ry + offsetY, y)) {
            return false;
        }
    }
    return true;
}

inline bool place(InfiniteLifeBoard& board,
                  const LifePattern& pattern,
                  InfiniteLifeBoard::Coord originX,
                  InfiniteLifeBoard::Coord originY,
                  int rotation) {
    if (!canPlace(pattern, originX, originY, rotation)) return false;

    const auto [offsetX, offsetY] = rotationOffset(pattern, rotation);
    for (const PatternCell cell : pattern.cells) {
        const auto [rx, ry] = rotatedCell(cell, rotation);
        board.setAlive(originX + rx + offsetX, originY + ry + offsetY, true);
    }
    return true;
}

inline void draw(const InfiniteCamera& camera,
                 const LifePattern& pattern,
                 InfiniteLifeBoard::Coord originX,
                 InfiniteLifeBoard::Coord originY,
                 int rotation,
                 int boardViewWidth,
                 int boardViewHeight) {
    if (pattern.category == PatternCategory::Cell) return;

    const bool valid = canPlace(pattern, originX, originY, rotation);
    const unsigned int ghostColor = valid ? GetColor(80, 210, 255) : GetColor(255, 90, 90);
    const unsigned int anchorColor = valid ? GetColor(255, 220, 90) : GetColor(255, 90, 90);
    const int size = camera.cellSize();
    const auto [offsetX, offsetY] = rotationOffset(pattern, rotation);

    if (valid) {
        for (const PatternCell cell : pattern.cells) {
            const auto [rx, ry] = rotatedCell(cell, rotation);
            const auto [sx, sy] = camera.boardToScreen(originX + rx + offsetX,
                                                       originY + ry + offsetY);
            if (sx + size <= 0 || sy + size <= 0 || sx >= boardViewWidth || sy >= boardViewHeight) continue;

            if (size == 1) {
                DrawPixel(sx, sy, ghostColor);
            } else {
                DrawBox(sx, sy, sx + size - 1, sy + size - 1, ghostColor, FALSE);
                if (size >= 4) {
                    DrawBox(sx + 2, sy + 2, sx + size - 3, sy + size - 3, ghostColor, TRUE);
                }
            }
        }
    }

    const auto [anchorX, anchorY] = camera.boardToScreen(originX, originY);
    if (anchorX >= 0 && anchorX < boardViewWidth && anchorY >= 0 && anchorY < boardViewHeight) {
        const int half = std::max(3, size / 2);
        const int centerX = anchorX + size / 2;
        const int centerY = anchorY + size / 2;
        DrawLine(centerX - half, centerY, centerX + half, centerY, anchorColor);
        DrawLine(centerX, centerY - half, centerX, centerY + half, anchorColor);
    }
}

} // namespace PatternPlacementPreview
