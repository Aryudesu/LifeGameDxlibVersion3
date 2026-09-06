#include "DxLib.h"
#include "InfiniteCamera.h"
#include "InfiniteLifeBoard.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>

namespace {
constexpr int ScreenWidth = 1024;
constexpr int ScreenHeight = 1024;
constexpr int MinCellSize = 1;
constexpr int MaxCellSize = 32;
constexpr double TargetFps = 60.0;
constexpr double MaxSimulationDeltaSeconds = 0.25;
constexpr std::array SimulationSpeeds = {1, 5, 10, 30, 60, 120, 300, 600};
constexpr std::size_t DefaultSimulationSpeedIndex = 4;

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
    bool previousPageUp = false;
    bool previousPageDown = false;
    std::uint64_t generation = 0;
    std::size_t simulationSpeedIndex = DefaultSimulationSpeedIndex;
    double simulationAccumulator = 0.0;
    double fps = 0.0;

    int previousMouseX = 0;
    int previousMouseY = 0;
    GetMousePoint(&previousMouseX, &previousMouseY);

    using Clock = std::chrono::steady_clock;
    const auto targetFrameDuration = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / TargetFps));
    auto previousFrameTime = Clock::now();
    auto nextFrameTime = previousFrameTime;
    auto fpsSampleStart = previousFrameTime;
    int fpsFrameCount = 0;

    while (ProcessMessage() == 0) {
        const auto frameStart = Clock::now();
        const double elapsedSeconds = std::chrono::duration<double>(frameStart - previousFrameTime).count();
        previousFrameTime = frameStart;

        const bool enter = CheckHitKey(KEY_INPUT_RETURN) != 0;
        const bool space = CheckHitKey(KEY_INPUT_SPACE) != 0;
        const bool del = CheckHitKey(KEY_INPUT_DELETE) != 0;
        const bool pageUp = CheckHitKey(KEY_INPUT_PGUP) != 0;
        const bool pageDown = CheckHitKey(KEY_INPUT_PGDN) != 0;

        if (enter && !previousEnter) {
            paused = !paused;
            simulationAccumulator = 0.0;
        }
        if (del && !previousDelete) {
            board.clear();
            generation = 0;
            paused = true;
            simulationAccumulator = 0.0;
        }
        if (pageUp && !previousPageUp && simulationSpeedIndex + 1 < SimulationSpeeds.size()) {
            ++simulationSpeedIndex;
            simulationAccumulator = 0.0;
        }
        if (pageDown && !previousPageDown && simulationSpeedIndex > 0) {
            --simulationSpeedIndex;
            simulationAccumulator = 0.0;
        }

        if (CheckHitKey(KEY_INPUT_LEFT)) camera.move(-4, 0);
        if (CheckHitKey(KEY_INPUT_RIGHT)) camera.move(4, 0);
        if (CheckHitKey(KEY_INPUT_UP)) camera.move(0, -4);
        if (CheckHitKey(KEY_INPUT_DOWN)) camera.move(0, 4);

        int mouseX = 0;
        int mouseY = 0;
        GetMousePoint(&mouseX, &mouseY);
        const int mouseDeltaX = mouseX - previousMouseX;
        const int mouseDeltaY = mouseY - previousMouseY;
        const int mouseInput = GetMouseInput();
        const bool left = (mouseInput & MOUSE_INPUT_LEFT) != 0;
        const bool right = (mouseInput & MOUSE_INPUT_RIGHT) != 0;
        const bool middle = (mouseInput & MOUSE_INPUT_MIDDLE) != 0;

        if (middle) camera.panByPixels(mouseDeltaX, mouseDeltaY);
        else camera.endPan();

        const int wheel = GetMouseWheelRotVol();
        if (wheel > 0) {
            for (int i = 0; i < wheel; ++i) {
                if (!camera.zoomInAt(mouseX, mouseY, MaxCellSize)) break;
            }
        } else if (wheel < 0) {
            for (int i = 0; i < -wheel; ++i) {
                if (!camera.zoomOutAt(mouseX, mouseY, MinCellSize)) break;
            }
        }

        if (paused && (left || right) && !middle) {
            const auto [x, y] = camera.screenToBoard(mouseX, mouseY);
            board.setAlive(x, y, left && !right);
        }

        if (paused) {
            simulationAccumulator = 0.0;
            if (space && !previousSpace) {
                board.step();
                ++generation;
            }
        } else {
            const double simulationSeconds = std::min(elapsedSeconds, MaxSimulationDeltaSeconds);
            simulationAccumulator += simulationSeconds * SimulationSpeeds[simulationSpeedIndex];
            const int generationsToAdvance = static_cast<int>(simulationAccumulator);
            simulationAccumulator -= generationsToAdvance;
            for (int i = 0; i < generationsToAdvance; ++i) {
                board.step();
                ++generation;
            }
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
        DrawFormatString(8, 74, GetColor(255, 255, 255), "FPS: %.1f  Speed: %d gen/s", fps, SimulationSpeeds[simulationSpeedIndex]);
        DrawString(8, 96, paused ? "PAUSED" : "RUNNING", paused ? GetColor(255, 210, 90) : GetColor(120, 230, 140));
        DrawString(8, 118, "Enter: pause  Space: step  PageUp/PageDown: speed", GetColor(180, 180, 180));
        DrawString(8, 140, "Arrows / Middle drag: move  Wheel: zoom", GetColor(180, 180, 180));
        DrawString(8, 162, "Paused: Left drag = alive  Right drag = dead  Delete: clear", GetColor(180, 180, 180));
        ScreenFlip();

        ++fpsFrameCount;
        const auto fpsSampleEnd = Clock::now();
        const double fpsSampleSeconds = std::chrono::duration<double>(fpsSampleEnd - fpsSampleStart).count();
        if (fpsSampleSeconds >= 0.5) {
            fps = fpsFrameCount / fpsSampleSeconds;
            fpsFrameCount = 0;
            fpsSampleStart = fpsSampleEnd;
        }

        previousEnter = enter;
        previousSpace = space;
        previousDelete = del;
        previousPageUp = pageUp;
        previousPageDown = pageDown;
        previousMouseX = mouseX;
        previousMouseY = mouseY;

        nextFrameTime += targetFrameDuration;
        const auto afterFrame = Clock::now();
        if (nextFrameTime > afterFrame) std::this_thread::sleep_until(nextFrameTime);
        else nextFrameTime = afterFrame;
    }

    DxLib_End();
    return 0;
}
