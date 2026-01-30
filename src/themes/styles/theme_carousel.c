#include "menu_theme_private.h"
#include "llz_sdk.h"
#include <math.h>

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT
#define CAROUSEL_CENTER_Y (SCREEN_HEIGHT / 2 + 20)

void ThemeCarouselDraw(int selected, float deltaTime,
                       Color dynamicAccent, Color dynamicAccentDim)
{
    Font font = MenuThemeFontsGetMenu();
    const MenuThemeColorPalette *colors = MenuThemeColorsGetPalette();
    MenuThemeState *state = MenuThemeGetState();

    int itemCount = MenuThemeGetItemCount();

    if (itemCount == 0) {
        DrawTextEx(font, "No plugins found",
                  (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, colors->textSecondary);
        DrawTextEx(font, "Place .so files in ./plugins",
                  (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, colors->textDim);
        return;
    }

    // Update carousel scroll to center on selected item
    float itemSpacing = CAROUSEL_ITEM_WIDTH + CAROUSEL_SPACING;
    state->scroll.carouselTarget = selected * itemSpacing;
    MenuThemeScrollUpdateCarousel(&state->scroll, deltaTime);

    float centerX = SCREEN_WIDTH / 2.0f;

    // Draw items with perspective scaling
    for (int i = 0; i < itemCount; i++) {
        const char *itemName = MenuThemeGetItemName(i);
        bool isFolder = MenuThemeIsItemFolder(i);

        // Calculate horizontal position relative to center
        float itemCenterX = i * itemSpacing - state->scroll.carouselOffset + centerX;
        float distFromCenter = fabsf(itemCenterX - centerX);

        // Skip items too far off screen
        if (itemCenterX < -CAROUSEL_ITEM_WIDTH || itemCenterX > SCREEN_WIDTH + CAROUSEL_ITEM_WIDTH) continue;

        // Scale and alpha based on distance from center (Apple Music cover flow effect)
        float maxDist = SCREEN_WIDTH / 2.0f;
        float normalizedDist = fminf(distFromCenter / maxDist, 1.0f);

        // Center item is full size, others shrink
        float scale = 1.0f - normalizedDist * 0.35f;
        float alpha = 1.0f - normalizedDist * 0.6f;

        // 3D-ish perspective: items offset vertically as they move away
        float yOffset = normalizedDist * 30.0f;

        bool isSelected = (i == selected);

        // Calculate card dimensions with scale
        float cardWidth = CAROUSEL_ITEM_WIDTH * scale;
        float cardHeight = CAROUSEL_ITEM_HEIGHT * scale;
        float cardX = itemCenterX - cardWidth / 2.0f;
        float cardY = CAROUSEL_CENTER_Y - cardHeight / 2.0f + yOffset;

        Rectangle cardRect = {cardX, cardY, cardWidth, cardHeight};

        // Card background with depth shadow
        if (scale > 0.7f) {
            Color shadowColor = ColorAlpha(BLACK, 0.3f * alpha);
            Rectangle shadowRect = {cardX + 8, cardY + 8, cardWidth, cardHeight};
            DrawRectangleRounded(shadowRect, 0.12f, 8, shadowColor);
        }

        // Use folder color for folders
        Color itemAccent = isFolder ? colors->folder : dynamicAccent;
        Color itemAccentDim = isFolder ? ColorAlpha(colors->folder, 0.5f) : dynamicAccentDim;

        Color cardBg = isSelected ? colors->cardSelected : colors->cardBg;
        Color borderColor = isSelected ? itemAccent : colors->cardBorder;

        DrawRectangleRounded(cardRect, 0.12f, 8, ColorAlpha(cardBg, alpha));
        DrawRectangleRoundedLines(cardRect, 0.12f, 8,
                                  ColorAlpha(borderColor, alpha * (isSelected ? 0.8f : 0.3f)));

        // Selection glow ring for center item
        if (isSelected && normalizedDist < 0.1f) {
            Rectangle glowRect = {cardX - 4, cardY - 4, cardWidth + 8, cardHeight + 8};
            DrawRectangleRoundedLines(glowRect, 0.12f, 8, ColorAlpha(itemAccent, 0.4f));
        }

        // Plugin/folder icon placeholder (large centered circle)
        float iconRadius = cardHeight * 0.25f;
        float iconY = cardY + cardHeight * 0.35f;
        DrawCircle((int)(cardX + cardWidth / 2), (int)iconY, iconRadius,
                   ColorAlpha(itemAccentDim, alpha * 0.4f));
        DrawCircleLines((int)(cardX + cardWidth / 2), (int)iconY, iconRadius,
                        ColorAlpha(itemAccent, alpha * 0.6f));

        // First letter as icon (or folder icon)
        if (itemName && itemName[0]) {
            const char *iconChar = isFolder ? "F" : NULL;
            char initial[2] = {0};
            if (!iconChar) {
                initial[0] = itemName[0];
                initial[1] = '\0';
                iconChar = initial;
            }
            float initialSize = iconRadius * 1.2f;
            Vector2 initialDim = MeasureTextEx(font, iconChar, initialSize, 1);
            DrawTextEx(font, iconChar,
                      (Vector2){cardX + cardWidth / 2 - initialDim.x / 2, iconY - initialDim.y / 2},
                      initialSize, 1, ColorAlpha(isFolder ? colors->folder : colors->textPrimary, alpha));
        }

        // Item name below icon (larger font, no description)
        float fontSize = 26 * scale;
        if (fontSize > 14 && itemName) {
            Vector2 nameSize = MeasureTextEx(font, itemName, fontSize, 1);
            float nameX = cardX + (cardWidth - nameSize.x) / 2;
            float nameY = cardY + cardHeight * 0.75f;
            Color nameColor = isSelected ? colors->textPrimary : colors->textSecondary;
            DrawTextEx(font, itemName,
                      (Vector2){nameX, nameY}, fontSize, 1, ColorAlpha(nameColor, alpha));
        }
    }

    // Navigation dots at bottom
    float dotY = CAROUSEL_CENTER_Y + CAROUSEL_ITEM_HEIGHT / 2 + 50;
    float totalDotsWidth = itemCount * 16;
    float dotStartX = (SCREEN_WIDTH - totalDotsWidth) / 2;

    for (int i = 0; i < itemCount; i++) {
        float dotX = dotStartX + i * 16 + 4;
        Color dotColor = (i == selected) ? dynamicAccent : ColorAlpha(colors->textDim, 0.4f);
        float dotRadius = (i == selected) ? 5.0f : 3.0f;
        DrawCircle((int)dotX, (int)dotY, dotRadius, dotColor);
    }
}
