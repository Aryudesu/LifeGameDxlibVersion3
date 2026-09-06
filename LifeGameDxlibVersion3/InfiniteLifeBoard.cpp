#include "InfiniteLifeBoard.h"

#include <bit>
#include <utility>

namespace {
std::size_t mix64(std::uint64_t value) noexcept {
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return static_cast<std::size_t>(value);
}
}

std::size_t InfiniteLifeBoard::ChunkCoordHash::operator()(const ChunkCoord& value) const noexcept {
    const auto hx = mix64(static_cast<std::uint64_t>(value.x));
    const auto hy = mix64(static_cast<std::uint64_t>(value.y));
    return hx ^ (hy + 0x9e3779b97f4a7c15ULL + (hx << 6) + (hx >> 2));
}

std::size_t InfiniteLifeBoard::CellCoordHash::operator()(const CellCoord& value) const noexcept {
    const auto hx = mix64(static_cast<std::uint64_t>(value.x));
    const auto hy = mix64(static_cast<std::uint64_t>(value.y));
    return hx ^ (hy + 0x9e3779b97f4a7c15ULL + (hx << 6) + (hx >> 2));
}

bool InfiniteLifeBoard::Chunk::empty() const noexcept {
    for (const std::uint64_t row : rows) {
        if (row != 0) return false;
    }
    return true;
}

InfiniteLifeBoard::Coord InfiniteLifeBoard::floorDiv(Coord value, Coord divisor) noexcept {
    Coord quotient = value / divisor;
    const Coord remainder = value % divisor;
    if (remainder < 0) --quotient;
    return quotient;
}

int InfiniteLifeBoard::floorMod(Coord value, int divisor) noexcept {
    Coord remainder = value % divisor;
    if (remainder < 0) remainder += divisor;
    return static_cast<int>(remainder);
}

InfiniteLifeBoard::ChunkCoord InfiniteLifeBoard::chunkCoordOf(Coord x, Coord y) noexcept {
    return {floorDiv(x, ChunkSize), floorDiv(y, ChunkSize)};
}

int InfiniteLifeBoard::localX(Coord x) noexcept { return floorMod(x, ChunkSize); }
int InfiniteLifeBoard::localY(Coord y) noexcept { return floorMod(y, ChunkSize); }

bool InfiniteLifeBoard::isAlive(Coord x, Coord y) const noexcept {
    const ChunkCoord chunkCoord = chunkCoordOf(x, y);
    const auto it = chunks_.find(chunkCoord);
    if (it == chunks_.end()) return false;
    const std::uint64_t mask = std::uint64_t{1} << localX(x);
    return (it->second.rows[localY(y)] & mask) != 0;
}

void InfiniteLifeBoard::setAlive(Coord x, Coord y, bool alive) {
    const ChunkCoord chunkCoord = chunkCoordOf(x, y);
    const int lx = localX(x);
    const int ly = localY(y);
    const std::uint64_t mask = std::uint64_t{1} << lx;

    if (alive) {
        Chunk& chunk = chunks_[chunkCoord];
        if ((chunk.rows[ly] & mask) == 0) {
            chunk.rows[ly] |= mask;
            ++aliveCellCount_;
        }
        return;
    }

    const auto it = chunks_.find(chunkCoord);
    if (it == chunks_.end() || (it->second.rows[ly] & mask) == 0) return;
    it->second.rows[ly] &= ~mask;
    --aliveCellCount_;
    if (it->second.empty()) chunks_.erase(it);
}

void InfiniteLifeBoard::clear() noexcept {
    chunks_.clear();
    aliveCellCount_ = 0;
}

void InfiniteLifeBoard::forEachAliveCell(const std::function<void(Coord, Coord)>& visitor) const {
    for (const auto& [chunkCoord, chunk] : chunks_) {
        const Coord baseX = chunkCoord.x * ChunkSize;
        const Coord baseY = chunkCoord.y * ChunkSize;
        for (int y = 0; y < ChunkSize; ++y) {
            std::uint64_t bits = chunk.rows[y];
            while (bits != 0) {
                const int x = std::countr_zero(bits);
                visitor(baseX + x, baseY + y);
                bits &= bits - 1;
            }
        }
    }
}

void InfiniteLifeBoard::step() {
    std::unordered_map<CellCoord, std::uint8_t, CellCoordHash> neighborCounts;
    neighborCounts.reserve(static_cast<std::size_t>(aliveCellCount_) * 6 + 32);

    forEachAliveCell([&](Coord x, Coord y) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                ++neighborCounts[{x + dx, y + dy}];
            }
        }
    });

    InfiniteLifeBoard next;
    for (const auto& [cell, count] : neighborCounts) {
        if (count == 3 || (count == 2 && isAlive(cell.x, cell.y))) {
            next.setAlive(cell.x, cell.y, true);
        }
    }

    chunks_.swap(next.chunks_);
    aliveCellCount_ = next.aliveCellCount_;
}
