#include "InfiniteLifeBoard.h"

#include <bit>
#include <limits>
#include <unordered_set>
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
    if (chunks_.empty()) return;

    constexpr Coord MinChunkCoord = std::numeric_limits<Coord>::min() / ChunkSize;
    constexpr Coord MaxChunkCoord = std::numeric_limits<Coord>::max() / ChunkSize;
    constexpr std::uint64_t WestEdgeMask = 1ULL;
    constexpr std::uint64_t EastEdgeMask = 1ULL << (ChunkSize - 1);

    // A new live cell can only appear in a neighboring chunk when at least one
    // live cell touches the corresponding boundary of the source chunk. This
    // is much tighter than blindly expanding every active chunk to all 3x3
    // neighbors, which is especially important for sparse moving patterns.
    std::unordered_set<ChunkCoord, ChunkCoordHash> candidates;
    candidates.reserve(chunks_.size() * 2 + 16);

    const auto addCandidate = [&](Coord x, Coord y) {
        if (x < MinChunkCoord || x > MaxChunkCoord ||
            y < MinChunkCoord || y > MaxChunkCoord) {
            return;
        }
        candidates.insert({x, y});
    };

    for (const auto& [coord, chunk] : chunks_) {
        addCandidate(coord.x, coord.y);

        bool touchesWest = false;
        bool touchesEast = false;
        for (const std::uint64_t row : chunk.rows) {
            touchesWest = touchesWest || (row & WestEdgeMask) != 0;
            touchesEast = touchesEast || (row & EastEdgeMask) != 0;
            if (touchesWest && touchesEast) break;
        }

        const bool touchesNorth = chunk.rows.front() != 0;
        const bool touchesSouth = chunk.rows.back() != 0;

        if (touchesWest && coord.x > MinChunkCoord) addCandidate(coord.x - 1, coord.y);
        if (touchesEast && coord.x < MaxChunkCoord) addCandidate(coord.x + 1, coord.y);
        if (touchesNorth && coord.y > MinChunkCoord) addCandidate(coord.x, coord.y - 1);
        if (touchesSouth && coord.y < MaxChunkCoord) addCandidate(coord.x, coord.y + 1);

        if ((chunk.rows.front() & WestEdgeMask) != 0 &&
            coord.x > MinChunkCoord && coord.y > MinChunkCoord) {
            addCandidate(coord.x - 1, coord.y - 1);
        }
        if ((chunk.rows.front() & EastEdgeMask) != 0 &&
            coord.x < MaxChunkCoord && coord.y > MinChunkCoord) {
            addCandidate(coord.x + 1, coord.y - 1);
        }
        if ((chunk.rows.back() & WestEdgeMask) != 0 &&
            coord.x > MinChunkCoord && coord.y < MaxChunkCoord) {
            addCandidate(coord.x - 1, coord.y + 1);
        }
        if ((chunk.rows.back() & EastEdgeMask) != 0 &&
            coord.x < MaxChunkCoord && coord.y < MaxChunkCoord) {
            addCandidate(coord.x + 1, coord.y + 1);
        }
    }

    InfiniteLifeBoard next;
    next.chunks_.reserve(candidates.size());

    const auto chunkAt = [&](Coord x, Coord y) -> const Chunk* {
        if (x < MinChunkCoord || x > MaxChunkCoord ||
            y < MinChunkCoord || y > MaxChunkCoord) {
            return nullptr;
        }
        const auto it = chunks_.find({x, y});
        return it == chunks_.end() ? nullptr : &it->second;
    };

    std::uint64_t nextAliveCellCount = 0;

    for (const ChunkCoord& coord : candidates) {
        // Resolve the 3x3 neighborhood once per candidate chunk. The 64 row
        // updates below then use raw pointers rather than repeating hash lookups.
        const Chunk* neighborhood[3][3]{};
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const bool xOutside =
                    (dx < 0 && coord.x == MinChunkCoord) ||
                    (dx > 0 && coord.x == MaxChunkCoord);
                const bool yOutside =
                    (dy < 0 && coord.y == MinChunkCoord) ||
                    (dy > 0 && coord.y == MaxChunkCoord);
                if (xOutside || yOutside) continue;
                neighborhood[dy + 1][dx + 1] = chunkAt(coord.x + dx, coord.y + dy);
            }
        }

        const auto rowFrom = [&](int horizontalChunkOffset, int row) noexcept -> std::uint64_t {
            int verticalChunkOffset = 0;
            int localRow = row;
            if (row < 0) {
                verticalChunkOffset = -1;
                localRow += ChunkSize;
            } else if (row >= ChunkSize) {
                verticalChunkOffset = 1;
                localRow -= ChunkSize;
            }

            const Chunk* source = neighborhood[verticalChunkOffset + 1][horizontalChunkOffset + 1];
            return source == nullptr ? 0 : source->rows[localRow];
        };

        Chunk nextChunk;
        for (int y = 0; y < ChunkSize; ++y) {
            const std::uint64_t topWest = rowFrom(-1, y - 1);
            const std::uint64_t top = rowFrom(0, y - 1);
            const std::uint64_t topEast = rowFrom(1, y - 1);
            const std::uint64_t middleWest = rowFrom(-1, y);
            const std::uint64_t middle = rowFrom(0, y);
            const std::uint64_t middleEast = rowFrom(1, y);
            const std::uint64_t bottomWest = rowFrom(-1, y + 1);
            const std::uint64_t bottom = rowFrom(0, y + 1);
            const std::uint64_t bottomEast = rowFrom(1, y + 1);

            const std::uint64_t neighborMasks[8] = {
                (top << 1) | (topWest >> 63),
                top,
                (top >> 1) | ((topEast & 1ULL) << 63),
                (middle << 1) | (middleWest >> 63),
                (middle >> 1) | ((middleEast & 1ULL) << 63),
                (bottom << 1) | (bottomWest >> 63),
                bottom,
                (bottom >> 1) | ((bottomEast & 1ULL) << 63)
            };

            // Bit-sliced addition: each bit position independently counts its
            // eight neighbors without creating a hash entry per cell.
            std::uint64_t ones = 0;
            std::uint64_t twos = 0;
            std::uint64_t fours = 0;
            std::uint64_t eights = 0;
            for (const std::uint64_t mask : neighborMasks) {
                const std::uint64_t carryToTwos = ones & mask;
                ones ^= mask;
                const std::uint64_t carryToFours = twos & carryToTwos;
                twos ^= carryToTwos;
                const std::uint64_t carryToEights = fours & carryToFours;
                fours ^= carryToFours;
                eights ^= carryToEights;
            }

            const std::uint64_t exactlyTwo = ~eights & ~fours & twos & ~ones;
            const std::uint64_t exactlyThree = ~eights & ~fours & twos & ones;
            const std::uint64_t nextRow = exactlyThree | (middle & exactlyTwo);
            nextChunk.rows[y] = nextRow;
            nextAliveCellCount += static_cast<std::uint64_t>(std::popcount(nextRow));
        }

        if (!nextChunk.empty()) {
            next.chunks_.emplace(coord, std::move(nextChunk));
        }
    }

    chunks_.swap(next.chunks_);
    aliveCellCount_ = nextAliveCellCount;
}
