#include "menu_theme_private.h"
#include <string.h>

// Standard dark theme color palette
static MenuThemeColorPalette g_colors = {
    .bgDark       = {18, 18, 22, 255},
    .bgGradient   = {28, 24, 38, 255},
    .accent       = {138, 106, 210, 255},
    .accentDim    = {90, 70, 140, 255},
    .textPrimary  = {245, 245, 250, 255},
    .textSecondary= {160, 160, 175, 255},
    .textDim      = {100, 100, 115, 255},
    .cardBg       = {32, 30, 42, 255},
    .cardSelected = {48, 42, 68, 255},
    .cardBorder   = {60, 55, 80, 255},
    .folder       = {100, 180, 255, 255}
};

// Grid style white theme colors (Apple-inspired)
static MenuThemeGridColors g_gridColors = {
    .bgWhite      = {250, 250, 252, 255},
    .tileBg       = {255, 255, 255, 255},
    .tileHover    = {248, 248, 250, 255},
    .textPrimary  = {30, 30, 32, 255},
    .textSecondary= {100, 100, 105, 255},
    .textDim      = {160, 160, 165, 255},
    .border       = {220, 220, 225, 255},
    .appleRed     = {255, 95, 86, 255},
    .appleYellow  = {255, 189, 46, 255},
    .appleGreen   = {39, 201, 63, 255},
    .appleBlue    = {0, 122, 255, 255},
    .appleOrange  = {255, 159, 10, 255}
};

// Spotify green for CarThing style
static const Color COLOR_SPOTIFY_GREEN = {30, 215, 96, 255};

void MenuThemeColorsInit(void)
{
    // Colors are statically initialized, nothing to do
}

const MenuThemeColorPalette* MenuThemeColorsGetPalette(void)
{
    return &g_colors;
}

const MenuThemeGridColors* MenuThemeColorsGetGridPalette(void)
{
    return &g_gridColors;
}
