#pragma once

#include "PatternLibrary.h"

#include <cstddef>
#include <string>
#include <vector>

struct ImportPattern {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<PatternCell> cells;
};

class ImportPatternLibrary {
public:
    bool load(std::string& errorMessage);
    bool importFile(const std::string& sourcePath, std::size_t& importedIndex,
                    std::string& errorMessage);

    std::size_t size() const noexcept { return patterns_.size(); }
    const ImportPattern& at(std::size_t index) const { return patterns_.at(index); }

private:
    std::vector<ImportPattern> patterns_;
};
