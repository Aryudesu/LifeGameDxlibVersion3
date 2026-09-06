#include "InfiniteCamera.h"

void InfiniteCamera::panByPixels(int deltaScreenX, int deltaScreenY) noexcept {
    panRemainderX_ -= deltaScreenX;
    panRemainderY_ -= deltaScreenY;

    const int deltaCellsX = panRemainderX_ / cellSize_;
    const int deltaCellsY = panRemainderY_ / cellSize_;

    panRemainderX_ -= deltaCellsX * cellSize_;
    panRemainderY_ -= deltaCellsY * cellSize_;

    move(deltaCellsX, deltaCellsY);
}

void InfiniteCamera::endPan() noexcept {
    panRemainderX_ = 0;
    panRemainderY_ = 0;
}

bool InfiniteCamera::zoomInAt(int screenX, int screenY, int maxCellSize) noexcept {
    if (cellSize_ >= maxCellSize) return false;
    return zoomAt(screenX, screenY, cellSize_ * 2);
}

bool InfiniteCamera::zoomOutAt(int screenX, int screenY, int minCellSize) noexcept {
    if (cellSize_ <= minCellSize) return false;
    return zoomAt(screenX, screenY, cellSize_ / 2);
}

bool InfiniteCamera::zoomAt(int screenX, int screenY, int newCellSize) noexcept {
    if (newCellSize <= 0 || newCellSize == cellSize_) return false;
    const auto [anchorX, anchorY] = screenToBoard(screenX, screenY);
    cellSize_ = newCellSize;
    x_ = anchorX - screenX / cellSize_;
    y_ = anchorY - screenY / cellSize_;
    endPan();
    return true;
}

std::pair<InfiniteCamera::Coord, InfiniteCamera::Coord> InfiniteCamera::screenToBoard(int screenX, int screenY) const noexcept {
    return {x_ + screenX / cellSize_, y_ + screenY / cellSize_};
}

std::pair<int, int> InfiniteCamera::boardToScreen(Coord boardX, Coord boardY) const noexcept {
    const Coord sx = (boardX - x_) * cellSize_;
    const Coord sy = (boardY - y_) * cellSize_;
    return {static_cast<int>(sx), static_cast<int>(sy)};
}
