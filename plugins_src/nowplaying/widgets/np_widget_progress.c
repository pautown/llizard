#include "nowplaying/widgets/np_widget_progress.h"
#include "nowplaying/core/np_theme.h"
#include <stddef.h>

void NpProgressInit(NpProgressBar *bar, Rectangle bounds) {
    bar->bounds = bounds;
    bar->value = 0.0f;
    bar->roundness = 0.5f;
    bar->showThumb = false;
}

void NpProgressSetValue(NpProgressBar *bar, float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    bar->value = value;
}

void NpProgressDrawWithColors(const NpProgressBar *bar, const Color *customTrackColor, const Color *customFillColor, const Color *customThumbColor) {
    // Use custom colors if provided, otherwise fall back to theme
    Color trackColor = customTrackColor ? *customTrackColor : NpThemeGetColor(NP_COLOR_PANEL_HOVER);
    Color fillColor = customFillColor ? *customFillColor : NpThemeGetColor(NP_COLOR_ACCENT);
    Color thumbColor = customThumbColor ? *customThumbColor : NpThemeGetColor(NP_COLOR_ACCENT);

    // Draw track
    DrawRectangleRounded(bar->bounds, bar->roundness, 8, trackColor);

    // Draw fill
    if (bar->value > 0.0f) {
        Rectangle fill = bar->bounds;
        fill.width = bar->bounds.width * bar->value;
        DrawRectangleRounded(fill, bar->roundness, 8, fillColor);
    }

    // Draw thumb if enabled
    if (bar->showThumb) {
        float thumbX = bar->bounds.x + bar->bounds.width * bar->value;
        float thumbY = bar->bounds.y + bar->bounds.height * 0.5f;
        DrawCircle((int)thumbX, (int)thumbY, 8, thumbColor);
        DrawCircle((int)thumbX, (int)thumbY, 6, NpThemeGetColor(NP_COLOR_BG_DARK));
    }
}

void NpProgressDraw(const NpProgressBar *bar) {
    NpProgressDrawWithColors(bar, NULL, NULL, NULL);
}

bool NpProgressHandleScrub(NpProgressBar *bar, Vector2 mousePos, bool mousePressed, float *newValue) {
    // Expanded hit area for easier scrubbing
    Rectangle hitArea = {
        bar->bounds.x,
        bar->bounds.y - 12,
        bar->bounds.width,
        bar->bounds.height + 24
    };

    if (mousePressed && CheckCollisionPointRec(mousePos, hitArea)) {
        float normalizedX = (mousePos.x - bar->bounds.x) / bar->bounds.width;
        if (normalizedX < 0.0f) normalizedX = 0.0f;
        if (normalizedX > 1.0f) normalizedX = 1.0f;

        if (newValue) *newValue = normalizedX;
        return true;
    }
    return false;
}
