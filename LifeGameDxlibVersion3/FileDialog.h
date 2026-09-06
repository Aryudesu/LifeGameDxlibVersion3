#pragma once

#include <string>

namespace FileDialog {
bool chooseSavePath(std::string& path);
bool chooseLoadPath(std::string& path);
void showError(const std::string& message);
} // namespace FileDialog
