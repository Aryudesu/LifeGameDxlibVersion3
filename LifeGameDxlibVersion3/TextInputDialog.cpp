#include "TextInputDialog.h"

#include <windows.h>

namespace {
constexpr int EditId = 1001;
constexpr int OkId = IDOK;
constexpr int CancelId = IDCANCEL;

struct State {
    HWND window = nullptr;
    HWND edit = nullptr;
    std::string* value = nullptr;
    bool accepted = false;
    bool done = false;
};

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    State* state = reinterpret_cast<State*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
        state = static_cast<State*>(create->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->window = hwnd;
    }
    if (!state) return DefWindowProcA(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_COMMAND:
        if (LOWORD(wParam) == OkId) {
            char buffer[256]{};
            GetWindowTextA(state->edit, buffer, static_cast<int>(sizeof(buffer)));
            if (buffer[0] != '\0') {
                *state->value = buffer;
                state->accepted = true;
                state->done = true;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        if (LOWORD(wParam) == CancelId) {
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        state->done = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        state->done = true;
        return 0;
    }
    return DefWindowProcA(hwnd, message, wParam, lParam);
}
}

bool TextInputDialog::show(void* ownerWindow, const char* title, const char* prompt, std::string& value) {
    const HINSTANCE instance = GetModuleHandleA(nullptr);
    constexpr const char* ClassName = "LifeGameTextInputDialog";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = windowProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = ClassName;
        if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        registered = true;
    }

    State state;
    state.value = &value;
    const HWND owner = static_cast<HWND>(ownerWindow);
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    const int width = 420, height = 170;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
    HWND window = CreateWindowExA(WS_EX_DLGMODALFRAME, ClassName, title,
        WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
        x, y, width, height, owner, nullptr, instance, &state);
    if (!window) return false;

    CreateWindowExA(0, "STATIC", prompt, WS_CHILD | WS_VISIBLE,
        16, 16, 380, 20, window, nullptr, instance, nullptr);
    state.edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", value.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        16, 42, 380, 26, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(EditId)), instance, nullptr);
    CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        214, 86, 86, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(OkId)), instance, nullptr);
    CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        310, 86, 86, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CancelId)), instance, nullptr);

    SendMessageA(state.edit, EM_SETLIMITTEXT, 120, 0);
    SetFocus(state.edit);
    SendMessageA(state.edit, EM_SETSEL, 0, -1);
    EnableWindow(owner, FALSE);

    MSG msg{};
    while (!state.done && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            state.done = true;
            DestroyWindow(window);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return state.accepted;
}
