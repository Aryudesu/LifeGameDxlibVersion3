#include "ImportPatternLibrary.h"

#include "RleParser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {
namespace fs = std::filesystem;
const fs::path ImportPatternDirectory = fs::path("patterns") / "import";

bool readPattern(const fs::path& path, ImportPattern& pattern, std::string& parseError) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
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
        errorMessage = "Failed to create patterns/import directory.";
        return false;
    }

    for (const auto& entry : fs::directory_iterator(ImportPatternDirectory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".rle") continue;
        ImportPattern pattern;
        std::string parseError;
        if (readPattern(entry.path(), pattern, parseError)) patterns_.push_back(std::move(pattern));
    }
    if (ec) {
        errorMessage = "Failed to read patterns/import directory.";
        return false;
    }
    std::sort(patterns_.begin(), patterns_.end(), [](const ImportPattern& a, const ImportPattern& b) {
        return a.name < b.name;
    });
    return true;
}

bool ImportPatternLibrary::importFile(const std::string& sourcePath, std::size_t& importedIndex,
                                      std::string& errorMessage) {
    errorMessage.clear();
    const fs::path source(sourcePath);
    if (source.extension() != ".rle") {
        errorMessage = "The selected file is not an RLE file.";
        return false;
    }

    // Validate before copying so malformed files never enter the import library.
    ImportPattern validated;
    std::string parseError;
    if (!readPattern(source, validated, parseError)) {
        errorMessage = parseError.empty() ? "Failed to read the selected RLE file." : parseError;
        return false;
    }

    std::error_code ec;
    fs::create_directories(ImportPatternDirectory, ec);
    if (ec) {
        errorMessage = "Failed to create patterns/import directory.";
        return false;
    }

    const fs::path destination = ImportPatternDirectory / source.filename();
    if (fs::exists(destination, ec)) {
        errorMessage = "An imported pattern with the same file name already exists.";
        return false;
    }
    if (ec) {
        errorMessage = "Failed to check the import destination.";
        return false;
    }

    fs::copy_file(source, destination, fs::copy_options::none, ec);
    if (ec) {
        errorMessage = "Failed to copy the RLE file to patterns/import.";
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
    errorMessage = "The imported pattern could not be found after reloading.";
    return false;
}
