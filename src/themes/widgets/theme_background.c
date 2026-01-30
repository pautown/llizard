#include "menu_theme_private.h"
#include "llz_sdk.h"

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT

void ThemeBackgroundDraw(void)
{
    const MenuThemeColorPalette *colors = MenuThemeColorsGetPalette();

    if (LlzBackgroundIsEnabled()) {
        // Use SDK animated background when enabled
        LlzBackgroundDraw();
    } else {
        // Subtle gradient background
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                               colors->bgDark, colors->bgGradient);

        // Subtle accent glow at top
        for (int i = 0; i < 3; i++) {
            float alpha = 0.03f - i * 0.01f;
            Color glow = ColorAlpha(colors->accent, alpha);
            DrawCircleGradient(SCREEN_WIDTH / 2, -100 + i * 50, 400 - i * 80,
                               glow, ColorAlpha(glow, 0.0f));
        }
    }
}
