from pathlib import Path

path = Path('LifeGameDxlibVersion3/Main.cpp')
s = path.read_text(encoding='utf-8')

def rep(old, new):
    global s
    if old not in s:
        raise RuntimeError('anchor not found: ' + old[:100].replace('\n', '\\n'))
    s = s.replace(old, new, 1)

rep('constexpr int ToolButtonY = 44;\n', '''constexpr int PanelTabY = 44;
constexpr int PanelTabHeight = 30;
constexpr int PanelTabGap = 4;
constexpr int PanelTabWidth = (PanelContentWidth - PanelTabGap * 2) / 3;
constexpr int ToolButtonY = 92;
constexpr int EditButtonHeight = 32;
constexpr int EditButtonGap = 8;
''')
rep('constexpr std::array ToolCategories = {\n', '''enum class PanelTab { Draw, Pattern, Edit };
constexpr std::array<const char*, 3> PanelTabLabels = {"DRAW", "PATTERN", "EDIT"};

constexpr std::array ToolCategories = {
''')
rep('    int patternRotation = 0;\n', '    int patternRotation = 0;\n    PanelTab panelTab = PanelTab::Draw;\n')
rep('        if (v && !previousV && paused && !ctrl) {\n', '        if (v && !previousV && paused && !ctrl) {\n            panelTab = PanelTab::Edit;\n')
rep('        if (p && !previousP) {\n', '        if (p && !previousP) {\n            panelTab = PanelTab::Pattern;\n')
rep('        if (mouseOnPanel && inRect(mouseX, mouseY, PanelContentX, PatternListY, WindowWidth - PanelPadding, PatternListBottom)) {\n            patternListScroll.scroll(toolCategory, wheel);\n        }\n', '''        if (mouseOnPanel && panelTab == PanelTab::Pattern &&
            inRect(mouseX, mouseY, PanelContentX, PatternListY, WindowWidth - PanelPadding, PatternListBottom)) {
            patternListScroll.scroll(toolCategory, wheel);
        }
''')

rep('            for (std::size_t i = 0; i < ShapeTools.size() && !handled; ++i) {\n', '''            for (int i = 0; i < 3 && !handled; ++i) {
                const int leftX = PanelContentX + i * (PanelTabWidth + PanelTabGap);
                if (inRect(mouseX, mouseY, leftX, PanelTabY, leftX + PanelTabWidth, PanelTabY + PanelTabHeight)) {
                    panelTab = static_cast<PanelTab>(i);
                    if (panelTab == PanelTab::Draw) {
                        selectionMode = false; selection.clear(); pasteMode = false;
                        selectedPatternIndex = 0; patternRotation = 0;
                    } else if (panelTab == PanelTab::Pattern) {
                        selectionMode = false; selection.clear(); pasteMode = false;
                    } else {
                        selectedPatternIndex = 0; patternRotation = 0;
                        shapeDragActive = false; cellStrokeHasLastCell = false;
                    }
                    handled = true;
                }
            }

            if (panelTab == PanelTab::Draw) for (std::size_t i = 0; i < ShapeTools.size() && !handled; ++i) {
''')
rep('            for (std::size_t i = 0; i < ToolCategories.size() && !handled; ++i) {\n', '            if (panelTab == PanelTab::Pattern) for (std::size_t i = 0; i < ToolCategories.size() && !handled; ++i) {\n')
rep('            if (!handled) {\n                const int scrollOffset = patternListScroll.offset(toolCategory);\n', '            if (!handled && panelTab == PanelTab::Pattern) {\n                const int scrollOffset = patternListScroll.offset(toolCategory);\n')
rep('            if (!handled && selectedPatternIndex != 0) {\n', '            if (!handled && panelTab == PanelTab::Pattern && selectedPatternIndex != 0) {\n')

needle = '            if (!handled && panelTab == PanelTab::Pattern && selectedPatternIndex != 0) {'
start = s.index(needle)
end_marker = '        }\n\n        if (mouseOnBoard) {'
end = s.index(end_marker, start)
edit_ui = '''

            if (!handled && panelTab == PanelTab::Edit && paused) {
                const int fullLeft = PanelContentX, fullRight = WindowWidth - PanelPadding;
                const int row0 = 92, row1 = row0 + EditButtonHeight + EditButtonGap;
                const int row2 = row1 + EditButtonHeight + EditButtonGap;
                const int row3 = row2 + EditButtonHeight + EditButtonGap;
                const int row4 = row3 + EditButtonHeight + EditButtonGap;
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
                }
            }
'''
s = s[:end] + edit_ui + s[end:]

rep('        for (std::size_t i = 0; i < ShapeTools.size(); ++i) {\n', '''        for (int i = 0; i < 3; ++i) {
            const int leftX = PanelContentX + i * (PanelTabWidth + PanelTabGap);
            const bool active = static_cast<int>(panelTab) == i;
            DrawBox(leftX, PanelTabY, leftX + PanelTabWidth, PanelTabY + PanelTabHeight, active ? selected : section, TRUE);
            DrawString(leftX + 10, PanelTabY + 7, PanelTabLabels[i], active ? text : muted);
        }

        if (panelTab == PanelTab::Draw) for (std::size_t i = 0; i < ShapeTools.size(); ++i) {
''')
rep('        for (std::size_t i = 0; i < ToolCategories.size(); ++i) {\n', '        if (panelTab == PanelTab::Pattern) for (std::size_t i = 0; i < ToolCategories.size(); ++i) {\n')
rep('        const int scrollOffset = patternListScroll.offset(toolCategory);\n        int categoryRow = 0;\n        for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {\n', '        const int scrollOffset = patternListScroll.offset(toolCategory);\n        int categoryRow = 0;\n        if (panelTab == PanelTab::Pattern) for (std::size_t i = 1; i < PatternLibrary::size(); ++i) {\n')
rep('        if (patternCount > PatternListScroll::VisibleRows) {\n', '        if (panelTab == PanelTab::Pattern && patternCount > PatternListScroll::VisibleRows) {\n')
rep('        DrawFormatString(PanelContentX, infoY + 242, text, "Tool        %s",\n                         selectedPatternIndex == 0 ? ShapeDrawing::toolName(shapeTool) : "Pattern");\n', '''        const char* panelToolName = panelTab == PanelTab::Edit ? "Edit" :
                                    (panelTab == PanelTab::Pattern ? "Pattern" : ShapeDrawing::toolName(shapeTool));
        DrawFormatString(PanelContentX, infoY + 242, text, "Tool        %s", panelToolName);
''')

rep('        DrawString(PanelContentX, 876, "ROTATION", muted);\n', '''        if (panelTab == PanelTab::Edit) {
            const int fullLeft = PanelContentX, fullRight = WindowWidth - PanelPadding;
            const int row0 = 92, row1 = row0 + EditButtonHeight + EditButtonGap;
            const int row2 = row1 + EditButtonHeight + EditButtonGap;
            const int row3 = row2 + EditButtonHeight + EditButtonGap;
            const int row4 = row3 + EditButtonHeight + EditButtonGap;
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
        }

        if (panelTab == PanelTab::Pattern) {
        DrawString(PanelContentX, 876, "ROTATION", muted);
''')
rep('        DrawString(RotationRightX + 17, RotationY + 6, ">", rotationEnabled ? text : muted);\n\n        const char* hoverHelp', '        DrawString(RotationRightX + 17, RotationY + 6, ">", rotationEnabled ? text : muted);\n        }\n\n        const char* hoverHelp')
rep('            for (std::size_t i = 0; i < ShapeIcons.size() && hoverHelp == nullptr; ++i) {\n', '            if (panelTab == PanelTab::Draw) for (std::size_t i = 0; i < ShapeIcons.size() && hoverHelp == nullptr; ++i) {\n')

path.write_text(s, encoding='utf-8')
print('panel refactor applied')
