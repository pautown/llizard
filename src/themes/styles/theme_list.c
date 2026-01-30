#include "menu_theme_private.h"
#include "llz_sdk.h"
#include <stdio.h>

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT
#define MENU_VISIBLE_AREA (SCREEN_HEIGHT - MENU_PADDING_TOP)

void ThemeListDraw(int selected, float deltaTime,
                   Color dynamicAccent, Color dynamicAccentDim)
{
    Font font = MenuThemeFontsGetMenu();
    const MenuThemeColorPalette *colors = MenuThemeColorsGetPalette();
    MenuThemeState *state = MenuThemeGetState();

    int itemCount = MenuThemeGetItemCount();

    if (itemCount == 0) {
        if (MenuThemeIsInsideFolder()) {
            DrawTextEx(font, "Folder is empty",
                      (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, colors->textSecondary);
        } else {
            DrawTextEx(font, "No plugins found",
                      (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, colors->textSecondary);
            DrawTextEx(font, "Place .so files in ./plugins",
                      (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, colors->textDim);
        }
        return;
    }

    // Update scroll target and animation
    state->scroll.targetScrollOffset = MenuThemeScrollCalculateTarget(
        selected, itemCount, state->scroll.targetScrollOffset);
    MenuThemeScrollUpdate(&state->scroll, deltaTime);

    // Calculate scroll indicators
    float itemTotalHeight = MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
    float totalListHeight = itemCount * itemTotalHeight;
    float maxScroll = totalListHeight - MENU_VISIBLE_AREA;
    if (maxScroll < 0) maxScroll = 0;

    bool canScrollUp = state->scroll.scrollOffset > 1.0f;
    bool canScrollDown = state->scroll.scrollOffset < maxScroll - 1.0f;

    // Clipping region for list
    BeginScissorMode(0, MENU_PADDING_TOP, SCREEN_WIDTH, (int)MENU_VISIBLE_AREA);

    // Draw items
    for (int i = 0; i < itemCount; ++i) {
        float itemY = MENU_PADDING_TOP + i * itemTotalHeight - state->scroll.scrollOffset;

        // Skip items outside visible area
        if (itemY < MENU_PADDING_TOP - MENU_ITEM_HEIGHT || itemY > SCREEN_HEIGHT) continue;

        bool isSelected = (i == selected);
        float cardX = MENU_PADDING_X - 12;
        float cardWidth = SCREEN_WIDTH - (MENU_PADDING_X - 12) * 2;

        const char *name = MenuThemeGetItemName(i);
        const char *desc = MenuThemeGetItemDescription(i);
        bool isFolder = MenuThemeIsItemFolder(i);
        int folderCount = MenuThemeGetFolderPluginCount(i);

        ThemeItemDraw(cardX, itemY, cardWidth, MENU_ITEM_HEIGHT,
                      name, desc, isFolder, isSelected, folderCount,
                      dynamicAccent, dynamicAccentDim);
    }

    EndScissorMode();

    // Scroll indicators
    if (canScrollUp) {
        for (int i = 0; i < 30; i++) {
            float alpha = (30 - i) / 30.0f * 0.8f;
            Color fade = ColorAlpha(colors->bgDark, alpha);
            DrawRectangle(0, MENU_PADDING_TOP + i, SCREEN_WIDTH, 1, fade);
        }
        DrawTextEx(font, "▲", (Vector2){SCREEN_WIDTH / 2 - 6, MENU_PADDING_TOP + 4},
                   14, 1, ColorAlpha(colors->textDim, 0.6f));
    }

    if (canScrollDown) {
        int bottomY = MENU_PADDING_TOP + (int)MENU_VISIBLE_AREA;
        for (int i = 0; i < 30; i++) {
            float alpha = i / 30.0f * 0.8f;
            Color fade = ColorAlpha(colors->bgDark, alpha);
            DrawRectangle(0, bottomY - 30 + i, SCREEN_WIDTH, 1, fade);
        }
        DrawTextEx(font, "▼", (Vector2){SCREEN_WIDTH / 2 - 6, bottomY - 18},
                   14, 1, ColorAlpha(colors->textDim, 0.6f));
    }

    // Selection counter at bottom
    char counterStr[64];
    if (MenuThemeIsInsideFolder()) {
        snprintf(counterStr, sizeof(counterStr), "%s: %d of %d",
                 LLZ_CATEGORY_NAMES[MenuThemeGetCurrentFolder()], selected + 1, itemCount);
    } else {
        snprintf(counterStr, sizeof(counterStr), "%d of %d", selected + 1, itemCount);
    }
    Vector2 counterSize = MeasureTextEx(font, counterStr, 16, 1);
    DrawTextEx(font, counterStr,
              (Vector2){SCREEN_WIDTH - counterSize.x - MENU_PADDING_X, SCREEN_HEIGHT - 28},
              16, 1, colors->textDim);
}
