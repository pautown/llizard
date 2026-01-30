#include "menu_theme_private.h"

void ThemeItemDraw(float x, float y, float width, float height,
                   const char *name, const char *description,
                   bool isFolder, bool isSelected, int itemCount,
                   Color dynamicAccent, Color dynamicAccentDim)
{
    (void)dynamicAccentDim;  // Reserved for future use

    Font font = MenuThemeFontsGetMenu();
    const MenuThemeColorPalette *colors = MenuThemeColorsGetPalette();

    Rectangle cardRect = {x, y, width, height};

    Color cardBg = isSelected ? colors->cardSelected : colors->cardBg;
    Color borderColor = isSelected ? dynamicAccent : colors->cardBorder;

    // Draw card with rounded corners
    DrawRectangleRounded(cardRect, 0.15f, 8, cardBg);

    // Selection accent bar on left
    if (isSelected) {
        Rectangle accentBar = {cardRect.x, cardRect.y + 8, 4, cardRect.height - 16};
        Color barColor = isFolder ? colors->folder : dynamicAccent;
        DrawRectangleRounded(accentBar, 0.5f, 4, barColor);
    }

    // Subtle border
    DrawRectangleRoundedLines(cardRect, 0.15f, 8, ColorAlpha(borderColor, isSelected ? 0.6f : 0.2f));

    // Icon for folders
    float textStartX = MENU_PADDING_X + 8;
    if (isFolder) {
        // Draw folder icon
        Color iconColor = isSelected ? colors->folder : ColorAlpha(colors->folder, 0.6f);
        DrawTextEx(font, "📁", (Vector2){textStartX, y + 20}, 24, 1, iconColor);
        textStartX += 36;
    }

    // Name
    Color nameColor = isSelected ? colors->textPrimary : colors->textSecondary;
    DrawTextEx(font, name, (Vector2){textStartX, y + 16}, 24, 1.5f, nameColor);

    // Description or plugin count for folders
    if (isFolder && itemCount > 0) {
        char countStr[32];
        snprintf(countStr, sizeof(countStr), "%d plugin%s", itemCount, itemCount == 1 ? "" : "s");
        Color descColor = isSelected ? colors->textSecondary : colors->textDim;
        DrawTextEx(font, countStr, (Vector2){textStartX, y + 46}, 16, 1, descColor);

        // Arrow indicator on right
        Vector2 arrowSize = MeasureTextEx(font, "▶", 18, 1);
        Color arrowColor = isSelected ? dynamicAccent : colors->textDim;
        DrawTextEx(font, "▶",
                  (Vector2){cardRect.x + cardRect.width - arrowSize.x - 16, y + (height - 18) / 2},
                  18, 1, arrowColor);
    } else if (description) {
        Color descColor = isSelected ? colors->textSecondary : colors->textDim;
        DrawTextEx(font, description, (Vector2){textStartX, y + 46}, 16, 1, descColor);
    }
}
