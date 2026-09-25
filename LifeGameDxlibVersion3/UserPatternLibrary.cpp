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
        errorMessage = "\x83\x86\x81\x5B\x83\x55\x81\x5B\x83\x70\x83\x5E\x81\x5B\x83\x93\x95\xDB\x91\xB6\x97\x70\x83\x74\x83\x48\x83\x8B\x83\x5F\x81\x5B\x20\x70\x61\x74\x74\x65\x72\x6E\x73\x2F\x75\x73\x65\x72\x20\x82\xF0\x8D\xEC\x90\xAC\x82\xC5\x82\xAB\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81\x42";
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
        errorMessage = "\x83\x86\x81\x5B\x83\x55\x81\x5B\x83\x70\x83\x5E\x81\x5B\x83\x93\x95\xDB\x91\xB6\x97\x70\x83\x74\x83\x48\x83\x8B\x83\x5F\x81\x5B\x20\x70\x61\x74\x74\x65\x72\x6E\x73\x2F\x75\x73\x65\x72\x20\x82\xF0\x93\xC7\x82\xDD\x8D\x9E\x82\xDF\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81\x42";
        return false;
    }
    return true;
}

bool UserPatternLibrary::save(const std::string& name, int width, int height,
                              const std::vector<PatternCell>& cells, std::string& errorMessage) {
    errorMessage.clear();
    if (!validFileName(name)) {
        errorMessage = "\x83\x70\x83\x5E\x81\x5B\x83\x93\x96\xBC\x82\xAA\x8B\xF3\x81\x41\x82\xDC\x82\xBD\x82\xCD\x83\x74\x83\x40\x83\x43\x83\x8B\x96\xBC\x82\xC9\x8E\x67\x97\x70\x82\xC5\x82\xAB\x82\xC8\x82\xA2\x95\xB6\x8E\x9A\x82\xAA\x8A\xDC\x82\xDC\x82\xEA\x82\xC4\x82\xA2\x82\xDC\x82\xB7\x81\x42";
        return false;
    }
    if (width <= 0 || height <= 0 || cells.empty()) {
        errorMessage = "\x91\x49\x91\xF0\x94\xCD\x88\xCD\x82\xC9\x90\xB6\x91\xB6\x83\x5A\x83\x8B\x82\xAA\x82\xA0\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x81\x42";
        return false;
    }

    std::error_code ec;
    fs::create_directories(UserPatternDirectory, ec);
    if (ec) {
        errorMessage = "\x83\x86\x81\x5B\x83\x55\x81\x5B\x83\x70\x83\x5E\x81\x5B\x83\x93\x95\xDB\x91\xB6\x97\x70\x83\x74\x83\x48\x83\x8B\x83\x5F\x81\x5B\x20\x70\x61\x74\x74\x65\x72\x6E\x73\x2F\x75\x73\x65\x72\x20\x82\xF0\x8D\xEC\x90\xAC\x82\xC5\x82\xAB\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81\x42";
        return false;
    }
    const fs::path path = UserPatternDirectory / (name + ".rle");
    if (fs::exists(path, ec)) {
        errorMessage = "\x93\xAF\x82\xB6\x96\xBC\x91\x4F\x82\xCC\x83\x86\x81\x5B\x83\x55\x81\x5B\x83\x70\x83\x5E\x81\x5B\x83\x93\x82\xAA\x8A\xF9\x82\xC9\x91\xB6\x8D\xDD\x82\xB5\x82\xDC\x82\xB7\x81\x42";
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        errorMessage = "\x83\x86\x81\x5B\x83\x55\x81\x5B\x83\x70\x83\x5E\x81\x5B\x83\x93\x82\xCC\x52\x4C\x45\x83\x74\x83\x40\x83\x43\x83\x8B\x82\xF0\x8D\xEC\x90\xAC\x82\xC5\x82\xAB\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81\x42";
        return false;
    }
    output << encodeRle(width, height, cells);
    if (!output) {
        errorMessage = "\x83\x86\x81\x5B\x83\x55\x81\x5B\x83\x70\x83\x5E\x81\x5B\x83\x93\x82\xCC\x52\x4C\x45\x83\x74\x83\x40\x83\x43\x83\x8B\x82\xD6\x82\xCC\x8F\x91\x82\xAB\x8D\x9E\x82\xDD\x82\xC9\x8E\xB8\x94\x73\x82\xB5\x82\xDC\x82\xB5\x82\xBD\x81\x42";
        return false;
    }
    output.close();
    if (!output) {
        errorMessage = "\x83\x86\x81\x5B\x83\x55\x81\x5B\x83\x70\x83\x5E\x81\x5B\x83\x93\x82\xCC\x52\x4C\x45\x83\x74\x83\x40\x83\x43\x83\x8B\x82\xF0\x90\xB3\x8F\xED\x82\xC9\x95\xC2\x82\xB6\x82\xE7\x82\xEA\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81\x42";
        return false;
    }
    return load(errorMessage);
}
