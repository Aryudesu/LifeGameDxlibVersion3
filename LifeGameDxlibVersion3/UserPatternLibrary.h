#pragma once

#include "PatternLibrary.h"

#include <cstddef>
#include <string>
#include <vector>

struct UserPattern {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<PatternCell> cells;
};

class UserPatternLibrary {
public:
    bool load(std::string& errorMessage);
    bool save(const std::string& name, int width, int height,
              const std::vector<PatternCell>& cells, std::string& errorMessage);

    std::size_t size() const noexcept { return patterns_.size(); }
    const UserPattern& at(std::size_t index) const { return patterns_.at(index); }
    const std::vector<UserPattern>& patterns() const noexcept { return patterns_; }

private:
    std::vector<UserPattern> patterns_;
};
