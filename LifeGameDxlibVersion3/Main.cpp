#include "DxLib.h"
#include "InfiniteCamera.h"
#include "InfiniteLifeBoard.h"

#include <cstdint>

namespace {
constexpr int ScreenWidth = 1024;
constexpr int ScreenHeight = 1024;
constexpr int MinCellSize = 1;
constexpr int MaxCellSize = 32;

void seedGlider(InfiniteLifeBoard& board) {
    board.setAlive(1, 0, true);
    board.setAlive(2, 1, true);
    board.setAlive(0, 2, true);
    board.setAlive(1, 2, true);
    board.setAlive(2, 2, true);
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetMainWindowText("LifeGameDxlibVersion3 - Infinite Plane Prototype");
    SetWindowSizeChangeEnableFlag(FALSE);
    ChangeWindowMode(TRUE);
    SetGraphMode(ScreenWidth, ScreenHeight, 32);
    SetOutApplicationLogValidFlag(FALSE);
    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);

    InfiniteLifeBoard board;
    InfiniteCamera camera(ScreenWidth, ScreenHeight, 8);
    seedGlider(board);

    bool paused = false;
    bool previousEnter = false;
    bool previousSpace = false;
    bool previousDelete = false;
    bool previousLeft = false;
    std::uint64_t generation = 0;

    while (ProcessMessage() == 0) {
        const bool enter = CheckHitKey(KEY_INPUT_RETURN) != 0;
        const bool space = CheckHitKey(KEY_INPUT_SPACE) != 0;
        const bool del = CheckHitKey(KEY_INPUT_DELETE) != 0;
        if (enter && !previousEnter) paused = !paused;
        if (del && !previousDelete) { board.clear(); generation = 0; paused = true; }

        if (CheckHitKey(KEY_INPUT_LEFT)) camera.move(-4, 0);
        if (CheckHitKey(KEY_INPUT_RIGHT)) camera.move(4, 0);
        if (CheckHitKey(KEY_INPUT_UP)) camera.move(0, -4);
        if (CheckHitKey(KEY_INPUT_DOWN)) camera.move(0, 4);

        int mouseX = 0, mouseY = 0;
        GetMousePoint(&mouseX, &mouseY);
        const int wheel = GetMouseWheelRotVol();
        if (wheel > 0) camera.zoomInAt(mouseX, mouseY, MaxCellSize);
        else if (wheel < 0) camera.zoomOutAt(mouseX, mouseY, MinCellSize);

        const bool left = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;
        if (paused && left && !previousLeft) {
            const auto [x, y] = camera.screenToBoard(mouseX, mouseY);
            board.setAlive(x, y, !board.isAlive(x, y));
        }

        if ((!paused || (space && !previousSpace))) {
            board.step();
            ++generation;
        }

        ClearDrawScreen();
        const unsigned int aliveColor = GetColor(0, 255, 0);
        board.forEachAliveCell([&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
            const auto [sx, sy] = camera.boardToScreen(x, y);
            const int size = camera.cellSize();
            if (sx + size <= 0 || sy + size <= 0 || sx >= ScreenWidth || sy >= ScreenHeight) return;
            DrawBox(sx, sy, sx + size - 1, sy + size - 1, aliveColor, TRUE);
        });
        DrawFormatString(8, 8, GetColor(255, 255, 255), "Generation: %llu", static_cast<unsigned long long>(generation));
        DrawFormatString(8, 30, GetColor(255, 255, 255), "Alive: %llu  Chunks: %llu", static_cast<unsigned long long>(board.aliveCellCount()), static_cast<unsigned long long>(board.chunkCount()));
        DrawFormatString(8, 52, GetColor(255, 255, 255), "Camera: (%lld, %lld)  Zoom: %d", static_cast<long long>(camera.x()), static_cast<long long>(camera.y()), camera.cellSize());
        DrawString(8, 74, paused ? "PAUSED" : "RUNNING", paused ? GetColor(255, 210, 90) : GetColor(120, 230, 140));
        DrawString(8, 96, "Enter: pause  Space: step  Arrows: move  Wheel: zoom", GetColor(180, 180, 180));
        DrawString(8, 118, "Paused + Left click: toggle cell  Delete: clear", GetColor(180, 180, 180));
        ScreenFlip();

        previousEnter = enter;
        previousSpace = space;
        previousDelete = del;
        previousLeft = left;
    }

    DxLib_End();
    return 0;
}
