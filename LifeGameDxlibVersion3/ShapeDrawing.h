#pragma once

#include "InfiniteCamera.h"
#include "InfiniteLifeBoard.h"

#include "DxLib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace ShapeDrawing {

using Coord = InfiniteLifeBoard::Coord;

enum class Tool { Cell, Line, Rectangle, Circle };

inline const char* toolName(Tool tool) noexcept {
    switch (tool) {
    case Tool::Line: return "Line";
    case Tool::Rectangle: return "Rectangle";
    case Tool::Circle: return "Circle";
    default: return "Cell";
    }
}

inline std::uint64_t distance(Coord a, Coord b) noexcept {
    const auto ua = static_cast<std::uint64_t>(a);
    const auto ub = static_cast<std::uint64_t>(b);
    return a >= b ? ua - ub : ub - ua;
}

inline bool offsetCoord(Coord origin, std::uint64_t magnitude, bool positive, Coord& result) noexcept {
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<Coord>::max())) return false;
    const Coord delta = static_cast<Coord>(magnitude);
    if (positive) {
        if (origin > std::numeric_limits<Coord>::max() - delta) return false;
        result = origin + delta;
    } else {
        if (origin < std::numeric_limits<Coord>::min() + delta) return false;
        result = origin - delta;
    }
    return true;
}

// Shape preview/commit can be evaluated after mouse state processing, so read the
// physical Shift state directly instead of relying on DxLib's per-key polling here.
inline bool shiftConstraintActive() noexcept {
    return (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
}

// Shift: Line -> nearest 45-degree direction, Rectangle -> square.
inline std::pair<Coord, Coord> constrainedEnd(Tool tool, Coord x0, Coord y0, Coord x1, Coord y1,
                                               bool constrain) noexcept {
    if (!constrain || (tool != Tool::Line && tool != Tool::Rectangle)) return {x1, y1};

    const std::uint64_t dx = distance(x0, x1);
    const std::uint64_t dy = distance(y0, y1);
    const bool positiveX = x1 >= x0;
    const bool positiveY = y1 >= y0;
    std::uint64_t snappedX = dx;
    std::uint64_t snappedY = dy;

    if (tool == Tool::Rectangle) {
        const std::uint64_t side = std::max(dx, dy);
        snappedX = side;
        snappedY = side;
    } else {
        constexpr long double Tan22_5 = 0.4142135623730950488L;
        constexpr long double Tan67_5 = 2.4142135623730950488L;
        const long double fx = static_cast<long double>(dx);
        const long double fy = static_cast<long double>(dy);
        if (fy <= fx * Tan22_5) {
            snappedY = 0;
        } else if (fy >= fx * Tan67_5) {
            snappedX = 0;
        } else {
            const std::uint64_t diagonal = dx / 2 + dy / 2 + (((dx & 1U) + (dy & 1U)) >= 1U ? 1U : 0U);
            snappedX = diagonal;
            snappedY = diagonal;
        }
    }

    Coord resultX = x1;
    Coord resultY = y1;
    if (!offsetCoord(x0, snappedX, positiveX, resultX) ||
        !offsetCoord(y0, snappedY, positiveY, resultY)) return {x1, y1};
    return {resultX, resultY};
}

template <typename Visitor>
inline bool visitLine(Coord x0, Coord y0, Coord x1, Coord y1, Visitor&& visitor,
                      std::uint64_t maxSpan = 16384) {
    const std::uint64_t spanX = distance(x0, x1);
    const std::uint64_t spanY = distance(y0, y1);
    if (std::max(spanX, spanY) > maxSpan) return false;
    const std::int64_t dx = static_cast<std::int64_t>(spanX);
    const std::int64_t dy = -static_cast<std::int64_t>(spanY);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    std::int64_t error = dx + dy;
    for (;;) {
        visitor(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        const std::int64_t twiceError = error * 2;
        if (twiceError >= dy) { error += dy; x0 += sx; }
        if (twiceError <= dx) { error += dx; y0 += sy; }
    }
    return true;
}

template <typename Visitor>
inline bool visitRectangle(Coord x0, Coord y0, Coord x1, Coord y1, Visitor&& visitor,
                           std::uint64_t maxSpan = 16384) {
    if (distance(x0, x1) > maxSpan || distance(y0, y1) > maxSpan) return false;
    visitLine(x0, y0, x1, y0, visitor, maxSpan);
    if (y1 != y0) visitLine(x0, y1, x1, y1, visitor, maxSpan);
    if (x1 != x0) {
        visitLine(x0, y0, x0, y1, visitor, maxSpan);
        visitLine(x1, y0, x1, y1, visitor, maxSpan);
    }
    return true;
}

template <typename Visitor>
inline bool visitCircle(Coord cx, Coord cy, Coord edgeX, Coord edgeY, Visitor&& visitor,
                        std::uint64_t maxRadius = 8192) {
    const std::uint64_t dx = distance(cx, edgeX);
    const std::uint64_t dy = distance(cy, edgeY);
    if (dx > maxRadius || dy > maxRadius) return false;
    const long double r2 = static_cast<long double>(dx) * static_cast<long double>(dx) +
                           static_cast<long double>(dy) * static_cast<long double>(dy);
    const std::uint64_t radius = static_cast<std::uint64_t>(std::sqrt(r2) + 0.5L);
    if (radius > maxRadius) return false;

    auto checkedVisit = [&](std::int64_t ox, std::int64_t oy) {
        if (ox > 0 && cx > std::numeric_limits<Coord>::max() - ox) return;
        if (ox < 0 && cx < std::numeric_limits<Coord>::min() - ox) return;
        if (oy > 0 && cy > std::numeric_limits<Coord>::max() - oy) return;
        if (oy < 0 && cy < std::numeric_limits<Coord>::min() - oy) return;
        visitor(cx + ox, cy + oy);
    };

    std::int64_t x = static_cast<std::int64_t>(radius);
    std::int64_t y = 0;
    std::int64_t error = 1 - x;
    while (x >= y) {
        checkedVisit( x,  y); checkedVisit( y,  x); checkedVisit(-y,  x); checkedVisit(-x,  y);
        checkedVisit(-x, -y); checkedVisit(-y, -x); checkedVisit( y, -x); checkedVisit( x, -y);
        ++y;
        if (error < 0) error += 2 * y + 1;
        else { --x; error += 2 * (y - x) + 1; }
    }
    return true;
}

template <typename Visitor>
inline bool visitShape(Tool tool, Coord x0, Coord y0, Coord x1, Coord y1, Visitor&& visitor) {
    const auto [endX, endY] = constrainedEnd(tool, x0, y0, x1, y1, shiftConstraintActive());
    switch (tool) {
    case Tool::Line: return visitLine(x0, y0, endX, endY, visitor);
    case Tool::Rectangle: return visitRectangle(x0, y0, endX, endY, visitor);
    case Tool::Circle: return visitCircle(x0, y0, endX, endY, visitor);
    default: visitor(endX, endY); return true;
    }
}

inline void drawPreview(Tool tool, const InfiniteCamera& camera,
                        Coord x0, Coord y0, Coord x1, Coord y1,
                        int boardWidth, int boardHeight, bool erase) {
    const unsigned int color = erase ? GetColor(255, 110, 110) : GetColor(80, 210, 255);
    const int size = camera.cellSize();
    visitShape(tool, x0, y0, x1, y1, [&](Coord x, Coord y) {
        const auto [sx, sy] = camera.boardToScreen(x, y);
        if (sx + size <= 0 || sy + size <= 0 || sx >= boardWidth || sy >= boardHeight) return;
        if (size == 1) DrawPixel(sx, sy, color);
        else DrawBox(sx, sy, sx + size - 1, sy + size - 1, color, FALSE);
    });
}

} // namespace ShapeDrawing
