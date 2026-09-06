#include "DxLib.h"
#include "EditHistory.h"
#include "FileDialog.h"
#include "InfiniteCamera.h"
#include "InfiniteLifeBoard.h"
#include "InfiniteLifeFile.h"
#include "PatternLibrary.h"
#include "PatternListScroll.h"
#include "PatternPlacementPreview.h"
#include "PerformanceLogger.h"
#include "ShapeDrawing.h"
#include "ToolbarIcons.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

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
constexpr int PanelContentWidth = PanelWidth - PanelPadding * 2;

constexpr int ActionButtonSize = 28;
constexpr int ActionButtonGap = 4;
constexpr int ActionButtonY = 8;
constexpr int ActionToolbarWidth = ActionButtonSize * 4 + ActionButtonGap * 3;
constexpr int ActionToolbarX = WindowWidth - PanelPadding - ActionToolbarWidth;

constexpr int ToolButtonY = 44;
constexpr int ToolButtonSize = 34;
constexpr int ToolButtonGap = 8;
constexpr int ToolToolbarWidth = ToolButtonSize * 4 + ToolButtonGap * 3;
constexpr int ToolToolbarX = PanelContentX + (PanelContentWidth - ToolToolbarWidth) / 2;

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

constexpr std::array ActionIcons = {
    ToolbarIcons::Icon::Undo,
    ToolbarIcons::Icon::Redo,
    ToolbarIcons::Icon::Save,
    ToolbarIcons::Icon::Load
};

constexpr std::array ShapeTools = {
    ShapeDrawing::Tool::Cell,
    ShapeDrawing::Tool::Line,
    ShapeDrawing::Tool::Rectangle,
    ShapeDrawing::Tool::Circle
};

constexpr std::array ShapeIcons = {
    ToolbarIcons::Icon::Cell,
    ToolbarIcons::Icon::Line,
    ToolbarIcons::Icon::Rectangle,
    ToolbarIcons::Icon::Circle
};

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
    for (int x = 0; x <= BoardViewWidth; x += cellSize) DrawLine(x, 0, x, ScreenHeight, gridColor);
    for (int y = 0; y <= ScreenHeight; y += cellSize) DrawLine(0, y, BoardViewWidth, y, gridColor);
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
    PerformanceLogger performanceLogger;
    EditHistory editHistory;
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
    bool previousSaveShortcut = false;
    bool previousLoadShortcut = false;
    bool previousUndoShortcut = false;
    bool previousRedoShortcut = false;
    bool previousLeft = false;
    bool previousRight = false;

    bool cellStrokeActive = false;
    bool cellStrokeHasLastCell = false;
    InfiniteLifeBoard::Coord lastCellStrokeX = 0;
    InfiniteLifeBoard::Coord lastCellStrokeY = 0;

    ShapeDrawing::Tool shapeTool = ShapeDrawing::Tool::Cell;
    bool shapeDragActive = false;
    bool shapeErase = false;
    InfiniteLifeBoard::Coord shapeStartX = 0;
    InfiniteLifeBoard::Coord shapeStartY = 0;
    InfiniteLifeBoard::Coord shapeEndX = 0;
    InfiniteLifeBoard::Coord shapeEndY = 0;

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
    const auto targetFrameDuration = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / TargetFps));
    auto previousFrameTime = Clock::now();
    auto nextFrameTime = previousFrameTime;
    auto fpsSampleStart = previousFrameTime;
    int fpsFrameCount = 0;

    auto resetTimingAfterDialog = [&]() {
        const auto resetTime = Clock::now();
        previousFrameTime = resetTime;
        nextFrameTime = resetTime;
        fpsSampleStart = resetTime;
        fpsFrameCount = 0;
    };

    auto saveWithDialog = [&]() {
        std::string path;
        if (FileDialog::chooseSavePath(path)) {
            std::string errorMessage;
            if (!InfiniteLifeFile::save(board, generation, path, errorMessage)) FileDialog::showError(errorMessage);
        }
        resetTimingAfterDialog();
    };

    auto loadWithDialog = [&]() {
        std::string path;
        if (FileDialog::chooseLoadPath(path)) {
            std::string errorMessage;
            if (InfiniteLifeFile::load(board, generation, path, errorMessage)) {
                editHistory.clear();
                cellStrokeActive = false;
                cellStrokeHasLastCell = false;
                shapeDragActive = false;
                paused = true;
                simulationAccumulator = 0.0;
            } else {
                FileDialog::showError(errorMessage);
            }
        }
        resetTimingAfterDialog();
    };

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
        const bool escape = CheckHitKey(KEY_INPUT_ESCAPE) != 0;
        const bool ctrl = CheckHitKey(KEY_INPUT_LCONTROL) != 0 || CheckHitKey(KEY_INPUT_RCONTROL) != 0;
        const bool shift = CheckHitKey(KEY_INPUT_LSHIFT) != 0 || CheckHitKey(KEY_INPUT_RSHIFT) != 0;
        const bool saveShortcut = ctrl && CheckHitKey(KEY_INPUT_S) != 0;
        const bool loadShortcut = ctrl && CheckHitKey(KEY_INPUT_L) != 0;
        const bool undoShortcut = ctrl && CheckHitKey(KEY_INPUT_Z) != 0 && !shift;
        const bool redoShortcut = (ctrl && CheckHitKey(KEY_INPUT_Y) != 0) ||
                                  (ctrl && shift && CheckHitKey(KEY_INPUT_Z) != 0);

        if (enter && !previousEnter) {
            paused = !paused;
            simulationAccumulator = 0.0;
            shapeDragActive = false;
        }
        if (del && !previousDelete) {
            board.clear();
            editHistory.clear();
            cellStrokeActive = false;
            cellStrokeHasLastCell = false;
            shapeDragActive = false;
            generation = 0;
            paused = true;
            simulationAccumulator = 0.0;
        }
        if (g && !previousG) showGrid = !showGrid;
        if (escape) {
            if (selectedPatternIndex != 0) { selectedPatternIndex = 0; patternRotation = 0; }
            shapeDragActive = false;
        }

        if (paused && undoShortcut && !previousUndoShortcut && !cellStrokeActive && !shapeDragActive) editHistory.undo(board);
        if (paused && redoShortcut && !previousRedoShortcut && !cellStrokeActive && !shapeDragActive) editHistory.redo(board);
        if (saveShortcut && !previousSaveShortcut) saveWithDialog();
        if (loadShortcut && !previousLoadShortcut) loadWithDialog();

        if (pageUp && !previousPageUp && simulationSpeedIndex + 1 < SimulationSpeeds.size()) { ++simulationSpeedIndex; simulationAccumulator = 0.0; }
        if (pageDown && !previousPageDown && simulationSpeedIndex > 0) { --simulationSpeedIndex; simulationAccumulator = 0.0; }
        if (p && !previousP) {
            shapeTool = ShapeDrawing::Tool::Cell;
            shapeDragActive = false;
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

        int mouseX = 0, mouseY = 0;
        GetMousePoint(&mouseX, &mouseY);
        const int mouseDeltaX = mouseX - previousMouseX;
        const int mouseDeltaY = mouseY - previousMouseY;
        const int mouseInput = GetMouseInput();
        const bool left = (mouseInput & MOUSE_INPUT_LEFT) != 0;
        const bool right = (mouseInput & MOUSE_INPUT_RIGHT) != 0;
        const bool middle = (mouseInput & MOUSE_INPUT_MIDDLE) != 0;
        const bool leftPressed = left && !previousLeft;
        const bool rightPressed = right && !previousRight;
        const bool leftReleased = !left && previousLeft;
        const bool rightReleased = !right && previousRight;
        const int wheel = GetMouseWheelRotVol();
        const bool mouseOnBoard = mouseX >= 0 && mouseX < BoardViewWidth && mouseY >= 0 && mouseY < ScreenHeight;
        const bool mouseOnPanel = mouseX >= PanelX && mouseX < WindowWidth && mouseY >= 0 && mouseY < ScreenHeight;

        if (mouseOnPanel && inRect(mouseX, mouseY, PanelContentX, PatternListY, WindowWidth - PanelPadding, PatternListBottom)) {
            patternListScroll.scroll(toolCategory, wheel);
        }

        if (mouseOnPanel && leftPressed) {
            bool handled = false;
            const bool historyEnabled = paused && !cellStrokeActive && !shapeDragActive;

            for (std::size_t i = 0; i < ActionIcons.size() && !handled; ++i) {
                const int leftX = ActionToolbarX + static_cast<int>(i) * (ActionButtonSize + ActionButtonGap);
                if (!ToolbarIcons::hit(mouseX, mouseY, leftX, ActionButtonY, ActionButtonSize)) continue;

                switch (ActionIcons[i]) {
                case ToolbarIcons::Icon::Undo:
                    if (historyEnabled && editHistory.undoCount() > 0) editHistory.undo(board);
                    break;
                case ToolbarIcons::Icon::Redo:
                    if (historyEnabled && editHistory.redoCount() > 0) editHistory.redo(board);
                    break;
                case ToolbarIcons::Icon::Save:
                    saveWithDialog();
                    break;
                case ToolbarIcons::Icon::Load:
                    loadWithDialog();
                    break;
                default:
                    break;
                }
                handled = true;
            }

            for (std::size_t i = 0; i < ShapeTools.size() && !handled; ++i) {
                const int leftX = ToolToolbarX + static_cast<int>(i) * (ToolButtonSize + ToolButtonGap);
                if (ToolbarIcons::hit(mouseX, mouseY, leftX, ToolButtonY, ToolButtonSize)) {
                    shapeTool = ShapeTools[i];
                    selectedPatternIndex = 0;
                    patternRotation = 0;
                    shapeDragActive = false;
                    cellStrokeHasLastCell = false;
                    handled = true;
                }
            }

            for (std::size_t i = 0; i < ToolCategories.size() && !handled; ++i) {
                const int column = static_cast<int>(i % 2), row = static_cast<int>(i / 2);
                const int leftX = PanelContentX + column * 144, top = 86 + row * 36;
                if (inRect(mouseX, mouseY, leftX, top, leftX + 136, top + 28)) {
                    shapeTool = ShapeDrawing::Tool::Cell;
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
                        const int top = PatternListY + (categoryRow - scrollOffset) * PatternRowHeight;
                        if (inRect(mouseX, mouseY, PanelContentX, top, WindowWidth - PanelPadding, top + 24)) {
                            shapeTool = ShapeDrawing::Tool::Cell;
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
                if (inRect(mouseX, mouseY, RotationLeftX, RotationY, RotationLeftX + RotationButtonWidth, RotationY + RotationHeight)) patternRotation = (patternRotation + 3) & 3;
                else if (inRect(mouseX, mouseY, RotationValueX, RotationY, RotationValueX + RotationValueWidth, RotationY + RotationHeight)) patternRotation = 0;
                else if (inRect(mouseX, mouseY, RotationRightX, RotationY, RotationRightX + RotationButtonWidth, RotationY + RotationHeight)) patternRotation = (patternRotation + 1) & 3;
            }
        }

        if (mouseOnBoard) {
            if (middle) camera.panByPixels(mouseDeltaX, mouseDeltaY); else camera.endPan();
            if (wheel > 0) for (int i = 0; i < wheel; ++i) { if (!camera.zoomInAt(mouseX, mouseY, MaxCellSize)) break; }
            else if (wheel < 0) for (int i = 0; i < -wheel; ++i) { if (!camera.zoomOutAt(mouseX, mouseY, MinCellSize)) break; }

            if (middle || wheel != 0) {
                cellStrokeHasLastCell = false;
                shapeDragActive = false;
            }

            if (paused && !middle) {
                const auto [x, y] = camera.screenToBoard(mouseX, mouseY);
                if (selectedPatternIndex == 0) {
                    if (shapeTool == ShapeDrawing::Tool::Cell) {
                        if ((leftPressed || rightPressed) && (left || right) && !cellStrokeActive) {
                            editHistory.begin();
                            cellStrokeActive = true;
                            cellStrokeHasLastCell = false;
                        }
                        if (cellStrokeActive && (left || right)) {
                            const bool alive = left && !right;
                            if (cellStrokeHasLastCell) {
                                ShapeDrawing::visitLine(lastCellStrokeX, lastCellStrokeY, x, y,
                                    [&](InfiniteLifeBoard::Coord cellX, InfiniteLifeBoard::Coord cellY) {
                                        editHistory.setAlive(board, cellX, cellY, alive);
                                    }, 4096);
                            } else {
                                editHistory.setAlive(board, x, y, alive);
                            }
                            lastCellStrokeX = x;
                            lastCellStrokeY = y;
                            cellStrokeHasLastCell = true;
                        }
                    } else {
                        if ((leftPressed || rightPressed) && !shapeDragActive) {
                            shapeStartX = shapeEndX = x;
                            shapeStartY = shapeEndY = y;
                            shapeErase = rightPressed;
                            shapeDragActive = true;
                        }
                        if (shapeDragActive && (left || right)) {
                            shapeEndX = x;
                            shapeEndY = y;
                        }
                    }
                } else {
                    cellStrokeHasLastCell = false;
                    shapeDragActive = false;
                    const LifePattern& pattern = PatternLibrary::at(selectedPatternIndex);
                    if (rightPressed) {
                        selectedPatternIndex = 0;
                        patternRotation = 0;
                    } else if (leftPressed && PatternPlacementPreview::canPlace(pattern, x, y, patternRotation)) {
                        editHistory.begin();
                        const auto [offsetX, offsetY] = PatternPlacementPreview::rotationOffset(pattern, patternRotation);
                        for (const PatternCell cell : pattern.cells) {
                            const auto [rx, ry] = PatternPlacementPreview::rotatedCell(cell, patternRotation);
                            InfiniteLifeBoard::Coord targetX = 0;
                            InfiniteLifeBoard::Coord targetY = 0;
                            if (PatternPlacementPreview::checkedAdd(x, rx + offsetX, targetX) &&
                                PatternPlacementPreview::checkedAdd(y, ry + offsetY, targetY)) {
                                editHistory.setAlive(board, targetX, targetY, true);
                            }
                        }
                        editHistory.commit();
                    }
                }
            }
        } else {
            camera.endPan();
            cellStrokeHasLastCell = false;
        }

        if (cellStrokeActive && !left && !right) {
            editHistory.commit();
            cellStrokeActive = false;
            cellStrokeHasLastCell = false;
        }

        if (shapeDragActive && ((shapeErase && rightReleased) || (!shapeErase && leftReleased))) {
            editHistory.begin();
            const bool valid = ShapeDrawing::visitShape(shapeTool, shapeStartX, shapeStartY, shapeEndX, shapeEndY,
                [&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
                    editHistory.setAlive(board, x, y, !shapeErase);
                });
            if (valid) editHistory.commit();
            else editHistory.discardActive();
            shapeDragActive = false;
        }

        if (paused) {
            simulationAccumulator = 0.0;
            if (space && !previousSpace) {
                editHistory.clear();
                cellStrokeActive = false;
                cellStrokeHasLastCell = false;
                shapeDragActive = false;
                board.step();
                ++generation;
            }
        } else {
            const double simulationSeconds = std::min(elapsedSeconds, MaxSimulationDeltaSeconds);
            simulationAccumulator += simulationSeconds * SimulationSpeeds[simulationSpeedIndex];
            const int generationsToAdvance = static_cast<int>(simulationAccumulator);
            simulationAccumulator -= generationsToAdvance;
            if (generationsToAdvance > 0) {
                editHistory.clear();
                cellStrokeActive = false;
                cellStrokeHasLastCell = false;
                shapeDragActive = false;
            }
            for (int i = 0; i < generationsToAdvance; ++i) { board.step(); ++generation; }
        }

        ClearDrawScreen();
        const unsigned int aliveColor = GetColor(0, 255, 0);
        board.forEachAliveCell([&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
            const auto [sx, sy] = camera.boardToScreen(x, y);
            const int size = camera.cellSize();
            if (sx + size <= 0 || sy + size <= 0 || sx >= BoardViewWidth || sy >= ScreenHeight) return;
            if (size == 1) DrawPixel(sx, sy, aliveColor);
            else DrawBox(sx, sy, sx + size - 1, sy + size - 1, aliveColor, TRUE);
        });
        if (showGrid) drawGrid(camera);

        if (paused && shapeDragActive && selectedPatternIndex == 0 && shapeTool != ShapeDrawing::Tool::Cell) {
            ShapeDrawing::drawPreview(shapeTool, camera, shapeStartX, shapeStartY, shapeEndX, shapeEndY,
                                      BoardViewWidth, ScreenHeight, shapeErase);
        }

        if (paused && mouseOnBoard && !middle && selectedPatternIndex != 0) {
            const auto [previewX, previewY] = camera.screenToBoard(mouseX, mouseY);
            PatternPlacementPreview::draw(camera, PatternLibrary::at(selectedPatternIndex), previewX, previewY,
                                          patternRotation, BoardViewWidth, ScreenHeight);
        }

        const unsigned int background = GetColor(28, 30, 34);
        const unsigned int section = GetColor(45, 48, 54);
        const unsigned int selected = GetColor(70, 105, 75);
        const unsigned int text = GetColor(235, 235, 235);
        const unsigned int muted = GetColor(170, 175, 180);
        DrawBox(PanelX, 0, WindowWidth, ScreenHeight, background, TRUE);
        DrawString(PanelContentX, 14, "LIFE GAME TOOLS", text);

        const bool historyEnabled = paused && !cellStrokeActive && !shapeDragActive;
        for (std::size_t i = 0; i < ActionIcons.size(); ++i) {
            const int leftX = ActionToolbarX + static_cast<int>(i) * (ActionButtonSize + ActionButtonGap);
            bool enabled = true;
            if (ActionIcons[i] == ToolbarIcons::Icon::Undo) enabled = historyEnabled && editHistory.undoCount() > 0;
            if (ActionIcons[i] == ToolbarIcons::Icon::Redo) enabled = historyEnabled && editHistory.redoCount() > 0;
            ToolbarIcons::drawButton(mouseX, mouseY, leftX, ActionButtonY, ActionButtonSize, ActionIcons[i], false, enabled);
        }

        for (std::size_t i = 0; i < ShapeTools.size(); ++i) {
            const int leftX = ToolToolbarX + static_cast<int>(i) * (ToolButtonSize + ToolButtonGap);
            const bool isSelected = selectedPatternIndex == 0 && shapeTool == ShapeTools[i];
            ToolbarIcons::drawButton(mouseX, mouseY, leftX, ToolButtonY, ToolButtonSize, ShapeIcons[i], isSelected, true);
        }

        for (std::size_t i = 0; i < ToolCategories.size(); ++i) {
            const int column = static_cast<int>(i % 2), row = static_cast<int>(i / 2);
            const int leftX = PanelContentX + column * 144, top = 86 + row * 36;
            DrawBox(leftX, top, leftX + 136, top + 28, selectedPatternIndex != 0 && toolCategory == ToolCategories[i] ? selected : section, TRUE);
            DrawString(leftX + 8, top + 6, PatternLibrary::categoryName(ToolCategories[i]), text);
        }

        const int scrollOffset = patternListScroll.offset(toolCategory);
        int categoryRow = 0;
        for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {
            const LifePattern& pattern = PatternLibrary::at(i);
            if (pattern.category != toolCategory) continue;
            if (categoryRow >= scrollOffset && categoryRow < scrollOffset + PatternListScroll::VisibleRows) {
                const int top = PatternListY + (categoryRow - scrollOffset) * PatternRowHeight;
                DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24, selectedPatternIndex == i ? selected : section, TRUE);
                DrawString(PanelContentX + 8, top + 5, pattern.name, text);
            }
            ++categoryRow;
        }

        const int patternCount = patternListScroll.count(toolCategory);
        if (patternCount > PatternListScroll::VisibleRows) {
            const int firstVisible = scrollOffset + 1;
            const int lastVisible = std::min(scrollOffset + PatternListScroll::VisibleRows, patternCount);
            DrawFormatString(WindowWidth - 122, PatternListBottom + 4, muted, "%d-%d / %d", firstVisible, lastVisible, patternCount);
        }

        const int infoY = 602;
        DrawString(PanelContentX, infoY, "STATUS", muted);
        DrawFormatString(PanelContentX, infoY + 26, text, "Generation  %llu", static_cast<unsigned long long>(generation));
        DrawFormatString(PanelContentX, infoY + 50, text, "FPS         %.1f", fps);
        DrawFormatString(PanelContentX, infoY + 74, text, "Speed       %d gen/s", SimulationSpeeds[simulationSpeedIndex]);
        DrawFormatString(PanelContentX, infoY + 98, text, "Alive       %llu", static_cast<unsigned long long>(board.aliveCellCount()));
        DrawFormatString(PanelContentX, infoY + 122, text, "Chunks      %llu", static_cast<unsigned long long>(board.chunkCount()));
        DrawFormatString(PanelContentX, infoY + 146, text, "Camera      (%lld, %lld)", static_cast<long long>(camera.x()), static_cast<long long>(camera.y()));
        DrawFormatString(PanelContentX, infoY + 170, text, "Zoom        %d", camera.cellSize());
        DrawFormatString(PanelContentX, infoY + 194, text, "Grid        %s", showGrid ? "ON" : "OFF");
        DrawFormatString(PanelContentX, infoY + 218, text, "Undo/Redo   %llu / %llu",
                         static_cast<unsigned long long>(editHistory.undoCount()),
                         static_cast<unsigned long long>(editHistory.redoCount()));
        DrawFormatString(PanelContentX, infoY + 242, text, "Tool        %s",
                         selectedPatternIndex == 0 ? ShapeDrawing::toolName(shapeTool) : "Pattern");

        DrawString(PanelContentX, 876, "ROTATION", muted);
        const bool rotationEnabled = selectedPatternIndex != 0;
        const unsigned int rotationButton = rotationEnabled ? section : background;
        DrawBox(RotationLeftX, RotationY, RotationLeftX + RotationButtonWidth, RotationY + RotationHeight, rotationButton, TRUE);
        DrawBox(RotationValueX, RotationY, RotationValueX + RotationValueWidth, RotationY + RotationHeight, section, TRUE);
        DrawBox(RotationRightX, RotationY, RotationRightX + RotationButtonWidth, RotationY + RotationHeight, rotationButton, TRUE);
        DrawString(RotationLeftX + 16, RotationY + 6, "<", rotationEnabled ? text : muted);
        DrawFormatString(RotationValueX + 30, RotationY + 6, text, "R%d", patternRotation * 90);
        DrawString(RotationRightX + 17, RotationY + 6, ">", rotationEnabled ? text : muted);

        const char* hoverHelp = nullptr;
        if (mouseOnPanel) {
            for (std::size_t i = 0; i < ActionIcons.size() && hoverHelp == nullptr; ++i) {
                const int leftX = ActionToolbarX + static_cast<int>(i) * (ActionButtonSize + ActionButtonGap);
                if (ToolbarIcons::hit(mouseX, mouseY, leftX, ActionButtonY, ActionButtonSize)) hoverHelp = ToolbarIcons::label(ActionIcons[i]);
            }
            for (std::size_t i = 0; i < ShapeIcons.size() && hoverHelp == nullptr; ++i) {
                const int leftX = ToolToolbarX + static_cast<int>(i) * (ToolButtonSize + ToolButtonGap);
                if (ToolbarIcons::hit(mouseX, mouseY, leftX, ToolButtonY, ToolButtonSize)) hoverHelp = ToolbarIcons::label(ShapeIcons[i]);
            }
        }

        DrawString(PanelContentX, 944, paused ? "PAUSED" : "RUNNING", paused ? GetColor(255, 210, 90) : GetColor(120, 230, 140));
        DrawString(PanelContentX, 966, hoverHelp != nullptr ? hoverHelp : "Shape: drag LMB add / RMB erase", muted);
        DrawString(PanelContentX, 988, "Shortcuts: Ctrl+Z/Y/S/L, P, Q/E, G", muted);
        ScreenFlip();

        ++fpsFrameCount;
        const auto fpsSampleEnd = Clock::now();
        const double fpsSampleSeconds = std::chrono::duration<double>(fpsSampleEnd - fpsSampleStart).count();
        if (fpsSampleSeconds >= 0.5) {
            fps = fpsFrameCount / fpsSampleSeconds;
            fpsFrameCount = 0;
            fpsSampleStart = fpsSampleEnd;
        }

        performanceLogger.record(fps, SimulationSpeeds[simulationSpeedIndex], generation,
                                 board.aliveCellCount(), board.chunkCount(), paused);

        previousEnter = enter;
        previousSpace = space;
        previousDelete = del;
        previousPageUp = pageUp;
        previousPageDown = pageDown;
        previousP = p;
        previousQ = q;
        previousE = e;
        previousG = g;
        previousSaveShortcut = saveShortcut;
        previousLoadShortcut = loadShortcut;
        previousUndoShortcut = undoShortcut;
        previousRedoShortcut = redoShortcut;
        previousLeft = left;
        previousRight = right;
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
