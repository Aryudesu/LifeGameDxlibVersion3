#pragma once

#include "DxLib.h"

namespace ToolbarIcons {

enum class Icon { Undo, Redo, Save, Load, Cell, Line, Rectangle, Circle };

inline const char* label(Icon icon) noexcept {
    switch (icon) {
    case Icon::Undo: return "Undo (Ctrl+Z)";
    case Icon::Redo: return "Redo (Ctrl+Y / Ctrl+Shift+Z)";
    case Icon::Save: return "Save (Ctrl+S)";
    case Icon::Load: return "Load (Ctrl+L)";
    case Icon::Cell: return "Cell";
    case Icon::Line: return "Line (Shift: 45 deg snap)";
    case Icon::Rectangle: return "Rectangle (Shift: square)";
    case Icon::Circle: return "Circle";
    }
    return "";
}

inline bool hit(int mouseX, int mouseY, int x, int y, int size) noexcept {
    return mouseX >= x && mouseX < x + size && mouseY >= y && mouseY < y + size;
}

inline void draw(Icon icon, int x, int y, unsigned int color, unsigned int panelColor) {
    switch (icon) {
    case Icon::Undo:
    case Icon::Redo: {
        const bool redo = icon == Icon::Redo;
        const int s = redo ? 1 : -1;
        const int cx = x + 12, cy = y + 12;
        DrawCircle(cx, cy, 7, color, FALSE);
        DrawBox(redo ? cx - 8 : cx + 1, cy + 1, redo ? cx : cx + 9, cy + 9, panelColor, TRUE);
        const int tipX = cx + s * 8;
        DrawTriangle(tipX, cy - 6, tipX - s * 5, cy - 10, tipX - s * 5, cy - 2, color, TRUE);
        break;
    }
    case Icon::Save:
        DrawBox(x + 5, y + 4, x + 19, y + 20, color, FALSE);
        DrawBox(x + 8, y + 5, x + 16, y + 10, color, FALSE);
        DrawBox(x + 8, y + 14, x + 16, y + 19, color, FALSE);
        break;
    case Icon::Load:
        DrawLine(x + 5, y + 7, x + 9, y + 4, color);
        DrawLine(x + 9, y + 4, x + 14, y + 4, color);
        DrawLine(x + 14, y + 4, x + 17, y + 7, color);
        DrawBox(x + 4, y + 7, x + 20, y + 19, color, FALSE);
        DrawLine(x + 12, y + 9, x + 12, y + 16, color);
        DrawTriangle(x + 8, y + 13, x + 16, y + 13, x + 12, y + 18, color, TRUE);
        break;
    case Icon::Cell:
        DrawBox(x + 7, y + 7, x + 17, y + 17, color, TRUE);
        break;
    case Icon::Line:
        DrawLine(x + 5, y + 18, x + 19, y + 5, color, 2);
        break;
    case Icon::Rectangle:
        DrawBox(x + 5, y + 6, x + 19, y + 18, color, FALSE);
        break;
    case Icon::Circle:
        DrawCircle(x + 12, y + 12, 7, color, FALSE);
        break;
    }
}

inline void drawButton(int mouseX, int mouseY, int x, int y, int size, Icon icon,
                       bool selected, bool enabled) {
    const bool hover = hit(mouseX, mouseY, x, y, size);
    const unsigned int normal = GetColor(45, 48, 54);
    const unsigned int hovered = GetColor(60, 64, 72);
    const unsigned int selectedColor = GetColor(70, 105, 75);
    const unsigned int disabled = GetColor(105, 110, 115);
    const unsigned int foreground = enabled ? GetColor(235, 235, 235) : disabled;
    const unsigned int background = selected ? selectedColor : (hover && enabled ? hovered : normal);

    DrawBox(x, y, x + size - 1, y + size - 1, background, TRUE);
    draw(icon, x + (size - 24) / 2, y + (size - 24) / 2, foreground, background);
}

} // namespace ToolbarIcons
