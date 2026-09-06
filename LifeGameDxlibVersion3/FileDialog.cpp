#include "FileDialog.h"

#include "DxLib.h"

#include <commdlg.h>
#include <cstring>
#include <windows.h>

#pragma comment(lib, "Comdlg32.lib")

namespace {
constexpr const char* SaveFileDefaultName = "LifeGame.ary3";
constexpr const char* SaveFileExtension = "ary3";
constexpr const char SaveFileFilter[] =
    "LifeGame Version3 (*.ary3)\0*.ary3\0All Files (*.*)\0*.*\0\0";
constexpr const char* WindowTitle = "LifeGameDxlibVersion3";
}

namespace FileDialog {
bool chooseSavePath(std::string& path) {
    char fileName[MAX_PATH] = {};
    strcpy_s(fileName, SaveFileDefaultName);

    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetMainWindowHandle();
    dialog.lpstrFilter = SaveFileFilter;
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = SaveFileExtension;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileNameA(&dialog) == FALSE) return false;

    path = fileName;
    return true;
}

bool chooseLoadPath(std::string& path) {
    char fileName[MAX_PATH] = {};

    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetMainWindowHandle();
    dialog.lpstrFilter = SaveFileFilter;
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = SaveFileExtension;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&dialog) == FALSE) return false;

    path = fileName;
    return true;
}

void showError(const std::string& message) {
    MessageBoxA(GetMainWindowHandle(), message.c_str(), WindowTitle, MB_OK | MB_ICONERROR);
}
} // namespace FileDialog
