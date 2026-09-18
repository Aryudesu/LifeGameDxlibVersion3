#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <unordered_map>

class InfiniteLifeBoard {
public:
    using Coord = std::int64_t;
    static constexpr int ChunkSize = 64;

    bool isAlive(Coord x, Coord y) const noexcept;
    void setAlive(Coord x, Coord y, bool alive);
    void clear() noexcept;

    struct StepProfile {
        double candidateBuildMs = 0.0;
        double candidateEvaluateMs = 0.0;
        double neighborhoodLookupEstimatedMs = 0.0;
        std::size_t candidateCount = 0;
        std::size_t neighborhoodLookupSamples = 0;
        std::uint64_t rowsEvaluated = 0;
    };

    void step();
    const StepProfile& lastStepProfile() const noexcept { return lastStepProfile_; }
    std::uint64_t aliveCellCount() const noexcept { return aliveCellCount_; }
    std::size_t chunkCount() const noexcept { return chunks_.size(); }

    void forEachAliveCell(const std::function<void(Coord, Coord)>& visitor) const;

    // Visits live cells in the half-open rectangle [minX, maxX) x [minY, maxY).
    // Only chunks intersecting the rectangle are looked up, so rendering cost is
    // independent of live cells/chunks that are far outside the camera view.
    void forEachAliveCellInRect(
        Coord minX,
        Coord minY,
        Coord maxX,
        Coord maxY,
        const std::function<void(Coord, Coord)>& visitor) const {
        if (minX >= maxX || minY >= maxY) return;

        const Coord minChunkX = floorDiv(minX, ChunkSize);
        const Coord minChunkY = floorDiv(minY, ChunkSize);
        const Coord maxChunkX = floorDiv(maxX - 1, ChunkSize);
        const Coord maxChunkY = floorDiv(maxY - 1, ChunkSize);

        for (Coord chunkY = minChunkY;; ++chunkY) {
            const int firstY = chunkY == minChunkY ? localY(minY) : 0;
            const int lastY = chunkY == maxChunkY ? localY(maxY - 1) : ChunkSize - 1;

            for (Coord chunkX = minChunkX;; ++chunkX) {
                const auto it = chunks_.find({chunkX, chunkY});
                if (it != chunks_.end()) {
                    const Chunk& chunk = it->second;
                    const int firstX = chunkX == minChunkX ? localX(minX) : 0;
                    const int lastX = chunkX == maxChunkX ? localX(maxX - 1) : ChunkSize - 1;
                    const std::uint64_t leftMask = ~std::uint64_t{0} << firstX;
                    const std::uint64_t rightMask = lastX == ChunkSize - 1
                        ? ~std::uint64_t{0}
                        : (std::uint64_t{1} << (lastX + 1)) - 1;
                    const std::uint64_t visibleMask = leftMask & rightMask;
                    const Coord baseX = chunkX * ChunkSize;
                    const Coord baseY = chunkY * ChunkSize;

                    std::uint64_t activeRows = chunk.nonEmptyRows;
                    if (firstY != 0) activeRows &= ~((std::uint64_t{1} << firstY) - 1);
                    if (lastY != ChunkSize - 1) {
                        activeRows &= (std::uint64_t{1} << (lastY + 1)) - 1;
                    }

                    while (activeRows != 0) {
                        const int y = std::countr_zero(activeRows);
                        std::uint64_t bits = chunk.rows[y] & visibleMask;
                        while (bits != 0) {
                            const int x = std::countr_zero(bits);
                            visitor(baseX + x, baseY + y);
                            bits &= bits - 1;
                        }
                        activeRows &= activeRows - 1;
                    }
                }

                if (chunkX == maxChunkX) break;
            }
            if (chunkY == maxChunkY) break;
        }
    }

private:
    struct ChunkCoord {
        Coord x = 0;
        Coord y = 0;
        bool operator==(const ChunkCoord&) const noexcept = default;
    };

    struct ChunkCoordHash {
        std::size_t operator()(const ChunkCoord& value) const noexcept;
    };

    struct Chunk {
        std::array<std::uint64_t, ChunkSize> rows{};
        std::uint64_t nonEmptyRows = 0;
        std::uint64_t westEdgeRows = 0;
        std::uint64_t eastEdgeRows = 0;

        bool empty() const noexcept { return nonEmptyRows == 0; }
    };

    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks_;
    std::uint64_t aliveCellCount_ = 0;
    StepProfile lastStepProfile_{};

    static Coord floorDiv(Coord value, Coord divisor) noexcept;
    static int floorMod(Coord value, int divisor) noexcept;
    static ChunkCoord chunkCoordOf(Coord x, Coord y) noexcept;
    static int localX(Coord x) noexcept;
    static int localY(Coord y) noexcept;
};
