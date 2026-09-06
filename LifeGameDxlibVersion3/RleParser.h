#pragma once

#include "PatternLibrary.h"

#include <string>
#include <string_view>
#include <vector>

struct ParsedRlePattern {
    int width = 0;
    int height = 0;
    std::string rule;
    std::vector<PatternCell> cells;
};

namespace RleParser {
bool parse(std::string_view source, ParsedRlePattern& out, std::string& errorMessage);
} // namespace RleParser
