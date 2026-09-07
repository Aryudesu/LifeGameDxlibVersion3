#include "InfiniteLifeFile.h"

#include "InfiniteLifeBoard.h"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <new>
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

std::string coordinateDebugLogPath;

void setCoordinateDebugLogPath(const std::string& savePath) {
    coordinateDebugLogPath = savePath + ".coord.log";
}

void debugLog(const std::string& message) {
    if (!coordinateDebugLogPath.empty()) {
        std::ofstream log(coordinateDebugLogPath, std::ios::app);
        if (log) log << message << '\n';
    }

    const std::string debuggerMessage = "[LifeGameCoord] " + message + "\n";
    OutputDebugStringA(debuggerMessage.c_str());
}

void debugLogCoord(const char* stage, InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
    const std::string message =
        std::string(stage) +
        " x=" + std::to_string(x) +
        " y=" + std::to_string(y) +
        " x%64=" + std::to_string(x % InfiniteLifeBoard::ChunkSize) +
        " y%64=" + std::to_string(y % InfiniteLifeBoard::ChunkSize);
    debugLog(message);
}

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
} // namespace

namespace InfiniteLifeFile {
bool save(
    const InfiniteLifeBoard& board,
    std::uint64_t generation,
    const std::string& path,
    std::string& errorMessage) {
    errorMessage.clear();

    setCoordinateDebugLogPath(path);
    debugLog("=== SAVE BEGIN path=" + path + " ===");
    debugLog("LOG PATH=" + coordinateDebugLogPath);

    const std::uint64_t cellCount = board.aliveCellCount();
    if (cellCount > MaxCellCount) {
        errorMessage = "The board is too large to save in this format.";
        debugLog("SAVE ERROR: too many cells");
        return false;
    }

    const std::uint32_t storedChecksum = calculateChecksum(board, generation);
    const std::string temporaryPath = path + ".tmp";
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        errorMessage = "Could not open the temporary save file.";
        debugLog("SAVE ERROR: could not open temporary file");
        return false;
    }

    output.write(Magic.data(), static_cast<std::streamsize>(Magic.size()));
    writeU32(output, FormatVersion);
    writeU64(output, generation);
    writeU64(output, cellCount);
    writeU32(output, storedChecksum);

    board.forEachAliveCell([&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
        debugLogCoord("SAVE-WRITE", x, y);
        writeU64(output, std::bit_cast<std::uint64_t>(x));
        writeU64(output, std::bit_cast<std::uint64_t>(y));
    });
    output.flush();

    if (!output) {
        output.close();
        DeleteFileA(temporaryPath.c_str());
        errorMessage = "Failed while writing the save file.";
        debugLog("SAVE ERROR: write failed");
        return false;
    }

    output.close();
    if (!replaceFile(temporaryPath, path)) {
        DeleteFileA(temporaryPath.c_str());
        errorMessage = "Could not replace the destination save file.";
        debugLog("SAVE ERROR: replace failed");
        return false;
    }

    debugLog("=== SAVE END cells=" + std::to_string(cellCount) + " ===");
    return true;
}

bool load(
    InfiniteLifeBoard& board,
    std::uint64_t& generation,
    const std::string& path,
    std::string& errorMessage) {
    errorMessage.clear();

    setCoordinateDebugLogPath(path);
    debugLog("=== LOAD BEGIN path=" + path + " ===");
    debugLog("LOG PATH=" + coordinateDebugLogPath);

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        errorMessage = "Could not open the save file.";
        debugLog("LOAD ERROR: could not open file");
        return false;
    }

    std::array<char, Magic.size()> magic{};
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!input || magic != Magic) {
        errorMessage = "This is not a LifeGameDxlibVersion3 save file.";
        debugLog("LOAD ERROR: invalid magic");
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
        debugLog("LOAD ERROR: truncated header");
        return false;
    }

    if (version != FormatVersion) {
        errorMessage = "Unsupported save file version.";
        debugLog("LOAD ERROR: unsupported version");
        return false;
    }
    if (cellCount > MaxCellCount) {
        errorMessage = "The save file contains too many live cells.";
        debugLog("LOAD ERROR: too many cells");
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
                debugLog("LOAD ERROR: truncated cell data");
                return false;
            }

            checksumU64(calculatedChecksum, xBits);
            checksumU64(calculatedChecksum, yBits);

            const auto x = std::bit_cast<InfiniteLifeBoard::Coord>(xBits);
            const auto y = std::bit_cast<InfiniteLifeBoard::Coord>(yBits);
            debugLogCoord("LOAD-READ", x, y);

            loadedBoard.setAlive(x, y, true);
            debugLog(
                "LOAD-SET x=" + std::to_string(x) +
                " y=" + std::to_string(y) +
                " isAliveRequested=" + std::to_string(loadedBoard.isAlive(x, y) ? 1 : 0));
        }
    } catch (const std::bad_alloc&) {
        errorMessage = "Not enough memory to load the save file.";
        debugLog("LOAD ERROR: bad_alloc");
        return false;
    }

    debugLog("--- LOAD ENUMERATE BEGIN ---");
    loadedBoard.forEachAliveCell([&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
        debugLogCoord("LOAD-ENUM", x, y);
    });
    debugLog("--- LOAD ENUMERATE END ---");

    if (input.peek() != EOF) {
        errorMessage = "Unexpected extra data was found in the save file.";
        debugLog("LOAD ERROR: unexpected extra data");
        return false;
    }
    if (calculatedChecksum != storedChecksum) {
        errorMessage = "The save file checksum does not match.";
        debugLog("LOAD ERROR: checksum mismatch");
        return false;
    }
    if (loadedBoard.aliveCellCount() != cellCount) {
        errorMessage = "The save file contains duplicate cell coordinates.";
        debugLog("LOAD ERROR: duplicate coordinates");
        return false;
    }

    board = std::move(loadedBoard);
    generation = loadedGeneration;
    debugLog("=== LOAD END cells=" + std::to_string(cellCount) + " ===");
    return true;
}
} // namespace InfiniteLifeFile
