#include "nowplaying/overlays/np_overlay_colorpicker.h"
#include "nowplaying/core/np_theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Predefined solid colors for backgrounds
static NpColorOption g_defaultColors[] = {
    {"Black", {0, 0, 0, 255}},
    {"Navy", {10, 20, 50, 255}},
    {"Forest", {10, 40, 20, 255}},
    {"Wine", {40, 10, 15, 255}},
    {"Purple", {30, 10, 40, 255}},
    {"Brown", {40, 25, 15, 255}},
    {"Slate", {30, 35, 40, 255}},
    {"Midnight", {15, 20, 35, 255}},
    {"Teal", {10, 35, 35, 255}},
    {"Crimson", {50, 10, 20, 255}},
    {"Indigo", {20, 10, 50, 255}},
    {"Olive", {35, 40, 15, 255}}
};

#define NUM_DEFAULT_COLORS (sizeof(g_defaultColors) / sizeof(g_defaultColors[0]))

void NpColorPickerOverlayInit(NpColorPickerOverlay *overlay) {
    memset(overlay, 0, sizeof(*overlay));
    overlay->visible = false;
    overlay->active = false;
    overlay->colorSelected = false;
    overlay->selectedIndex = 0;
    overlay->animAlpha = 0.0f;
    overlay->numColors = NUM_DEFAULT_COLORS;
    overlay->colors = g_defaultColors;
}

void NpColorPickerOverlayUpdate(NpColorPickerOverlay *overlay, const LlzInputState *input, float deltaTime) {
    if (!overlay) return;

    // Animate alpha
    float targetAlpha = overlay->visible ? 1.0f : 0.0f;
    overlay->animAlpha += (targetAlpha - overlay->animAlpha) * 10.0f * deltaTime;
    if (fabsf(overlay->animAlpha - targetAlpha) < 0.01f) {
        overlay->animAlpha = targetAlpha;
    }

    // Update active state - active while visible or still fading out
    overlay->active = overlay->visible || overlay->animAlpha > 0.01f;

    // Don't process input if not visible
    if (!overlay->visible) return;

    // Navigate with scroll (flipped direction) or buttons
    int delta = 0;
    if (input->scrollDelta != 0.0f) {
        // Flipped: positive scroll = next, negative = previous
        delta = (input->scrollDelta > 0) ? 1 : -1;
    }
    if (input->upPressed || input->button1Pressed) delta = -1;
    if (input->downPressed || input->button2Pressed) delta = 1;

    if (delta != 0) {
        overlay->selectedIndex += delta;
        if (overlay->selectedIndex < 0) overlay->selectedIndex = overlay->numColors - 1;
        if (overlay->selectedIndex >= overlay->numColors) overlay->selectedIndex = 0;
    }

    // Close on back button release (cancel - no color selected)
    if (input->backReleased) {
        overlay->colorSelected = false;
        overlay->visible = false;
    }

    // Select on enter/select - apply color and close
    if (input->selectPressed || input->tap) {
        overlay->colorSelected = true;
        overlay->visible = false;
    }
}

void NpColorPickerOverlayDraw(const NpColorPickerOverlay *overlay, const NpAlbumArtUIColors *uiColors) {
    if (!overlay || overlay->animAlpha <= 0.01f) return;

    int screenWidth = 800;
    int screenHeight = 480;

    // Determine accent color for UI elements
    Color accentColor;
    if (uiColors && uiColors->hasColors) {
        accentColor = uiColors->accent;
    } else {
        accentColor = NpThemeGetColor(NP_COLOR_ACCENT);
    }

    // Draw full-screen backdrop with selected color preview
    Color selectedBg = overlay->colors[overlay->selectedIndex].bgColor;
    selectedBg.a = (unsigned char)(200 * overlay->animAlpha);
    DrawRectangle(0, 0, screenWidth, screenHeight, selectedBg);

    // Semi-transparent overlay
    Color dimOverlay = {0, 0, 0, (unsigned char)(100 * overlay->animAlpha)};
    DrawRectangle(0, 0, screenWidth, screenHeight, dimOverlay);

    Font font = NpThemeGetFont();

    // Draw title at top
    const char *title = "Choose Background Color";
    float titleSize = 32.0f;
    Vector2 titleMeasure = MeasureTextEx(font, title, titleSize, 2.0f);
    float titleX = (screenWidth - titleMeasure.x) / 2.0f;
    float titleY = 30.0f;
    Color titleColor = {255, 255, 255, (unsigned char)(255 * overlay->animAlpha)};
    DrawTextEx(font, title, (Vector2){titleX, titleY}, titleSize, 2.0f, titleColor);

    // Draw horizontal color strip in center
    int swatchWidth = 100;
    int swatchHeight = 140;
    int swatchSpacing = 12;
    int visibleSwatches = 6;  // Show 6 at a time
    int stripWidth = visibleSwatches * swatchWidth + (visibleSwatches - 1) * swatchSpacing;
    int stripX = (screenWidth - stripWidth) / 2;
    int stripY = (screenHeight - swatchHeight) / 2 - 20;

    // Calculate which swatches to show (center on selected)
    int startIdx = overlay->selectedIndex - visibleSwatches / 2;

    for (int i = 0; i < visibleSwatches; i++) {
        int colorIdx = startIdx + i;
        // Wrap around
        while (colorIdx < 0) colorIdx += overlay->numColors;
        while (colorIdx >= overlay->numColors) colorIdx -= overlay->numColors;

        int x = stripX + i * (swatchWidth + swatchSpacing);
        int y = stripY;

        // Draw swatch
        Color swatchColor = overlay->colors[colorIdx].bgColor;
        swatchColor.a = (unsigned char)(255 * overlay->animAlpha);

        // Scale up the selected swatch
        bool isSelected = (colorIdx == overlay->selectedIndex);
        int drawWidth = swatchWidth;
        int drawHeight = swatchHeight;
        int drawX = x;
        int drawY = y;

        if (isSelected) {
            int grow = 16;
            drawWidth += grow * 2;
            drawHeight += grow * 2;
            drawX -= grow;
            drawY -= grow;
        }

        // Draw shadow for selected
        if (isSelected) {
            Color shadow = {0, 0, 0, (unsigned char)(100 * overlay->animAlpha)};
            DrawRectangle(drawX + 4, drawY + 4, drawWidth, drawHeight, shadow);
        }

        DrawRectangle(drawX, drawY, drawWidth, drawHeight, swatchColor);

        // Draw border - use contextual accent for selected
        Color borderColor;
        if (isSelected) {
            borderColor = ColorAlpha(accentColor, overlay->animAlpha);
            DrawRectangleLinesEx((Rectangle){drawX, drawY, drawWidth, drawHeight}, 3, borderColor);
        } else {
            borderColor = (Color){100, 100, 100, (unsigned char)(150 * overlay->animAlpha)};
            DrawRectangleLinesEx((Rectangle){drawX, drawY, drawWidth, drawHeight}, 1, borderColor);
        }

        // Draw color name below swatch
        const char *name = overlay->colors[colorIdx].name;
        float nameSize = isSelected ? 22.0f : 18.0f;
        Vector2 nameMeasure = MeasureTextEx(font, name, nameSize, 1.4f);
        float nameX = drawX + (drawWidth - nameMeasure.x) / 2.0f;
        float nameY = drawY + drawHeight + 8.0f;
        Color textColor = {255, 255, 255, (unsigned char)((isSelected ? 255 : 180) * overlay->animAlpha)};
        DrawTextEx(font, name, (Vector2){nameX, nameY}, nameSize, 1.4f, textColor);
    }

    // Draw navigation hint
    const char *hint = "Scroll to browse  -  Press to select  -  Back to cancel";
    float hintSize = 20.0f;
    Vector2 hintMeasure = MeasureTextEx(font, hint, hintSize, 1.4f);
    float hintX = (screenWidth - hintMeasure.x) / 2.0f;
    float hintY = screenHeight - 50.0f;
    Color hintColor = {200, 200, 200, (unsigned char)(180 * overlay->animAlpha)};
    DrawTextEx(font, hint, (Vector2){hintX, hintY}, hintSize, 1.4f, hintColor);

    // Draw index indicator
    char indexText[32];
    snprintf(indexText, sizeof(indexText), "%d / %d", overlay->selectedIndex + 1, overlay->numColors);
    float indexSize = 24.0f;
    Vector2 indexMeasure = MeasureTextEx(font, indexText, indexSize, 1.4f);
    float indexX = (screenWidth - indexMeasure.x) / 2.0f;
    float indexY = stripY + swatchHeight + 60.0f;
    Color indexColor = {255, 255, 255, (unsigned char)(220 * overlay->animAlpha)};
    DrawTextEx(font, indexText, (Vector2){indexX, indexY}, indexSize, 1.4f, indexColor);
}

void NpColorPickerOverlayShow(NpColorPickerOverlay *overlay) {
    if (!overlay) return;
    overlay->visible = true;
    overlay->active = true;
    overlay->colorSelected = false;
}

void NpColorPickerOverlayHide(NpColorPickerOverlay *overlay) {
    if (!overlay) return;
    overlay->visible = false;
    // active remains true until animation completes
}

bool NpColorPickerOverlayIsActive(const NpColorPickerOverlay *overlay) {
    if (!overlay) return false;
    return overlay->active;
}

const Color* NpColorPickerOverlayGetSelectedColor(const NpColorPickerOverlay *overlay) {
    if (!overlay || overlay->selectedIndex < 0 || overlay->selectedIndex >= overlay->numColors) {
        return NULL;
    }
    return &overlay->colors[overlay->selectedIndex].bgColor;
}

bool NpColorPickerOverlayWasColorSelected(const NpColorPickerOverlay *overlay) {
    return overlay && overlay->colorSelected;
}

void NpColorPickerOverlayShutdown(NpColorPickerOverlay *overlay) {
    if (!overlay) return;
    memset(overlay, 0, sizeof(*overlay));
}
