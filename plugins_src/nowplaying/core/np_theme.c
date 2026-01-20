#include "nowplaying/core/np_theme.h"
#include "llz_sdk.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Unicode codepoint ranges for international character support
// Build array of codepoints for font loading
static int *BuildUnicodeCodepoints(int *outCount) {
    // Define ranges: start, end (inclusive)
    static const int ranges[][2] = {
        // Basic ASCII (standard printable characters)
        {0x0020, 0x007E},   // Space to tilde (95 chars)

        // Latin-1 Supplement (French accents: é, è, ê, ë, à, â, ç, ù, û, ü, ô, î, ï, etc.)
        {0x00A0, 0x00FF},   // 96 chars

        // Latin Extended-A (additional accented: Œ, œ, Ÿ, etc.)
        {0x0100, 0x017F},   // 128 chars

        // Latin Extended-B (less common, but useful)
        {0x0180, 0x024F},   // 208 chars

        // Cyrillic (Russian: А-Я, а-я, Ё, ё, etc.)
        {0x0400, 0x04FF},   // 256 chars - full Cyrillic block

        // Cyrillic Supplement (additional Cyrillic characters)
        {0x0500, 0x052F},   // 48 chars
    };

    const int numRanges = sizeof(ranges) / sizeof(ranges[0]);

    // Calculate total codepoints
    int total = 0;
    for (int i = 0; i < numRanges; i++) {
        total += (ranges[i][1] - ranges[i][0] + 1);
    }

    // Allocate array
    int *codepoints = (int *)malloc(total * sizeof(int));
    if (!codepoints) {
        *outCount = 0;
        return NULL;
    }

    // Fill array
    int idx = 0;
    for (int i = 0; i < numRanges; i++) {
        for (int cp = ranges[i][0]; cp <= ranges[i][1]; cp++) {
            codepoints[idx++] = cp;
        }
    }

    *outCount = total;
    return codepoints;
}

// Color token
typedef struct {
    Color color;
} ColorToken;

// Typography styles
typedef struct {
    Font font;
    float fontSize;
    float spacing;
    bool uppercase;
} TypographyStyle;

// Theme data per variant
typedef struct {
    ColorToken colors[NP_COLOR_COUNT];
    TypographyStyle typography[NP_TYPO_COUNT];
} ThemeData;

// Global theme state
static struct {
    ThemeData variants[NP_THEME_VARIANT_COUNT];
    int activeVariant;
    Font mainFont;
    int screenWidth;
    int screenHeight;
    bool initialized;
    bool hasCustomColor;
    ThemeData customTheme;
} g_theme;

// Helper function to create colors
static Color MakeColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return (Color){ r, g, b, a };
}

// Initialize all theme color palettes
static void InitializePalettes(void) {
    // Zune (default) - Original Zune HD inspired midnight colors
    ThemeData *zune = &g_theme.variants[NP_THEME_VARIANT_ZUNE];
    zune->colors[NP_COLOR_BG_DARK].color = MakeColor(24, 12, 15, 255);
    zune->colors[NP_COLOR_BG_MEDIUM].color = MakeColor(51, 32, 41, 255);
    zune->colors[NP_COLOR_BG_LIGHT].color = MakeColor(74, 48, 61, 255);
    zune->colors[NP_COLOR_PANEL].color = MakeColor(0, 0, 0, 46);
    zune->colors[NP_COLOR_PANEL_HOVER].color = MakeColor(0, 0, 0, 76);
    zune->colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(255, 255, 255, 20);
    zune->colors[NP_COLOR_ACCENT].color = MakeColor(240, 92, 43, 255);
    zune->colors[NP_COLOR_ACCENT_SOFT].color = MakeColor(240, 92, 43, 76);
    zune->colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(245, 242, 240, 255);
    zune->colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(245, 242, 240, 173);
    zune->colors[NP_COLOR_BORDER].color = MakeColor(240, 92, 43, 115);
    zune->colors[NP_COLOR_OVERLAY].color = MakeColor(255, 255, 255, 20);

    // High Contrast - Pure black/white for maximum visibility
    ThemeData *highContrast = &g_theme.variants[NP_THEME_VARIANT_HIGH_CONTRAST];
    highContrast->colors[NP_COLOR_BG_DARK].color = MakeColor(0, 0, 0, 255);
    highContrast->colors[NP_COLOR_BG_MEDIUM].color = MakeColor(20, 20, 20, 255);
    highContrast->colors[NP_COLOR_BG_LIGHT].color = MakeColor(40, 40, 40, 255);
    highContrast->colors[NP_COLOR_PANEL].color = MakeColor(255, 255, 255, 30);
    highContrast->colors[NP_COLOR_PANEL_HOVER].color = MakeColor(255, 255, 255, 60);
    highContrast->colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(255, 255, 255, 40);
    highContrast->colors[NP_COLOR_ACCENT].color = MakeColor(255, 255, 255, 255);
    highContrast->colors[NP_COLOR_ACCENT_SOFT].color = MakeColor(255, 255, 255, 100);
    highContrast->colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(255, 255, 255, 255);
    highContrast->colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(200, 200, 200, 255);
    highContrast->colors[NP_COLOR_BORDER].color = MakeColor(255, 255, 255, 150);
    highContrast->colors[NP_COLOR_OVERLAY].color = MakeColor(255, 255, 255, 30);

    // Digital - Cyan/blue digital theme
    ThemeData *digital = &g_theme.variants[NP_THEME_VARIANT_DIGITAL];
    digital->colors[NP_COLOR_BG_DARK].color = MakeColor(5, 15, 25, 255);
    digital->colors[NP_COLOR_BG_MEDIUM].color = MakeColor(10, 25, 40, 255);
    digital->colors[NP_COLOR_BG_LIGHT].color = MakeColor(15, 35, 55, 255);
    digital->colors[NP_COLOR_PANEL].color = MakeColor(0, 200, 255, 30);
    digital->colors[NP_COLOR_PANEL_HOVER].color = MakeColor(0, 200, 255, 60);
    digital->colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(0, 255, 255, 30);
    digital->colors[NP_COLOR_ACCENT].color = MakeColor(0, 200, 255, 255);
    digital->colors[NP_COLOR_ACCENT_SOFT].color = MakeColor(0, 200, 255, 80);
    digital->colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(180, 240, 255, 255);
    digital->colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(100, 180, 220, 255);
    digital->colors[NP_COLOR_BORDER].color = MakeColor(0, 200, 255, 120);
    digital->colors[NP_COLOR_OVERLAY].color = MakeColor(0, 255, 255, 20);

    // Matrix - Green terminal/matrix theme
    ThemeData *matrix = &g_theme.variants[NP_THEME_VARIANT_MATRIX];
    matrix->colors[NP_COLOR_BG_DARK].color = MakeColor(0, 10, 0, 255);
    matrix->colors[NP_COLOR_BG_MEDIUM].color = MakeColor(0, 20, 0, 255);
    matrix->colors[NP_COLOR_BG_LIGHT].color = MakeColor(0, 30, 0, 255);
    matrix->colors[NP_COLOR_PANEL].color = MakeColor(0, 255, 0, 20);
    matrix->colors[NP_COLOR_PANEL_HOVER].color = MakeColor(0, 255, 0, 40);
    matrix->colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(0, 255, 0, 30);
    matrix->colors[NP_COLOR_ACCENT].color = MakeColor(0, 255, 65, 255);
    matrix->colors[NP_COLOR_ACCENT_SOFT].color = MakeColor(0, 255, 65, 80);
    matrix->colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(100, 255, 100, 255);
    matrix->colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(50, 200, 50, 255);
    matrix->colors[NP_COLOR_BORDER].color = MakeColor(0, 255, 65, 120);
    matrix->colors[NP_COLOR_OVERLAY].color = MakeColor(0, 255, 0, 15);

    // Daytime - Bright, warm daytime theme
    ThemeData *daytime = &g_theme.variants[NP_THEME_VARIANT_DAYTIME];
    daytime->colors[NP_COLOR_BG_DARK].color = MakeColor(245, 240, 230, 255);
    daytime->colors[NP_COLOR_BG_MEDIUM].color = MakeColor(255, 250, 240, 255);
    daytime->colors[NP_COLOR_BG_LIGHT].color = MakeColor(255, 255, 250, 255);
    daytime->colors[NP_COLOR_PANEL].color = MakeColor(0, 0, 0, 15);
    daytime->colors[NP_COLOR_PANEL_HOVER].color = MakeColor(0, 0, 0, 30);
    daytime->colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(255, 255, 255, 40);
    daytime->colors[NP_COLOR_ACCENT].color = MakeColor(255, 140, 0, 255);
    daytime->colors[NP_COLOR_ACCENT_SOFT].color = MakeColor(255, 140, 0, 60);
    daytime->colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(30, 30, 30, 255);
    daytime->colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(80, 80, 80, 255);
    daytime->colors[NP_COLOR_BORDER].color = MakeColor(255, 140, 0, 100);
    daytime->colors[NP_COLOR_OVERLAY].color = MakeColor(255, 255, 255, 30);

    // Pink - Magenta/pink theme
    ThemeData *pink = &g_theme.variants[NP_THEME_VARIANT_PINK];
    pink->colors[NP_COLOR_BG_DARK].color = MakeColor(25, 10, 20, 255);
    pink->colors[NP_COLOR_BG_MEDIUM].color = MakeColor(40, 20, 35, 255);
    pink->colors[NP_COLOR_BG_LIGHT].color = MakeColor(60, 30, 50, 255);
    pink->colors[NP_COLOR_PANEL].color = MakeColor(255, 0, 150, 25);
    pink->colors[NP_COLOR_PANEL_HOVER].color = MakeColor(255, 0, 150, 50);
    pink->colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(255, 100, 200, 30);
    pink->colors[NP_COLOR_ACCENT].color = MakeColor(255, 50, 150, 255);
    pink->colors[NP_COLOR_ACCENT_SOFT].color = MakeColor(255, 50, 150, 80);
    pink->colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(255, 200, 230, 255);
    pink->colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(200, 150, 180, 255);
    pink->colors[NP_COLOR_BORDER].color = MakeColor(255, 50, 150, 120);
    pink->colors[NP_COLOR_OVERLAY].color = MakeColor(255, 100, 200, 20);

    // Blue - Cool blue theme
    ThemeData *blue = &g_theme.variants[NP_THEME_VARIANT_BLUE];
    blue->colors[NP_COLOR_BG_DARK].color = MakeColor(10, 15, 30, 255);
    blue->colors[NP_COLOR_BG_MEDIUM].color = MakeColor(20, 30, 55, 255);
    blue->colors[NP_COLOR_BG_LIGHT].color = MakeColor(30, 45, 80, 255);
    blue->colors[NP_COLOR_PANEL].color = MakeColor(50, 100, 255, 30);
    blue->colors[NP_COLOR_PANEL_HOVER].color = MakeColor(50, 100, 255, 60);
    blue->colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(100, 150, 255, 30);
    blue->colors[NP_COLOR_ACCENT].color = MakeColor(70, 130, 255, 255);
    blue->colors[NP_COLOR_ACCENT_SOFT].color = MakeColor(70, 130, 255, 80);
    blue->colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(220, 235, 255, 255);
    blue->colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(150, 180, 220, 255);
    blue->colors[NP_COLOR_BORDER].color = MakeColor(70, 130, 255, 120);
    blue->colors[NP_COLOR_OVERLAY].color = MakeColor(100, 150, 255, 20);

    // Moon - Silver/moonlight theme
    ThemeData *moon = &g_theme.variants[NP_THEME_VARIANT_MOON];
    moon->colors[NP_COLOR_BG_DARK].color = MakeColor(15, 18, 25, 255);
    moon->colors[NP_COLOR_BG_MEDIUM].color = MakeColor(30, 35, 45, 255);
    moon->colors[NP_COLOR_BG_LIGHT].color = MakeColor(45, 52, 65, 255);
    moon->colors[NP_COLOR_PANEL].color = MakeColor(200, 200, 220, 30);
    moon->colors[NP_COLOR_PANEL_HOVER].color = MakeColor(200, 200, 220, 60);
    moon->colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(220, 220, 240, 30);
    moon->colors[NP_COLOR_ACCENT].color = MakeColor(180, 190, 220, 255);
    moon->colors[NP_COLOR_ACCENT_SOFT].color = MakeColor(180, 190, 220, 80);
    moon->colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(230, 235, 245, 255);
    moon->colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(160, 170, 190, 255);
    moon->colors[NP_COLOR_BORDER].color = MakeColor(180, 190, 220, 120);
    moon->colors[NP_COLOR_OVERLAY].color = MakeColor(220, 220, 240, 20);
}

// Initialize typography styles for all variants
static void InitializeTypography(void) {
    for (int variant = 0; variant < NP_THEME_VARIANT_COUNT; ++variant) {
        ThemeData *data = &g_theme.variants[variant];

        data->typography[NP_TYPO_TITLE] = (TypographyStyle){g_theme.mainFont, 38.0f, 2.4f, false};
        data->typography[NP_TYPO_BODY] = (TypographyStyle){g_theme.mainFont, 27.0f, 1.6f, false};
        data->typography[NP_TYPO_DETAIL] = (TypographyStyle){g_theme.mainFont, 21.0f, 1.4f, false};
        data->typography[NP_TYPO_BUTTON] = (TypographyStyle){g_theme.mainFont, 24.0f, 1.6f, false};
    }
}

void NpThemeInit(int width, int height) {
    g_theme.screenWidth = width;
    g_theme.screenHeight = height;

    // Build Unicode codepoints array for international character support
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    // Initialize SDK font system and use its path discovery
    LlzFontInit();

    // Try to load via SDK which searches all the correct paths
    g_theme.mainFont = GetFontDefault();
    const char *fontPath = LlzFontGetPath(LLZ_FONT_UI);
    if (fontPath) {
        Font loaded = LoadFontEx(fontPath, 48, codepoints, codepointCount);
        if (loaded.texture.id != 0) {
            g_theme.mainFont = loaded;
            SetTextureFilter(g_theme.mainFont.texture, TEXTURE_FILTER_BILINEAR);
            printf("NowPlaying: Loaded font %s with %d Unicode codepoints\n", fontPath, codepointCount);
        }
    }

    // Free codepoints array
    if (codepoints) {
        free(codepoints);
    }

    // Initialize all theme variants
    InitializePalettes();
    InitializeTypography();

    // Default to Zune theme
    g_theme.activeVariant = NP_THEME_VARIANT_ZUNE;
    g_theme.initialized = true;
}

void NpThemeShutdown(void) {
    Font defaultFont = GetFontDefault();
    if (g_theme.mainFont.texture.id != 0 && g_theme.mainFont.texture.id != defaultFont.texture.id) {
        UnloadFont(g_theme.mainFont);
    }
    g_theme.initialized = false;
}

// Theme variant management
void NpThemeSetVariant(NpThemeVariant variant) {
    if (variant < 0 || variant >= NP_THEME_VARIANT_COUNT) return;
    if (g_theme.activeVariant == (int)variant) return;
    g_theme.activeVariant = variant;
}

NpThemeVariant NpThemeGetVariant(void) {
    return (NpThemeVariant)g_theme.activeVariant;
}

const char* NpThemeGetVariantName(NpThemeVariant variant) {
    switch (variant) {
        case NP_THEME_VARIANT_ZUNE:          return "Zune";
        case NP_THEME_VARIANT_HIGH_CONTRAST: return "High Contrast";
        case NP_THEME_VARIANT_DIGITAL:       return "Digital";
        case NP_THEME_VARIANT_MATRIX:        return "Matrix";
        case NP_THEME_VARIANT_DAYTIME:       return "Daytime";
        case NP_THEME_VARIANT_PINK:          return "Pink";
        case NP_THEME_VARIANT_BLUE:          return "Blue";
        case NP_THEME_VARIANT_MOON:          return "Moon";
        default:                             return "Unknown";
    }
}

// Color functions
Color NpThemeGetColor(NpColorId id) {
    if (id < 0 || id >= NP_COLOR_COUNT) return WHITE;
    if (g_theme.hasCustomColor) {
        return g_theme.customTheme.colors[id].color;
    }
    return g_theme.variants[g_theme.activeVariant].colors[id].color;
}

Color NpThemeGetColorAlpha(NpColorId id, float alpha) {
    Color c = NpThemeGetColor(id);
    c.a = (unsigned char)(alpha * 255.0f);
    return c;
}

// Typography functions
void NpThemeDrawText(NpTypographyId typo, const char *text, Vector2 pos) {
    NpThemeDrawTextColored(typo, text, pos, NpThemeGetColor(NP_COLOR_TEXT_PRIMARY));
}

void NpThemeDrawTextColored(NpTypographyId typo, const char *text, Vector2 pos, Color color) {
    if (!text || typo < 0 || typo >= NP_TYPO_COUNT) return;
    TypographyStyle *style = &g_theme.variants[g_theme.activeVariant].typography[typo];

    const char *source = text;
    char buffer[256];
    if (style->uppercase) {
        size_t len = strlen(text);
        if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
        for (size_t i = 0; i < len; i++) buffer[i] = (char)toupper((unsigned char)text[i]);
        buffer[len] = '\0';
        source = buffer;
    }

    DrawTextEx(style->font, source, pos, style->fontSize, style->spacing, color);
}

float NpThemeMeasureTextWidth(NpTypographyId typo, const char *text) {
    if (!text || typo < 0 || typo >= NP_TYPO_COUNT) return 0.0f;
    TypographyStyle *style = &g_theme.variants[g_theme.activeVariant].typography[typo];
    Vector2 size = MeasureTextEx(style->font, text, style->fontSize, style->spacing);
    return size.x;
}

float NpThemeGetLineHeight(NpTypographyId typo) {
    if (typo < 0 || typo >= NP_TYPO_COUNT) return 20.0f;
    return g_theme.variants[g_theme.activeVariant].typography[typo].fontSize;
}

// Drawing functions
void NpThemeDrawBackground(void) {
    ClearBackground((Color){12, 5, 8, 255});

    int splitX = (int)(g_theme.screenWidth * 0.55f);
    Color start = NpThemeGetColor(NP_COLOR_BG_DARK);
    Color mid = NpThemeGetColor(NP_COLOR_BG_MEDIUM);
    Color end = NpThemeGetColor(NP_COLOR_BG_LIGHT);

    DrawRectangleGradientH(0, 0, splitX, g_theme.screenHeight, start, mid);
    DrawRectangleGradientH(splitX, 0, g_theme.screenWidth - splitX, g_theme.screenHeight, mid, end);
}

Font NpThemeGetFont(void) {
    return g_theme.mainFont;
}

// Helper function to calculate relative luminance
static float CalculateLuminance(Color c) {
    float r = c.r / 255.0f;
    float g = c.g / 255.0f;
    float b = c.b / 255.0f;

    // Apply gamma correction
    r = (r <= 0.03928f) ? r / 12.92f : powf((r + 0.055f) / 1.055f, 2.4f);
    g = (g <= 0.03928f) ? g / 12.92f : powf((g + 0.055f) / 1.055f, 2.4f);
    b = (b <= 0.03928f) ? b / 12.92f : powf((b + 0.055f) / 1.055f, 2.4f);

    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

// Helper to lighten/darken a color
static Color AdjustBrightness(Color base, float factor) {
    return (Color){
        (unsigned char)(base.r * factor > 255 ? 255 : base.r * factor),
        (unsigned char)(base.g * factor > 255 ? 255 : base.g * factor),
        (unsigned char)(base.b * factor > 255 ? 255 : base.b * factor),
        base.a
    };
}

// Helper to blend two colors
static Color BlendColors(Color a, Color b, float t) {
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

// Custom color functions
void NpThemeSetCustomBackgroundColor(Color bgColor) {
    g_theme.hasCustomColor = true;

    // Calculate luminance to determine if background is dark or light
    float luminance = CalculateLuminance(bgColor);
    bool isDark = luminance < 0.5f;

    // Generate background gradient colors
    g_theme.customTheme.colors[NP_COLOR_BG_DARK].color = bgColor;
    g_theme.customTheme.colors[NP_COLOR_BG_MEDIUM].color = AdjustBrightness(bgColor, isDark ? 1.3f : 0.9f);
    g_theme.customTheme.colors[NP_COLOR_BG_LIGHT].color = AdjustBrightness(bgColor, isDark ? 1.6f : 0.8f);

    // Generate panel colors
    if (isDark) {
        g_theme.customTheme.colors[NP_COLOR_PANEL].color = MakeColor(255, 255, 255, 30);
        g_theme.customTheme.colors[NP_COLOR_PANEL_HOVER].color = MakeColor(255, 255, 255, 60);
        g_theme.customTheme.colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(255, 255, 255, 25);
    } else {
        g_theme.customTheme.colors[NP_COLOR_PANEL].color = MakeColor(0, 0, 0, 25);
        g_theme.customTheme.colors[NP_COLOR_PANEL_HOVER].color = MakeColor(0, 0, 0, 50);
        g_theme.customTheme.colors[NP_COLOR_PANEL_SHEEN].color = MakeColor(255, 255, 255, 35);
    }

    // Generate accent color based on background
    Vector3 hsv = ColorToHSV(bgColor);
    Color accentBase = ColorFromHSV(fmodf(hsv.x + 180.0f, 360.0f),
                                     hsv.y * 0.8f + 0.2f,
                                     isDark ? 0.9f : 0.6f);
    g_theme.customTheme.colors[NP_COLOR_ACCENT].color = accentBase;
    g_theme.customTheme.colors[NP_COLOR_ACCENT_SOFT].color = Fade(accentBase, 0.3f);

    // Generate text colors based on luminance
    if (isDark) {
        g_theme.customTheme.colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(245, 245, 250, 255);
        g_theme.customTheme.colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(200, 200, 210, 180);
    } else {
        g_theme.customTheme.colors[NP_COLOR_TEXT_PRIMARY].color = MakeColor(20, 20, 25, 255);
        g_theme.customTheme.colors[NP_COLOR_TEXT_SECONDARY].color = MakeColor(60, 60, 70, 200);
    }

    // Border and overlay
    g_theme.customTheme.colors[NP_COLOR_BORDER].color = Fade(accentBase, 0.5f);
    g_theme.customTheme.colors[NP_COLOR_OVERLAY].color = isDark ?
        MakeColor(255, 255, 255, 20) : MakeColor(0, 0, 0, 20);

    // Copy typography from active variant
    memcpy(g_theme.customTheme.typography,
           g_theme.variants[g_theme.activeVariant].typography,
           sizeof(g_theme.customTheme.typography));
}

void NpThemeClearCustomBackgroundColor(void) {
    g_theme.hasCustomColor = false;
}

bool NpThemeHasCustomBackgroundColor(void) {
    return g_theme.hasCustomColor;
}
