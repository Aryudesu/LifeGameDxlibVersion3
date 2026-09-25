from pathlib import Path
p=Path('LifeGameDxlibVersion3/Main.cpp')
s=p.read_text(encoding='utf-8')
def rep(a,b):
    global s
    if a not in s: raise SystemExit('missing snippet: '+a[:100])
    s=s.replace(a,b,1)
rep('#include "ToolbarIcons.h"', '#include "ToolbarIcons.h"\n#include "TextInputDialog.h"\n#include "UserPatternLibrary.h"')
rep('#include <cstdint>\n#include <string>', '#include <cstdint>\n#include <limits>\n#include <string>\n#include <vector>')
rep('    EditHistory editHistory;\n    seedGlider(board);', '    EditHistory editHistory;\n    UserPatternLibrary userPatterns;\n    std::string userPatternLoadError;\n    if (!userPatterns.load(userPatternLoadError) && !userPatternLoadError.empty()) FileDialog::showError(userPatternLoadError);\n    seedGlider(board);')
rep('    PanelTab panelTab = PanelTab::Draw;\n    double simulationAccumulator', '    PanelTab panelTab = PanelTab::Draw;\n    bool userPatternCategory = false;\n    int userPatternScrollOffset = 0;\n    double simulationAccumulator')
rep('    auto saveWithDialog = [&]() {', '''    auto selectedLifePattern = [&]() -> LifePattern {
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
            FileDialog::showError("The selected area is too large to save as a pattern.");
            return;
        }
        std::vector<PatternCell> cells;
        board.forEachAliveCellInRect(minX, minY, maxX + 1, maxY + 1,
            [&](InfiniteLifeBoard::Coord x, InfiniteLifeBoard::Coord y) {
                cells.push_back({static_cast<int>(x - minX), static_cast<int>(y - minY)});
            });
        if (cells.empty()) { FileDialog::showError("The selection does not contain any live cells."); return; }
        std::string name;
        if (!TextInputDialog::show(GetMainWindowHandle(), "Save User Pattern", "Pattern name:", name)) {
            resetTimingAfterDialog();
            return;
        }
        std::string errorMessage;
        if (!userPatterns.save(name, static_cast<int>(maxX - minX + 1), static_cast<int>(maxY - minY + 1), cells, errorMessage))
            FileDialog::showError(errorMessage);
        resetTimingAfterDialog();
    };

    auto saveWithDialog = [&]() {''')
rep('''        if (mouseOnPanel && panelTab == PanelTab::Pattern &&
            inRect(mouseX, mouseY, PanelContentX, PatternListY, WindowWidth - PanelPadding, PatternListBottom)) {
            patternListScroll.scroll(toolCategory, wheel);
        }''', '''        if (mouseOnPanel && panelTab == PanelTab::Pattern &&
            inRect(mouseX, mouseY, PanelContentX, PatternListY, WindowWidth - PanelPadding, PatternListBottom)) {
            if (userPatternCategory) {
                const int maxOffset = std::max(0, static_cast<int>(userPatterns.size()) - PatternListScroll::VisibleRows);
                userPatternScrollOffset = std::clamp(userPatternScrollOffset - wheel, 0, maxOffset);
            } else patternListScroll.scroll(toolCategory, wheel);
        }''')
rep('''                    toolCategory = ToolCategories[i];
                    selectedPatternIndex = firstPatternInCategory(toolCategory);''', '''                    userPatternCategory = false;
                    toolCategory = ToolCategories[i];
                    selectedPatternIndex = firstPatternInCategory(toolCategory);''')
rep('''            if (!handled && panelTab == PanelTab::Pattern) {
                const int scrollOffset = patternListScroll.offset(toolCategory);''', '''            if (!handled && panelTab == PanelTab::Pattern) {
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
                const int scrollOffset = patternListScroll.offset(toolCategory);''')
# Input-side EDIT layout (first occurrence)
rep('''                const int row4 = row3 + EditButtonHeight + EditButtonGap;
                const int halfGap''', '''                const int row4 = row3 + EditButtonHeight + EditButtonGap;
                const int row5 = row4 + EditButtonHeight + EditButtonGap;
                const int halfGap''')
rep('''                } else if (inRect(mouseX, mouseY, rightLeft, row4, fullRight, row4 + EditButtonHeight)) {
                    if (clipboard.hasData()) clipboard.flipVertical(); handled = true;
                }''', '''                } else if (inRect(mouseX, mouseY, rightLeft, row4, fullRight, row4 + EditButtonHeight)) {
                    if (clipboard.hasData()) clipboard.flipVertical(); handled = true;
                } else if (inRect(mouseX, mouseY, fullLeft, row5, fullRight, row5 + EditButtonHeight)) {
                    if (selection.active() && !selection.dragging()) saveSelectionAsPattern();
                    handled = true;
                }''')
rep('                    const LifePattern& pattern = PatternLibrary::at(selectedPatternIndex);', '                    const LifePattern pattern = selectedLifePattern();')
rep('''            PatternPlacementPreview::draw(camera, PatternLibrary::at(selectedPatternIndex), previewX, previewY,
                                          patternRotation, BoardViewWidth, ScreenHeight);''', '''            const LifePattern pattern = selectedLifePattern();
            PatternPlacementPreview::draw(camera, pattern, previewX, previewY,
                                          patternRotation, BoardViewWidth, ScreenHeight);''')
rep('''            DrawBox(leftX, top, leftX + 136, top + 28, selectedPatternIndex != 0 && toolCategory == ToolCategories[i] ? selected : section, TRUE);''', '''            DrawBox(leftX, top, leftX + 136, top + 28, !userPatternCategory && selectedPatternIndex != 0 && toolCategory == ToolCategories[i] ? selected : section, TRUE);''')
rep('''        const int scrollOffset = patternListScroll.offset(toolCategory);
        int categoryRow = 0;
        if (panelTab == PanelTab::Pattern) for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {''', '''        if (panelTab == PanelTab::Pattern) {
            const int userLeft = PanelContentX + 144, userTop = 86 + 2 * 36;
            DrawBox(userLeft, userTop, userLeft + 136, userTop + 28, userPatternCategory ? selected : section, TRUE);
            DrawString(userLeft + 8, userTop + 6, "User", text);
        }

        const int scrollOffset = userPatternCategory ? userPatternScrollOffset : patternListScroll.offset(toolCategory);
        int categoryRow = 0;
        if (panelTab == PanelTab::Pattern && !userPatternCategory) for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {''')
rep('        const int patternCount = patternListScroll.count(toolCategory);', '''        if (panelTab == PanelTab::Pattern && userPatternCategory) {
            for (std::size_t i = 0; i < userPatterns.size(); ++i) {
                const int row = static_cast<int>(i) - userPatternScrollOffset;
                if (row < 0 || row >= PatternListScroll::VisibleRows) continue;
                const int top = PatternListY + row * PatternRowHeight;
                const std::size_t encoded = PatternLibrary::size() + i;
                DrawBox(PanelContentX, top, WindowWidth - PanelPadding, top + 24, selectedPatternIndex == encoded ? selected : section, TRUE);
                DrawString(PanelContentX + 8, top + 5, userPatterns.at(i).name.c_str(), text);
            }
        }

        const int patternCount = userPatternCategory ? static_cast<int>(userPatterns.size()) : patternListScroll.count(toolCategory);''')
# Draw-side EDIT layout (second occurrence remains)
rep('''            const int row4 = row3 + EditButtonHeight + EditButtonGap;
            const int halfGap''', '''            const int row4 = row3 + EditButtonHeight + EditButtonGap;
            const int row5 = row4 + EditButtonHeight + EditButtonGap;
            const int halfGap''')
rep('            editButton(rightLeft, row4, fullRight, "FLIP V [J]", clipboard.hasData());', '            editButton(rightLeft, row4, fullRight, "FLIP V [J]", clipboard.hasData());\n            editButton(fullLeft, row5, fullRight, "SAVE PATTERN", selection.active() && !selection.dragging());')
p.write_text(s,encoding='utf-8')
