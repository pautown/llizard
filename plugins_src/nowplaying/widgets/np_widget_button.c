#include "nowplaying/widgets/np_widget_button.h"
#include "nowplaying/core/np_theme.h"

void NpButtonInit(NpButton *btn, Rectangle bounds, const char *label) {
    btn->bounds = bounds;
    btn->label = label;
    btn->isActive = false;
    btn->isHovered = false;
    btn->roundness = 0.4f;
}

bool NpButtonUpdate(NpButton *btn, Vector2 mousePos, bool mousePressed, bool mouseJustPressed) {
    btn->isHovered = CheckCollisionPointRec(mousePos, btn->bounds);

    if (btn->isHovered && mouseJustPressed) {
        return true;  // Button was clicked
    }
    return false;
}

void NpButtonDraw(const NpButton *btn) {
    Color bg = NpThemeGetColor(NP_COLOR_PANEL);
    Color text = NpThemeGetColor(NP_COLOR_TEXT_PRIMARY);

    if (btn->isActive) {
        bg = NpThemeGetColor(NP_COLOR_ACCENT);
        text = NpThemeGetColor(NP_COLOR_BG_DARK);
    } else if (btn->isHovered) {
        bg = NpThemeGetColor(NP_COLOR_PANEL_HOVER);
    }

    NpButtonDrawWithColors(btn, bg, text);
}

void NpButtonDrawWithColors(const NpButton *btn, Color bg, Color text) {
    DrawRectangleRounded(btn->bounds, btn->roundness, 10, bg);

    if (btn->label) {
        float textWidth = NpThemeMeasureTextWidth(NP_TYPO_BUTTON, btn->label);
        float textHeight = NpThemeGetLineHeight(NP_TYPO_BUTTON);
        Vector2 textPos = {
            btn->bounds.x + (btn->bounds.width - textWidth) * 0.5f,
            btn->bounds.y + (btn->bounds.height - textHeight) * 0.5f
        };
        NpThemeDrawTextColored(NP_TYPO_BUTTON, btn->label, textPos, text);
    }
}
