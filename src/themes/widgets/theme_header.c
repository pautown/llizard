#include "menu_theme_private.h"
#include "llz_sdk.h"

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH

void ThemeHeaderDraw(int selected, Color dynamicAccent, Color complementary)
{
    Font font = MenuThemeFontsGetMenu();
    const MenuThemeColorPalette *colors = MenuThemeColorsGetPalette();

    // Header - show folder name with back hint if inside folder
    if (MenuThemeIsInsideFolder()) {
        // Back arrow and folder name
        DrawTextEx(font, "◀", (Vector2){MENU_PADDING_X, 32}, 24, 1, colors->textDim);
        const char *folderName = LLZ_CATEGORY_NAMES[MenuThemeGetCurrentFolder()];
        DrawTextEx(font, folderName, (Vector2){MENU_PADDING_X + 34, 28}, 38, 2, colors->textPrimary);

        // Subtle accent underline
        Vector2 folderSize = MeasureTextEx(font, folderName, 38, 2);
        DrawRectangle(MENU_PADDING_X + 34, 74, (int)folderSize.x, 3, colors->folder);

        // Back hint
        const char *instructions = "back to return • select to launch";
        DrawTextEx(font, instructions, (Vector2){MENU_PADDING_X, 88}, 16, 1, colors->textDim);
    } else {
        // Normal header
        DrawTextEx(font, "llizardOS", (Vector2){MENU_PADDING_X, 28}, 38, 2, colors->textPrimary);

        // Selected item name in top right - uses complementary color
        int itemCount = MenuThemeGetItemCount();
        if (itemCount > 0 && selected >= 0 && selected < itemCount) {
            const char *itemName = MenuThemeGetItemName(selected);
            float fontSize = 36;
            float spacing = 2;
            Vector2 textSize = MeasureTextEx(font, itemName, fontSize, spacing);
            float textX = SCREEN_WIDTH - textSize.x - MENU_PADDING_X;
            float textY = 28;
            DrawTextEx(font, itemName, (Vector2){textX, textY}, fontSize, spacing, complementary);
        }

        // Subtle accent underline
        DrawRectangle(MENU_PADDING_X, 74, 160, 3, dynamicAccent);

        // Instruction text
        const char *instructions = "scroll to navigate • select to launch";
        DrawTextEx(font, instructions, (Vector2){MENU_PADDING_X, 88}, 16, 1, colors->textDim);
    }
}
