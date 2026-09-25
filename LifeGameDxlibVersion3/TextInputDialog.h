#pragma once

#include <string>

namespace TextInputDialog {
bool show(void* ownerWindow, const char* title, const char* prompt, std::string& value);
}
