#include "InfiniteLifeBoard.h"

#include <bit>
#include <limits>
#include <unordered_map>
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

struct SplitCoordinate {
    InfiniteLifeBoard::Coord chunk = 0;
    int local = 0;
};

constexpr SplitCoordinate splitCoordinate(InfiniteLifeBoard::Coord value) noexcept {
    constexpr std::uint64_t ChunkSize = InfiniteLifeBoard::ChunkSize;
    constexpr std::uint64_t ChunkMask = ChunkSize - 1;
    static_assert((ChunkSize & ChunkMask) == 0, "ChunkSize must be a power of two");

    const std::uint64_t bits = static_cast<std::uint64_t>(value);
    const int local = static_cast<int>(bits & ChunkMask);

    if (value >= 0) {
        return {static_cast<InfiniteLifeBoard::Coord>(bits / ChunkSize), local};
    }

    // Convert the magnitude in unsigned arithmetic so INT64_MIN is safe.
    // The ceil division gives floor(value / ChunkSize) for negative values.
    const std::uint64_t magnitude = std::uint64_t{0} - bits;
    const std::uint64_t chunkMagnitude = (magnitude + ChunkMask) / ChunkSize;
    return {-static_cast<InfiniteLifeBoard::Coord>(chunkMagnitude), local};
}

static_assert(splitCoordinate(-129).chunk == -3 && splitCoordinate(-129).local == 63);
static_assert(splitCoordinate(-128).chunk == -2 && splitCoordinate(-128).local == 0);
static_assert(splitCoordinate(-127).chunk == -2 && splitCoordinate(-127).local == 1);
static_assert(splitCoordinate(-65).chunk == -2 && splitCoordinate(-65).local == 63);
static_assert(splitCoordinate(-64).chunk == -1 && splitCoordinate(-64).local == 0);
static_assert(splitCoordinate(-63).chunk == -1 && splitCoordinate(-63).local == 1);
static_assert(splitCoordinate(-1).chunk == -1 && splitCoordinate(-1).local == 63);
static_assert(splitCoordinate(0).chunk == 0 && splitCoordinate(0).local == 0);
static_assert(splitCoordinate(63).chunk == 0 && splitCoordinate(63).local == 63);
static_assert(splitCoordinate(64).chunk == 1 && splitCoordinate(64).local == 0);
static_assert(splitCoordinate(std::numeric_limits<InfiniteLifeBoard::Coord>::min()).local == 0);
static_assert(splitCoordinate(std::numeric_limits<InfiniteLifeBoard::Coord>::max()).local == 63);
}

std::size_t InfiniteLifeBoard::ChunkCoordHash::operator()(const ChunkCoord& value) const noexcept {
    const auto hx = mix64(static_cast<std::uint64_t>(value.x));
    const auto hy = mix64(static_cast<std::uint64_t>(value.y));
    return hx ^ (hy + 0x9e3779b97f4a7c15ULL + (hx << 6) + (hx >> 2));
}

InfiniteLifeBoard::Coord InfiniteLifeBoard::floorDiv(Coord value, Coord divisor) noexcept {
    if (divisor == ChunkSize) return splitCoordinate(value).chunk;

    Coord quotient = value / divisor;
    const Coord remainder = value % divisor;
    if (remainder < 0) --quotient;
    return quotient;
}

int InfiniteLifeBoard::floorMod(Coord value, int divisor) noexcept {
    if (divisor == ChunkSize) return splitCoordinate(value).local;

    Coord remainder = value % divisor;
    if (remainder < 0) remainder += divisor;
    return static_cast<int>(remainder);
}

InfiniteLifeBoard::ChunkCoord InfiniteLifeBoard::chunkCoordOf(Coord x, Coord y) noexcept {
    const SplitCoordinate sx = splitCoordinate(x);
    const SplitCoordinate sy = splitCoordinate(y);
    return {sx.chunk, sy.chunk};
}

int InfiniteLifeBoard::localX(Coord x) noexcept { return splitCoordinate(x).local; }
int InfiniteLifeBoard::localY(Coord y) noexcept { return splitCoordinate(y).local; }

bool InfiniteLifeBoard::isAlive(Coord x, Coord y) const noexcept {
    const SplitCoordinate sx = splitCoordinate(x);
    const SplitCoordinate sy = splitCoordinate(y);
    const ChunkCoord chunkCoord{sx.chunk, sy.chunk};
    const auto it = chunks_.find(chunkCoord);
    if (it == chunks_.end()) return false;
    const std::uint64_t mask = std::uint64_t{1} << sx.local;
    return (it->second.rows[sy.local] & mask) != 0;
}

void InfiniteLifeBoard::setAlive(Coord x, Coord y, bool alive) {
    constexpr std::uint64_t WestEdgeMask = 1ULL;
    constexpr std::uint64_t EastEdgeMask = 1ULL << (ChunkSize - 1);

    const SplitCoordinate sx = splitCoordinate(x);
    const SplitCoordinate sy = splitCoordinate(y);
    const ChunkCoord chunkCoord{sx.chunk, sy.chunk};
    const int lx = sx.local;
    const int ly = sy.local;
    const std::uint64_t cellMask = std::uint64_t{1} << lx;
    const std::uint64_t rowMask = std::uint64_t{1} << ly;

    if (alive) {
        Chunk& chunk = chunks_[chunkCoord];
        if ((chunk.rows[ly] & cellMask) == 0) {
            chunk.rows[ly] |= cellMask;
            chunk.nonEmptyRows |= rowMask;
            if (lx == 0) chunk.westEdgeRows |= rowMask;
            if (lx == ChunkSize - 1) chunk.eastEdgeRows |= rowMask;
            ++aliveCellCount_;
        }
        return;
    }

    const auto it = chunks_.find(chunkCoord);
    if (it == chunks_.end() || (it->second.rows[ly] & cellMask) == 0) return;

    Chunk& chunk = it->second;
    chunk.rows[ly] &= ~cellMask;
    if (chunk.rows[ly] == 0) chunk.nonEmptyRows &= ~rowMask;
    if (lx == 0 && (chunk.rows[ly] & WestEdgeMask) == 0) chunk.westEdgeRows &= ~rowMask;
    if (lx == ChunkSize - 1 && (chunk.rows[ly] & EastEdgeMask) == 0) chunk.eastEdgeRows &= ~rowMask;
    --aliveCellCount_;
    if (chunk.empty()) chunks_.erase(it);
}

void InfiniteLifeBoard::clear() noexcept {
    chunks_.clear();
    aliveCellCount_ = 0;
}

void InfiniteLifeBoard::forEachAliveCell(const std::function<void(Coord, Coord)>& visitor) const {
    for (const auto& [chunkCoord, chunk] : chunks_) {
        const Coord baseX = chunkCoord.x * ChunkSize;
        const Coord baseY = chunkCoord.y * ChunkSize;
        std::uint64_t activeRows = chunk.nonEmptyRows;
        while (activeRows != 0) {
            const int y = std::countr_zero(activeRows);
            std::uint64_t bits = chunk.rows[y];
            while (bits != 0) {
                const int x = std::countr_zero(bits);
                visitor(baseX + x, baseY + y);
                bits &= bits - 1;
            }
            activeRows &= activeRows - 1;
        }
    }
}

void InfiniteLifeBoard::step() {
    if (chunks_.empty()) return;

    constexpr Coord MinChunkCoord = std::numeric_limits<Coord>::min() / ChunkSize;
    constexpr Coord MaxChunkCoord = std::numeric_limits<Coord>::max() / ChunkSize;
    constexpr std::uint64_t WestEdgeMask = 1ULL;
    constexpr std::uint64_t EastEdgeMask = 1ULL << (ChunkSize - 1);
    constexpr std::uint64_t NorthRowMask = 1ULL;
    constexpr std::uint64_t SouthRowMask = 1ULL << (ChunkSize - 1);

    struct CandidateNeighborhood {
        const Chunk* chunks[3][3]{};
    };

    std::unordered_map<ChunkCoord, CandidateNeighborhood, ChunkCoordHash> candidates;
    candidates.reserve(chunks_.size() * 2 + 16);

    const auto addCandidate = [&](Coord x, Coord y, const Chunk* source, int sourceDx, int sourceDy) {
        if (x < MinChunkCoord || x > MaxChunkCoord ||
            y < MinChunkCoord || y > MaxChunkCoord) {
            return;
        }
        auto [it, inserted] = candidates.try_emplace(ChunkCoord{x, y});
        it->second.chunks[sourceDy + 1][sourceDx + 1] = source;
    };

    for (const auto& [coord, chunk] : chunks_) {
        // Build each candidate's 3x3 neighborhood while discovering candidates.
        // sourceDx/sourceDy are the source chunk's offsets from the candidate.
        addCandidate(coord.x, coord.y, &chunk, 0, 0);

        if (chunk.westEdgeRows != 0 && coord.x > MinChunkCoord) {
            addCandidate(coord.x - 1, coord.y, &chunk, 1, 0);
        }
        if (chunk.eastEdgeRows != 0 && coord.x < MaxChunkCoord) {
            addCandidate(coord.x + 1, coord.y, &chunk, -1, 0);
        }
        if ((chunk.nonEmptyRows & NorthRowMask) != 0 && coord.y > MinChunkCoord) {
            addCandidate(coord.x, coord.y - 1, &chunk, 0, 1);
        }
        if ((chunk.nonEmptyRows & SouthRowMask) != 0 && coord.y < MaxChunkCoord) {
            addCandidate(coord.x, coord.y + 1, &chunk, 0, -1);
        }

        if ((chunk.westEdgeRows & NorthRowMask) != 0 &&
            coord.x > MinChunkCoord && coord.y > MinChunkCoord) {
            addCandidate(coord.x - 1, coord.y - 1, &chunk, 1, 1);
        }
        if ((chunk.eastEdgeRows & NorthRowMask) != 0 &&
            coord.x < MaxChunkCoord && coord.y > MinChunkCoord) {
            addCandidate(coord.x + 1, coord.y - 1, &chunk, -1, 1);
        }
        if ((chunk.westEdgeRows & SouthRowMask) != 0 &&
            coord.x > MinChunkCoord && coord.y < MaxChunkCoord) {
            addCandidate(coord.x - 1, coord.y + 1, &chunk, 1, -1);
        }
        if ((chunk.eastEdgeRows & SouthRowMask) != 0 &&
            coord.x < MaxChunkCoord && coord.y < MaxChunkCoord) {
            addCandidate(coord.x + 1, coord.y + 1, &chunk, -1, -1);
        }
    }

    InfiniteLifeBoard next;
    next.chunks_.reserve(candidates.size());

    std::uint64_t nextAliveCellCount = 0;

    for (const auto& [coord, candidate] : candidates) {
        const auto& neighborhood = candidate.chunks;

        const Chunk* west = neighborhood[1][0];
        const Chunk* center = neighborhood[1][1];
        const Chunk* east = neighborhood[1][2];
        const Chunk* northWest = neighborhood[0][0];
        const Chunk* north = neighborhood[0][1];
        const Chunk* northEast = neighborhood[0][2];
        const Chunk* southWest = neighborhood[2][0];
        const Chunk* south = neighborhood[2][1];
        const Chunk* southEast = neighborhood[2][2];

        // Track row occupancy and edge occupancy in each chunk so sparse chunks
        // can skip untouched rows. A target row only needs work when a live
        // source exists in that row or one of its two vertical neighbors.
        std::uint64_t sourceRows = center == nullptr ? 0 : center->nonEmptyRows;
        if (west != nullptr) sourceRows |= west->eastEdgeRows;
        if (east != nullptr) sourceRows |= east->westEdgeRows;

        std::uint64_t rowsToUpdate = sourceRows | (sourceRows << 1) | (sourceRows >> 1);

        const bool hasNorthSource =
            (north != nullptr && (north->nonEmptyRows & SouthRowMask) != 0) ||
            (northWest != nullptr && (northWest->eastEdgeRows & SouthRowMask) != 0) ||
            (northEast != nullptr && (northEast->westEdgeRows & SouthRowMask) != 0);
        if (hasNorthSource) rowsToUpdate |= NorthRowMask;

        const bool hasSouthSource =
            (south != nullptr && (south->nonEmptyRows & NorthRowMask) != 0) ||
            (southWest != nullptr && (southWest->eastEdgeRows & NorthRowMask) != 0) ||
            (southEast != nullptr && (southEast->westEdgeRows & NorthRowMask) != 0);
        if (hasSouthSource) rowsToUpdate |= SouthRowMask;

        if (rowsToUpdate == 0) continue;

        // Materialize only source rows that can actually be read by an
        // evaluated target row. #23 copied all 64 rows from each horizontal
        // chunk; sparse candidates need only rowsToUpdate and its +/-1 rows.
        const std::uint64_t neededLocalRows =
            rowsToUpdate | (rowsToUpdate << 1) | (rowsToUpdate >> 1);

        std::uint64_t westRows[ChunkSize + 2]{};
        std::uint64_t centerRows[ChunkSize + 2]{};
        std::uint64_t eastRows[ChunkSize + 2]{};

        if ((rowsToUpdate & NorthRowMask) != 0) {
            westRows[0] = northWest == nullptr ? 0 : northWest->rows[ChunkSize - 1];
            centerRows[0] = north == nullptr ? 0 : north->rows[ChunkSize - 1];
            eastRows[0] = northEast == nullptr ? 0 : northEast->rows[ChunkSize - 1];
        }

        std::uint64_t pendingSourceRows = neededLocalRows;
        while (pendingSourceRows != 0) {
            const int y = std::countr_zero(pendingSourceRows);
            const int rowIndex = y + 1;
            westRows[rowIndex] = west == nullptr ? 0 : west->rows[y];
            centerRows[rowIndex] = center == nullptr ? 0 : center->rows[y];
            eastRows[rowIndex] = east == nullptr ? 0 : east->rows[y];
            pendingSourceRows &= pendingSourceRows - 1;
        }

        if ((rowsToUpdate & SouthRowMask) != 0) {
            westRows[ChunkSize + 1] = southWest == nullptr ? 0 : southWest->rows[0];
            centerRows[ChunkSize + 1] = south == nullptr ? 0 : south->rows[0];
            eastRows[ChunkSize + 1] = southEast == nullptr ? 0 : southEast->rows[0];
        }

        Chunk nextChunk;
        std::uint64_t pendingRows = rowsToUpdate;
        while (pendingRows != 0) {
            const int y = std::countr_zero(pendingRows);
            const std::uint64_t rowMask = std::uint64_t{1} << y;
            const int rowIndex = y + 1;

            const std::uint64_t topWest = westRows[rowIndex - 1];
            const std::uint64_t top = centerRows[rowIndex - 1];
            const std::uint64_t topEast = eastRows[rowIndex - 1];
            const std::uint64_t middleWest = westRows[rowIndex];
            const std::uint64_t middle = centerRows[rowIndex];
            const std::uint64_t middleEast = eastRows[rowIndex];
            const std::uint64_t bottomWest = westRows[rowIndex + 1];
            const std::uint64_t bottom = centerRows[rowIndex + 1];
            const std::uint64_t bottomEast = eastRows[rowIndex + 1];

            const std::uint64_t a = (top << 1) | (topWest >> 63);
            const std::uint64_t b = top;
            const std::uint64_t c = (top >> 1) | ((topEast & WestEdgeMask) << 63);
            const std::uint64_t d = (middle << 1) | (middleWest >> 63);
            const std::uint64_t e = (middle >> 1) | ((middleEast & WestEdgeMask) << 63);
            const std::uint64_t f = (bottom << 1) | (bottomWest >> 63);
            const std::uint64_t g = bottom;
            const std::uint64_t h = (bottom >> 1) | ((bottomEast & WestEdgeMask) << 63);

            // Carry-save adder tree: reduce 8 one-bit neighbor masks to the
            // binary bitplanes of the population count with a shallow circuit.
            const auto csa = [](std::uint64_t& carry, std::uint64_t& sum,
                                std::uint64_t x, std::uint64_t y, std::uint64_t z) noexcept {
                const std::uint64_t u = x ^ y;
                sum = u ^ z;
                carry = (x & y) | (u & z);
            };

            std::uint64_t onesAB, twosAB;
            std::uint64_t onesCD, twosCD;
            std::uint64_t onesEF, twosEF;
            csa(twosAB, onesAB, a, b, c);
            csa(twosCD, onesCD, d, e, f);
            csa(twosEF, onesEF, g, h, 0);

            std::uint64_t twosFromOnes, ones;
            csa(twosFromOnes, ones, onesAB, onesCD, onesEF);

            const std::uint64_t twosXor = twosAB ^ twosCD;
            const std::uint64_t twos = twosXor ^ twosEF ^ twosFromOnes;
            const std::uint64_t fours =
                (twosAB & twosCD) |
                (twosEF & twosFromOnes) |
                (twosXor & (twosEF ^ twosFromOnes));

            // Counts 2 and 3 share bit1=1 and all higher bits=0.
            const std::uint64_t lowCounts = twos & ~fours;
            const std::uint64_t exactlyTwo = lowCounts & ~ones;
            const std::uint64_t exactlyThree = lowCounts & ones;
            const std::uint64_t nextRow = exactlyThree | (middle & exactlyTwo);

            if (nextRow != 0) {
                nextChunk.rows[y] = nextRow;
                nextChunk.nonEmptyRows |= rowMask;
                if ((nextRow & WestEdgeMask) != 0) nextChunk.westEdgeRows |= rowMask;
                if ((nextRow & EastEdgeMask) != 0) nextChunk.eastEdgeRows |= rowMask;
                nextAliveCellCount += static_cast<std::uint64_t>(std::popcount(nextRow));
            }

            pendingRows &= pendingRows - 1;
        }

        if (!nextChunk.empty()) {
            next.chunks_.emplace(coord, std::move(nextChunk));
        }
    }

    chunks_.swap(next.chunks_);
    aliveCellCount_ = nextAliveCellCount;
}
