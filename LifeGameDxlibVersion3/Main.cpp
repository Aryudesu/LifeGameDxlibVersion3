#include "DxLib.h"
#include "EditHistory.h"
#include "FileDialog.h"
#include "InfiniteCamera.h"
#include "InfiniteLifeBoard.h"
#include "InfiniteLifeFile.h"
#include "LifeStepSelfTest.h"
#include "PatternLibrary.h"
#include "PatternListScroll.h"
#include "PatternPlacementPreview.h"
#include "PerformanceLogger.h"
#include "ShapeDrawing.h"
#include "SelectionMask.h"
#include "SelectionClipboard.h"
#include "ToolbarIcons.h"
#include "TextInputDialog.h"
#include "UserPatternLibrary.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <thread>
#include <windows.h>

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

constexpr int PanelTabY = 44;
constexpr int PanelTabHeight = 30;
constexpr int PanelTabGap = 4;
constexpr int PanelTabWidth = (PanelContentWidth - PanelTabGap * 2) / 3;
constexpr int ToolButtonY = 92;
constexpr int EditButtonHeight = 32;
constexpr int EditButtonGap = 8;
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

enum class PanelTab { Draw, Pattern, Edit };
constexpr std::array<const char*, 3> PanelTabLabels = {"DRAW", "PATTERN", "EDIT"};

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

    // The board origin is not necessarily aligned with the screen origin after
    // panning/zooming. Start from the screen position of a board-cell boundary,
    // then normalize it into one cell-size period so lines extend both left/up
    // and right/down across the whole viewport.
    const auto [originX, originY] = camera.boardToScreen(0, 0);
    const int firstX = ((originX % cellSize) + cellSize) % cellSize;
    const int firstY = ((originY % cellSize) + cellSize) % cellSize;

    for (int x = firstX; x <= BoardViewWidth; x += cellSize) {
        DrawLine(x, 0, x, ScreenHeight, gridColor);
    }
    for (int y = firstY; y <= ScreenHeight; y += cellSize) {
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
    PerformanceLogger performanceLogger;
    EditHistory editHistory;
    UserPatternLibrary userPatterns;
    std::string userPatternLoadError;
    if (!userPatterns.load(userPatternLoadError) && !userPatternLoadError.empty()) FileDialog::showError(userPatternLoadError);
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
    bool previousV = false;
    bool previousM = false;
    bool previousH = false;
    bool previousJ = false;
    bool previousCopyShortcut = false;
    bool previousCutShortcut = false;
    bool previousPasteShortcut = false;
    bool previousF9 = false;
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
    bool selectionMode = false;
    SelectionMask selection;
    SelectionClipboard clipboard;
    bool pasteMode = false;
    InfiniteLifeBoard::Coord shapeStartX = 0;
    InfiniteLifeBoard::Coord shapeStartY = 0;
    InfiniteLifeBoard::Coord shapeEndX = 0;
    InfiniteLifeBoard::Coord shapeEndY = 0;

    std::uint64_t generation = 0;
    std::size_t simulationSpeedIndex = DefaultSimulationSpeedIndex;
    std::size_t selectedPatternIndex = 0;
    std::size_t rememberedPatternIndex = 0;
    int rememberedPatternRotation = 0;
    PatternCategory toolCategory = PatternCategory::StillLife;
    int patternRotation = 0;
    PanelTab panelTab = PanelTab::Draw;
    bool userPatternCategory = false;
    int userPatternScrollOffset = 0;
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

    auto selectedLifePattern = [&]() -> LifePattern {
        if (selectedPatternIndex < PatternLibrary::size()) return PatternLibrary::at(selectedPatternIndex);
        const std::size_t userIndex = selectedPatternIndex - PatternLibrary::size();
        const UserPattern& user = userPatterns.at(userIndex);
        return {user.name.c_str(), PatternCategory::Cell, user.cells};
    };

    auto saveSelectionAsPattern = [&]() {
        if (!selection.active() || selection.dragging()) return;
        const auto minX = selection.minX(), minY = selection.minY();
        const auto maxX = selection.maxX(), maxY = selection.maxY();
        if (maxX - minX >= std::numeric_limits<int>::max() || maxY - minY >= std::numeric_limits<int>::max()) {
            FileDialog::showError("選択範囲が大きすぎるため、パターンとして保存できません。");
            return;
        }
        std::vector<PatternCell> cells;
        board.forEachAliveCellInRect(minX, minY, maxX + 1, maxY + 1,
            [&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
                cells.push_back({static_cast<int>(x - minX), static_cast<int>(y - minY)});
            });
        if (cells.empty()) { FileDialog::showError("選択範囲に生存セルがありません。"); return; }
        std::string name;
        if (!TextInputDialog::show(GetMainWindowHandle(), "ユーザーパターンを保存", "パターン名:", name)) {
            resetTimingAfterDialog();
            return;
        }
        std::string errorMessage;
        if (!userPatterns.save(name, static_cast<int>(maxX - minX + 1), static_cast<int>(maxY - minY + 1), cells, errorMessage))
            FileDialog::showError(errorMessage);
        resetTimingAfterDialog();
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
                pasteMode = false;
                selectionMode = false;
                selection.clear();
                clipboard.clear();
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
        const bool v = CheckHitKey(KEY_INPUT_V) != 0;
        const bool m = CheckHitKey(KEY_INPUT_M) != 0;
        const bool h = CheckHitKey(KEY_INPUT_H) != 0;
        const bool j = CheckHitKey(KEY_INPUT_J) != 0;
        const bool f9 = CheckHitKey(KEY_INPUT_F9) != 0;
        const bool escape = CheckHitKey(KEY_INPUT_ESCAPE) != 0;
        const bool ctrl = CheckHitKey(KEY_INPUT_LCONTROL) != 0 || CheckHitKey(KEY_INPUT_RCONTROL) != 0;
        const bool shift = CheckHitKey(KEY_INPUT_LSHIFT) != 0 || CheckHitKey(KEY_INPUT_RSHIFT) != 0;
        const bool saveShortcut = ctrl && CheckHitKey(KEY_INPUT_S) != 0;
        const bool loadShortcut = ctrl && CheckHitKey(KEY_INPUT_L) != 0;
        const bool undoShortcut = ctrl && CheckHitKey(KEY_INPUT_Z) != 0 && !shift;
        const bool redoShortcut = (ctrl && CheckHitKey(KEY_INPUT_Y) != 0) ||
                                  (ctrl && shift && CheckHitKey(KEY_INPUT_Z) != 0);
        const bool copyShortcut = ctrl && CheckHitKey(KEY_INPUT_C) != 0;
        const bool cutShortcut = ctrl && CheckHitKey(KEY_INPUT_X) != 0;
        const bool pasteShortcut = ctrl && CheckHitKey(KEY_INPUT_V) != 0;

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
            pasteMode = false;
            selectionMode = false;
            selection.clear();
            clipboard.clear();
            generation = 0;
            paused = true;
            simulationAccumulator = 0.0;
        }
        if (g && !previousG) showGrid = !showGrid;
        if (v && !previousV && paused && !ctrl) {
            panelTab = PanelTab::Edit;
            // SELECT and PASTE are mutually exclusive edit modes.
            // Entering SELECT must cancel an active paste preview first.
            pasteMode = false;
            selectionMode = !selectionMode;
            shapeDragActive = false;
            cellStrokeHasLastCell = false;
            selectedPatternIndex = 0;
            patternRotation = 0;
        }
        if (m && !previousM && paused && selection.active() && !selection.dragging() && !pasteMode) {
            selection.setLiveOnly(board, !selection.liveOnly());
        }
        if (paused && copyShortcut && !previousCopyShortcut && selection.active() && !selection.dragging()) {
            if (clipboard.copy(board, selection)) pasteMode = false;
        }
        if (paused && cutShortcut && !previousCutShortcut && selection.active() && !selection.dragging()) {
            if (clipboard.copy(board, selection)) {
                editHistory.begin();
                if (selection.liveOnly()) {
                    for (const SelectionMask::Cell& cell : selection.cells()) editHistory.setAlive(board, cell.x, cell.y, false);
                } else {
                    board.forEachAliveCellInRect(selection.minX(), selection.minY(), selection.maxX() + 1, selection.maxY() + 1,
                        [&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) { editHistory.setAlive(board, x, y, false); });
                }
                editHistory.commit();
                selection.clear();
                selectionMode = false;
                pasteMode = true;
            }
        }
        if (paused && pasteShortcut && !previousPasteShortcut && clipboard.hasData()) {
            pasteMode = true;
            selectionMode = false;
            selection.clear();
        }
        if (paused && h && !previousH && clipboard.hasData()) clipboard.flipHorizontal();
        if (paused && j && !previousJ && clipboard.hasData()) clipboard.flipVertical();
        if (paused && q && !previousQ) {
            if (pasteMode && clipboard.hasData()) clipboard.rotateCounterClockwise();
            else if (panelTab == PanelTab::Pattern && selectedPatternIndex != 0) patternRotation = (patternRotation + 3) & 3;
        }
        if (paused && e && !previousE) {
            if (pasteMode && clipboard.hasData()) clipboard.rotateClockwise();
            else if (panelTab == PanelTab::Pattern && selectedPatternIndex != 0) patternRotation = (patternRotation + 1) & 3;
        }
        if (escape && pasteMode) pasteMode = false;
        if (saveShortcut && !previousSaveShortcut) saveWithDialog();
        if (loadShortcut && !previousLoadShortcut) loadWithDialog();

        int mouseX = 0, mouseY = 0;
        GetMousePoint(&mouseX, &mouseY);
        const int mouseDeltaX = mouseX - previousMouseX;
        const int mouseDeltaY = mouseY - previousMouseY;
        const int mouseButtons = GetMouseInput();
        const bool left = (mouseButtons & MOUSE_INPUT_LEFT) != 0;
        const bool right = (mouseButtons & MOUSE_INPUT_RIGHT) != 0;
        const bool middle = (mouseButtons & MOUSE_INPUT_MIDDLE) != 0;
        const bool leftPressed = left && !previousLeft;
        const bool rightPressed = right && !previousRight;
        const bool leftReleased = !left && previousLeft;
        const bool rightReleased = !right && previousRight;
        const int wheel = GetMouseWheelRotVol();
        const bool mouseOnBoard = mouseX >= 0 && mouseX < BoardViewWidth && mouseY >= 0 && mouseY < ScreenHeight;

        const bool historyEnabled = paused && !cellStrokeActive && !shapeDragActive;
        if (historyEnabled && undoShortcut && !previousUndoShortcut && editHistory.undoCount() > 0) editHistory.undo(board);
        if (historyEnabled && redoShortcut && !previousRedoShortcut && editHistory.redoCount() > 0) editHistory.redo(board);

        if (leftPressed) {
            bool handled = false;
            for (std::size_t i = 0; i < ActionIcons.size(); ++i) {
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

            for (int i = 0; i < 3 && !handled; ++i) {
                const int leftX = PanelContentX + i * (PanelTabWidth + PanelTabGap);
                if (inRect(mouseX, mouseY, leftX, PanelTabY, leftX + PanelTabWidth, PanelTabY + PanelTabHeight)) {
                    const PanelTab previousTab = panelTab;
                    const PanelTab nextTab = static_cast<PanelTab>(i);

                    if (previousTab == PanelTab::Pattern) {
                        rememberedPatternIndex = selectedPatternIndex;
                        rememberedPatternRotation = patternRotation;
                        selectedPatternIndex = 0;
                    }
                    if (previousTab == PanelTab::Edit && nextTab != PanelTab::Edit) {
                        selectionMode = false;
                        selection.clear();
                        pasteMode = false;
                        shapeTool = ShapeDrawing::Tool::Cell;
                        shapeDragActive = false;
                        cellStrokeHasLastCell = false;
                    }

                    panelTab = nextTab;
                    if (panelTab == PanelTab::Pattern) {
                        selectedPatternIndex = rememberedPatternIndex;
                        patternRotation = rememberedPatternRotation;
                    } else {
                        selectedPatternIndex = 0;
                    }
                    if (panelTab == PanelTab::Edit) {
                        shapeDragActive = false;
                        cellStrokeHasLastCell = false;
                    }
                    handled = true;
                }
            }

            if (panelTab == PanelTab::Draw) for (std::size_t i = 0; i < ShapeTools.size() && !handled; ++i) {
                const int leftX = ToolToolbarX + static_cast<int>(i) * (ToolButtonSize + ToolButtonGap);
                if (ToolbarIcons::hit(mouseX, mouseY, leftX, ToolButtonY, ToolButtonSize)) {
                    shapeTool = ShapeTools[i];
                    selectionMode = false;
                    selection.clear();
                    selectedPatternIndex = 0;
                    patternRotation = 0;
                    shapeDragActive = false;
                    cellStrokeHasLastCell = false;
                    handled = true;
                }
            }

            if (panelTab == PanelTab::Pattern) for (std::size_t i = 0; i < ToolCategories.size() && !handled; ++i) {
                const int column = static_cast<int>(i % 2), row = static_cast<int>(i / 2);
                const int leftX = PanelContentX + column * 144, top = 86 + row * 36;
                if (inRect(mouseX, mouseY, leftX, top, leftX + 136, top + 28)) {
                    selectionMode = false;
                    selection.clear();
                    userPatternCategory = false;
                    toolCategory = ToolCategories[i];
                    selectedPatternIndex = firstPatternInCategory(toolCategory);
                    patternRotation = 0;
                    rememberedPatternIndex = selectedPatternIndex;
                    rememberedPatternRotation = patternRotation;
                    patternListScroll.ensurePatternVisible(selectedPatternIndex);
                    handled = true;
                }
            }

            if (!handled && panelTab == PanelTab::Pattern) {
                const int userLeft = PanelContentX + 144, userTop = 86 + 2 * 36;
                if (inRect(mouseX, mouseY, userLeft, userTop, userLeft + 136, userTop + 28)) {
                    userPatternCategory = true;
                    patternRotation = 0;
                    selectedPatternIndex = userPatterns.size() > 0 ? PatternLibrary::size() : 0;
                    rememberedPatternIndex = selectedPatternIndex;
                    rememberedPatternRotation = 0;
                    handled = true;
                }
            }

            if (!handled && panelTab == PanelTab::Pattern && userPatternCategory) {
                for (std::size_t i = 0; i < userPatterns.size(); ++i) {
                    const int row = static_cast<int>(i) - userPatternScrollOffset;
                    if (row < 0 || row >= PatternListScroll::VisibleRows) continue;
                    const int top = PatternListY + row * PatternRowHeight;
                    if (inRect(mouseX, mouseY, PanelContentX, top, WindowWidth - PanelPadding, top + 24)) {
                        selectedPatternIndex = PatternLibrary::size() + i;
                        patternRotation = 0;
                        rememberedPatternIndex = selectedPatternIndex;
                        rememberedPatternRotation = 0;
                        handled = true;
                        break;
                    }
                }
            }

            if (!handled && panelTab == PanelTab::Pattern && !userPatternCategory) {
                const int scrollOffset = patternListScroll.offset(toolCategory);
                int categoryRow = 0;
                for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {
                    const LifePattern& pattern = PatternLibrary::at(i);
                    if (pattern.category != toolCategory) continue;
                    if (categoryRow >= scrollOffset && categoryRow < scrollOffset + PatternListScroll::VisibleRows) {
                        const int top = PatternListY + (categoryRow - scrollOffset) * PatternRowHeight;
                        if (inRect(mouseX, mouseY, PanelContentX, top, WindowWidth - PanelPadding, top + 24)) {
                            selectedPatternIndex = i;
                            patternRotation = 0;
                            rememberedPatternIndex = selectedPatternIndex;
                            rememberedPatternRotation = patternRotation;
                            handled = true;
                            break;
                        }
                    }
                    ++categoryRow;
                }
            }

            if (!handled && panelTab == PanelTab::Pattern && selectedPatternIndex != 0) {
                if (inRect(mouseX, mouseY, RotationLeftX, RotationY, RotationLeftX + RotationButtonWidth, RotationY + RotationHeight)) patternRotation = (patternRotation + 3) & 3;
                else if (inRect(mouseX, mouseY, RotationValueX, RotationY, RotationValueX + RotationValueWidth, RotationY + RotationHeight)) patternRotation = 0;
                else if (inRect(mouseX, mouseY, RotationRightX, RotationY, RotationRightX + RotationButtonWidth, RotationY + RotationHeight)) patternRotation = (patternRotation + 1) & 3;
            }


            if (!handled && panelTab == PanelTab::Edit && paused) {
                const int fullLeft = PanelContentX, fullRight = WindowWidth - PanelPadding;
                const int row0 = 92, row1 = row0 + EditButtonHeight + EditButtonGap;
                const int row2 = row1 + EditButtonHeight + EditButtonGap;
                const int row3 = row2 + EditButtonHeight + EditButtonGap;
                const int row4 = row3 + EditButtonHeight + EditButtonGap;
                const int row5 = row4 + EditButtonHeight + EditButtonGap;
                const int halfGap = 8, halfWidth = (PanelContentWidth - halfGap) / 2;
                const int rightLeft = fullLeft + halfWidth + halfGap;

                if (inRect(mouseX, mouseY, fullLeft, row0, fullRight, row0 + EditButtonHeight)) {
                    selectionMode = !selectionMode; pasteMode = false;
                    shapeDragActive = false; cellStrokeHasLastCell = false;
                    if (!selectionMode) selection.clear(); handled = true;
                } else if (inRect(mouseX, mouseY, fullLeft, row1, fullLeft + halfWidth, row1 + EditButtonHeight)) {
                    if (selection.active() && !selection.dragging()) selection.setLiveOnly(board, !selection.liveOnly());
                    handled = true;
                } else if (inRect(mouseX, mouseY, rightLeft, row1, fullRight, row1 + EditButtonHeight)) {
                    if (selection.active() && !selection.dragging()) clipboard.copy(board, selection);
                    handled = true;
                } else if (inRect(mouseX, mouseY, fullLeft, row2, fullLeft + halfWidth, row2 + EditButtonHeight)) {
                    if (selection.active() && !selection.dragging() && clipboard.copy(board, selection)) {
                        editHistory.begin();
                        if (selection.liveOnly()) {
                            for (const SelectionMask::Cell& cell : selection.cells()) editHistory.setAlive(board, cell.x, cell.y, false);
                        } else {
                            board.forEachAliveCellInRect(selection.minX(), selection.minY(), selection.maxX() + 1, selection.maxY() + 1,
                                [&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) { editHistory.setAlive(board, x, y, false); });
                        }
                        editHistory.commit(); selection.clear(); selectionMode = false; pasteMode = true;
                    }
                    handled = true;
                } else if (inRect(mouseX, mouseY, rightLeft, row2, fullRight, row2 + EditButtonHeight)) {
                    if (clipboard.hasData()) { pasteMode = true; selectionMode = false; selection.clear(); }
                    handled = true;
                } else if (inRect(mouseX, mouseY, fullLeft, row3, fullLeft + halfWidth, row3 + EditButtonHeight)) {
                    if (clipboard.hasData()) clipboard.rotateCounterClockwise(); handled = true;
                } else if (inRect(mouseX, mouseY, rightLeft, row3, fullRight, row3 + EditButtonHeight)) {
                    if (clipboard.hasData()) clipboard.rotateClockwise(); handled = true;
                } else if (inRect(mouseX, mouseY, fullLeft, row4, fullLeft + halfWidth, row4 + EditButtonHeight)) {
                    if (clipboard.hasData()) clipboard.flipHorizontal(); handled = true;
                } else if (inRect(mouseX, mouseY, rightLeft, row4, fullRight, row4 + EditButtonHeight)) {
                    if (clipboard.hasData()) clipboard.flipVertical(); handled = true;
                } else if (inRect(mouseX, mouseY, fullLeft, row5, fullRight, row5 + EditButtonHeight)) {
                    if (selection.active() && !selection.dragging()) saveSelectionAsPattern();
                    handled = true;
                }
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
                if (pasteMode && clipboard.hasData()) {
                    if (rightPressed) {
                        pasteMode = false;
                    } else if (leftPressed) {
                        editHistory.begin();
                        clipboard.paste(x, y, [&](InfiniteLifeBoard::Coord cellX, InfiniteLifeBoard::Coord cellY, bool alive) {
                            editHistory.setAlive(board, cellX, cellY, alive);
                        });
                        editHistory.commit();
                    }
                } else if (selectionMode) {
                    if (leftPressed) selection.begin(x, y);
                    if (selection.dragging() && left) selection.update(x, y);
                    if (selection.dragging() && leftReleased) {
                        selection.update(x, y);
                        selection.finish(board, false);
                    }
                } else if (selectedPatternIndex == 0) {
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
                    const LifePattern pattern = selectedLifePattern();
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
        const auto [minVisibleX, minVisibleY] = camera.screenToBoard(0, 0);
        const auto [maxVisibleX, maxVisibleY] = camera.screenToBoard(BoardViewWidth, ScreenHeight);
        const int cellSize = camera.cellSize();
        board.forEachAliveCellInRect(
            minVisibleX, minVisibleY, maxVisibleX, maxVisibleY,
            [&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
                const auto [sx, sy] = camera.boardToScreen(x, y);
                if (sx + cellSize <= 0 || sy + cellSize <= 0 ||
                    sx >= BoardViewWidth || sy >= ScreenHeight) {
                    return;
                }
                if (cellSize == 1) DrawPixel(sx, sy, aliveColor);
                else DrawBox(sx, sy, sx + cellSize - 1, sy + cellSize - 1, aliveColor, TRUE);
            });


        if (paused && shapeDragActive && selectedPatternIndex == 0 && shapeTool != ShapeDrawing::Tool::Cell) {
            ShapeDrawing::drawPreview(shapeTool, camera, shapeStartX, shapeStartY, shapeEndX, shapeEndY,
                                      BoardViewWidth, ScreenHeight, shapeErase);
        }

        if (paused && mouseOnBoard && !middle && pasteMode && clipboard.hasData()) {
            const auto [previewX, previewY] = camera.screenToBoard(mouseX, mouseY);
            const unsigned int ghostColor = GetColor(90, 220, 255);
            const int inset = cellSize >= 4 ? 2 : 0;
            if (clipboard.replaceRectangle()) {
                const auto [sx0, sy0] = camera.boardToScreen(previewX, previewY);
                const auto [sx1, sy1] = camera.boardToScreen(previewX + clipboard.width(), previewY + clipboard.height());
                DrawBox(sx0, sy0, sx1 - 1, sy1 - 1, GetColor(55, 70, 75), TRUE);
            }
            for (const SelectionClipboard::Cell& cell : clipboard.cells()) {
                const auto [sx, sy] = camera.boardToScreen(previewX + cell.x, previewY + cell.y);
                if (sx + cellSize <= 0 || sy + cellSize <= 0 || sx >= BoardViewWidth || sy >= ScreenHeight) continue;
                if (cellSize == 1) DrawPixel(sx, sy, ghostColor);
                else DrawBox(sx + inset, sy + inset, sx + cellSize - 1 - inset, sy + cellSize - 1 - inset, ghostColor, TRUE);
            }
            const auto [sx0, sy0] = camera.boardToScreen(previewX, previewY);
            const auto [sx1, sy1] = camera.boardToScreen(previewX + clipboard.width(), previewY + clipboard.height());
            DrawBox(sx0, sy0, sx1 - 1, sy1 - 1, ghostColor, FALSE);
        } else if (paused && mouseOnBoard && !middle && selectedPatternIndex != 0) {
            const auto [previewX, previewY] = camera.screenToBoard(mouseX, mouseY);
            const LifePattern pattern = selectedLifePattern();
            PatternPlacementPreview::draw(camera, pattern, previewX, previewY,
                                          patternRotation, BoardViewWidth, ScreenHeight);
        }

        // Keep grid lines visible over pattern/clipboard ghosts.
        if (showGrid) drawGrid(camera);

        // Selection is a UI overlay: keep its border/mask above the grid.
        if (paused && selection.active()) {
            const auto [sx0, sy0] = camera.boardToScreen(selection.minX(), selection.minY());
            const auto [sx1, sy1] = camera.boardToScreen(selection.maxX() + 1, selection.maxY() + 1);
            const unsigned int selectionColor = selection.liveOnly()
                ? GetColor(255, 190, 80)
                : GetColor(80, 190, 255);

            if (selection.liveOnly()) {
                // A live-cell mask is sparse: visualize the actual masked cells
                // instead of only changing the rectangle color.
                const int inset = cellSize >= 4 ? 2 : 0;
                for (const SelectionMask::Cell& cell : selection.cells()) {
                    const auto [sx, sy] = camera.boardToScreen(cell.x, cell.y);
                    if (sx + cellSize <= 0 || sy + cellSize <= 0 ||
                        sx >= BoardViewWidth || sy >= ScreenHeight) {
                        continue;
                    }
                    if (cellSize == 1) {
                        DrawPixel(sx, sy, selectionColor);
                    } else {
                        DrawBox(sx + inset, sy + inset,
                                sx + cellSize - 1 - inset, sy + cellSize - 1 - inset,
                                selectionColor, TRUE);
                    }
                }
            }
            DrawBox(sx0, sy0, sx1 - 1, sy1 - 1, selectionColor, FALSE);
        }

        DrawBox(PanelX, 0, WindowWidth, ScreenHeight, GetColor(24, 24, 28), TRUE);

        const int actionMouseX = mouseX;
        const int actionMouseY = mouseY;
        for (std::size_t i = 0; i < ActionIcons.size(); ++i) {
            const int leftX = ActionToolbarX + static_cast<int>(i) * (ActionButtonSize + ActionButtonGap);
            const bool enabled = (ActionIcons[i] != ToolbarIcons::Icon::Undo && ActionIcons[i] != ToolbarIcons::Icon::Redo) ||
                                 (historyEnabled && (ActionIcons[i] == ToolbarIcons::Icon::Undo ? editHistory.undoCount() > 0 : editHistory.redoCount() > 0));
            ToolbarIcons::draw(ActionIcons[i], leftX, ActionButtonY, ActionButtonSize, actionMouseX, actionMouseY, enabled);
        }

        for (int i = 0; i < 3; ++i) {
            const int leftX = PanelContentX + i * (PanelTabWidth + PanelTabGap);
            const bool active = panelTab == static_cast<PanelTab>(i);
            DrawBox(leftX, PanelTabY, leftX + PanelTabWidth, PanelTabY + PanelTabHeight,
                    active ? GetColor(72, 72, 82) : GetColor(44, 44, 50), TRUE);
            DrawString(leftX + 10, PanelTabY + 7, PanelTabLabels[i],
                       active ? GetColor(255, 255, 255) : GetColor(185, 185, 195));
        }

        if (panelTab == PanelTab::Draw) {
            DrawString(PanelContentX, 76, "Tool", GetColor(190, 190, 200));
            for (std::size_t i = 0; i < ShapeTools.size(); ++i) {
                const int leftX = ToolToolbarX + static_cast<int>(i) * (ToolButtonSize + ToolButtonGap);
                ToolbarIcons::draw(ShapeIcons[i], leftX, ToolButtonY, ToolButtonSize,
                                   mouseX, mouseY, shapeTool == ShapeTools[i]);
            }
        } else if (panelTab == PanelTab::Pattern) {
            DrawString(PanelContentX, 76, "Category", GetColor(190, 190, 200));
            for (std::size_t i = 0; i < ToolCategories.size(); ++i) {
                const int column = static_cast<int>(i % 2), row = static_cast<int>(i / 2);
                const int leftX = PanelContentX + column * 144, top = 86 + row * 36;
                const bool active = !userPatternCategory && toolCategory == ToolCategories[i];
                DrawBox(leftX, top, leftX + 136, top + 28, active ? GetColor(78, 78, 88) : GetColor(44, 44, 50), TRUE);
                DrawString(leftX + 8, top + 6, PatternLibrary::categoryName(ToolCategories[i]), GetColor(225, 225, 230));
            }
            {
                const int leftX = PanelContentX + 144, top = 86 + 2 * 36;
                DrawBox(leftX, top, leftX + 136, top + 28, userPatternCategory ? GetColor(78, 78, 88) : GetColor(44, 44, 50), TRUE);
                DrawString(leftX + 8, top + 6, "User", GetColor(225, 225, 230));
            }

            DrawString(PanelContentX, 174, "Pattern", GetColor(190, 190, 200));
            if (userPatternCategory) {
                for (std::size_t i = 0; i < userPatterns.size(); ++i) {
                    const int row = static_cast<int>(i) - userPatternScrollOffset;
                    if (row < 0 || row >= PatternListScroll::VisibleRows) continue;
                    const int top = PatternListY + row * PatternRowHeight;
                    const bool selected = selectedPatternIndex == PatternLibrary::size() + i;
                    if (selected) DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24, GetColor(62, 62, 72), TRUE);
                    DrawString(PanelContentX + 8, top + 5, userPatterns.at(i).name.c_str(), GetColor(225, 225, 230));
                }
            } else {
                const int scrollOffset = patternListScroll.offset(toolCategory);
                int categoryRow = 0;
                for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {
                    const LifePattern& pattern = PatternLibrary::at(i);
                    if (pattern.category != toolCategory) continue;
                    if (categoryRow >= scrollOffset && categoryRow < scrollOffset + PatternListScroll::VisibleRows) {
                        const int top = PatternListY + (categoryRow - scrollOffset) * PatternRowHeight;
                        if (selectedPatternIndex == i) DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24, GetColor(62, 62, 72), TRUE);
                        DrawString(PanelContentX + 8, top + 5, pattern.name, GetColor(225, 225, 230));
                    }
                    ++categoryRow;
                }
            }

            if (selectedPatternIndex != 0) {
                DrawString(PanelContentX, RotationY - 24, "Rotation", GetColor(190, 190, 200));
                DrawBox(RotationLeftX, RotationY, RotationLeftX + RotationButtonWidth, RotationY + RotationHeight, GetColor(44, 44, 50), TRUE);
                DrawString(RotationLeftX + 17, RotationY + 7, "<", GetColor(225, 225, 230));
                DrawBox(RotationValueX, RotationY, RotationValueX + RotationValueWidth, RotationY + RotationHeight, GetColor(62, 62, 72), TRUE);
                const std::string rotationText = std::to_string(patternRotation * 90) + " deg";
                DrawString(RotationValueX + 24, RotationY + 7, rotationText.c_str(), GetColor(225, 225, 230));
                DrawBox(RotationRightX, RotationY, RotationRightX + RotationButtonWidth, RotationY + RotationHeight, GetColor(44, 44, 50), TRUE);
                DrawString(RotationRightX + 17, RotationY + 7, ">", GetColor(225, 225, 230));
            }
        } else {
            DrawString(PanelContentX, 76, "Edit", GetColor(190, 190, 200));
            const int fullLeft = PanelContentX, fullRight = WindowWidth - PanelPadding;
            const int row0 = 92, row1 = row0 + EditButtonHeight + EditButtonGap;
            const int row2 = row1 + EditButtonHeight + EditButtonGap;
            const int row3 = row2 + EditButtonHeight + EditButtonGap;
            const int row4 = row3 + EditButtonHeight + EditButtonGap;
            const int row5 = row4 + EditButtonHeight + EditButtonGap;
            const int halfGap = 8, halfWidth = (PanelContentWidth - halfGap) / 2;
            const int rightLeft = fullLeft + halfWidth + halfGap;
            auto drawEditButton = [&](int leftX, int top, int rightX, const char* label, bool enabled, bool active = false) {
                const unsigned int bg = !enabled ? GetColor(34, 34, 38) : active ? GetColor(72, 72, 82) : GetColor(44, 44, 50);
                const unsigned int fg = enabled ? GetColor(225, 225, 230) : GetColor(105, 105, 112);
                DrawBox(leftX, top, rightX, top + EditButtonHeight, bg, TRUE);
                const int width = GetDrawStringWidth(label, static_cast<int>(std::char_traits<char>::length(label)));
                DrawString(leftX + (rightX - leftX - width) / 2, top + 8, label, fg);
            };
            const bool hasSelection = selection.active() && !selection.dragging();
            drawEditButton(fullLeft, row0, fullRight, "SELECT", paused, selectionMode);
            drawEditButton(fullLeft, row1, fullLeft + halfWidth, selection.liveOnly() ? "MASK: LIVE" : "MASK: RECT", paused && hasSelection, selection.liveOnly());
            drawEditButton(rightLeft, row1, fullRight, "COPY", paused && hasSelection);
            drawEditButton(fullLeft, row2, fullLeft + halfWidth, "CUT", paused && hasSelection);
            drawEditButton(rightLeft, row2, fullRight, "PASTE", paused && clipboard.hasData(), pasteMode);
            drawEditButton(fullLeft, row3, fullLeft + halfWidth, "ROTATE CCW", paused && clipboard.hasData());
            drawEditButton(rightLeft, row3, fullRight, "ROTATE CW", paused && clipboard.hasData());
            drawEditButton(fullLeft, row4, fullLeft + halfWidth, "FLIP H", paused && clipboard.hasData());
            drawEditButton(rightLeft, row4, fullRight, "FLIP V", paused && clipboard.hasData());
            drawEditButton(fullLeft, row5, fullRight, "SAVE PATTERN", paused && hasSelection);
        }

        DrawFormatString(PanelContentX, 940, GetColor(255, 255, 255), "Generation: %llu", static_cast<unsigned long long>(generation));
        DrawFormatString(PanelContentX, 962, GetColor(180, 180, 190), "FPS: %.1f", fps);
        DrawFormatString(PanelContentX, 984, GetColor(180, 180, 190), "Speed: %d gen/s", SimulationSpeeds[simulationSpeedIndex]);

        ScreenFlip();

        ++fpsFrameCount;
        const auto fpsNow = Clock::now();
        const double fpsElapsed = std::chrono::duration<double>(fpsNow - fpsSampleStart).count();
        if (fpsElapsed >= 0.5) {
            fps = fpsFrameCount / fpsElapsed;
            fpsFrameCount = 0;
            fpsSampleStart = fpsNow;
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
        previousV = v;
        previousM = m;
        previousH = h;
        previousJ = j;
        previousCopyShortcut = copyShortcut;
        previousCutShortcut = cutShortcut;
        previousPasteShortcut = pasteShortcut;
        previousF9 = f9;
        previousSaveShortcut = saveShortcut;
        previousLoadShortcut = loadShortcut;
        previousUndoShortcut = undoShortcut;
        previousRedoShortcut = redoShortcut;
        previousLeft = left;
        previousRight = right;
        previousMouseX = mouseX;
        previousMouseY = mouseY;

        nextFrameTime += targetFrameDuration;
        const auto now = Clock::now();
        if (nextFrameTime > now) std::this_thread::sleep_until(nextFrameTime);
        else if (now - nextFrameTime > targetFrameDuration * 4) nextFrameTime = now;
    }

    DxLib_End();
    return 0;
}
