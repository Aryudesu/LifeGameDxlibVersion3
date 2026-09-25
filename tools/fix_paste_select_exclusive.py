from pathlib import Path

p = Path('LifeGameDxlibVersion3/Main.cpp')
s = p.read_text(encoding='utf-8')
old = '''        if (v && !previousV && paused && !ctrl) {
            panelTab = PanelTab::Edit;
            selectionMode = !selectionMode;
            shapeDragActive = false;
            cellStrokeHasLastCell = false;
            selectedPatternIndex = 0;
            patternRotation = 0;
        }
'''
new = '''        if (v && !previousV && paused && !ctrl) {
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
'''
if old not in s:
    raise SystemExit('target V block not found')
p.write_text(s.replace(old, new, 1), encoding='utf-8')
