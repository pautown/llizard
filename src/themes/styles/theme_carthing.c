#include "menu_theme_private.h"
#include "llz_sdk.h"
#include <math.h>
#include <string.h>

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT

// Spotify green color for authentic CarThing look
static const Color COLOR_SPOTIFY_GREEN = {30, 215, 96, 255};

void ThemeCarThingDraw(int selected, float deltaTime, Color dynamicAccent)
{
    (void)dynamicAccent;  // CarThing uses Spotify green

    MenuThemeState *state = MenuThemeGetState();
    const MenuThemeColorPalette *colors = MenuThemeColorsGetPalette();

    // Get fonts (lazy loaded)
    Font textFont = MenuThemeFontsGetTracklister();
    Font brandFont = MenuThemeFontsGetOmicron();

    int itemCount = MenuThemeGetItemCount();

    if (itemCount == 0) {
        DrawTextEx(textFont, "No plugins",
                  (Vector2){SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 - 20}, 32, 1, colors->textSecondary);
        return;
    }

    const char *itemName = MenuThemeGetItemName(selected);
    bool isFolder = MenuThemeIsItemFolder(selected);

    // Detect selection change and trigger crossfade
    if (state->carThing.lastSelected != selected) {
        state->carThing.fadeAlpha = 0.0f;  // Start fade from zero
        state->carThing.lastSelected = selected;
    }

    // Gentle crossfade animation
    float fadeSpeed = 5.0f;
    state->carThing.fadeAlpha += fadeSpeed * deltaTime;
    if (state->carThing.fadeAlpha > 1.0f) state->carThing.fadeAlpha = 1.0f;

    // Smooth easing for gentle feel (smoothstep)
    float t = state->carThing.fadeAlpha;
    float contentAlpha = t * t * (3.0f - 2.0f * t);

    // Use folder color or Spotify green
    Color accentColor = isFolder ? colors->folder : COLOR_SPOTIFY_GREEN;

    // Aero glass overlay effect
    // Layer 1: Semi-transparent tint
    Color aeroTint = isFolder ? (Color){40, 100, 180, 40} : (Color){20, 180, 80, 40};
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, aeroTint);

    // Layer 2: Subtle gradient from top (lighter) to bottom (darker)
    for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
        float gradientAlpha = 0.02f + (float)y / SCREEN_HEIGHT * 0.06f;
        Color gradLine = ColorAlpha(accentColor, gradientAlpha);
        DrawRectangle(0, y, SCREEN_WIDTH, 4, gradLine);
    }

    // Layer 3: Glassy highlight at top (aero reflection)
    for (int i = 0; i < 80; i++) {
        float highlightAlpha = (80 - i) / 80.0f * 0.08f;
        DrawRectangle(0, i, SCREEN_WIDTH, 1, ColorAlpha(WHITE, highlightAlpha));
    }

    // Layer 4: Subtle vignette for depth
    for (int i = 0; i < 60; i++) {
        float vignetteAlpha = i / 60.0f * 0.15f;
        DrawRectangle(0, SCREEN_HEIGHT - 60 + i, SCREEN_WIDTH, 1, ColorAlpha(BLACK, vignetteAlpha));
    }

    // Calculate layout - icon is vertically centered
    float iconRadius = 70;
    float iconCenterY = SCREEN_HEIGHT / 2;
    float iconX = SCREEN_WIDTH / 2;

    // Item name below icon
    float mainFontSize = 64;
    Vector2 mainSize = itemName ? MeasureTextEx(textFont, itemName, mainFontSize, 2) : (Vector2){0, 0};

    // If name is too long, shrink it
    while (itemName && mainSize.x > SCREEN_WIDTH - 80 && mainFontSize > 32) {
        mainFontSize -= 4;
        mainSize = MeasureTextEx(textFont, itemName, mainFontSize, 2);
    }

    float mainX = (SCREEN_WIDTH - mainSize.x) / 2;
    float mainY = iconCenterY + iconRadius + 30;

    // Flat circle behind letter - crossfades with content
    Color circleBg = isFolder ? (Color){20, 50, 100, 200} : (Color){15, 60, 35, 200};
    DrawCircle((int)iconX, (int)iconCenterY, iconRadius, ColorAlpha(circleBg, contentAlpha));

    // Accent ring
    DrawCircleLines((int)iconX, (int)iconCenterY, iconRadius, ColorAlpha(accentColor, contentAlpha));

    // Initial letter inside - crossfades
    if (itemName && itemName[0]) {
        const char *iconChar = isFolder ? "F" : NULL;
        char initial[2] = {0};
        if (!iconChar) {
            initial[0] = itemName[0];
            initial[1] = '\0';
            iconChar = initial;
        }
        float initialSize = 60;
        Vector2 initialDim = MeasureTextEx(textFont, iconChar, initialSize, 1);
        DrawTextEx(textFont, iconChar,
                  (Vector2){iconX - initialDim.x / 2, iconCenterY - initialDim.y / 2},
                  initialSize, 1, ColorAlpha(accentColor, contentAlpha));
    }

    // Main item name below icon - centered, crossfades
    if (itemName) {
        DrawTextEx(textFont, itemName, (Vector2){mainX, mainY}, mainFontSize, 2,
                   ColorAlpha(WHITE, contentAlpha));
    }

    // Accent underline - centered, crossfades
    float underlineWidth = fminf(mainSize.x + 40, SCREEN_WIDTH - 100);
    float underlineX = (SCREEN_WIDTH - underlineWidth) / 2;
    DrawRectangle((int)underlineX, (int)(mainY + mainSize.y + 12), (int)underlineWidth, 4,
                 ColorAlpha(accentColor, contentAlpha));

    // Item counter below - centered, crossfades
    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d / %d", selected + 1, itemCount);
    Vector2 counterSize = MeasureTextEx(textFont, counterStr, 24, 1);
    DrawTextEx(textFont, counterStr,
              (Vector2){(SCREEN_WIDTH - counterSize.x) / 2, mainY + mainSize.y + 40},
              24, 1, ColorAlpha(WHITE, 0.5f * contentAlpha));

    // Minimal navigation hints at sides (fixed position, always visible)
    float sideY = SCREEN_HEIGHT / 2;

    if (selected > 0) {
        // Left arrow
        DrawTextEx(textFont, "◀", (Vector2){40, sideY - 12}, 28, 1, ColorAlpha(accentColor, 0.4f));

        // Previous item name (dim)
        const char *prevName = MenuThemeGetItemName(selected - 1);
        if (prevName) {
            Vector2 prevSize = MeasureTextEx(textFont, prevName, 16, 1);
            if (prevSize.x > 120) {
                char truncName[20];
                strncpy(truncName, prevName, 15);
                truncName[15] = '.';
                truncName[16] = '.';
                truncName[17] = '.';
                truncName[18] = '\0';
                DrawTextEx(textFont, truncName, (Vector2){40, sideY + 24}, 16, 1,
                           ColorAlpha(WHITE, 0.25f));
            } else {
                DrawTextEx(textFont, prevName, (Vector2){40, sideY + 24}, 16, 1,
                           ColorAlpha(WHITE, 0.25f));
            }
        }
    }

    if (selected < itemCount - 1) {
        // Right arrow
        Vector2 arrowSize = MeasureTextEx(textFont, "▶", 28, 1);
        DrawTextEx(textFont, "▶", (Vector2){SCREEN_WIDTH - 40 - arrowSize.x, sideY - 12}, 28, 1,
                   ColorAlpha(accentColor, 0.4f));

        // Next item name (dim)
        const char *nextName = MenuThemeGetItemName(selected + 1);
        if (nextName) {
            Vector2 nextSize = MeasureTextEx(textFont, nextName, 16, 1);
            if (nextSize.x > 120) {
                char truncName[20];
                strncpy(truncName, nextName, 15);
                truncName[15] = '.';
                truncName[16] = '.';
                truncName[17] = '.';
                truncName[18] = '\0';
                Vector2 truncSize = MeasureTextEx(textFont, truncName, 16, 1);
                DrawTextEx(textFont, truncName, (Vector2){SCREEN_WIDTH - 40 - truncSize.x, sideY + 24},
                           16, 1, ColorAlpha(WHITE, 0.25f));
            } else {
                DrawTextEx(textFont, nextName, (Vector2){SCREEN_WIDTH - 40 - nextSize.x, sideY + 24},
                           16, 1, ColorAlpha(WHITE, 0.25f));
            }
        }
    }

    // "llizardOS" branding in top left (uses Omicron font)
    DrawTextEx(brandFont, "llizardOS", (Vector2){24, 20}, 18, 1, ColorAlpha(WHITE, 0.4f));
}
