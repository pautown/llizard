#include "menu_theme_private.h"
#include "llz_sdk.h"
#include <stdio.h>

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT

void ThemeCardsDraw(int selected, Color dynamicAccent, Color complementary)
{
    Font font = MenuThemeFontsGetMenu();
    const MenuThemeColorPalette *colors = MenuThemeColorsGetPalette();

    int itemCount = MenuThemeGetItemCount();

    if (itemCount == 0) {
        DrawTextEx(font, "No plugins found",
                  (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, colors->textSecondary);
        DrawTextEx(font, "Place .so files in ./plugins",
                  (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, colors->textDim);
        return;
    }

    const char *selectedName = MenuThemeGetItemName(selected);
    const char *selectedDesc = MenuThemeGetItemDescription(selected);
    bool isFolder = MenuThemeIsItemFolder(selected);
    int folderCount = MenuThemeGetFolderPluginCount(selected);

    // Use folder color for folders
    Color itemAccent = isFolder ? colors->folder : dynamicAccent;

    // Large card dimensions - fills most of the screen
    float cardWidth = SCREEN_WIDTH - 80;
    float cardHeight = 280;
    float cardX = 40;
    float cardY = MENU_PADDING_TOP + 20;

    // Main card background with gradient
    Rectangle cardRect = {cardX, cardY, cardWidth, cardHeight};

    // Spotify-style gradient background on card
    Color gradientTop = ColorAlpha(itemAccent, 0.15f);
    Color gradientBottom = colors->cardBg;
    DrawRectangleGradientV((int)cardX, (int)cardY, (int)cardWidth, (int)cardHeight,
                           gradientTop, gradientBottom);
    DrawRectangleRoundedLines(cardRect, 0.05f, 8, ColorAlpha(itemAccent, 0.3f));

    // Large plugin icon/initial on the left side
    float iconSize = 160;
    float iconX = cardX + 40;
    float iconY = cardY + (cardHeight - iconSize) / 2;

    // Icon background circle
    DrawCircle((int)(iconX + iconSize / 2), (int)(iconY + iconSize / 2),
               iconSize / 2 + 4, ColorAlpha(itemAccent, 0.2f));
    DrawCircle((int)(iconX + iconSize / 2), (int)(iconY + iconSize / 2),
               iconSize / 2, colors->cardSelected);

    // Large initial letter (or folder icon)
    if (selectedName && selectedName[0]) {
        const char *iconChar = isFolder ? "F" : NULL;
        char initial[2] = {0};
        if (!iconChar) {
            initial[0] = selectedName[0];
            initial[1] = '\0';
            iconChar = initial;
        }
        float initialSize = iconSize * 0.6f;
        Vector2 initialDim = MeasureTextEx(font, iconChar, initialSize, 1);
        DrawTextEx(font, iconChar,
                  (Vector2){iconX + iconSize / 2 - initialDim.x / 2,
                           iconY + iconSize / 2 - initialDim.y / 2},
                  initialSize, 1, itemAccent);
    }

    // Plugin info on the right side
    float textX = iconX + iconSize + 40;

    // Large plugin/folder name
    if (selectedName) {
        DrawTextEx(font, selectedName, (Vector2){textX, cardY + 50}, 42, 2, colors->textPrimary);
    }

    // Description (or folder plugin count)
    if (isFolder) {
        char folderDesc[64];
        snprintf(folderDesc, sizeof(folderDesc), "%d plugin%s", folderCount, folderCount == 1 ? "" : "s");
        DrawTextEx(font, folderDesc, (Vector2){textX, cardY + 105}, 20, 1, colors->textSecondary);
    } else if (selectedDesc) {
        DrawTextEx(font, selectedDesc, (Vector2){textX, cardY + 105}, 20, 1, colors->textSecondary);
    }

    // Item index badge
    char indexStr[32];
    snprintf(indexStr, sizeof(indexStr), "%s %d of %d", isFolder ? "Folder" : "Plugin", selected + 1, itemCount);
    DrawTextEx(font, indexStr, (Vector2){textX, cardY + 150}, 16, 1, colors->textDim);

    // "Press select to launch/open" hint
    const char *actionHint = isFolder ? "Press SELECT to open" : "Press SELECT to launch";
    DrawTextEx(font, actionHint, (Vector2){textX, cardY + cardHeight - 60}, 18, 1, complementary);

    // Previous/Next preview cards (smaller, on sides)
    float previewWidth = 140;
    float previewHeight = 100;
    float previewY = cardY + cardHeight + 30;

    // Previous item preview (if exists)
    if (selected > 0) {
        const char *prevName = MenuThemeGetItemName(selected - 1);
        Rectangle prevRect = {40, previewY, previewWidth, previewHeight};
        DrawRectangleRounded(prevRect, 0.1f, 6, ColorAlpha(colors->cardBg, 0.6f));
        DrawRectangleRoundedLines(prevRect, 0.1f, 6, ColorAlpha(colors->cardBorder, 0.3f));

        // Arrow and name
        DrawTextEx(font, "◀", (Vector2){50, previewY + 15}, 24, 1, colors->textDim);
        if (prevName) {
            DrawTextEx(font, prevName, (Vector2){50, previewY + 50}, 16, 1, colors->textSecondary);
        }
    }

    // Next item preview (if exists)
    if (selected < itemCount - 1) {
        const char *nextName = MenuThemeGetItemName(selected + 1);
        float nextX = SCREEN_WIDTH - 40 - previewWidth;
        Rectangle nextRect = {nextX, previewY, previewWidth, previewHeight};
        DrawRectangleRounded(nextRect, 0.1f, 6, ColorAlpha(colors->cardBg, 0.6f));
        DrawRectangleRoundedLines(nextRect, 0.1f, 6, ColorAlpha(colors->cardBorder, 0.3f));

        // Arrow and name (right-aligned)
        Vector2 arrowSize = MeasureTextEx(font, "▶", 24, 1);
        DrawTextEx(font, "▶", (Vector2){nextX + previewWidth - arrowSize.x - 10, previewY + 15},
                   24, 1, colors->textDim);

        if (nextName) {
            Vector2 nameSize = MeasureTextEx(font, nextName, 16, 1);
            float nameX = nextX + previewWidth - nameSize.x - 10;
            DrawTextEx(font, nextName, (Vector2){nameX, previewY + 50}, 16, 1, colors->textSecondary);
        }
    }

    // Progress bar at bottom showing position
    float barWidth = SCREEN_WIDTH - 160;
    float barX = 80;
    float barY = SCREEN_HEIGHT - 30;
    float barHeight = 4;

    DrawRectangleRounded((Rectangle){barX, barY, barWidth, barHeight}, 0.5f, 4,
                         ColorAlpha(colors->cardBorder, 0.3f));

    // Progress indicator
    float progress = (float)selected / (float)(itemCount - 1 > 0 ? itemCount - 1 : 1);
    float indicatorWidth = barWidth / itemCount;
    float indicatorX = barX + progress * (barWidth - indicatorWidth);
    DrawRectangleRounded((Rectangle){indicatorX, barY, indicatorWidth, barHeight}, 0.5f, 4, itemAccent);
}
