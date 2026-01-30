#include "menu_theme_private.h"
#include "llz_sdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Font state
static Font g_menuFont;
static bool g_menuFontLoaded = false;

static Font g_omicronFont;
static bool g_omicronFontLoaded = false;

static Font g_tracklisterFont;
static bool g_tracklisterFontLoaded = false;

static Font g_ibrandFont;
static bool g_ibrandFontLoaded = false;

// Build Unicode codepoints for international character support
int* MenuThemeFontsBuildCodepoints(int *outCount)
{
    static const int ranges[][2] = {
        {0x0020, 0x007E},   // ASCII
        {0x00A0, 0x00FF},   // Latin-1 Supplement
        {0x0100, 0x017F},   // Latin Extended-A
        {0x0180, 0x024F},   // Latin Extended-B
        {0x0400, 0x04FF},   // Cyrillic
        {0x0500, 0x052F},   // Cyrillic Supplement
    };

    const int numRanges = sizeof(ranges) / sizeof(ranges[0]);
    int total = 0;
    for (int i = 0; i < numRanges; i++) {
        total += (ranges[i][1] - ranges[i][0] + 1);
    }

    int *codepoints = (int *)malloc(total * sizeof(int));
    if (!codepoints) {
        *outCount = 0;
        return NULL;
    }

    int idx = 0;
    for (int i = 0; i < numRanges; i++) {
        for (int cp = ranges[i][0]; cp <= ranges[i][1]; cp++) {
            codepoints[idx++] = cp;
        }
    }

    *outCount = total;
    return codepoints;
}

static void LoadMenuFont(void)
{
    if (g_menuFontLoaded) return;

    int codepointCount = 0;
    int *codepoints = MenuThemeFontsBuildCodepoints(&codepointCount);

    // Initialize SDK font system and use its path discovery
    LlzFontInit();

    // Try to load via SDK which searches all the correct paths
    const char *fontPath = LlzFontGetPath(LLZ_FONT_UI);
    if (fontPath) {
        Font loaded = LoadFontEx(fontPath, 48, codepoints, codepointCount);
        if (loaded.texture.id != 0) {
            g_menuFont = loaded;
            g_menuFontLoaded = true;
            SetTextureFilter(g_menuFont.texture, TEXTURE_FILTER_BILINEAR);
            printf("MenuTheme: Loaded font %s\n", fontPath);
        }
    }

    // Fallback to default if SDK font not found
    if (!g_menuFontLoaded) {
        g_menuFont = GetFontDefault();
        printf("MenuTheme: Using default font\n");
    }

    if (codepoints) free(codepoints);
}

static void LoadOmicronFont(void)
{
    if (g_omicronFontLoaded) return;

    int codepointCount = 0;
    int *codepoints = MenuThemeFontsBuildCodepoints(&codepointCount);

    const char *fontPaths[] = {
        "./fonts/Omicron Regular.otf",
        "./fonts/Omicron Light.otf",
        "/tmp/fonts/Omicron Regular.otf",
        "/tmp/fonts/Omicron Light.otf",
        "/var/local/fonts/Omicron Regular.otf",
        "/var/local/fonts/Omicron Light.otf",
    };

    for (int i = 0; i < 6; i++) {
        if (FileExists(fontPaths[i])) {
            Font loaded = LoadFontEx(fontPaths[i], 72, codepoints, codepointCount);
            if (loaded.texture.id != 0) {
                g_omicronFont = loaded;
                g_omicronFontLoaded = true;
                SetTextureFilter(g_omicronFont.texture, TEXTURE_FILTER_BILINEAR);
                printf("MenuTheme: Loaded Omicron font from %s\n", fontPaths[i]);
                break;
            }
        }
    }

    // Fallback to menu font if not found
    if (!g_omicronFontLoaded) {
        g_omicronFont = g_menuFont;
        printf("MenuTheme: Omicron font not found, using menu font\n");
    }

    if (codepoints) free(codepoints);
}

static void LoadTracklisterFont(void)
{
    if (g_tracklisterFontLoaded) return;

    int codepointCount = 0;
    int *codepoints = MenuThemeFontsBuildCodepoints(&codepointCount);

    const char *fontPaths[] = {
        "./fonts/Tracklister-Medium.ttf",
        "./fonts/Tracklister-Regular.ttf",
        "./fonts/Tracklister-Semibold.ttf",
        "/tmp/fonts/Tracklister-Medium.ttf",
        "/tmp/fonts/Tracklister-Regular.ttf",
        "/tmp/fonts/Tracklister-Semibold.ttf",
        "/var/local/fonts/Tracklister-Medium.ttf",
        "/var/local/fonts/Tracklister-Regular.ttf",
    };

    for (int i = 0; i < 8; i++) {
        if (FileExists(fontPaths[i])) {
            Font loaded = LoadFontEx(fontPaths[i], 72, codepoints, codepointCount);
            if (loaded.texture.id != 0) {
                g_tracklisterFont = loaded;
                g_tracklisterFontLoaded = true;
                SetTextureFilter(g_tracklisterFont.texture, TEXTURE_FILTER_BILINEAR);
                printf("MenuTheme: Loaded Tracklister font from %s\n", fontPaths[i]);
                break;
            }
        }
    }

    // Fallback to menu font if not found
    if (!g_tracklisterFontLoaded) {
        g_tracklisterFont = g_menuFont;
        printf("MenuTheme: Tracklister font not found, using menu font\n");
    }

    if (codepoints) free(codepoints);
}

static void LoadIBrandFont(void)
{
    if (g_ibrandFontLoaded) return;

    int codepointCount = 0;
    int *codepoints = MenuThemeFontsBuildCodepoints(&codepointCount);

    const char *fontPaths[] = {
        "./fonts/Ibrand.otf",
        "/tmp/fonts/Ibrand.otf",
        "/var/local/fonts/Ibrand.otf",
    };

    for (int i = 0; i < 3; i++) {
        if (FileExists(fontPaths[i])) {
            Font loaded = LoadFontEx(fontPaths[i], 72, codepoints, codepointCount);
            if (loaded.texture.id != 0) {
                g_ibrandFont = loaded;
                g_ibrandFontLoaded = true;
                SetTextureFilter(g_ibrandFont.texture, TEXTURE_FILTER_BILINEAR);
                printf("MenuTheme: Loaded iBrand font from %s\n", fontPaths[i]);
                break;
            }
        }
    }

    // Fallback to menu font if not found
    if (!g_ibrandFontLoaded) {
        g_ibrandFont = g_menuFont;
        printf("MenuTheme: iBrand font not found, using menu font\n");
    }

    if (codepoints) free(codepoints);
}

void MenuThemeFontsInit(void)
{
    // Load main menu font immediately
    LoadMenuFont();

    // Other fonts are lazy-loaded when their themes are used
}

void MenuThemeFontsShutdown(void)
{
    Font defaultFont = GetFontDefault();

    // Unload iBrand font
    if (g_ibrandFontLoaded && g_ibrandFont.texture.id != 0 &&
        g_ibrandFont.texture.id != defaultFont.texture.id &&
        g_ibrandFont.texture.id != g_menuFont.texture.id) {
        UnloadFont(g_ibrandFont);
    }
    g_ibrandFontLoaded = false;

    // Unload Tracklister font
    if (g_tracklisterFontLoaded && g_tracklisterFont.texture.id != 0 &&
        g_tracklisterFont.texture.id != defaultFont.texture.id &&
        g_tracklisterFont.texture.id != g_menuFont.texture.id) {
        UnloadFont(g_tracklisterFont);
    }
    g_tracklisterFontLoaded = false;

    // Unload Omicron font
    if (g_omicronFontLoaded && g_omicronFont.texture.id != 0 &&
        g_omicronFont.texture.id != defaultFont.texture.id &&
        g_omicronFont.texture.id != g_menuFont.texture.id) {
        UnloadFont(g_omicronFont);
    }
    g_omicronFontLoaded = false;

    // Unload menu font
    if (g_menuFontLoaded && g_menuFont.texture.id != 0 &&
        g_menuFont.texture.id != defaultFont.texture.id) {
        UnloadFont(g_menuFont);
    }
    g_menuFontLoaded = false;
}

Font MenuThemeFontsGetMenu(void)
{
    if (!g_menuFontLoaded) {
        LoadMenuFont();
    }
    return g_menuFont;
}

Font MenuThemeFontsGetOmicron(void)
{
    if (!g_omicronFontLoaded) {
        LoadOmicronFont();
    }
    return g_omicronFont;
}

Font MenuThemeFontsGetTracklister(void)
{
    if (!g_tracklisterFontLoaded) {
        LoadTracklisterFont();
    }
    return g_tracklisterFont;
}

Font MenuThemeFontsGetIBrand(void)
{
    if (!g_ibrandFontLoaded) {
        LoadIBrandFont();
    }
    return g_ibrandFont;
}
