#include "DxLib.h"
#include "EditHistory.h"
#include "FileDialog.h"
#include "FileDrop.h"
#include "InfiniteCamera.h"
#include "InfiniteLifeBoard.h"
#include "InfiniteLifeFile.h"
#include "ImportPatternLibrary.h"
#include "LifeStepSelfTest.h"
#include "PatternLibrary.h"
#include "PatternListScroll.h"
#include "PatternMetadataStore.h"
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
constexpr int PatternManageY = 862;
constexpr int PatternManageHeight = 28;
constexpr int PatternManageGap = 6;
constexpr int PatternManageButtonWidth = (PanelContentWidth - PatternManageGap * 2) / 3;
constexpr int PatternFavoriteX = PanelContentX;
constexpr int PatternRenameX = PatternFavoriteX + PatternManageButtonWidth + PatternManageGap;
constexpr int PatternDeleteX = PatternRenameX + PatternManageButtonWidth + PatternManageGap;
constexpr int RotationLabelY = 898;
constexpr int RotationY = 916;
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
    FileDrop::installHook();
    if (DxLib_Init() == -1) return -1;
    FileDrop::enable();
    SetDrawScreen(DX_SCREEN_BACK);

    InfiniteLifeBoard board;
    InfiniteCamera camera(BoardViewWidth, ScreenHeight, 8);
    PatternListScroll patternListScroll;
    PerformanceLogger performanceLogger;
    EditHistory editHistory;
    UserPatternLibrary userPatterns;
    std::string userPatternLoadError;
    if (!userPatterns.load(userPatternLoadError) && !userPatternLoadError.empty()) FileDialog::showError(userPatternLoadError);
    ImportPatternLibrary importPatterns;
    std::string importPatternLoadError;
    if (!importPatterns.load(importPatternLoadError) && !importPatternLoadError.empty()) FileDialog::showError(importPatternLoadError);
    PatternMetadataStore patternMetadata;
    std::string patternMetadataLoadError;
    if (!patternMetadata.load(patternMetadataLoadError) && !patternMetadataLoadError.empty()) FileDialog::showError(patternMetadataLoadError);
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
    bool importPatternCategory = false;
    bool favoritePatternCategory = false;
    int userPatternScrollOffset = 0;
    int importPatternScrollOffset = 0;
    int favoritePatternScrollOffset = 0;
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

        std::size_t externalIndex = selectedPatternIndex - PatternLibrary::size();
        if (externalIndex < userPatterns.size()) {
            const UserPattern& user = userPatterns.at(externalIndex);
            return {user.name.c_str(), PatternCategory::Cell, user.cells};
        }

        externalIndex -= userPatterns.size();
        const ImportPattern& imported = importPatterns.at(externalIndex);
        return {imported.name.c_str(), PatternCategory::Cell, imported.cells};
    };

    auto selectImportedPattern = [&](std::size_t importedIndex) {
        panelTab = PanelTab::Pattern;
        userPatternCategory = false;
        importPatternCategory = true;
        importPatternScrollOffset = std::max(
            0,
            static_cast<int>(importedIndex + 1) - PatternListScroll::VisibleRows + 1);
        selectedPatternIndex = PatternLibrary::size() + userPatterns.size() + importedIndex;
        patternRotation = 0;
        rememberedPatternIndex = selectedPatternIndex;
        rememberedPatternRotation = 0;

        selectionMode = false;
        selection.clear();
        pasteMode = false;
        shapeDragActive = false;
        cellStrokeHasLastCell = false;
    };

    auto importPatternFromPath = [&](const std::string& path) {
        std::size_t importedIndex = 0;
        std::string errorMessage;
        if (!importPatterns.importFile(path, importedIndex, errorMessage)) {
            FileDialog::showError(errorMessage);
            return false;
        }

        selectImportedPattern(importedIndex);
        return true;
    };

    auto importPatternWithDialog = [&]() {
        std::string path;
        if (FileDialog::chooseRleImportPath(path)) {
            importPatternFromPath(path);
        }
        resetTimingAfterDialog();
    };

    auto userPatternKey = [&](std::size_t index) {
        return std::string("user/") + userPatterns.at(index).fileName;
    };

    auto importPatternKey = [&](std::size_t index) {
        return std::string("import/") + importPatterns.at(index).fileName;
    };

    auto selectedUserPattern = [&](std::size_t& index) {
        if (selectedPatternIndex < PatternLibrary::size()) return false;
        index = selectedPatternIndex - PatternLibrary::size();
        return index < userPatterns.size();
    };

    auto selectedImportPattern = [&](std::size_t& index) {
        const std::size_t importBase = PatternLibrary::size() + userPatterns.size();
        if (selectedPatternIndex < importBase) return false;
        index = selectedPatternIndex - importBase;
        return index < importPatterns.size();
    };

    auto selectedPatternMetadataKey = [&](std::string& key) {
        std::size_t index = 0;
        if (selectedUserPattern(index)) {
            key = userPatternKey(index);
            return true;
        }
        if (selectedImportPattern(index)) {
            key = importPatternKey(index);
            return true;
        }
        key.clear();
        return false;
    };

    auto externalPatternName = [&](std::size_t encodedIndex) -> const std::string& {
        const std::size_t userBase = PatternLibrary::size();
        const std::size_t importBase = userBase + userPatterns.size();
        if (encodedIndex >= userBase && encodedIndex < importBase)
            return userPatterns.at(encodedIndex - userBase).name;
        return importPatterns.at(encodedIndex - importBase).name;
    };

    auto favoritePatternIndices = [&]() {
        std::vector<std::size_t> indices;
        indices.reserve(userPatterns.size() + importPatterns.size());
        for (std::size_t i = 0; i < userPatterns.size(); ++i) {
            if (patternMetadata.isFavorite(userPatternKey(i)))
                indices.push_back(PatternLibrary::size() + i);
        }
        const std::size_t importBase = PatternLibrary::size() + userPatterns.size();
        for (std::size_t i = 0; i < importPatterns.size(); ++i) {
            if (patternMetadata.isFavorite(importPatternKey(i)))
                indices.push_back(importBase + i);
        }
        std::sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
            const std::string& nameA = externalPatternName(a);
            const std::string& nameB = externalPatternName(b);
            if (nameA != nameB) return nameA < nameB;
            return a < b;
        });
        return indices;
    };

    auto ensureUserPatternVisible = [&](std::size_t index) {
        const int row = static_cast<int>(index);
        if (row < userPatternScrollOffset) userPatternScrollOffset = row;
        else if (row >= userPatternScrollOffset + PatternListScroll::VisibleRows)
            userPatternScrollOffset = row - PatternListScroll::VisibleRows + 1;
        const int maxOffset = std::max(0, static_cast<int>(userPatterns.size()) - PatternListScroll::VisibleRows);
        userPatternScrollOffset = std::clamp(userPatternScrollOffset, 0, maxOffset);
    };

    auto ensureImportPatternVisible = [&](std::size_t index) {
        const int logicalRow = static_cast<int>(index) + 1;
        if (logicalRow < importPatternScrollOffset) importPatternScrollOffset = logicalRow;
        else if (logicalRow >= importPatternScrollOffset + PatternListScroll::VisibleRows)
            importPatternScrollOffset = logicalRow - PatternListScroll::VisibleRows + 1;
        const int maxOffset = std::max(
            0, static_cast<int>(importPatterns.size()) + 1 - PatternListScroll::VisibleRows);
        importPatternScrollOffset = std::clamp(importPatternScrollOffset, 0, maxOffset);
    };

    auto ensureFavoritePatternVisible = [&](std::size_t encodedIndex) {
        const std::vector<std::size_t> favorites = favoritePatternIndices();
        const auto it = std::find(favorites.begin(), favorites.end(), encodedIndex);
        if (it == favorites.end()) return;

        const int row = static_cast<int>(std::distance(favorites.begin(), it));
        if (row < favoritePatternScrollOffset) favoritePatternScrollOffset = row;
        else if (row >= favoritePatternScrollOffset + PatternListScroll::VisibleRows)
            favoritePatternScrollOffset = row - PatternListScroll::VisibleRows + 1;
        const int maxOffset = std::max(
            0, static_cast<int>(favorites.size()) - PatternListScroll::VisibleRows);
        favoritePatternScrollOffset = std::clamp(favoritePatternScrollOffset, 0, maxOffset);
    };

    auto toggleSelectedFavorite = [&]() {
        std::string key;
        if (!selectedPatternMetadataKey(key)) return;

        const bool wasFavorite = patternMetadata.isFavorite(key);
        std::vector<std::size_t> before;
        std::size_t oldFavoritePosition = 0;
        if (favoritePatternCategory && wasFavorite) {
            before = favoritePatternIndices();
            const auto it = std::find(before.begin(), before.end(), selectedPatternIndex);
            if (it != before.end())
                oldFavoritePosition = static_cast<std::size_t>(std::distance(before.begin(), it));
        }

        std::string errorMessage;
        if (!patternMetadata.setFavorite(key, !wasFavorite, errorMessage)) {
            FileDialog::showError(errorMessage);
            return;
        }

        if (favoritePatternCategory && wasFavorite) {
            const std::vector<std::size_t> after = favoritePatternIndices();
            if (after.empty()) {
                selectedPatternIndex = 0;
                favoritePatternScrollOffset = 0;
            } else {
                const std::size_t next = std::min(oldFavoritePosition, after.size() - 1);
                selectedPatternIndex = after[next];
                ensureFavoritePatternVisible(selectedPatternIndex);
            }
            patternRotation = 0;
        }

        rememberedPatternIndex = selectedPatternIndex;
        rememberedPatternRotation = patternRotation;
    };

    auto renameSelectedPattern = [&]() {
        std::size_t index = 0;
        const bool isUser = selectedUserPattern(index);
        const bool isImport = !isUser && selectedImportPattern(index);
        if (!isUser && !isImport) return;

        const std::string oldMetadataKey = isUser ? userPatternKey(index) : importPatternKey(index);
        std::string newName = isUser ? userPatterns.at(index).name : importPatterns.at(index).name;
        if (!TextInputDialog::show(
                GetMainWindowHandle(), "Rename Pattern", "New name:", newName)) {
            resetTimingAfterDialog();
            return;
        }

        std::size_t renamedIndex = 0;
        std::string errorMessage;
        const bool renamed = isUser
            ? userPatterns.rename(index, newName, renamedIndex, errorMessage)
            : importPatterns.rename(index, newName, renamedIndex, errorMessage);
        if (!renamed) {
            FileDialog::showError(errorMessage);
            resetTimingAfterDialog();
            return;
        }

        const std::string newMetadataKey = isUser
            ? userPatternKey(renamedIndex)
            : importPatternKey(renamedIndex);
        std::string metadataError;
        if (!patternMetadata.renameKey(oldMetadataKey, newMetadataKey, metadataError) &&
            !metadataError.empty()) {
            FileDialog::showError(metadataError);
        }

        if (isUser) {
            selectedPatternIndex = PatternLibrary::size() + renamedIndex;
            if (favoritePatternCategory) ensureFavoritePatternVisible(selectedPatternIndex);
            else ensureUserPatternVisible(renamedIndex);
        } else {
            selectedPatternIndex = PatternLibrary::size() + userPatterns.size() + renamedIndex;
            if (favoritePatternCategory) ensureFavoritePatternVisible(selectedPatternIndex);
            else ensureImportPatternVisible(renamedIndex);
        }
        rememberedPatternIndex = selectedPatternIndex;
        rememberedPatternRotation = patternRotation;
        resetTimingAfterDialog();
    };

    auto deleteSelectedPattern = [&]() {
        std::size_t index = 0;
        const bool isUser = selectedUserPattern(index);
        const bool isImport = !isUser && selectedImportPattern(index);
        if (!isUser && !isImport) return;

        const std::string metadataKey = isUser ? userPatternKey(index) : importPatternKey(index);
        std::size_t oldFavoritePosition = 0;
        if (favoritePatternCategory) {
            const std::vector<std::size_t> favorites = favoritePatternIndices();
            const auto it = std::find(favorites.begin(), favorites.end(), selectedPatternIndex);
            if (it != favorites.end())
                oldFavoritePosition = static_cast<std::size_t>(std::distance(favorites.begin(), it));
        }

        const int answer = MessageBoxW(
            GetMainWindowHandle(),
            L"\u9078\u629e\u3057\u305f\u30d1\u30bf\u30fc\u30f3\u3092\u524a\u9664\u3057\u307e\u3059\u304b\uff1f\n\u3053\u306e\u64cd\u4f5c\u306f\u53d6\u308a\u6d88\u305b\u307e\u305b\u3093\u3002",
            L"\u30d1\u30bf\u30fc\u30f3\u306e\u524a\u9664",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (answer != IDYES) {
            resetTimingAfterDialog();
            return;
        }

        std::string errorMessage;
        const bool removed = isUser
            ? userPatterns.remove(index, errorMessage)
            : importPatterns.remove(index, errorMessage);
        if (!removed) {
            FileDialog::showError(errorMessage);
            resetTimingAfterDialog();
            return;
        }

        std::string metadataError;
        if (!patternMetadata.removeKey(metadataKey, metadataError) && !metadataError.empty())
            FileDialog::showError(metadataError);

        if (favoritePatternCategory) {
            const std::vector<std::size_t> favorites = favoritePatternIndices();
            if (favorites.empty()) {
                selectedPatternIndex = 0;
                favoritePatternScrollOffset = 0;
            } else {
                const std::size_t next = std::min(oldFavoritePosition, favorites.size() - 1);
                selectedPatternIndex = favorites[next];
                ensureFavoritePatternVisible(selectedPatternIndex);
            }
        } else if (isUser) {
            if (userPatterns.size() == 0) {
                selectedPatternIndex = 0;
            } else {
                const std::size_t next = std::min(index, userPatterns.size() - 1);
                selectedPatternIndex = PatternLibrary::size() + next;
                ensureUserPatternVisible(next);
            }
        } else {
            if (importPatterns.size() == 0) {
                selectedPatternIndex = 0;
            } else {
                const std::size_t next = std::min(index, importPatterns.size() - 1);
                selectedPatternIndex = PatternLibrary::size() + userPatterns.size() + next;
                ensureImportPatternVisible(next);
            }
        }

        patternRotation = 0;
        rememberedPatternIndex = selectedPatternIndex;
        rememberedPatternRotation = 0;
        resetTimingAfterDialog();
    };

    auto saveSelectionAsPattern = [&]() {
        if (!selection.active() || selection.dragging()) return;

        const auto minX = selection.minX(), minY = selection.minY();
        const auto maxX = selection.maxX(), maxY = selection.maxY();

        if (maxX - minX >= std::numeric_limits<int>::max() ||
            maxY - minY >= std::numeric_limits<int>::max()) {
            // 選択範囲が大きすぎるため、パターンとして保存できません。
            FileDialog::showError(
                "\x91\x49\x91\xF0\x94\xCD\x88\xCD"
                "\x82\xAA\x91\xE5\x82\xAB\x82\xB7\x82\xAC\x82\xE9"
                "\x82\xBD\x82\xDF\x81\x41"
                "\x83\x70\x83\x5E\x81\x5B\x83\x93"
                "\x82\xC6\x82\xB5\x82\xC4"
                "\x95\xDB\x91\xB6"
                "\x82\xC5\x82\xAB\x82\xDC\x82\xB9\x82\xF1\x81\x42"
            );
            return;
        }

        std::vector<PatternCell> cells;
        board.forEachAliveCellInRect(
            minX, minY, maxX + 1, maxY + 1,
            [&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
                cells.push_back({
                    static_cast<int>(x - minX),
                    static_cast<int>(y - minY)
                    });
            }
        );

        if (cells.empty()) {
            // 選択範囲に生存セルがありません。
            FileDialog::showError(
                "\x91\x49\x91\xF0\x94\xCD\x88\xCD"
                "\x82\xC9\x90\xB6\x91\xB6"
                "\x83\x5A\x83\x8B"
                "\x82\xAA\x82\xA0\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x81\x42"
            );
            return;
        }

        std::string name;

        // 「ユーザーパターンを保存」「パターン名:」
        if (!TextInputDialog::show(
            GetMainWindowHandle(),
            "\x83\x86\x81\x5B\x83\x55\x81\x5B"
            "\x83\x70\x83\x5E\x81\x5B\x83\x93"
            "\x82\xF0\x95\xDB\x91\xB6",
            "\x83\x70\x83\x5E\x81\x5B\x83\x93\x96\xBC\x3A",
            name)) {
            resetTimingAfterDialog();
            return;
        }

        std::string errorMessage;
        if (!userPatterns.save(
            name,
            static_cast<int>(maxX - minX + 1),
            static_cast<int>(maxY - minY + 1),
            cells,
            errorMessage)) {
            FileDialog::showError(errorMessage);
        }

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

        for (const std::string& droppedPath : FileDrop::takeDroppedPaths()) {
            importPatternFromPath(droppedPath);
        }

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
                    for (const SelectionMask::Cell& cell : selection.cells()) {
                        editHistory.setAlive(board, cell.x, cell.y, false);
                    }
                } else {
                    board.forEachAliveCellInRect(selection.minX(), selection.minY(),
                        selection.maxX() + 1, selection.maxY() + 1,
                        [&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
                            editHistory.setAlive(board, x, y, false);
                        });
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
            selectedPatternIndex = 0;
            shapeDragActive = false;
            cellStrokeHasLastCell = false;
        }
        if (f9 && !previousF9) {
            std::string report;
            const bool passed = LifeStepSelfTest::run(report);
            MessageBoxA(GetMainWindowHandle(), report.c_str(),
                passed ? "Life step self-test: PASS" : "Life step self-test: FAILED",
                MB_OK | (passed ? MB_ICONINFORMATION : MB_ICONERROR));
            resetTimingAfterDialog();
        }
        if (escape) {
            if (selectedPatternIndex != 0) { selectedPatternIndex = 0; patternRotation = 0; }
            shapeDragActive = false;
            pasteMode = false;
            selectionMode = false;
            selection.clear();
        }

        if (paused && undoShortcut && !previousUndoShortcut && !cellStrokeActive && !shapeDragActive) editHistory.undo(board);
        if (paused && redoShortcut && !previousRedoShortcut && !cellStrokeActive && !shapeDragActive) editHistory.redo(board);
        if (saveShortcut && !previousSaveShortcut) saveWithDialog();
        if (loadShortcut && !previousLoadShortcut) loadWithDialog();

        if (pageUp && !previousPageUp && simulationSpeedIndex + 1 < SimulationSpeeds.size()) { ++simulationSpeedIndex; simulationAccumulator = 0.0; }
        if (pageDown && !previousPageDown && simulationSpeedIndex > 0) { --simulationSpeedIndex; simulationAccumulator = 0.0; }
        if (p && !previousP) {
            panelTab = PanelTab::Pattern;
            selectionMode = false;
            selection.clear();
            shapeDragActive = false;
            if (shift) selectedPatternIndex = (selectedPatternIndex + PatternLibrary::size() - 1) % PatternLibrary::size();
            else selectedPatternIndex = (selectedPatternIndex + 1) % PatternLibrary::size();
            patternRotation = 0;
            if (selectedPatternIndex != 0) {
                toolCategory = PatternLibrary::at(selectedPatternIndex).category;
                patternListScroll.ensurePatternVisible(selectedPatternIndex);
            }
        }
        if (q && !previousQ) {
            if (pasteMode && clipboard.hasData()) clipboard.rotateCounterClockwise();
            else if (selectedPatternIndex != 0) patternRotation = (patternRotation + 3) & 3;
        }
        if (e && !previousE) {
            if (pasteMode && clipboard.hasData()) clipboard.rotateClockwise();
            else if (selectedPatternIndex != 0) patternRotation = (patternRotation + 1) & 3;
        }
        if (pasteMode && clipboard.hasData() && h && !previousH) clipboard.flipHorizontal();
        if (pasteMode && clipboard.hasData() && j && !previousJ) clipboard.flipVertical();

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

        // Make the current board interaction visible at a glance.
        // Selection uses a crosshair; middle-button camera movement uses the
        // standard four-way move cursor. Other tools keep the normal arrow.
        HCURSOR desiredCursor = LoadCursor(nullptr, IDC_ARROW);
        if (mouseOnBoard) {
            if (middle) desiredCursor = LoadCursor(nullptr, IDC_SIZEALL);
            else if (paused && selectionMode) desiredCursor = LoadCursor(nullptr, IDC_CROSS);
        }
        SetCursor(desiredCursor);

        if (mouseOnPanel && panelTab == PanelTab::Pattern &&
            inRect(mouseX, mouseY, PanelContentX, PatternListY, WindowWidth - PanelPadding, PatternListBottom)) {
            if (userPatternCategory) {
                const int maxOffset = std::max(0, static_cast<int>(userPatterns.size()) - PatternListScroll::VisibleRows);
                userPatternScrollOffset = std::clamp(userPatternScrollOffset - wheel, 0, maxOffset);
            } else if (importPatternCategory) {
                const int importRows = static_cast<int>(importPatterns.size()) + 1;
                const int maxOffset = std::max(0, importRows - PatternListScroll::VisibleRows);
                importPatternScrollOffset = std::clamp(importPatternScrollOffset - wheel, 0, maxOffset);
            } else if (favoritePatternCategory) {
                const int count = static_cast<int>(favoritePatternIndices().size());
                const int maxOffset = std::max(0, count - PatternListScroll::VisibleRows);
                favoritePatternScrollOffset = std::clamp(favoritePatternScrollOffset - wheel, 0, maxOffset);
            } else patternListScroll.scroll(toolCategory, wheel);
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
                int leftX = 0;
                int top = 0;
                int width = 136;
                if (i < 4) {
                    const int column = static_cast<int>(i % 2), row = static_cast<int>(i / 2);
                    leftX = PanelContentX + column * 144;
                    top = 86 + row * 36;
                } else {
                    constexpr int thirdGap = 4;
                    constexpr int thirdWidth = (PanelContentWidth - thirdGap * 3) / 4;
                    leftX = PanelContentX;
                    top = 86 + 2 * 36;
                    width = thirdWidth;
                }
                if (inRect(mouseX, mouseY, leftX, top, leftX + width, top + 28)) {
                    selectionMode = false;
                    selection.clear();
                    userPatternCategory = false;
                    importPatternCategory = false;
                    favoritePatternCategory = false;
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
                constexpr int thirdGap = 4;
                constexpr int thirdWidth = (PanelContentWidth - thirdGap * 3) / 4;
                const int top = 86 + 2 * 36;
                const int userLeft = PanelContentX + thirdWidth + thirdGap;
                const int importLeft = PanelContentX + (thirdWidth + thirdGap) * 2;
                const int favoriteLeft = PanelContentX + (thirdWidth + thirdGap) * 3;

                if (inRect(mouseX, mouseY, userLeft, top, userLeft + thirdWidth, top + 28)) {
                    userPatternCategory = true;
                    importPatternCategory = false;
                    favoritePatternCategory = false;
                    patternRotation = 0;
                    selectedPatternIndex = userPatterns.size() > 0 ? PatternLibrary::size() : 0;
                    rememberedPatternIndex = selectedPatternIndex;
                    rememberedPatternRotation = 0;
                    handled = true;
                } else if (inRect(mouseX, mouseY, importLeft, top, importLeft + thirdWidth, top + 28)) {
                    userPatternCategory = false;
                    importPatternCategory = true;
                    favoritePatternCategory = false;
                    patternRotation = 0;
                    selectedPatternIndex = importPatterns.size() > 0
                        ? PatternLibrary::size() + userPatterns.size()
                        : 0;
                    rememberedPatternIndex = selectedPatternIndex;
                    rememberedPatternRotation = 0;
                    handled = true;
                } else if (inRect(mouseX, mouseY, favoriteLeft, top, favoriteLeft + thirdWidth, top + 28)) {
                    userPatternCategory = false;
                    importPatternCategory = false;
                    favoritePatternCategory = true;
                    patternRotation = 0;
                    const std::vector<std::size_t> favorites = favoritePatternIndices();
                    selectedPatternIndex = favorites.empty() ? 0 : favorites.front();
                    favoritePatternScrollOffset = 0;
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

            if (!handled && panelTab == PanelTab::Pattern && importPatternCategory) {
                const int importRows = static_cast<int>(importPatterns.size()) + 1;
                for (int logicalRow = 0; logicalRow < importRows; ++logicalRow) {
                    const int row = logicalRow - importPatternScrollOffset;
                    if (row < 0 || row >= PatternListScroll::VisibleRows) continue;
                    const int top = PatternListY + row * PatternRowHeight;
                    if (!inRect(mouseX, mouseY, PanelContentX, top, WindowWidth - PanelPadding, top + 24)) continue;

                    if (logicalRow == 0) {
                        importPatternWithDialog();
                    } else {
                        const std::size_t importIndex = static_cast<std::size_t>(logicalRow - 1);
                        selectedPatternIndex = PatternLibrary::size() + userPatterns.size() + importIndex;
                        patternRotation = 0;
                        rememberedPatternIndex = selectedPatternIndex;
                        rememberedPatternRotation = 0;
                    }
                    handled = true;
                    break;
                }
            }

            if (!handled && panelTab == PanelTab::Pattern && favoritePatternCategory) {
                const std::vector<std::size_t> favorites = favoritePatternIndices();
                for (std::size_t i = 0; i < favorites.size(); ++i) {
                    const int row = static_cast<int>(i) - favoritePatternScrollOffset;
                    if (row < 0 || row >= PatternListScroll::VisibleRows) continue;
                    const int top = PatternListY + row * PatternRowHeight;
                    if (inRect(mouseX, mouseY, PanelContentX, top, WindowWidth - PanelPadding, top + 24)) {
                        selectedPatternIndex = favorites[i];
                        patternRotation = 0;
                        rememberedPatternIndex = selectedPatternIndex;
                        rememberedPatternRotation = 0;
                        handled = true;
                        break;
                    }
                }
            }

            if (!handled && panelTab == PanelTab::Pattern &&
                !userPatternCategory && !importPatternCategory && !favoritePatternCategory) {
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

            if (!handled && panelTab == PanelTab::Pattern) {
                std::size_t managedIndex = 0;
                const bool canManage = selectedUserPattern(managedIndex) || selectedImportPattern(managedIndex);
                if (canManage && inRect(
                        mouseX, mouseY,
                        PatternRenameX, PatternManageY,
                        PatternRenameX + PatternManageButtonWidth, PatternManageY + PatternManageHeight)) {
                    renameSelectedPattern();
                    handled = true;
                } else if (canManage && inRect(
                        mouseX, mouseY,
                        PatternDeleteX, PatternManageY,
                        PatternDeleteX + PatternManageButtonWidth, PatternManageY + PatternManageHeight)) {
                    deleteSelectedPattern();
                    handled = true;
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

        if (paused && selectionMode && selection.dragging() && leftReleased) {
            selection.finish(board, false);
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

        for (int i = 0; i < 3; ++i) {
            const int leftX = PanelContentX + i * (PanelTabWidth + PanelTabGap);
            const bool active = static_cast<int>(panelTab) == i;
            DrawBox(leftX, PanelTabY, leftX + PanelTabWidth, PanelTabY + PanelTabHeight, active ? selected : section, TRUE);
            DrawString(leftX + 10, PanelTabY + 7, PanelTabLabels[i], active ? text : muted);
        }

        if (panelTab == PanelTab::Draw) for (std::size_t i = 0; i < ShapeTools.size(); ++i) {
            const int leftX = ToolToolbarX + static_cast<int>(i) * (ToolButtonSize + ToolButtonGap);
            const bool isSelected = selectedPatternIndex == 0 && shapeTool == ShapeTools[i];
            ToolbarIcons::drawButton(mouseX, mouseY, leftX, ToolButtonY, ToolButtonSize, ShapeIcons[i], isSelected, true);
        }

        if (panelTab == PanelTab::Pattern) for (std::size_t i = 0; i < ToolCategories.size(); ++i) {
            int leftX = 0;
            int top = 0;
            int width = 136;
            if (i < 4) {
                const int column = static_cast<int>(i % 2), row = static_cast<int>(i / 2);
                leftX = PanelContentX + column * 144;
                top = 86 + row * 36;
            } else {
                constexpr int thirdGap = 6;
                constexpr int thirdWidth = (PanelContentWidth - thirdGap * 2) / 3;
                leftX = PanelContentX;
                top = 86 + 2 * 36;
                width = thirdWidth;
            }
            DrawBox(leftX, top, leftX + width, top + 28,
                    !userPatternCategory && !importPatternCategory && selectedPatternIndex != 0 && toolCategory == ToolCategories[i]
                        ? selected : section, TRUE);
            DrawString(leftX + 8, top + 6, PatternLibrary::categoryName(ToolCategories[i]), text);
        }

        if (panelTab == PanelTab::Pattern) {
            constexpr int thirdGap = 6;
            constexpr int thirdWidth = (PanelContentWidth - thirdGap * 2) / 3;
            const int top = 86 + 2 * 36;
            const int userLeft = PanelContentX + thirdWidth + thirdGap;
            const int importLeft = PanelContentX + (thirdWidth + thirdGap) * 2;
            DrawBox(userLeft, top, userLeft + thirdWidth, top + 28, userPatternCategory ? selected : section, TRUE);
            DrawString(userLeft + 8, top + 6, "User", text);
            DrawBox(importLeft, top, importLeft + thirdWidth, top + 28, importPatternCategory ? selected : section, TRUE);
            DrawString(importLeft + 8, top + 6, "Import", text);
        }

        const int scrollOffset = userPatternCategory
            ? userPatternScrollOffset
            : (importPatternCategory ? importPatternScrollOffset : patternListScroll.offset(toolCategory));
        int categoryRow = 0;
        if (panelTab == PanelTab::Pattern && !userPatternCategory && !importPatternCategory) for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {
            const LifePattern& pattern = PatternLibrary::at(i);
            if (pattern.category != toolCategory) continue;
            if (categoryRow >= scrollOffset && categoryRow < scrollOffset + PatternListScroll::VisibleRows) {
                const int top = PatternListY + (categoryRow - scrollOffset) * PatternRowHeight;
                DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24, selectedPatternIndex == i ? selected : section, TRUE);
                DrawString(PanelContentX + 8, top + 5, pattern.name, text);
            }
            ++categoryRow;
        }

        if (panelTab == PanelTab::Pattern && userPatternCategory) {
            for (std::size_t i = 0; i < userPatterns.size(); ++i) {
                const int row = static_cast<int>(i) - userPatternScrollOffset;
                if (row < 0 || row >= PatternListScroll::VisibleRows) continue;
                const int top = PatternListY + row * PatternRowHeight;
                const std::size_t encoded = PatternLibrary::size() + i;
                DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24, selectedPatternIndex == encoded ? selected : section, TRUE);
                DrawString(PanelContentX + 8, top + 5, userPatterns.at(i).name.c_str(), text);
            }
        }

        if (panelTab == PanelTab::Pattern && importPatternCategory) {
            const int importRows = static_cast<int>(importPatterns.size()) + 1;
            for (int logicalRow = 0; logicalRow < importRows; ++logicalRow) {
                const int row = logicalRow - importPatternScrollOffset;
                if (row < 0 || row >= PatternListScroll::VisibleRows) continue;
                const int top = PatternListY + row * PatternRowHeight;
                if (logicalRow == 0) {
                    DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24, section, TRUE);
                    DrawString(PanelContentX + 8, top + 5, "+ IMPORT RLE...", text);
                    continue;
                }

                const std::size_t importIndex = static_cast<std::size_t>(logicalRow - 1);
                const std::size_t encoded = PatternLibrary::size() + userPatterns.size() + importIndex;
                DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24,
                        selectedPatternIndex == encoded ? selected : section, TRUE);
                DrawString(PanelContentX + 8, top + 5, importPatterns.at(importIndex).name.c_str(), text);
            }
        }

        if (panelTab == PanelTab::Pattern && importPatternCategory) {
            const int importPatternCount = static_cast<int>(importPatterns.size());
            const int importRows = importPatternCount + 1; // + IMPORT RLE... action row
            if (importRows > PatternListScroll::VisibleRows && importPatternCount > 0) {
                const int firstVisible = std::max(1, importPatternScrollOffset);
                const int lastVisible = std::min(
                    importPatternCount,
                    importPatternScrollOffset + PatternListScroll::VisibleRows - 1);
                DrawFormatString(WindowWidth - 122, PatternListBottom + 4, muted,
                                 "%d-%d / %d", firstVisible, lastVisible, importPatternCount);
            }
        } else {
            const int patternCount = userPatternCategory
                ? static_cast<int>(userPatterns.size())
                : patternListScroll.count(toolCategory);
            if (panelTab == PanelTab::Pattern && patternCount > PatternListScroll::VisibleRows) {
                const int firstVisible = scrollOffset + 1;
                const int lastVisible = std::min(scrollOffset + PatternListScroll::VisibleRows, patternCount);
                DrawFormatString(WindowWidth - 122, PatternListBottom + 4, muted,
                                 "%d-%d / %d", firstVisible, lastVisible, patternCount);
            }
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
        const char* panelToolName = panelTab == PanelTab::Edit ? "Edit" :
                                    (panelTab == PanelTab::Pattern ? "Pattern" : ShapeDrawing::toolName(shapeTool));
        DrawFormatString(PanelContentX, infoY + 242, text, "Tool        %s", panelToolName);

        if (panelTab == PanelTab::Edit) {
            const int fullLeft = PanelContentX, fullRight = WindowWidth - PanelPadding;
            const int row0 = 92, row1 = row0 + EditButtonHeight + EditButtonGap;
            const int row2 = row1 + EditButtonHeight + EditButtonGap;
            const int row3 = row2 + EditButtonHeight + EditButtonGap;
            const int row4 = row3 + EditButtonHeight + EditButtonGap;
            const int row5 = row4 + EditButtonHeight + EditButtonGap;
            const int halfGap = 8, halfWidth = (PanelContentWidth - halfGap) / 2;
            const int rightLeft = fullLeft + halfWidth + halfGap;
            auto editButton = [&](int left, int top, int right, const char* label, bool enabled, bool active = false) {
                DrawBox(left, top, right, top + EditButtonHeight, active ? selected : section, TRUE);
                DrawString(left + 10, top + 8, label, enabled ? text : muted);
            };
            editButton(fullLeft, row0, fullRight, "SELECT [V]", paused, selectionMode);
            editButton(fullLeft, row1, fullLeft + halfWidth, "MASK [M]", selection.active(), selection.active() && selection.liveOnly());
            editButton(rightLeft, row1, fullRight, "COPY", selection.active());
            editButton(fullLeft, row2, fullLeft + halfWidth, "CUT", selection.active());
            editButton(rightLeft, row2, fullRight, "PASTE", clipboard.hasData(), pasteMode);
            editButton(fullLeft, row3, fullLeft + halfWidth, "ROT L [Q]", clipboard.hasData());
            editButton(rightLeft, row3, fullRight, "ROT R [E]", clipboard.hasData());
            editButton(fullLeft, row4, fullLeft + halfWidth, "FLIP H [H]", clipboard.hasData());
            editButton(rightLeft, row4, fullRight, "FLIP V [J]", clipboard.hasData());
            editButton(fullLeft, row5, fullRight, "SAVE PATTERN", selection.active() && !selection.dragging());
        }

        if (panelTab == PanelTab::Pattern) {
            std::size_t managedIndex = 0;
            const bool canManage = selectedUserPattern(managedIndex) || selectedImportPattern(managedIndex);
            DrawBox(PatternRenameX, PatternManageY,
                    PatternRenameX + PatternManageButtonWidth, PatternManageY + PatternManageHeight,
                    canManage ? section : background, TRUE);
            DrawBox(PatternDeleteX, PatternManageY,
                    PatternDeleteX + PatternManageButtonWidth, PatternManageY + PatternManageHeight,
                    canManage ? section : background, TRUE);
            DrawString(PatternRenameX + 10, PatternManageY + 6, "RENAME", canManage ? text : muted);
            DrawString(PatternDeleteX + 10, PatternManageY + 6, "DELETE", canManage ? text : muted);

            DrawString(PanelContentX, RotationLabelY, "ROTATION", muted);
            const bool rotationEnabled = selectedPatternIndex != 0;
            const unsigned int rotationButton = rotationEnabled ? section : background;
            DrawBox(RotationLeftX, RotationY, RotationLeftX + RotationButtonWidth, RotationY + RotationHeight, rotationButton, TRUE);
            DrawBox(RotationValueX, RotationY, RotationValueX + RotationValueWidth, RotationY + RotationHeight, section, TRUE);
            DrawBox(RotationRightX, RotationY, RotationRightX + RotationButtonWidth, RotationY + RotationHeight, rotationButton, TRUE);
            DrawString(RotationLeftX + 16, RotationY + 6, "<", rotationEnabled ? text : muted);
            DrawFormatString(RotationValueX + 30, RotationY + 6, text, "R%d", patternRotation * 90);
            DrawString(RotationRightX + 17, RotationY + 6, ">", rotationEnabled ? text : muted);
        }

        const char* hoverHelp = nullptr;
        if (mouseOnPanel) {
            for (std::size_t i = 0; i < ActionIcons.size() && hoverHelp == nullptr; ++i) {
                const int leftX = ActionToolbarX + static_cast<int>(i) * (ActionButtonSize + ActionButtonGap);
                if (ToolbarIcons::hit(mouseX, mouseY, leftX, ActionButtonY, ActionButtonSize)) hoverHelp = ToolbarIcons::label(ActionIcons[i]);
            }
            if (panelTab == PanelTab::Draw) for (std::size_t i = 0; i < ShapeIcons.size() && hoverHelp == nullptr; ++i) {
                const int leftX = ToolToolbarX + static_cast<int>(i) * (ToolButtonSize + ToolButtonGap);
                if (ToolbarIcons::hit(mouseX, mouseY, leftX, ToolButtonY, ToolButtonSize)) hoverHelp = ToolbarIcons::label(ShapeIcons[i]);
            }
        }

        DrawString(PanelContentX, 952, paused ? "PAUSED" : "RUNNING", paused ? GetColor(255, 210, 90) : GetColor(120, 230, 140));
        const char* defaultHelp = pasteMode
            ? "Paste: LMB place / Q/E rotate / H/J flip / RMB or Esc cancel"
            : (selectionMode
                ? (selection.liveOnly() ? "Select: LMB drag / M: full rectangle" : "Select: LMB drag / M: live-cell mask")
                : "Shape: drag LMB add / RMB erase");
        DrawString(PanelContentX, 974, hoverHelp != nullptr ? hoverHelp : defaultHelp, muted);
        DrawString(PanelContentX, 996, "Ctrl+C/X/V clipboard, V select, M mask, Ctrl+Z/Y", muted);
        if (hoverHelp != nullptr) ToolbarIcons::drawTooltip(mouseX, mouseY, hoverHelp, WindowWidth, ScreenHeight);
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
        const auto afterFrame = Clock::now();
        if (nextFrameTime > afterFrame) std::this_thread::sleep_until(nextFrameTime);
        else nextFrameTime = afterFrame;
    }

    DxLib_End();
    return 0;
}
