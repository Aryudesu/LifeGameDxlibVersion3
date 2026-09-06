#pragma once

#include <array>
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
    void step();
    std::uint64_t aliveCellCount() const noexcept { return aliveCellCount_; }
    std::size_t chunkCount() const noexcept { return chunks_.size(); }

    void forEachAliveCell(const std::function<void(Coord, Coord)>& visitor) const;

private:
    struct ChunkCoord {
        Coord x = 0;
        Coord y = 0;
        bool operator==(const ChunkCoord&) const noexcept = default;
    };

    struct ChunkCoordHash {
        std::size_t operator()(const ChunkCoord& value) const noexcept;
    };

    struct CellCoord {
        Coord x = 0;
        Coord y = 0;
        bool operator==(const CellCoord&) const noexcept = default;
    };

    struct CellCoordHash {
        std::size_t operator()(const CellCoord& value) const noexcept;
    };

    struct Chunk {
        std::array<std::uint64_t, ChunkSize> rows{};
        bool empty() const noexcept;
    };

    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks_;
    std::uint64_t aliveCellCount_ = 0;

    static Coord floorDiv(Coord value, Coord divisor) noexcept;
    static int floorMod(Coord value, int divisor) noexcept;
    static ChunkCoord chunkCoordOf(Coord x, Coord y) noexcept;
    static int localX(Coord x) noexcept;
    static int localY(Coord y) noexcept;
};
