#pragma once

#include <string>
#include <vector>

namespace FileDrop {
void installHook();
void enable();
std::vector<std::string> takeDroppedPaths();
}
