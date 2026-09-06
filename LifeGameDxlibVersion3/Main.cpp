#include "DxLib.h"
#include "InfiniteCamera.h"
#include "InfiniteLifeBoard.h"
#include "PatternLibrary.h"
#include "PatternListScroll.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <utility>

namespace {
constexpr int BoardViewWidth = 1024;
constexpr int ScreenHeight = 1024;
constexpr int PanelWidth = 320;
constexpr int WindowWidth = BoardViewWidth + PanelWidth;
constexpr int MinCellSize = 1;
constexpr int MaxCellSize = 32;
constexpr double TargetFps = 60.0;
constexpr double MaxSimulationDeltaSeconds = 0.25;
constexpr std::array SimulationSpeeds = {1, 5, 10, 30, 60, 120, 300, 600};
constexpr std::size_t DefaultSimulationSpeedIndex = 4;

constexpr int PanelX = BoardViewWidth;
constexpr int PanelPadding = 16;
constexpr int PanelContentX = PanelX + PanelPadding;
constexpr int PatternListY = 198;
constexpr int PatternRowHeight = 28;
constexpr int PatternListBottom = PatternListY + PatternListScroll::VisibleRows * PatternRowHeight;
constexpr int RotationY = 900;
constexpr int RotationButtonWidth = 48;
constexpr int RotationValueWidth = 104;
constexpr int RotationGap = 8;
constexpr int RotationLeftX = PanelContentX;
constexpr int RotationValueX = RotationLeftX + RotationButtonWidth + RotationGap;
constexpr int RotationRightX = RotationValueX + RotationValueWidth + RotationGap;
constexpr int RotationHeight = 30;
constexpr std::array ToolCategories = {
    PatternCategory::StillLife,
    PatternCategory::Oscillator,
    PatternCategory::Methuselah,
    PatternCategory::Spaceship,
    PatternCategory::Gun
};

bool inRect(int x, int y, int left, int top, int right, int bottom) noexcept {
    return x >= left && x < right && y >= top && y < bottom;
}

void seedGlider(InfiniteLifeBoard& board) {
    board.setAlive(1, 0, true);
    board.setAlive(2, 1, true);
    board.setAlive(0, 2, true);
    board.setAlive(1, 2, true);
    board.setAlive(2, 2, true);
}

std::pair<int, int> rotatedCell(PatternCell cell, int rotation) noexcept {
    switch (rotation & 3) {
    case 1: return {-cell.y, cell.x};
    case 2: return {-cell.x, -cell.y};
    case 3: return {cell.y, -cell.x};
    default: return {cell.x, cell.y};
    }
}

std::pair<int, int> rotationOffset(const LifePattern& pattern, int rotation) noexcept {
    int minX = 0;
    int minY = 0;
    for (const PatternCell cell : pattern.cells) {
        const auto [x, y] = rotatedCell(cell, rotation);
        minX = std::min(minX, x);
        minY = std::min(minY, y);
    }
    return {-minX, -minY};
}

void placePattern(InfiniteLifeBoard& board, const LifePattern& pattern,
                  InfiniteLifeBoard::Coord originX, InfiniteLifeBoard::Coord originY,
                  int rotation) {
    const auto [offsetX, offsetY] = rotationOffset(pattern, rotation);
    for (const PatternCell cell : pattern.cells) {
        const auto [rx, ry] = rotatedCell(cell, rotation);
        board.setAlive(originX + rx + offsetX, originY + ry + offsetY, true);
    }
}

std::size_t firstPatternInCategory(PatternCategory category) noexcept {
    for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {
        if (PatternLibrary::at(i).category == category) return i;
    }
    return 0;
}

void drawGrid(const InfiniteCamera& camera) {
    const int cellSize = camera.cellSize();
    if (cellSize <= 1) return;

    const unsigned int gridColor = GetColor(45, 45, 45);
    for (int x = 0; x <= BoardViewWidth; x += cellSize) {
        DrawLine(x, 0, x, ScreenHeight, gridColor);
    }
    for (int y = 0; y <= ScreenHeight; y += cellSize) {
        DrawLine(0, y, BoardViewWidth, y, gridColor);
    }
}
} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetMainWindowText("LifeGameDxlibVersion3 - Infinite Plane");
    SetWindowSizeChangeEnableFlag(FALSE);
    SetAlwaysRunFlag(TRUE);
    ChangeWindowMode(TRUE);
    SetGraphMode(WindowWidth, ScreenHeight, 32);
    SetOutApplicationLogValidFlag(FALSE);
    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);

    InfiniteLifeBoard board;
    InfiniteCamera camera(BoardViewWidth, ScreenHeight, 8);
    PatternListScroll patternListScroll;
    seedGlider(board);

    bool paused = false;
    bool showGrid = false;
    bool previousEnter = false;
    bool previousSpace = false;
    bool previousDelete = false;
    bool previousPageUp = false;
    bool previousPageDown = false;
    bool previousP = false;
    bool previousQ = false;
    bool previousE = false;
    bool previousG = false;
    bool previousLeft = false;
    std::uint64_t generation = 0;
    std::size_t simulationSpeedIndex = DefaultSimulationSpeedIndex;
    std::size_t selectedPatternIndex = 0;
    PatternCategory toolCategory = PatternCategory::StillLife;
    int patternRotation = 0;
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
        const bool p = CheckHitKey(KEY_INPUT_P) != 0;
        const bool q = CheckHitKey(KEY_INPUT_Q) != 0;
        const bool e = CheckHitKey(KEY_INPUT_E) != 0;
        const bool g = CheckHitKey(KEY_INPUT_G) != 0;
        const bool shift = CheckHitKey(KEY_INPUT_LSHIFT) != 0 || CheckHitKey(KEY_INPUT_RSHIFT) != 0;

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
        if (g && !previousG) showGrid = !showGrid;
        if (pageUp && !previousPageUp && simulationSpeedIndex + 1 < SimulationSpeeds.size()) {
            ++simulationSpeedIndex;
            simulationAccumulator = 0.0;
        }
        if (pageDown && !previousPageDown && simulationSpeedIndex > 0) {
            --simulationSpeedIndex;
            simulationAccumulator = 0.0;
        }
        if (p && !previousP) {
            if (shift) selectedPatternIndex = (selectedPatternIndex + PatternLibrary::size() - 1) % PatternLibrary::size();
            else selectedPatternIndex = (selectedPatternIndex + 1) % PatternLibrary::size();
            patternRotation = 0;
            if (selectedPatternIndex != 0) {
                toolCategory = PatternLibrary::at(selectedPatternIndex).category;
                patternListScroll.ensurePatternVisible(selectedPatternIndex);
            }
        }
        if (q && !previousQ && selectedPatternIndex != 0) patternRotation = (patternRotation + 3) & 3;
        if (e && !previousE && selectedPatternIndex != 0) patternRotation = (patternRotation + 1) & 3;

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
        const bool leftPressed = left && !previousLeft;
        const int wheel = GetMouseWheelRotVol();
        const bool mouseOnBoard = mouseX >= 0 && mouseX < BoardViewWidth && mouseY >= 0 && mouseY < ScreenHeight;
        const bool mouseOnPanel = mouseX >= PanelX && mouseX < WindowWidth && mouseY >= 0 && mouseY < ScreenHeight;

        if (mouseOnPanel && inRect(mouseX, mouseY, PanelContentX, PatternListY, WindowWidth - PanelPadding, PatternListBottom)) {
            patternListScroll.scroll(toolCategory, wheel);
        }

        if (mouseOnPanel && leftPressed) {
            if (inRect(mouseX, mouseY, PanelContentX, 42, WindowWidth - PanelPadding, 72)) {
                selectedPatternIndex = 0;
                patternRotation = 0;
            } else {
                bool handled = false;
                for (std::size_t i = 0; i < ToolCategories.size() && !handled; ++i) {
                    const int column = static_cast<int>(i % 2);
                    const int row = static_cast<int>(i / 2);
                    const int leftX = PanelContentX + column * 144;
                    const int top = 86 + row * 36;
                    if (inRect(mouseX, mouseY, leftX, top, leftX + 136, top + 28)) {
                        toolCategory = ToolCategories[i];
                        selectedPatternIndex = firstPatternInCategory(toolCategory);
                        patternRotation = 0;
                        patternListScroll.ensurePatternVisible(selectedPatternIndex);
                        handled = true;
                    }
                }

                if (!handled) {
                    const int scrollOffset = patternListScroll.offset(toolCategory);
                    int categoryRow = 0;
                    for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {
                        const LifePattern& pattern = PatternLibrary::at(i);
                        if (pattern.category != toolCategory) continue;
                        if (categoryRow >= scrollOffset && categoryRow < scrollOffset + PatternListScroll::VisibleRows) {
                            const int visibleRow = categoryRow - scrollOffset;
                            const int top = PatternListY + visibleRow * PatternRowHeight;
                            if (inRect(mouseX, mouseY, PanelContentX, top, WindowWidth - PanelPadding, top + 24)) {
                                selectedPatternIndex = i;
                                patternRotation = 0;
                                handled = true;
                                break;
                            }
                        }
                        ++categoryRow;
                    }
                }

                if (!handled && selectedPatternIndex != 0) {
                    if (inRect(mouseX, mouseY, RotationLeftX, RotationY, RotationLeftX + RotationButtonWidth, RotationY + RotationHeight)) {
                        patternRotation = (patternRotation + 3) & 3;
                    } else if (inRect(mouseX, mouseY, RotationValueX, RotationY, RotationValueX + RotationValueWidth, RotationY + RotationHeight)) {
                        patternRotation = 0;
                    } else if (inRect(mouseX, mouseY, RotationRightX, RotationY, RotationRightX + RotationButtonWidth, RotationY + RotationHeight)) {
                        patternRotation = (patternRotation + 1) & 3;
                    }
                }
            }
        }

        if (mouseOnBoard) {
            if (middle) camera.panByPixels(mouseDeltaX, mouseDeltaY);
            else camera.endPan();

            if (wheel > 0) {
                for (int i = 0; i < wheel; ++i) {
                    if (!camera.zoomInAt(mouseX, mouseY, MaxCellSize)) break;
                }
            } else if (wheel < 0) {
                for (int i = 0; i < -wheel; ++i) {
                    if (!camera.zoomOutAt(mouseX, mouseY, MinCellSize)) break;
                }
            }

            if (paused && !middle) {
                const LifePattern& pattern = PatternLibrary::at(selectedPatternIndex);
                const auto [x, y] = camera.screenToBoard(mouseX, mouseY);
                if (pattern.category == PatternCategory::Cell) {
                    if (left || right) board.setAlive(x, y, left && !right);
                } else if (leftPressed) {
                    placePattern(board, pattern, x, y, patternRotation);
                }
            }
        } else {
            camera.endPan();
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
            if (sx + size <= 0 || sy + size <= 0 || sx >= BoardViewWidth || sy >= ScreenHeight) return;
            DrawBox(sx, sy, sx + size - 1, sy + size - 1, aliveColor, TRUE);
        });
        if (showGrid) drawGrid(camera);

        const unsigned int background = GetColor(28, 30, 34);
        const unsigned int section = GetColor(45, 48, 54);
        const unsigned int selected = GetColor(70, 105, 75);
        const unsigned int text = GetColor(235, 235, 235);
        const unsigned int muted = GetColor(170, 175, 180);
        DrawBox(PanelX, 0, WindowWidth, ScreenHeight, background, TRUE);
        DrawString(PanelContentX, 14, "LIFE GAME TOOLS", text);

        DrawBox(PanelContentX, 42, WindowWidth - PanelPadding, 72,
                selectedPatternIndex == 0 ? selected : section, TRUE);
        DrawString(PanelContentX + 10, 49, "Cell", text);

        for (std::size_t i = 0; i < ToolCategories.size(); ++i) {
            const int column = static_cast<int>(i % 2);
            const int row = static_cast<int>(i / 2);
            const int leftX = PanelContentX + column * 144;
            const int top = 86 + row * 36;
            DrawBox(leftX, top, leftX + 136, top + 28,
                    selectedPatternIndex != 0 && toolCategory == ToolCategories[i] ? selected : section, TRUE);
            DrawString(leftX + 8, top + 6, PatternLibrary::categoryName(ToolCategories[i]), text);
        }

        const int scrollOffset = patternListScroll.offset(toolCategory);
        int categoryRow = 0;
        for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {
            const LifePattern& pattern = PatternLibrary::at(i);
            if (pattern.category != toolCategory) continue;
            if (categoryRow >= scrollOffset && categoryRow < scrollOffset + PatternListScroll::VisibleRows) {
                const int visibleRow = categoryRow - scrollOffset;
                const int top = PatternListY + visibleRow * PatternRowHeight;
                DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24,
                        selectedPatternIndex == i ? selected : section, TRUE);
                DrawString(PanelContentX + 8, top + 5, pattern.name, text);
            }
            ++categoryRow;
        }

        const int patternCount = patternListScroll.count(toolCategory);
        if (patternCount > PatternListScroll::VisibleRows) {
            const int firstVisible = scrollOffset + 1;
            const int lastVisible = std::min(scrollOffset + PatternListScroll::VisibleRows, patternCount);
            DrawFormatString(WindowWidth - 122, PatternListBottom + 4, muted,
                             "%d-%d / %d", firstVisible, lastVisible, patternCount);
        }

        const int infoY = 650;
        DrawString(PanelContentX, infoY, "STATUS", muted);
        DrawFormatString(PanelContentX, infoY + 26, text, "Generation  %llu", static_cast<unsigned long long>(generation));
        DrawFormatString(PanelContentX, infoY + 50, text, "FPS         %.1f", fps);
        DrawFormatString(PanelContentX, infoY + 74, text, "Speed       %d gen/s", SimulationSpeeds[simulationSpeedIndex]);
        DrawFormatString(PanelContentX, infoY + 98, text, "Alive       %llu", static_cast<unsigned long long>(board.aliveCellCount()));
        DrawFormatString(PanelContentX, infoY + 122, text, "Chunks      %llu", static_cast<unsigned long long>(board.chunkCount()));
        DrawFormatString(PanelContentX, infoY + 146, text, "Camera      (%lld, %lld)", static_cast<long long>(camera.x()), static_cast<long long>(camera.y()));
        DrawFormatString(PanelContentX, infoY + 170, text, "Zoom        %d", camera.cellSize());
        DrawFormatString(PanelContentX, infoY + 194, text, "Grid        %s", showGrid ? "ON" : "OFF");

        DrawString(PanelContentX, 876, "ROTATION", muted);
        const bool rotationEnabled = selectedPatternIndex != 0;
        const unsigned int rotationButton = rotationEnabled ? section : background;
        DrawBox(RotationLeftX, RotationY, RotationLeftX + RotationButtonWidth, RotationY + RotationHeight, rotationButton, TRUE);
        DrawBox(RotationValueX, RotationY, RotationValueX + RotationValueWidth, RotationY + RotationHeight, section, TRUE);
        DrawBox(RotationRightX, RotationY, RotationRightX + RotationButtonWidth, RotationY + RotationHeight, rotationButton, TRUE);
        DrawString(RotationLeftX + 16, RotationY + 6, "<", rotationEnabled ? text : muted);
        DrawFormatString(RotationValueX + 30, RotationY + 6, text, "R%d", patternRotation * 90);
        DrawString(RotationRightX + 17, RotationY + 6, ">", rotationEnabled ? text : muted);

        DrawString(PanelContentX, 944, paused ? "PAUSED" : "RUNNING",
                   paused ? GetColor(255, 210, 90) : GetColor(120, 230, 140));
        DrawString(PanelContentX, 966, "P/Shift+P select  Q/E rotate  G grid", muted);
        DrawString(PanelContentX, 988, "Wheel list / Click pattern", muted);
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
        previousP = p;
        previousQ = q;
        previousE = e;
        previousG = g;
        previousLeft = left;
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
