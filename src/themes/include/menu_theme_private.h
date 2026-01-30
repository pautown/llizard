#ifndef MENU_THEME_PRIVATE_H
#define MENU_THEME_PRIVATE_H

#include "menu_theme.h"
#include "menu_theme_types.h"
#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Internal Theme State Access
// ============================================================================

/**
 * Get pointer to global theme state (for internal use by theme modules).
 */
MenuThemeState* MenuThemeGetState(void);

// ============================================================================
// Scroll Functions (menu_theme_scroll.c)
// ============================================================================

void MenuThemeScrollInit(MenuThemeScrollState *scroll);
void MenuThemeScrollUpdate(MenuThemeScrollState *scroll, float deltaTime);
void MenuThemeScrollUpdateCarousel(MenuThemeScrollState *scroll, float deltaTime);
float MenuThemeScrollCalculateTarget(int selected, int count, float currentTarget);

// ============================================================================
// Indicator Functions (theme_indicator.c)
// ============================================================================

void MenuThemeIndicatorInit(MenuThemeIndicatorState *indicator);
void MenuThemeIndicatorUpdate(MenuThemeIndicatorState *indicator, float deltaTime);
void MenuThemeIndicatorShow(MenuThemeIndicatorState *indicator);
void MenuThemeIndicatorDraw(const MenuThemeIndicatorState *indicator,
                            MenuThemeStyle style, Font font);

// ============================================================================
// Font Functions (menu_theme_fonts.c)
// ============================================================================

void MenuThemeFontsInit(void);
void MenuThemeFontsShutdown(void);
Font MenuThemeFontsGetMenu(void);
Font MenuThemeFontsGetOmicron(void);
Font MenuThemeFontsGetTracklister(void);
Font MenuThemeFontsGetIBrand(void);

// Build Unicode codepoints for international character support
int* MenuThemeFontsBuildCodepoints(int *outCount);

// ============================================================================
// Color Functions (menu_theme_colors.c)
// ============================================================================

void MenuThemeColorsInit(void);
const MenuThemeColorPalette* MenuThemeColorsGetPalette(void);
const MenuThemeGridColors* MenuThemeColorsGetGridPalette(void);

// ============================================================================
// Style Draw Functions (individual theme implementations)
// ============================================================================

// theme_list.c
void ThemeListDraw(int selected, float deltaTime,
                   Color dynamicAccent, Color dynamicAccentDim);

// theme_carousel.c
void ThemeCarouselDraw(int selected, float deltaTime,
                       Color dynamicAccent, Color dynamicAccentDim);

// theme_cards.c
void ThemeCardsDraw(int selected, Color dynamicAccent, Color complementary);

// theme_carthing.c
void ThemeCarThingDraw(int selected, float deltaTime, Color dynamicAccent);

// theme_grid.c
void ThemeGridDraw(int selected, float deltaTime);

// ============================================================================
// Widget Functions (shared widgets)
// ============================================================================

// theme_header.c
void ThemeHeaderDraw(int selected, Color dynamicAccent, Color complementary);

// theme_item.c
void ThemeItemDraw(float x, float y, float width, float height,
                   const char *name, const char *description,
                   bool isFolder, bool isSelected, int itemCount,
                   Color dynamicAccent, Color dynamicAccentDim);

// theme_background.c (uses SDK background)
void ThemeBackgroundDraw(void);

#ifdef __cplusplus
}
#endif

#endif // MENU_THEME_PRIVATE_H
