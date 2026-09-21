#pragma once

#include "InfiniteLifeBoard.h"

#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace LifeStepSelfTest {
struct Cell {
    InfiniteLifeBoard::Coord x;
    InfiniteLifeBoard::Coord y;
    bool operator==(const Cell&) const noexcept = default;
};
struct CellHash {
    std::size_t operator()(const Cell& c) const noexcept {
        const auto x = static_cast<std::uint64_t>(c.x);
        const auto y = static_cast<std::uint64_t>(c.y);
        return static_cast<std::size_t>(x ^ (y + 0x9e3779b97f4a7c15ULL + (x << 6) + (x >> 2)));
    }
};

inline std::unordered_set<Cell, CellHash> snapshot(const InfiniteLifeBoard& board) {
    std::unordered_set<Cell, CellHash> cells;
    cells.reserve(static_cast<std::size_t>(board.aliveCellCount() * 2 + 16));
    board.forEachAliveCell([&](auto x, auto y) { cells.insert({x, y}); });
    return cells;
}

inline std::unordered_set<Cell, CellHash> naiveStep(
    const std::unordered_set<Cell, CellHash>& current) {
    std::unordered_map<Cell, unsigned char, CellHash> counts;
    counts.reserve(current.size() * 8 + 16);
    for (const auto& cell : current) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                ++counts[{cell.x + dx, cell.y + dy}];
            }
        }
    }

    std::unordered_set<Cell, CellHash> next;
    next.reserve(current.size() * 2 + 16);
    for (const auto& [cell, count] : counts) {
        if (count == 3 || (count == 2 && current.contains(cell))) next.insert(cell);
    }
    return next;
}

inline bool run(std::string& report) {
    using Coord = InfiniteLifeBoard::Coord;
    constexpr std::array<Coord, 15> anchors{
        -129, -128, -127, -65, -64, -63, -2, -1, 0, 1, 2, 63, 64, 65, 127
    };
    std::mt19937_64 rng(0x41525955ULL);
    constexpr int Cases = 5000;
    constexpr int Generations = 4;

    for (int caseIndex = 0; caseIndex < Cases; ++caseIndex) {
        InfiniteLifeBoard board;
        const Coord ax = anchors[static_cast<std::size_t>(rng() % anchors.size())];
        const Coord ay = anchors[static_cast<std::size_t>(rng() % anchors.size())];
        const int radius = 2 + static_cast<int>(rng() % 7);

        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if ((rng() & 3ULL) == 0) board.setAlive(ax + x, ay + y, true);
            }
        }

        auto expected = snapshot(board);
        for (int generation = 1; generation <= Generations; ++generation) {
            expected = naiveStep(expected);
            board.step();
            const auto actual = snapshot(board);
            if (actual != expected) {
                std::ostringstream out;
                out << "FAILED case=" << caseIndex
                    << " generation=" << generation
                    << " anchor=(" << ax << "," << ay << ")"
                    << " expected=" << expected.size()
                    << " actual=" << actual.size();
                for (const auto& cell : expected) {
                    if (!actual.contains(cell)) {
                        out << "\nmissing=(" << cell.x << "," << cell.y << ")";
                        break;
                    }
                }
                for (const auto& cell : actual) {
                    if (!expected.contains(cell)) {
                        out << "\nunexpected=(" << cell.x << "," << cell.y << ")";
                        break;
                    }
                }
                report = out.str();
                return false;
            }
        }
    }

    std::ostringstream out;
    out << "PASS: " << Cases << " random boundary cases x "
        << Generations << " generations matched the naive Conway implementation.";
    report = out.str();
    return true;
}
}
