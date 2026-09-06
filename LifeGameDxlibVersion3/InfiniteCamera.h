#pragma once

#include <cstdint>
#include <utility>

class InfiniteCamera {
public:
    using Coord = std::int64_t;

    InfiniteCamera(int screenWidth, int screenHeight, int cellSize)
        : screenWidth_(screenWidth), screenHeight_(screenHeight), cellSize_(cellSize) {}

    Coord x() const noexcept { return x_; }
    Coord y() const noexcept { return y_; }
    int cellSize() const noexcept { return cellSize_; }

    void move(Coord dx, Coord dy) noexcept { x_ += dx; y_ += dy; }
    bool zoomInAt(int screenX, int screenY, int maxCellSize) noexcept;
    bool zoomOutAt(int screenX, int screenY, int minCellSize) noexcept;
    std::pair<Coord, Coord> screenToBoard(int screenX, int screenY) const noexcept;
    std::pair<int, int> boardToScreen(Coord boardX, Coord boardY) const noexcept;

private:
    int screenWidth_;
    int screenHeight_;
    Coord x_ = 0;
    Coord y_ = 0;
    int cellSize_ = 1;

    bool zoomAt(int screenX, int screenY, int newCellSize) noexcept;
};
