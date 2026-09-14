#include "InfiniteLifeFile.h"

#include "InfiniteLifeBoard.h"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <utility>
#include <windows.h>

namespace {
constexpr std::array<char, 8> Magic = {'A', 'R', 'Y', 'L', 'I', 'F', 'E', '3'};
constexpr std::uint32_t FormatVersion = 1;
constexpr std::uint64_t BytesPerCell = 16;
constexpr std::uint64_t MaxPayloadBytes = 256ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t MaxCellCount = MaxPayloadBytes / BytesPerCell;
constexpr std::uint32_t FnvOffset = 2166136261u;
constexpr std::uint32_t FnvPrime = 16777619u;

void writeU32(std::ostream& output, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        output.put(static_cast<char>((value >> (i * 8)) & 0xff));
    }
}

void writeU64(std::ostream& output, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        output.put(static_cast<char>((value >> (i * 8)) & 0xff));
    }
}

bool readU32(std::istream& input, std::uint32_t& value) {
    value = 0;
    for (int i = 0; i < 4; ++i) {
        const int byte = input.get();
        if (byte == EOF) return false;
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(byte)) << (i * 8);
    }
    return true;
}

bool readU64(std::istream& input, std::uint64_t& value) {
    value = 0;
    for (int i = 0; i < 8; ++i) {
        const int byte = input.get();
        if (byte == EOF) return false;
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(byte)) << (i * 8);
    }
    return true;
}

void checksumByte(std::uint32_t& hash, std::uint8_t byte) noexcept {
    hash ^= byte;
    hash *= FnvPrime;
}

void checksumU64(std::uint32_t& hash, std::uint64_t value) noexcept {
    for (int i = 0; i < 8; ++i) {
        checksumByte(hash, static_cast<std::uint8_t>((value >> (i * 8)) & 0xff));
    }
}

std::uint32_t calculateChecksum(const InfiniteLifeBoard& board, std::uint64_t generation) {
    std::uint32_t hash = FnvOffset;
    checksumU64(hash, generation);
    checksumU64(hash, board.aliveCellCount());
    board.forEachAliveCell([&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
        checksumU64(hash, std::bit_cast<std::uint64_t>(x));
        checksumU64(hash, std::bit_cast<std::uint64_t>(y));
    });
    return hash;
}

bool replaceFile(const std::string& temporaryPath, const std::string& destinationPath) {
    return MoveFileExA(
        temporaryPath.c_str(),
        destinationPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

std::string diagnosticPathFor(const std::string& savePath) {
    return savePath + ".diagnostic.log";
}

void writeLoadDiagnostic(
    const std::string& savePath,
    const char* phase,
    std::uint64_t cellIndex,
    InfiniteLifeBoard::Coord x,
    InfiniteLifeBoard::Coord y,
    std::uint64_t expectedCellCount,
    const InfiniteLifeBoard& board) {
    std::ofstream output(diagnosticPathFor(savePath), std::ios::app);
    if (!output) return;

    output << "=== LOAD INTEGRITY ERROR ===\n"
           << "phase=" << phase << '\n'
           << "cellIndex=" << cellIndex << '\n'
           << "requestedX=" << x << '\n'
           << "requestedY=" << y << '\n'
           << "expectedCellCount=" << expectedCellCount << '\n'
           << "actualAliveCellCount=" << board.aliveCellCount() << '\n'
           << "chunkCount=" << board.chunkCount() << '\n'
           << "requestedCellAlive=" << (board.isAlive(x, y) ? 1 : 0) << '\n'
           << "=== END LOAD INTEGRITY ERROR ===\n";
}

std::string loadIntegrityError(const char* phase, const std::string& savePath) {
    std::ostringstream stream;
    stream << "The loaded board failed an internal integrity check during " << phase
           << ". The current board was not replaced. Diagnostic: "
           << diagnosticPathFor(savePath);
    return stream.str();
}
} // namespace

namespace InfiniteLifeFile {
bool save(
    const InfiniteLifeBoard& board,
    std::uint64_t generation,
    const std::string& path,
    std::string& errorMessage) {
    errorMessage.clear();

    const std::uint64_t cellCount = board.aliveCellCount();
    if (cellCount > MaxCellCount) {
        errorMessage = "The board is too large to save in this format.";
        return false;
    }

    const std::uint32_t storedChecksum = calculateChecksum(board, generation);
    const std::string temporaryPath = path + ".tmp";
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        errorMessage = "Could not open the temporary save file.";
        return false;
    }

    output.write(Magic.data(), static_cast<std::streamsize>(Magic.size()));
    writeU32(output, FormatVersion);
    writeU64(output, generation);
    writeU64(output, cellCount);
    writeU32(output, storedChecksum);

    board.forEachAliveCell([&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
        writeU64(output, std::bit_cast<std::uint64_t>(x));
        writeU64(output, std::bit_cast<std::uint64_t>(y));
    });
    output.flush();

    if (!output) {
        output.close();
        DeleteFileA(temporaryPath.c_str());
        errorMessage = "Failed while writing the save file.";
        return false;
    }

    output.close();
    if (!replaceFile(temporaryPath, path)) {
        DeleteFileA(temporaryPath.c_str());
        errorMessage = "Could not replace the destination save file.";
        return false;
    }

    return true;
}

bool load(
    InfiniteLifeBoard& board,
    std::uint64_t& generation,
    const std::string& path,
    std::string& errorMessage) {
    errorMessage.clear();

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        errorMessage = "Could not open the save file.";
        return false;
    }

    std::array<char, Magic.size()> magic{};
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!input || magic != Magic) {
        errorMessage = "This is not a LifeGameDxlibVersion3 save file.";
        return false;
    }

    std::uint32_t version = 0;
    std::uint64_t loadedGeneration = 0;
    std::uint64_t cellCount = 0;
    std::uint32_t storedChecksum = 0;
    if (!readU32(input, version) ||
        !readU64(input, loadedGeneration) ||
        !readU64(input, cellCount) ||
        !readU32(input, storedChecksum)) {
        errorMessage = "The save file header is truncated.";
        return false;
    }

    if (version != FormatVersion) {
        errorMessage = "Unsupported save file version.";
        return false;
    }
    if (cellCount > MaxCellCount) {
        errorMessage = "The save file contains too many live cells.";
        return false;
    }

    std::uint32_t calculatedChecksum = FnvOffset;
    checksumU64(calculatedChecksum, loadedGeneration);
    checksumU64(calculatedChecksum, cellCount);

    InfiniteLifeBoard loadedBoard;
    try {
        for (std::uint64_t i = 0; i < cellCount; ++i) {
            std::uint64_t xBits = 0;
            std::uint64_t yBits = 0;
            if (!readU64(input, xBits) || !readU64(input, yBits)) {
                errorMessage = "The save file data is truncated.";
                return false;
            }

            checksumU64(calculatedChecksum, xBits);
            checksumU64(calculatedChecksum, yBits);

            const auto x = std::bit_cast<InfiniteLifeBoard::Coord>(xBits);
            const auto y = std::bit_cast<InfiniteLifeBoard::Coord>(yBits);
            loadedBoard.setAlive(x, y, true);

            if (!loadedBoard.isAlive(x, y)) {
                writeLoadDiagnostic(path, "cell insertion", i, x, y, cellCount, loadedBoard);
                errorMessage = loadIntegrityError("cell insertion", path);
                return false;
            }
        }
    } catch (const std::bad_alloc&) {
        errorMessage = "Not enough memory to load the save file.";
        return false;
    }

    if (input.peek() != EOF) {
        errorMessage = "Unexpected extra data was found in the save file.";
        return false;
    }
    if (calculatedChecksum != storedChecksum) {
        errorMessage = "The save file checksum does not match.";
        return false;
    }
    if (loadedBoard.aliveCellCount() != cellCount) {
        errorMessage = "The save file contains duplicate cell coordinates.";
        return false;
    }

    std::uint64_t enumeratedCount = 0;
    bool boardConsistent = true;
    InfiniteLifeBoard::Coord badX = 0;
    InfiniteLifeBoard::Coord badY = 0;
    loadedBoard.forEachAliveCell([&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
        if (boardConsistent && !loadedBoard.isAlive(x, y)) {
            boardConsistent = false;
            badX = x;
            badY = y;
        }
        ++enumeratedCount;
    });

    if (!boardConsistent) {
        writeLoadDiagnostic(path, "post-load validation", enumeratedCount, badX, badY, cellCount, loadedBoard);
        errorMessage = loadIntegrityError("post-load validation", path);
        return false;
    }
    if (enumeratedCount != cellCount) {
        writeLoadDiagnostic(path, "cell-count validation", enumeratedCount, 0, 0, cellCount, loadedBoard);
        errorMessage = loadIntegrityError("cell-count validation", path);
        return false;
    }

    board = std::move(loadedBoard);
    generation = loadedGeneration;
    return true;
}
} // namespace InfiniteLifeFile
