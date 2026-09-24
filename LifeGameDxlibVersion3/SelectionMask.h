#pragma once

#include "InfiniteLifeBoard.h"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

class SelectionMask {
public:
    using Coord = InfiniteLifeBoard::Coord;

    struct Cell {
        Coord x = 0;
        Coord y = 0;
    };

    void clear() noexcept {
        active_ = false;
        dragging_ = false;
        liveOnly_ = false;
        cells_.clear();
    }

    void begin(Coord x, Coord y) noexcept {
        active_ = true;
        dragging_ = true;
        anchorX_ = currentX_ = x;
        anchorY_ = currentY_ = y;
        cells_.clear();
    }

    void update(Coord x, Coord y) noexcept {
        if (!dragging_) return;
        currentX_ = x;
        currentY_ = y;
    }

    void finish(const InfiniteLifeBoard& board, bool liveOnly) {
        if (!active_) return;
        dragging_ = false;
        liveOnly_ = liveOnly;
        rebuildMask(board);
    }

    bool active() const noexcept { return active_; }
    bool dragging() const noexcept { return dragging_; }
    bool liveOnly() const noexcept { return liveOnly_; }
    std::size_t maskedCellCount() const noexcept { return cells_.size(); }

    Coord minX() const noexcept { return std::min(anchorX_, currentX_); }
    Coord minY() const noexcept { return std::min(anchorY_, currentY_); }
    Coord maxX() const noexcept { return std::max(anchorX_, currentX_); }
    Coord maxY() const noexcept { return std::max(anchorY_, currentY_); }

    // Inclusive rectangle bounds. The mask itself is sparse so that a later
    // clipboard operation can reuse it without allocating the whole rectangle.
    const std::vector<Cell>& cells() const noexcept { return cells_; }

    void setLiveOnly(const InfiniteLifeBoard& board, bool liveOnly) {
        if (!active_ || dragging_ || liveOnly_ == liveOnly) return;
        liveOnly_ = liveOnly;
        rebuildMask(board);
    }

private:
    void rebuildMask(const InfiniteLifeBoard& board) {
        cells_.clear();
        const Coord left = minX();
        const Coord top = minY();
        const Coord right = maxX();
        const Coord bottom = maxY();

        if (liveOnly_) {
            // Avoid enumerating dead cells: the board already has an efficient
            // chunk-aware rectangular live-cell visitor.
            board.forEachAliveCellInRect(left, top, right + 1, bottom + 1,
                [&](Coord x, Coord y) { cells_.push_back({x, y}); });
            return;
        }

        // Full rectangular masks are represented lazily by bounds. cells_ is
        // intentionally empty; consumers can distinguish this via liveOnly().
    }

    bool active_ = false;
    bool dragging_ = false;
    bool liveOnly_ = false;
    Coord anchorX_ = 0;
    Coord anchorY_ = 0;
    Coord currentX_ = 0;
    Coord currentY_ = 0;
    std::vector<Cell> cells_;
};
