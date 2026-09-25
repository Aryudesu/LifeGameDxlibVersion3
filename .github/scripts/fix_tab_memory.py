from pathlib import Path

p = Path('LifeGameDxlibVersion3/Main.cpp')
s = p.read_text(encoding='utf-8')

decl = '    std::size_t selectedPatternIndex = 0;\n'
if 'rememberedPatternIndex' not in s:
    s = s.replace(decl, decl + '    std::size_t rememberedPatternIndex = 0;\n    int rememberedPatternRotation = 0;\n', 1)

# PATTERN操作でDRAWツールをCellへ書き換えない。
s = s.replace('            shapeTool = ShapeDrawing::Tool::Cell;\n            shapeDragActive = false;\n            if (shift)', '            shapeDragActive = false;\n            if (shift)', 1)
s = s.replace('                    shapeTool = ShapeDrawing::Tool::Cell;\n                    selectionMode = false;\n                    selection.clear();\n                    toolCategory = ToolCategories[i];', '                    selectionMode = false;\n                    selection.clear();\n                    toolCategory = ToolCategories[i];', 1)
s = s.replace('                            shapeTool = ShapeDrawing::Tool::Cell;\n                            selectedPatternIndex = i;', '                            selectedPatternIndex = i;', 1)

loop = s.index('            for (int i = 0; i < 3 && !handled; ++i) {')
start = s.index('                    panelTab = static_cast<PanelTab>(i);', loop)
marker = '                    handled = true;\n'
end = s.index(marker, start) + len(marker)
new = '''                    const PanelTab previousTab = panelTab;
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
'''
s = s[:start] + new + s[end:]

needle = '                    selectedPatternIndex = firstPatternInCategory(toolCategory);\n                    patternRotation = 0;\n'
s = s.replace(needle, needle + '                    rememberedPatternIndex = selectedPatternIndex;\n                    rememberedPatternRotation = patternRotation;\n', 1)
needle = '                            selectedPatternIndex = i;\n                            patternRotation = 0;\n'
s = s.replace(needle, needle + '                            rememberedPatternIndex = selectedPatternIndex;\n                            rememberedPatternRotation = patternRotation;\n', 1)

p.write_text(s, encoding='utf-8')
