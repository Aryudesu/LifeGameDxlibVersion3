#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
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

    // Investigation helper for the intermittent negative-coordinate chunk-boundary issue.
    // This is intentionally verbose and should be removed once the root cause is fixed.
    std::string debugCoordinateState(Coord x, Coord y) const;

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

    static Coord floorDiv(Coord value, Coord divisor) noexcept;
    static int floorMod(Coord value, int divisor) noexcept;
    static ChunkCoord chunkCoordOf(Coord x, Coord y) noexcept;
    static int localX(Coord x) noexcept;
    static int localY(Coord y) noexcept;
};

inline std::string InfiniteLifeBoard::debugCoordinateState(Coord x, Coord y) const {
    const ChunkCoord expectedChunk = chunkCoordOf(x, y);
    const int lx = localX(x);
    const int ly = localY(y);
    const std::uint64_t cellMask = std::uint64_t{1} << lx;
    const std::uint64_t rowMask = std::uint64_t{1} << ly;
    const ChunkCoordHash hasher;

    std::ostringstream stream;
    stream << "input x=" << x << " y=" << y << '\n';
    stream << "rawDivMod x/64=" << (x / ChunkSize)
           << " x%64=" << (x % ChunkSize)
           << " y/64=" << (y / ChunkSize)
           << " y%64=" << (y % ChunkSize) << '\n';
    stream << "expected chunk=(" << expectedChunk.x << ',' << expectedChunk.y << ")"
           << " local=(" << lx << ',' << ly << ")"
           << " hash=" << hasher(expectedChunk) << '\n';
    stream << "map size=" << chunks_.size()
           << " bucketCount=" << chunks_.bucket_count()
           << " loadFactor=" << chunks_.load_factor() << '\n';

    const auto expectedIt = chunks_.find(expectedChunk);
    if (expectedIt == chunks_.end()) {
        stream << "expectedChunkFound=0\n";
    } else {
        const Chunk& chunk = expectedIt->second;
        stream << "expectedChunkFound=1"
               << " bucket=" << chunks_.bucket(expectedChunk)
               << " rowBits=" << chunk.rows[ly]
               << " cellBit=" << ((chunk.rows[ly] & cellMask) != 0 ? 1 : 0)
               << " nonEmptyRowBit=" << ((chunk.nonEmptyRows & rowMask) != 0 ? 1 : 0)
               << " westEdgeRowBit=" << ((chunk.westEdgeRows & rowMask) != 0 ? 1 : 0)
               << " eastEdgeRowBit=" << ((chunk.eastEdgeRows & rowMask) != 0 ? 1 : 0)
               << '\n';
    }

    stream << "chunksContainingSameLocalBit:";
    bool foundAny = false;
    for (const auto& [coord, chunk] : chunks_) {
        if ((chunk.rows[ly] & cellMask) == 0) continue;
        foundAny = true;
        stream << " (" << coord.x << ',' << coord.y << ')';
    }
    if (!foundAny) stream << " none";
    stream << '\n';

    return stream.str();
}
