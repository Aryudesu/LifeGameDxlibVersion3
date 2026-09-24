#pragma once

#include "InfiniteLifeBoard.h"
#include "SelectionMask.h"

#include <cstdint>
#include <limits>
#include <vector>

class SelectionClipboard {
public:
    using Coord = InfiniteLifeBoard::Coord;

    struct Cell {
        Coord x = 0;
        Coord y = 0;
    };

    void clear() noexcept {
        cells_.clear();
        width_ = 0;
        height_ = 0;
        hasData_ = false;
        replaceRectangle_ = false;
    }

    bool hasData() const noexcept { return hasData_; }
    Coord width() const noexcept { return width_; }
    Coord height() const noexcept { return height_; }
    const std::vector<Cell>& cells() const noexcept { return cells_; }
    bool replaceRectangle() const noexcept { return replaceRectangle_; }

    bool copy(const InfiniteLifeBoard& board, const SelectionMask& selection) {
        if (!selection.active() || selection.dragging()) return false;

        const Coord left = selection.minX();
        const Coord top = selection.minY();
        const Coord right = selection.maxX();
        const Coord bottom = selection.maxY();

        width_ = right - left + 1;
        height_ = bottom - top + 1;
        cells_.clear();
        replaceRectangle_ = !selection.liveOnly();

        // The clipboard always stores live cells sparsely. In full-rectangle
        // mode the rectangle bounds are also authoritative, so paste clears
        // destination cells that correspond to dead source cells. In live-only
        // mode the same sparse cells behave like a transparent stamp.
        board.forEachAliveCellInRect(left, top, right + 1, bottom + 1,
            [&](Coord x, Coord y) {
                cells_.push_back({x - left, y - top});
            });

        hasData_ = true;
        return true;
    }

    template <class SetAlive>
    void paste(Coord left, Coord top, SetAlive&& setAlive) const {
        if (!hasData_) return;

        if (replaceRectangle_) {
            for (Coord y = 0; y < height_; ++y) {
                for (Coord x = 0; x < width_; ++x) {
                    setAlive(left + x, top + y, false);
                }
            }
        }

        for (const Cell& cell : cells_) {
            setAlive(left + cell.x, top + cell.y, true);
        }
    }

private:
    bool hasData_ = false;
    bool replaceRectangle_ = false;
    Coord width_ = 0;
    Coord height_ = 0;
    std::vector<Cell> cells_;
};
