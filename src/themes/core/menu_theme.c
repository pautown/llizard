#include "menu_theme.h"
#include "menu_theme_private.h"
#include "llz_sdk.h"
#include <stdio.h>
#include <math.h>

// Style names for display
static const char *g_styleNames[] = {
    "List",
    "Carousel",
    "Cards",
    "CarThing",
    "Grid"
};

// Global theme state
static MenuThemeState g_state = {0};

MenuThemeState* MenuThemeGetState(void)
{
    return &g_state;
}

bool MenuThemeInit(int screenWidth, int screenHeight)
{
    if (g_state.initialized) return true;

    g_state.screenWidth = screenWidth;
    g_state.screenHeight = screenHeight;
    g_state.currentStyle = MENU_THEME_LIST;

    // Initialize subsystems
    MenuThemeColorsInit();
    MenuThemeFontsInit();
    MenuThemeScrollInit(&g_state.scroll);
    MenuThemeIndicatorInit(&g_state.indicator);

    // Initialize CarThing state
    g_state.carThing.fadeAlpha = 1.0f;
    g_state.carThing.lastSelected = -1;

    g_state.initialized = true;
    printf("MenuTheme: Initialized (%dx%d)\n", screenWidth, screenHeight);

    return true;
}

void MenuThemeShutdown(void)
{
    if (!g_state.initialized) return;

    MenuThemeFontsShutdown();

    g_state.initialized = false;
    printf("MenuTheme: Shutdown\n");
}

void MenuThemeUpdate(float deltaTime)
{
    if (!g_state.initialized) return;

    // Update scroll animations
    MenuThemeScrollUpdate(&g_state.scroll, deltaTime);
    MenuThemeScrollUpdateCarousel(&g_state.scroll, deltaTime);

    // Update style indicator overlay
    MenuThemeIndicatorUpdate(&g_state.indicator, deltaTime);
}

void MenuThemeDraw(const PluginRegistry *registry, int selected, float deltaTime)
{
    if (!g_state.initialized) return;

    // Draw background (uses SDK animated background)
    ThemeBackgroundDraw();

    // Get dynamic accent color from background palette
    Color dynamicAccent = MenuThemeGetDynamicAccent();
    Color dynamicAccentDim = ColorAlpha(dynamicAccent, 0.6f);
    Color complementary = MenuThemeGetComplementaryColor();

    // Draw header for styles that use shared header
    if (g_state.currentStyle == MENU_THEME_LIST ||
        g_state.currentStyle == MENU_THEME_CAROUSEL ||
        g_state.currentStyle == MENU_THEME_CARDS) {
        ThemeHeaderDraw(selected, dynamicAccent, complementary);
    }

    // Dispatch to style-specific renderer
    switch (g_state.currentStyle) {
        case MENU_THEME_LIST:
            ThemeListDraw(selected, deltaTime, dynamicAccent, dynamicAccentDim);
            break;

        case MENU_THEME_CAROUSEL:
            ThemeCarouselDraw(selected, deltaTime, dynamicAccent, dynamicAccentDim);
            break;

        case MENU_THEME_CARDS:
            ThemeCardsDraw(selected, dynamicAccent, complementary);
            break;

        case MENU_THEME_CARTHING:
            ThemeCarThingDraw(selected, deltaTime, dynamicAccent);
            break;

        case MENU_THEME_GRID:
            ThemeGridDraw(selected, deltaTime);
            break;

        default:
            ThemeListDraw(selected, deltaTime, dynamicAccent, dynamicAccentDim);
            break;
    }

    // Draw style indicator overlay (shows when style changes)
    MenuThemeIndicatorDraw(&g_state.indicator, g_state.currentStyle,
                           MenuThemeFontsGetMenu());
}

void MenuThemeCycleNext(void)
{
    g_state.currentStyle = (g_state.currentStyle + 1) % MENU_THEME_COUNT;
    MenuThemeIndicatorShow(&g_state.indicator);
    printf("MenuTheme: Style changed to %s\n", g_styleNames[g_state.currentStyle]);
}

void MenuThemeSetStyle(MenuThemeStyle style)
{
    if (style >= 0 && style < MENU_THEME_COUNT) {
        g_state.currentStyle = style;
    }
}

MenuThemeStyle MenuThemeGetStyle(void)
{
    return g_state.currentStyle;
}

const char* MenuThemeGetStyleName(MenuThemeStyle style)
{
    if (style >= 0 && style < MENU_THEME_COUNT) {
        return g_styleNames[style];
    }
    return "Unknown";
}

void MenuThemeResetScroll(void)
{
    MenuThemeScrollInit(&g_state.scroll);
}

float MenuThemeGetScrollOffset(void)
{
    return g_state.scroll.scrollOffset;
}

float MenuThemeGetCarouselOffset(void)
{
    return g_state.scroll.carouselOffset;
}

const MenuThemeColorPalette* MenuThemeGetColors(void)
{
    return MenuThemeColorsGetPalette();
}

const MenuThemeGridColors* MenuThemeGetGridColors(void)
{
    return MenuThemeColorsGetGridPalette();
}

Color MenuThemeGetDynamicAccent(void)
{
    const LlzBackgroundPalette *palette = LlzBackgroundGetPalette();
    if (palette) {
        return palette->colors[1];
    }
    return MenuThemeColorsGetPalette()->accent;
}

Color MenuThemeGetComplementaryColor(void)
{
    const LlzBackgroundPalette *palette = LlzBackgroundGetPalette();
    Color primaryColor = palette ? palette->colors[0] : MenuThemeColorsGetPalette()->accent;

    // Calculate complementary color (opposite hue)
    Vector3 hsv = ColorToHSV(primaryColor);
    float compHue = fmodf(hsv.x + 180.0f, 360.0f);
    return ColorFromHSV(compHue, fminf(hsv.y * 0.8f, 0.7f), fminf(hsv.z + 0.2f, 0.9f));
}

Font MenuThemeGetFont(void)
{
    return MenuThemeFontsGetMenu();
}

Font MenuThemeGetOmicronFont(void)
{
    return MenuThemeFontsGetOmicron();
}

Font MenuThemeGetTracklisterFont(void)
{
    return MenuThemeFontsGetTracklister();
}

Font MenuThemeGetIBrandFont(void)
{
    return MenuThemeFontsGetIBrand();
}
