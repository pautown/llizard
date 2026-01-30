#include "menu_theme_private.h"
#include "llz_sdk.h"

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT

// Style names for indicator display
static const char *g_styleNames[] = {
    "List",
    "Carousel",
    "Cards",
    "CarThing",
    "Grid"
};

void MenuThemeIndicatorInit(MenuThemeIndicatorState *indicator)
{
    indicator->alpha = 0.0f;
    indicator->timer = 0.0f;
}

void MenuThemeIndicatorShow(MenuThemeIndicatorState *indicator)
{
    indicator->alpha = 1.0f;
    indicator->timer = 2.0f;  // Show for 2 seconds
}

void MenuThemeIndicatorUpdate(MenuThemeIndicatorState *indicator, float deltaTime)
{
    if (indicator->timer > 0.0f) {
        indicator->timer -= deltaTime;
        if (indicator->timer <= 0.5f) {
            // Fade out in last 0.5 seconds
            indicator->alpha = indicator->timer / 0.5f;
        }
    } else {
        indicator->alpha = 0.0f;
    }
}

void MenuThemeIndicatorDraw(const MenuThemeIndicatorState *indicator,
                            MenuThemeStyle style, Font font)
{
    if (indicator->alpha <= 0.0f) return;

    const MenuThemeColorPalette *colors = MenuThemeColorsGetPalette();
    const char *styleName = g_styleNames[style];

    // Background pill
    float fontSize = 24;
    Vector2 textSize = MeasureTextEx(font, styleName, fontSize, 1);
    float pillWidth = textSize.x + 40;
    float pillHeight = 44;
    float pillX = (SCREEN_WIDTH - pillWidth) / 2;
    float pillY = SCREEN_HEIGHT - 70;

    Color bgColor = ColorAlpha(colors->bgDark, 0.9f * indicator->alpha);
    Color borderColor = ColorAlpha(colors->accent, 0.6f * indicator->alpha);
    Color textColor = ColorAlpha(colors->textPrimary, indicator->alpha);

    Rectangle pill = {pillX, pillY, pillWidth, pillHeight};
    DrawRectangleRounded(pill, 0.5f, 8, bgColor);
    DrawRectangleRoundedLines(pill, 0.5f, 8, borderColor);

    DrawTextEx(font, styleName,
               (Vector2){pillX + 20, pillY + (pillHeight - fontSize) / 2},
               fontSize, 1, textColor);
}
