#include "RleParser.h"

#include <charconv>
#include <cctype>
#include <limits>

namespace {
std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}

bool parsePositiveInt(std::string_view text, int& value) {
    text = trim(text);
    if (text.empty()) return false;
    int parsed = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed <= 0) return false;
    value = parsed;
    return true;
}

bool parseHeader(std::string_view line, int& width, int& height, std::string& rule) {
    bool hasWidth = false;
    bool hasHeight = false;
    while (!line.empty()) {
        const std::size_t comma = line.find(',');
        std::string_view part = trim(line.substr(0, comma));
        if (comma == std::string_view::npos) line = {};
        else line.remove_prefix(comma + 1);

        const std::size_t equals = part.find('=');
        if (equals == std::string_view::npos) continue;
        const std::string_view key = trim(part.substr(0, equals));
        const std::string_view value = trim(part.substr(equals + 1));
        if (key == "x") hasWidth = parsePositiveInt(value, width);
        else if (key == "y") hasHeight = parsePositiveInt(value, height);
        else if (key == "rule") rule.assign(value.begin(), value.end());
    }
    return hasWidth && hasHeight;
}
} // namespace

namespace RleParser {
bool parse(std::string_view source, ParsedRlePattern& out, std::string& errorMessage) {
    ParsedRlePattern parsed;
    std::string body;
    bool headerFound = false;

    while (!source.empty()) {
        const std::size_t newline = source.find('\n');
        std::string_view line = source.substr(0, newline);
        if (newline == std::string_view::npos) source = {};
        else source.remove_prefix(newline + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;

        if (!headerFound) {
            if (!parseHeader(line, parsed.width, parsed.height, parsed.rule)) {
                errorMessage = "RLE header must contain positive x and y values.";
                return false;
            }
            headerFound = true;
            continue;
        }
        body.append(line.begin(), line.end());
    }

    if (!headerFound) {
        errorMessage = "RLE header was not found.";
        return false;
    }

    int x = 0;
    int y = 0;
    int runCount = 0;
    bool terminated = false;

    const auto consumeRun = [&runCount]() {
        const int run = runCount == 0 ? 1 : runCount;
        runCount = 0;
        return run;
    };

    for (const char ch : body) {
        if (std::isspace(static_cast<unsigned char>(ch))) continue;
        if (ch >= '0' && ch <= '9') {
            const int digit = ch - '0';
            if (runCount > (std::numeric_limits<int>::max() - digit) / 10) {
                errorMessage = "RLE run length is too large.";
                return false;
            }
            runCount = runCount * 10 + digit;
            continue;
        }

        if (ch == 'b' || ch == 'o') {
            const int run = consumeRun();
            if (run <= 0 || x > parsed.width - run) {
                errorMessage = "RLE row exceeds the declared width.";
                return false;
            }
            if (ch == 'o') {
                if (y < 0 || y >= parsed.height) {
                    errorMessage = "RLE live cell exceeds the declared height.";
                    return false;
                }
                parsed.cells.reserve(parsed.cells.size() + static_cast<std::size_t>(run));
                for (int i = 0; i < run; ++i) parsed.cells.push_back({x + i, y});
            }
            x += run;
            continue;
        }

        if (ch == '$') {
            const int run = consumeRun();
            y += run;
            x = 0;
            if (y > parsed.height) {
                errorMessage = "RLE row count exceeds the declared height.";
                return false;
            }
            continue;
        }

        if (ch == '!') {
            if (runCount != 0) {
                errorMessage = "Unexpected run length before RLE terminator.";
                return false;
            }
            terminated = true;
            break;
        }

        errorMessage = "RLE body contains an unsupported token.";
        return false;
    }

    if (!terminated) {
        errorMessage = "RLE terminator '!' was not found.";
        return false;
    }

    out = std::move(parsed);
    errorMessage.clear();
    return true;
}
} // namespace RleParser
