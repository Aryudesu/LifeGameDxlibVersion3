#pragma once

#include <cstdint>
#include <string>

class InfiniteLifeBoard;

namespace InfiniteLifeFile {
bool save(
    const InfiniteLifeBoard& board,
    std::uint64_t generation,
    const std::string& path,
    std::string& errorMessage);

bool load(
    InfiniteLifeBoard& board,
    std::uint64_t& generation,
    const std::string& path,
    std::string& errorMessage);
} // namespace InfiniteLifeFile
