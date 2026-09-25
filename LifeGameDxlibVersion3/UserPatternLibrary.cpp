#include "UserPatternLibrary.h"

#include "RleParser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
namespace fs = std::filesystem;

const fs::path UserPatternDirectory = fs::path("patterns") / "user";

bool validFileName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    static constexpr const char* Invalid = "\\/:*?\"<>|";
    if (name.find_first_of(Invalid) != std::string::npos) return false;
    if (name.back() == ' ' || name.back() == '.') return false;
    return true;
}

void appendRun(std::string& out, int count, char token) {
    if (count <= 0) return;
    if (count > 1) out += std::to_string(count);
    out += token;
}

std::string encodeRle(int width, int height, const std::vector<PatternCell>& cells) {
    std::vector<std::vector<int>> rows(static_cast<std::size_t>(height));
    for (const PatternCell cell : cells) {
        if (cell.x >= 0 && cell.x < width && cell.y >= 0 && cell.y < height)
            rows[static_cast<std::size_t>(cell.y)].push_back(cell.x);
    }
    for (auto& row : rows) {
        std::sort(row.begin(), row.end());
        row.erase(std::unique(row.begin(), row.end()), row.end());
    }

    std::string body;
    for (int y = 0; y < height; ++y) {
        int x = 0;
        const auto& row = rows[static_cast<std::size_t>(y)];
        for (const int liveX : row) {
            appendRun(body, liveX - x, 'b');
            appendRun(body, 1, 'o');
            x = liveX + 1;
        }
        if (y + 1 < height) appendRun(body, 1, '$');
    }
    body += '!';

    std::ostringstream output;
    output << "x = " << width << ", y = " << height << ", rule = B3/S23\n";
    constexpr std::size_t LineWidth = 70;
    for (std::size_t pos = 0; pos < body.size(); pos += LineWidth)
        output << body.substr(pos, LineWidth) << '\n';
    return output.str();
}
}

bool UserPatternLibrary::load(std::string& errorMessage) {
    errorMessage.clear();
    patterns_.clear();
    std::error_code ec;
    fs::create_directories(UserPatternDirectory, ec);
    if (ec) {
        errorMessage = "Could not create patterns/user directory.";
        return false;
    }

    for (const auto& entry : fs::directory_iterator(UserPatternDirectory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".rle") continue;
        std::ifstream input(entry.path(), std::ios::binary);
        if (!input) continue;
        const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        ParsedRlePattern parsed;
        std::string parseError;
        if (!RleParser::parse(source, parsed, parseError)) continue;
        patterns_.push_back({entry.path().stem().string(), parsed.width, parsed.height, std::move(parsed.cells)});
    }
    std::sort(patterns_.begin(), patterns_.end(), [](const UserPattern& a, const UserPattern& b) { return a.name < b.name; });
    if (ec) {
        errorMessage = "Could not enumerate patterns/user directory.";
        return false;
    }
    return true;
}

bool UserPatternLibrary::save(const std::string& name, int width, int height,
                              const std::vector<PatternCell>& cells, std::string& errorMessage) {
    errorMessage.clear();
    if (!validFileName(name)) {
        errorMessage = "Pattern name is empty or contains characters that cannot be used in a file name.";
        return false;
    }
    if (width <= 0 || height <= 0 || cells.empty()) {
        errorMessage = "The selection does not contain any live cells.";
        return false;
    }

    std::error_code ec;
    fs::create_directories(UserPatternDirectory, ec);
    if (ec) {
        errorMessage = "Could not create patterns/user directory.";
        return false;
    }
    const fs::path path = UserPatternDirectory / (name + ".rle");
    if (fs::exists(path, ec)) {
        errorMessage = "A user pattern with that name already exists.";
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        errorMessage = "Could not create the user pattern RLE file.";
        return false;
    }
    output << encodeRle(width, height, cells);
    if (!output) {
        errorMessage = "Failed while writing the user pattern RLE file.";
        return false;
    }
    output.close();
    if (!output) {
        errorMessage = "Failed while closing the user pattern RLE file.";
        return false;
    }
    return load(errorMessage);
}
