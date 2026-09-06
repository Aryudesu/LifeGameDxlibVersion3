#pragma once

#include "DxLib.h"

namespace ToolbarIcons {

enum class Icon { Undo, Redo, Save, Load, Cell, Line, Rectangle, Circle };

inline void draw(Icon icon, int x, int y, unsigned int color) {
    switch (icon) {
    case Icon::Undo:
    case Icon::Redo: {
        const bool redo = icon == Icon::Redo;
        const int s = redo ? 1 : -1;
        const int cx = x + 12, cy = y + 12;
        DrawCircle(cx, cy, 7, color, FALSE);
        DrawBox(redo ? cx - 8 : cx + 1, cy + 1, redo ? cx : cx + 9, cy + 9, GetColor(28, 30, 34), TRUE);
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
        DrawBox(x + 4, y + 8, x + 20, y + 19, color, FALSE);
        DrawLine(x + 5, y + 8, x + 9, y + 4, color);
        DrawLine(x + 9, y + 4, x + 14, y + 4, color);
        DrawLine(x + 14, y + 4, x + 17, y + 8, color);
        DrawLine(x + 12, y + 10, x + 12, y + 17, color);
        DrawTriangle(x + 8, y + 14, x + 16, y + 14, x + 12, y + 19, color, TRUE);
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

inline bool button(int mouseX, int mouseY, int x, int y, int size, Icon icon,
                   bool selected, bool enabled, bool pressed) {
    const bool hover = mouseX >= x && mouseX < x + size && mouseY >= y && mouseY < y + size;
    const unsigned int background = selected ? GetColor(70, 105, 75)
        : hover && enabled ? GetColor(60, 64, 72) : GetColor(45, 48, 54);
    const unsigned int foreground = enabled ? GetColor(235, 235, 235) : GetColor(105, 110, 115);
    DrawBox(x, y, x + size - 1, y + size - 1, background, TRUE);
    draw(icon, x + (size - 24) / 2, y + (size - 24) / 2, foreground);
    return hover && enabled && pressed;
}

} // namespace ToolbarIcons
