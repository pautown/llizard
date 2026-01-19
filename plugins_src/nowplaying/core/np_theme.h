#ifndef NP_PLUGIN_THEME_H
#define NP_PLUGIN_THEME_H

#include "raylib.h"

// Theme variants
typedef enum {
    NP_THEME_VARIANT_ZUNE,
    NP_THEME_VARIANT_HIGH_CONTRAST,
    NP_THEME_VARIANT_DIGITAL,
    NP_THEME_VARIANT_MATRIX,
    NP_THEME_VARIANT_DAYTIME,
    NP_THEME_VARIANT_PINK,
    NP_THEME_VARIANT_BLUE,
    NP_THEME_VARIANT_MOON,
    NP_THEME_VARIANT_COUNT
} NpThemeVariant;

// Theme color IDs
typedef enum {
    NP_COLOR_BG_DARK,
    NP_COLOR_BG_MEDIUM,
    NP_COLOR_BG_LIGHT,
    NP_COLOR_PANEL,
    NP_COLOR_PANEL_HOVER,
    NP_COLOR_PANEL_SHEEN,
    NP_COLOR_ACCENT,
    NP_COLOR_ACCENT_SOFT,
    NP_COLOR_TEXT_PRIMARY,
    NP_COLOR_TEXT_SECONDARY,
    NP_COLOR_BORDER,
    NP_COLOR_OVERLAY,
    NP_COLOR_COUNT
} NpColorId;

// Typography IDs
typedef enum {
    NP_TYPO_TITLE,
    NP_TYPO_BODY,
    NP_TYPO_DETAIL,
    NP_TYPO_BUTTON,
    NP_TYPO_COUNT
} NpTypographyId;

// Theme management functions
void NpThemeInit(int width, int height);
void NpThemeShutdown(void);

// Theme variant functions
void NpThemeSetVariant(NpThemeVariant variant);
NpThemeVariant NpThemeGetVariant(void);
const char* NpThemeGetVariantName(NpThemeVariant variant);

// Color functions
Color NpThemeGetColor(NpColorId id);
Color NpThemeGetColorAlpha(NpColorId id, float alpha);

// Custom color functions
void NpThemeSetCustomBackgroundColor(Color bgColor);
void NpThemeClearCustomBackgroundColor(void);
bool NpThemeHasCustomBackgroundColor(void);

// Typography functions
void NpThemeDrawText(NpTypographyId typo, const char *text, Vector2 pos);
void NpThemeDrawTextColored(NpTypographyId typo, const char *text, Vector2 pos, Color color);
float NpThemeMeasureTextWidth(NpTypographyId typo, const char *text);
float NpThemeGetLineHeight(NpTypographyId typo);

// Drawing functions
void NpThemeDrawBackground(void);
Font NpThemeGetFont(void);

#endif // NP_PLUGIN_THEME_H
