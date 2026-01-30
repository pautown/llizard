#include "menu_theme_private.h"
#include "llz_sdk.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT

void ThemeGridDraw(int selected, float deltaTime)
{
    MenuThemeState *state = MenuThemeGetState();
    const MenuThemeGridColors *gridColors = MenuThemeColorsGetGridPalette();

    // Draw white background (override animated background)
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, gridColors->bgWhite);

    // Get iBrand font (lazy loaded)
    Font gridFont = MenuThemeFontsGetIBrand();

    int itemCount = MenuThemeGetItemCount();

    if (itemCount == 0) {
        DrawTextEx(gridFont, "No plugins found",
                  (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, gridColors->textSecondary);
        DrawTextEx(gridFont, "Place .so files in ./plugins",
                  (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, gridColors->textDim);
        return;
    }

    // Header with traffic light dots
    float dotY = 36;
    float dotSpacing = 24;
    float dotRadius = 8;
    DrawCircle(GRID_PADDING_X + 8, dotY, dotRadius, gridColors->appleRed);
    DrawCircle(GRID_PADDING_X + 8 + dotSpacing, dotY, dotRadius, gridColors->appleYellow);
    DrawCircle(GRID_PADDING_X + 8 + dotSpacing * 2, dotY, dotRadius, gridColors->appleGreen);

    // Show folder context if inside folder
    const char *headerText = "llizardOS";
    if (MenuThemeIsInsideFolder()) {
        headerText = LLZ_CATEGORY_NAMES[MenuThemeGetCurrentFolder()];
    }
    DrawTextEx(gridFont, headerText, (Vector2){GRID_PADDING_X + dotSpacing * 3 + 20, 24},
               32, 2, gridColors->textPrimary);

    // Subtle divider line
    DrawRectangle(GRID_PADDING_X, 68, SCREEN_WIDTH - GRID_PADDING_X * 2, 1, gridColors->border);

    // Calculate which row the selected item is in for vertical scrolling
    int selectedRow = selected / GRID_COLS;
    float targetScrollY = 0;
    float maxVisibleRows = (SCREEN_HEIGHT - GRID_PADDING_TOP - 20) / (GRID_TILE_HEIGHT + GRID_SPACING);

    // Scroll to keep selected row visible
    if (selectedRow >= maxVisibleRows) {
        targetScrollY = (selectedRow - maxVisibleRows + 1) * (GRID_TILE_HEIGHT + GRID_SPACING);
    }

    // Smooth scroll update
    float diff = targetScrollY - state->scroll.scrollOffset;
    state->scroll.scrollOffset += diff * 10.0f * deltaTime;
    if (fabsf(diff) < 1.0f) state->scroll.scrollOffset = targetScrollY;

    // Draw grid of tiles
    BeginScissorMode(0, GRID_PADDING_TOP - 10, SCREEN_WIDTH, SCREEN_HEIGHT - GRID_PADDING_TOP + 10);

    for (int i = 0; i < itemCount; i++) {
        const char *itemName = MenuThemeGetItemName(i);
        bool isFolder = MenuThemeIsItemFolder(i);

        int col = i % GRID_COLS;
        int row = i / GRID_COLS;

        float tileX = GRID_PADDING_X + col * (GRID_TILE_WIDTH + GRID_SPACING);
        float tileY = GRID_PADDING_TOP + row * (GRID_TILE_HEIGHT + GRID_SPACING) - state->scroll.scrollOffset;

        // Skip tiles outside visible area
        if (tileY < GRID_PADDING_TOP - GRID_TILE_HEIGHT - 20 || tileY > SCREEN_HEIGHT + 20) continue;

        bool isSelected = (i == selected);
        Rectangle tileRect = {tileX, tileY, GRID_TILE_WIDTH, GRID_TILE_HEIGHT};

        // Soft shadow for all tiles
        Rectangle shadowRect = {tileX + 2, tileY + 2, GRID_TILE_WIDTH, GRID_TILE_HEIGHT};
        DrawRectangleRounded(shadowRect, 0.12f, 8, ColorAlpha(BLACK, isSelected ? 0.12f : 0.06f));

        // Tile background - white with subtle tint when selected
        Color tileBg = isSelected ? gridColors->tileBg : gridColors->tileHover;
        DrawRectangleRounded(tileRect, 0.12f, 8, tileBg);

        // Selection color - blue for folders, orange for plugins
        Color selectionColor = isFolder ? gridColors->appleBlue : gridColors->appleOrange;

        // Border - colored when selected
        Color borderColor = isSelected ? selectionColor : gridColors->border;
        DrawRectangleRoundedLines(tileRect, 0.12f, 8, borderColor);

        // Selection indicator - left edge colored bar
        if (isSelected) {
            Rectangle accentBar = {tileX, tileY + 10, 4, GRID_TILE_HEIGHT - 20};
            DrawRectangleRounded(accentBar, 1.0f, 4, selectionColor);
        }

        // Icon circle on left side of tile - fades out when selected
        float iconRadius = 50;
        float iconX = tileX + 70;
        float iconY = tileY + GRID_TILE_HEIGHT / 2;

        // Use blue for folders, cycle traffic light colors for plugins
        Color iconColors[] = {gridColors->appleRed, gridColors->appleYellow, gridColors->appleGreen};
        Color iconColor = isFolder ? gridColors->appleBlue : iconColors[i % 3];

        // Fade out icon when selected
        float iconAlpha = isSelected ? 0.0f : 1.0f;
        if (iconAlpha > 0.0f) {
            Color iconBg = ColorAlpha(iconColor, 0.08f * iconAlpha);
            DrawCircle((int)iconX, (int)iconY, iconRadius, iconBg);
            DrawCircleLines((int)iconX, (int)iconY, iconRadius, ColorAlpha(iconColor, 0.4f * iconAlpha));

            // Initial letter (F for folders)
            const char *iconChar = isFolder ? "F" : NULL;
            char initial[2] = {0};
            if (!iconChar && itemName && itemName[0]) {
                initial[0] = itemName[0];
                initial[1] = '\0';
                iconChar = initial;
            }
            if (iconChar) {
                float initialSize = 40;
                Vector2 initialDim = MeasureTextEx(gridFont, iconChar, initialSize, 1);
                Color initialColor = ColorAlpha(iconColor, 0.7f * iconAlpha);
                DrawTextEx(gridFont, iconChar,
                          (Vector2){iconX - initialDim.x / 2, iconY - initialDim.y / 2},
                          initialSize, 1, initialColor);
            }
        }

        // Item name - bigger when selected, positioned differently
        float textX = isSelected ? (tileX + 30) : (iconX + iconRadius + 30);
        float maxTextWidth = isSelected ? (GRID_TILE_WIDTH - 60) : (GRID_TILE_WIDTH - (textX - tileX) - 20);

        Color nameColor = isSelected ? gridColors->textPrimary : gridColors->textSecondary;
        float nameSize = isSelected ? 36 : 28;
        Vector2 nameDim = itemName ? MeasureTextEx(gridFont, itemName, nameSize, 1) : (Vector2){0, 0};

        // Vertically center the name in the tile
        float nameY = tileY + (GRID_TILE_HEIGHT - nameDim.y) / 2;

        // Truncate if too long
        if (itemName && nameDim.x > maxTextWidth) {
            char truncName[32];
            int maxChars = (int)(maxTextWidth / (nameDim.x / strlen(itemName)));
            if (maxChars > 28) maxChars = 28;
            if (maxChars > 3) {
                strncpy(truncName, itemName, maxChars - 3);
                truncName[maxChars - 3] = '.';
                truncName[maxChars - 2] = '.';
                truncName[maxChars - 1] = '.';
                truncName[maxChars] = '\0';
                DrawTextEx(gridFont, truncName, (Vector2){textX, nameY}, nameSize, 1, nameColor);
            }
        } else if (itemName) {
            DrawTextEx(gridFont, itemName, (Vector2){textX, nameY}, nameSize, 1, nameColor);
        }

        // Index badge in corner - subtle
        char indexStr[16];
        snprintf(indexStr, sizeof(indexStr), "%d", i + 1);
        Vector2 indexSize = MeasureTextEx(gridFont, indexStr, 14, 1);
        float indexXPos = tileX + GRID_TILE_WIDTH - indexSize.x - 12;
        float indexYPos = tileY + GRID_TILE_HEIGHT - 24;
        DrawTextEx(gridFont, indexStr, (Vector2){indexXPos, indexYPos}, 14, 1, gridColors->textDim);
    }

    EndScissorMode();

    // Page indicator at bottom
    char pageStr[32];
    snprintf(pageStr, sizeof(pageStr), "%d of %d", selected + 1, itemCount);
    Vector2 pageSize = MeasureTextEx(gridFont, pageStr, 16, 1);
    DrawTextEx(gridFont, pageStr, (Vector2){(SCREEN_WIDTH - pageSize.x) / 2, SCREEN_HEIGHT - 30},
               16, 1, gridColors->textSecondary);
}
