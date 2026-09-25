#include "FileDrop.h"

#include "DxLib.h"

#include <shellapi.h>
#include <windows.h>
#include <utility>

#pragma comment(lib, "Shell32.lib")

namespace {
std::vector<std::string> PendingPaths;

LRESULT CALLBACK hookWinProc(HWND, UINT message, WPARAM wParam, LPARAM) {
    if (message != WM_DROPFILES) return 0;

    const HDROP drop = reinterpret_cast<HDROP>(wParam);
    const UINT count = DragQueryFileA(drop, 0xFFFFFFFF, nullptr, 0);

    for (UINT i = 0; i < count; ++i) {
        const UINT length = DragQueryFileA(drop, i, nullptr, 0);
        std::string path(static_cast<std::size_t>(length) + 1, '\0');
        DragQueryFileA(drop, i, path.data(), length + 1);
        path.resize(length);
        PendingPaths.push_back(std::move(path));
    }

    DragFinish(drop);
    return 0;
}
}

namespace FileDrop {
void installHook() {
    SetHookWinProc(hookWinProc);
}

void enable() {
    DragAcceptFiles(GetMainWindowHandle(), TRUE);
}

std::vector<std::string> takeDroppedPaths() {
    std::vector<std::string> paths;
    paths.swap(PendingPaths);
    return paths;
}
}
