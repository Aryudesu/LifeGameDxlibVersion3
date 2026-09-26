#pragma once

#include "PatternLibrary.h"

#include <cstddef>
#include <string>
#include <vector>

struct ImportPattern {
    std::string name;
    std::string fileName;
    int width = 0;
    int height = 0;
    std::vector<PatternCell> cells;
};

class ImportPatternLibrary {
public:
    bool load(std::string& errorMessage);
    bool importFile(const std::string& sourcePath, std::size_t& importedIndex,
                    std::string& errorMessage);
    bool rename(std::size_t index, const std::string& newName,
                std::size_t& renamedIndex, std::string& errorMessage);
    bool remove(std::size_t index, std::string& errorMessage);

    std::size_t size() const noexcept { return patterns_.size(); }
    const ImportPattern& at(std::size_t index) const { return patterns_.at(index); }

private:
    std::vector<ImportPattern> patterns_;
};
