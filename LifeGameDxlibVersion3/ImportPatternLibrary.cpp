#include "ImportPatternLibrary.h"

#include "RleParser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace {
namespace fs = std::filesystem;
const fs::path ImportPatternDirectory = fs::path("patterns") / "import";

bool isRleExtension(const fs::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".rle";
}

std::string localizedParseError(const std::string& error) {
    if (error == "Failed to open the RLE file.")
        return "RLE\x83t\x83@\x83C\x83\x8B\x82\xF0\x8AJ\x82\xAF\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81B";
    if (error == "RLE header must contain positive x and y values.")
        return "RLE\x83w\x83b\x83_\x81[\x82\xCC x \x82\xC6 y \x82\xC9\x82\xCD\x90\xB3\x82\xCC\x92l\x82\xAA\x95K\x97v\x82\xC5\x82\xB7\x81B";
    if (error == "RLE header was not found.")
        return "RLE\x83w\x83b\x83_\x81[\x82\xAA\x8C\xA9\x82\xC2\x82\xA9\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x81B";
    if (error == "RLE run length is too large.")
        return "RLE\x82\xCC\x98A\x91\xB1\x90\x94\x82\xAA\x91\xE5\x82\xAB\x82\xB7\x82\xAC\x82\xDC\x82\xB7\x81B";
    if (error == "RLE row exceeds the declared width.")
        return "RLE\x82\xCC\x8Ds\x82\xAA\x83w\x83b\x83_\x81[\x82\xC5\x8Ew\x92\xE8\x82\xB3\x82\xEA\x82\xBD\x95\x9D\x82\xF0\x92\xB4\x82\xA6\x82\xC4\x82\xA2\x82\xDC\x82\xB7\x81B";
    if (error == "RLE live cell exceeds the declared height.")
        return "RLE\x82\xCC\x90\xB6\x91\xB6\x83Z\x83\x8B\x82\xAA\x83w\x83b\x83_\x81[\x82\xC5\x8Ew\x92\xE8\x82\xB3\x82\xEA\x82\xBD\x8D\x82\x82\xB3\x82\xF0\x92\xB4\x82\xA6\x82\xC4\x82\xA2\x82\xDC\x82\xB7\x81B";
    if (error == "RLE row count exceeds the declared height.")
        return "RLE\x82\xCC\x8Ds\x90\x94\x82\xAA\x83w\x83b\x83_\x81[\x82\xC5\x8Ew\x92\xE8\x82\xB3\x82\xEA\x82\xBD\x8D\x82\x82\xB3\x82\xF0\x92\xB4\x82\xA6\x82\xC4\x82\xA2\x82\xDC\x82\xB7\x81B";
    if (error == "Unexpected run length before RLE terminator.")
        return "RLE\x8FI\x92[\x8BL\x8D\x86\x82\xCC\x91O\x82\xC9\x95s\x90\xB3\x82\xC8\x98A\x91\xB1\x90\x94\x82\xAA\x82\xA0\x82\xE8\x82\xDC\x82\xB7\x81B";
    if (error == "RLE body contains an unsupported token.")
        return "RLE\x96{\x91\xCC\x82\xC9\x96\xA2\x91\xCE\x89\x9E\x82\xCC\x8BL\x8D\x86\x82\xAA\x8A\xDC\x82\xDC\x82\xEA\x82\xC4\x82\xA2\x82\xDC\x82\xB7\x81B";
    if (error == "RLE terminator '!' was not found.")
        return "RLE\x8FI\x92[\x8BL\x8D\x86\x81u!\x81v\x82\xAA\x8C\xA9\x82\xC2\x82\xA9\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x81B";
    return "RLE\x83t\x83@\x83C\x83\x8B\x82\xCC\x89\xF0\x90\xCD\x82\xC9\x8E\xB8\x94s\x82\xB5\x82\xDC\x82\xB5\x82\xBD\x81B";
}

bool readPattern(const fs::path& path, ImportPattern& pattern, std::string& parseError) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        parseError = "Failed to open the RLE file.";
        return false;
    }
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
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
        errorMessage = "patterns/import \x83t\x83H\x83\x8B\x83_\x81[\x82\xF0\x8D\xEC\x90\xAC\x82\xC5\x82\xAB\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81B";
        return false;
    }

    for (const auto& entry : fs::directory_iterator(ImportPatternDirectory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || !isRleExtension(entry.path())) continue;

        ImportPattern pattern;
        std::string parseError;
        if (!readPattern(entry.path(), pattern, parseError)) {
            errorMessage = "\x83C\x83\x93\x83|\x81[\x83g\x8D\xCF\x82\xDDRLE [" +
                           entry.path().filename().string() + "] \x82\xCC\x93\xC7\x82\xDD\x8D\x9E\x82\xDD\x82\xC9\x8E\xB8\x94s\x82\xB5\x82\xDC\x82\xB5\x82\xBD: " +
                           localizedParseError(parseError);
            return false;
        }
        patterns_.push_back(std::move(pattern));
    }

    if (ec) {
        errorMessage = "patterns/import \x83t\x83H\x83\x8B\x83_\x81[\x82\xF0\x93\xC7\x82\xDD\x8D\x9E\x82\xDF\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81B";
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
        errorMessage = "\x91I\x91\xF0\x82\xB3\x82\xEA\x82\xBD\x83t\x83@\x83C\x83\x8B\x82\xCDRLE\x83t\x83@\x83C\x83\x8B\x82\xC5\x82\xCD\x82\xA0\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x81B";
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
        errorMessage = "patterns/import \x83t\x83H\x83\x8B\x83_\x81[\x82\xF0\x8D\xEC\x90\xAC\x82\xC5\x82\xAB\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81B";
        return false;
    }

    const fs::path destination = ImportPatternDirectory / source.filename();
    if (fs::exists(destination, ec)) {
        errorMessage = "\x93\xAF\x82\xB6\x83t\x83@\x83C\x83\x8B\x96\xBC\x82\xCC\x83C\x83\x93\x83|\x81[\x83g\x8D\xCF\x82\xDD\x83p\x83^\x81[\x83\x93\x82\xAA\x8A\xF9\x82\xC9\x91\xB6\x8D\xDD\x82\xB5\x82\xDC\x82\xB7\x81B";
        return false;
    }
    if (ec) {
        errorMessage = "\x83C\x83\x93\x83|\x81[\x83g\x90\xE6\x82\xF0\x8Am\x94F\x82\xC5\x82\xAB\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81B";
        return false;
    }

    fs::copy_file(source, destination, fs::copy_options::none, ec);
    if (ec) {
        errorMessage = "RLE\x83t\x83@\x83C\x83\x8B\x82\xF0 patterns/import \x82\xC9\x83R\x83s\x81[\x82\xC5\x82\xAB\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81B";
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

    errorMessage = "\x8D\xC4\x93\xC7\x82\xDD\x8D\x9E\x82\xDD\x8C\xE3\x82\xC9\x83C\x83\x93\x83|\x81[\x83g\x82\xB5\x82\xBD\x83p\x83^\x81[\x83\x93\x82\xF0\x8C\xA9\x82\xC2\x82\xAF\x82\xE7\x82\xEA\x82\xDC\x82\xB9\x82\xF1\x82\xC5\x82\xB5\x82\xBD\x81B";
    return false;
}
