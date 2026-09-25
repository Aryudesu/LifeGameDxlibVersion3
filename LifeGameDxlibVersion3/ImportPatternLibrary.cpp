#include "ImportPatternLibrary.h"

#include "RleParser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace {
namespace fs = std::filesystem;
const fs::path ImportPatternDirectory = fs::path("patterns") / "import";

std::string ansiFromWide(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_ACP, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};

    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_ACP, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

bool isRleExtension(const fs::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".rle";
}

std::string localizedParseError(const std::string& error) {
    if (error == "Failed to open the RLE file.")
        return ansiFromWide(L"RLE\u30D5\u30A1\u30A4\u30EB\u3092\u958B\u3051\u307E\u305B\u3093\u3067\u3057\u305F\u3002");
    if (error == "RLE header must contain positive x and y values.")
        return ansiFromWide(L"RLE\u30D8\u30C3\u30C0\u30FC\u306E x \u3068 y \u306B\u306F\u6B63\u306E\u5024\u304C\u5FC5\u8981\u3067\u3059\u3002");
    if (error == "RLE header was not found.")
        return ansiFromWide(L"RLE\u30D8\u30C3\u30C0\u30FC\u304C\u898B\u3064\u304B\u308A\u307E\u305B\u3093\u3002");
    if (error == "RLE run length is too large.")
        return ansiFromWide(L"RLE\u306E\u9023\u7D9A\u6570\u304C\u5927\u304D\u3059\u304E\u307E\u3059\u3002");
    if (error == "RLE row exceeds the declared width.")
        return ansiFromWide(L"RLE\u306E\u884C\u304C\u30D8\u30C3\u30C0\u30FC\u3067\u6307\u5B9A\u3055\u308C\u305F\u5E45\u3092\u8D85\u3048\u3066\u3044\u307E\u3059\u3002");
    if (error == "RLE live cell exceeds the declared height.")
        return ansiFromWide(L"RLE\u306E\u751F\u5B58\u30BB\u30EB\u304C\u30D8\u30C3\u30C0\u30FC\u3067\u6307\u5B9A\u3055\u308C\u305F\u9AD8\u3055\u3092\u8D85\u3048\u3066\u3044\u307E\u3059\u3002");
    if (error == "RLE row count exceeds the declared height.")
        return ansiFromWide(L"RLE\u306E\u884C\u6570\u304C\u30D8\u30C3\u30C0\u30FC\u3067\u6307\u5B9A\u3055\u308C\u305F\u9AD8\u3055\u3092\u8D85\u3048\u3066\u3044\u307E\u3059\u3002");
    if (error == "Unexpected run length before RLE terminator.")
        return ansiFromWide(L"RLE\u7D42\u7AEF\u8A18\u53F7\u306E\u524D\u306B\u4E0D\u6B63\u306A\u9023\u7D9A\u6570\u304C\u3042\u308A\u307E\u3059\u3002");
    if (error == "RLE body contains an unsupported token.")
        return ansiFromWide(L"RLE\u672C\u4F53\u306B\u672A\u5BFE\u5FDC\u306E\u8A18\u53F7\u304C\u542B\u307E\u308C\u3066\u3044\u307E\u3059\u3002");
    if (error == "RLE terminator '!' was not found.")
        return ansiFromWide(L"RLE\u7D42\u7AEF\u8A18\u53F7\u300C!\u300D\u304C\u898B\u3064\u304B\u308A\u307E\u305B\u3093\u3002");
    return ansiFromWide(L"RLE\u30D5\u30A1\u30A4\u30EB\u306E\u89E3\u6790\u306B\u5931\u6557\u3057\u307E\u3057\u305F\u3002");
}

bool readPattern(const fs::path& path, ImportPattern& pattern, std::string& parseError) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        parseError = "Failed to open the RLE file.";
        return false;
    }

    const std::string source((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    ParsedRlePattern parsed;
    if (!RleParser::parse(source, parsed, parseError)) return false;

    pattern = {path.stem().string(), parsed.width, parsed.height, std::move(parsed.cells)};
    return true;
}
}

bool ImportPatternLibrary::load(std::string& errorMessage) {
    errorMessage.clear();
    patterns_.clear();

    std::error_code ec;
    fs::create_directories(ImportPatternDirectory, ec);
    if (ec) {
        errorMessage = ansiFromWide(
            L"patterns/import \u30D5\u30A9\u30EB\u30C0\u30FC\u3092\u4F5C\u6210\u3067\u304D\u307E\u305B\u3093\u3067\u3057\u305F\u3002");
        return false;
    }

    for (const auto& entry : fs::directory_iterator(ImportPatternDirectory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || !isRleExtension(entry.path())) continue;

        ImportPattern pattern;
        std::string parseError;
        if (!readPattern(entry.path(), pattern, parseError)) {
            errorMessage = ansiFromWide(L"\u30A4\u30F3\u30DD\u30FC\u30C8\u6E08\u307FRLE [") +
                           entry.path().filename().string() +
                           ansiFromWide(L"] \u306E\u8AAD\u307F\u8FBC\u307F\u306B\u5931\u6557\u3057\u307E\u3057\u305F: ") +
                           localizedParseError(parseError);
            return false;
        }
        patterns_.push_back(std::move(pattern));
    }

    if (ec) {
        errorMessage = ansiFromWide(
            L"patterns/import \u30D5\u30A9\u30EB\u30C0\u30FC\u3092\u8AAD\u307F\u8FBC\u3081\u307E\u305B\u3093\u3067\u3057\u305F\u3002");
        return false;
    }

    std::sort(patterns_.begin(), patterns_.end(),
        [](const ImportPattern& a, const ImportPattern& b) {
            return a.name < b.name;
        });
    return true;
}

bool ImportPatternLibrary::importFile(const std::string& sourcePath, std::size_t& importedIndex,
                                      std::string& errorMessage) {
    errorMessage.clear();
    const fs::path source(sourcePath);

    if (!isRleExtension(source)) {
        errorMessage = ansiFromWide(
            L"\u9078\u629E\u3055\u308C\u305F\u30D5\u30A1\u30A4\u30EB\u306FRLE\u30D5\u30A1\u30A4\u30EB\u3067\u306F\u3042\u308A\u307E\u305B\u3093\u3002");
        return false;
    }

    ImportPattern validated;
    std::string parseError;
    if (!readPattern(source, validated, parseError)) {
        errorMessage = localizedParseError(parseError);
        return false;
    }

    std::error_code ec;
    fs::create_directories(ImportPatternDirectory, ec);
    if (ec) {
        errorMessage = ansiFromWide(
            L"patterns/import \u30D5\u30A9\u30EB\u30C0\u30FC\u3092\u4F5C\u6210\u3067\u304D\u307E\u305B\u3093\u3067\u3057\u305F\u3002");
        return false;
    }

    const fs::path destination = ImportPatternDirectory / source.filename();
    if (fs::exists(destination, ec)) {
        errorMessage = ansiFromWide(
            L"\u540C\u3058\u30D5\u30A1\u30A4\u30EB\u540D\u306E\u30A4\u30F3\u30DD\u30FC\u30C8\u6E08\u307F\u30D1\u30BF\u30FC\u30F3\u304C\u65E2\u306B\u5B58\u5728\u3057\u307E\u3059\u3002");
        return false;
    }
    if (ec) {
        errorMessage = ansiFromWide(
            L"\u30A4\u30F3\u30DD\u30FC\u30C8\u5148\u3092\u78BA\u8A8D\u3067\u304D\u307E\u305B\u3093\u3067\u3057\u305F\u3002");
        return false;
    }

    fs::copy_file(source, destination, fs::copy_options::none, ec);
    if (ec) {
        errorMessage = ansiFromWide(
            L"RLE\u30D5\u30A1\u30A4\u30EB\u3092 patterns/import \u306B\u30B3\u30D4\u30FC\u3067\u304D\u307E\u305B\u3093\u3067\u3057\u305F\u3002");
        return false;
    }

    if (!load(errorMessage)) return false;

    const std::string importedName = destination.stem().string();
    for (std::size_t i = 0; i < patterns_.size(); ++i) {
        if (patterns_[i].name == importedName) {
            importedIndex = i;
            return true;
        }
    }

    errorMessage = ansiFromWide(
        L"\u518D\u8AAD\u307F\u8FBC\u307F\u5F8C\u306B\u30A4\u30F3\u30DD\u30FC\u30C8\u3057\u305F\u30D1\u30BF\u30FC\u30F3\u3092\u898B\u3064\u3051\u3089\u308C\u307E\u305B\u3093\u3067\u3057\u305F\u3002");
    return false;
}
