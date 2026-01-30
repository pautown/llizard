#ifndef MENU_THEME_TYPES_H
#define MENU_THEME_TYPES_H

#include "raylib.h"
#include "plugin_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

// Menu navigation styles
typedef enum {
    MENU_THEME_LIST = 0,      // Classic vertical list
    MENU_THEME_CAROUSEL,      // Apple Music inspired horizontal carousel
    MENU_THEME_CARDS,         // Spotify inspired large card view
    MENU_THEME_CARTHING,      // Spotify CarThing single-focus minimal
    MENU_THEME_GRID,          // Apple Music grid layout
    MENU_THEME_COUNT
} MenuThemeStyle;

// Standard color palette (dark theme, used by most styles)
typedef struct {
    Color bgDark;
    Color bgGradient;
    Color accent;
    Color accentDim;
    Color textPrimary;
    Color textSecondary;
    Color textDim;
    Color cardBg;
    Color cardSelected;
    Color cardBorder;
    Color folder;
} MenuThemeColorPalette;

// Grid-specific white theme colors
typedef struct {
    Color bgWhite;
    Color tileBg;
    Color tileHover;
    Color textPrimary;
    Color textSecondary;
    Color textDim;
    Color border;
    Color appleRed;
    Color appleYellow;
    Color appleGreen;
    Color appleBlue;
    Color appleOrange;
} MenuThemeGridColors;

// Scroll animation state
typedef struct {
    float scrollOffset;
    float targetScrollOffset;
    float carouselOffset;
    float carouselTarget;
} MenuThemeScrollState;

// Style indicator overlay state
typedef struct {
    float alpha;
    float timer;
} MenuThemeIndicatorState;

// CarThing style crossfade state
typedef struct {
    float fadeAlpha;
    int lastSelected;
} MenuThemeCarThingState;

// Complete theme state
typedef struct {
    MenuThemeStyle currentStyle;
    MenuThemeScrollState scroll;
    MenuThemeIndicatorState indicator;
    MenuThemeCarThingState carThing;
    int screenWidth;
    int screenHeight;
    bool initialized;
} MenuThemeState;

// Menu styling constants
#define MENU_ITEM_HEIGHT 72
#define MENU_ITEM_SPACING 8
#define MENU_PADDING_X 32
#define MENU_PADDING_TOP 120

// Carousel constants
#define CAROUSEL_ITEM_WIDTH 280
#define CAROUSEL_ITEM_HEIGHT 200
#define CAROUSEL_SPACING 40

// Grid constants
#define GRID_COLS 2
#define GRID_TILE_WIDTH 360
#define GRID_TILE_HEIGHT 180
#define GRID_SPACING 20
#define GRID_PADDING_X 40
#define GRID_PADDING_TOP 100

#ifdef __cplusplus
}
#endif

#endif // MENU_THEME_TYPES_H
