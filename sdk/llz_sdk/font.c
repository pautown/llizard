/**
 * font.c - Font Loading Module Implementation
 *
 * Provides centralized font management for the llizardgui SDK.
 * Handles font discovery across CarThing and desktop environments.
 */

#include "llz_sdk_font.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Internal Constants
// ============================================================================

#define MAX_CACHED_FONTS 16
#define DEFAULT_FONT_SIZE 20
#define DEFAULT_GLYPH_COUNT 256

// Font file names
static const char* FONT_FILENAMES[LLZ_FONT_COUNT] = {
    "ZegoeUI-U.ttf",         // LLZ_FONT_UI
    "ZegoeUI-U.ttf",         // LLZ_FONT_UI_BOLD (same file, bold via rendering)
    "DejaVuSansMono.ttf",    // LLZ_FONT_MONO
    "QuincyCapsRegular.ttf", // LLZ_FONT_DISPLAY
    "FlangeB.ttf"            // LLZ_FONT_ACCENT
};

// Search paths for fonts (in priority order)
static const char* FONT_SEARCH_PATHS[] = {
    // CarThing system fonts
    "/var/local/fonts/",
    // llizardOS system partition fonts
    "/usr/lib/llizard/data/fonts/",
    // Deployed fonts
    "/tmp/fonts/",
    // Desktop development paths
    "./fonts/",
    "../fonts/",
    "../../fonts/",
    // System fonts (fallback)
    "/usr/share/fonts/truetype/dejavu/",
    "/usr/share/fonts/TTF/",
    NULL
};

// ============================================================================
// Internal State
// ============================================================================

typedef struct {
    Font font;
    LlzFontType type;
    int size;
    bool inUse;
} CachedFont;

static struct {
    bool initialized;
    char fontDirectory[512];
    char fontPaths[LLZ_FONT_COUNT][512];
    CachedFont cache[MAX_CACHED_FONTS];
    Font defaultFont;
    bool defaultFontLoaded;
} g_fontState = {0};

// ============================================================================
// Internal Functions
// ============================================================================

static bool FontFileExists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

static bool FindFontFile(LlzFontType type, char* outPath, int outPathSize) {
    const char* filename = FONT_FILENAMES[type];

    for (int i = 0; FONT_SEARCH_PATHS[i] != NULL; i++) {
        snprintf(outPath, outPathSize, "%s%s", FONT_SEARCH_PATHS[i], filename);
        if (FontFileExists(outPath)) {
            return true;
        }
    }

    // Fallback for mono font - try DejaVu alternatives
    if (type == LLZ_FONT_MONO) {
        const char* monoFallbacks[] = {
            "DejaVuSansMono.ttf",
            "DejaVuSans-Bold.ttf",
            "DejaVuSans.ttf",
            NULL
        };

        for (int i = 0; monoFallbacks[i] != NULL; i++) {
            for (int j = 0; FONT_SEARCH_PATHS[j] != NULL; j++) {
                snprintf(outPath, outPathSize, "%s%s", FONT_SEARCH_PATHS[j], monoFallbacks[i]);
                if (FontFileExists(outPath)) {
                    return true;
                }
            }
        }
    }

    // Fallback for UI font - try various alternatives
    if (type == LLZ_FONT_UI || type == LLZ_FONT_UI_BOLD) {
        const char* uiFallbacks[] = {
            "ZegoeUI-U.ttf",
            "DejaVuSans-Bold.ttf",
            "DejaVuSans.ttf",
            "DejaVuSerif-Bold.ttf",
            "DejaVuSerif.ttf",
            NULL
        };

        for (int i = 0; uiFallbacks[i] != NULL; i++) {
            for (int j = 0; FONT_SEARCH_PATHS[j] != NULL; j++) {
                snprintf(outPath, outPathSize, "%s%s", FONT_SEARCH_PATHS[j], uiFallbacks[i]);
                if (FontFileExists(outPath)) {
                    return true;
                }
            }
        }
    }

    // Fallback for Display font - try caps/bold alternatives
    if (type == LLZ_FONT_DISPLAY) {
        const char* displayFallbacks[] = {
            "QuincyCapsRegular.ttf",
            "ZegoeCapsBold.ttf",
            "ZegoeUI-UBold.ttf",
            "DejaVuSans-Bold.ttf",
            NULL
        };

        for (int i = 0; displayFallbacks[i] != NULL; i++) {
            for (int j = 0; FONT_SEARCH_PATHS[j] != NULL; j++) {
                snprintf(outPath, outPathSize, "%s%s", FONT_SEARCH_PATHS[j], displayFallbacks[i]);
                if (FontFileExists(outPath)) {
                    return true;
                }
            }
        }
    }

    // Fallback for Accent font - try bold alternatives
    if (type == LLZ_FONT_ACCENT) {
        const char* accentFallbacks[] = {
            "FlangeB.ttf",
            "ZegoeUI-UBold.ttf",
            "ZegoeCapsBold.ttf",
            "DejaVuSans-Bold.ttf",
            NULL
        };

        for (int i = 0; accentFallbacks[i] != NULL; i++) {
            for (int j = 0; FONT_SEARCH_PATHS[j] != NULL; j++) {
                snprintf(outPath, outPathSize, "%s%s", FONT_SEARCH_PATHS[j], accentFallbacks[i]);
                if (FontFileExists(outPath)) {
                    return true;
                }
            }
        }
    }

    outPath[0] = '\0';
    return false;
}

static Font LoadFontInternal(const char* path, int size, int* codepoints, int codepointCount) {
    Font font = {0};

    if (!path || path[0] == '\0') {
        printf("[LlzFont] No font path provided, using default\n");
        return GetFontDefault();
    }

    // Use default ASCII codepoints if none provided
    int defaultCodepoints[DEFAULT_GLYPH_COUNT];
    if (!codepoints || codepointCount <= 0) {
        for (int i = 0; i < DEFAULT_GLYPH_COUNT; i++) {
            defaultCodepoints[i] = i;
        }
        codepoints = defaultCodepoints;
        codepointCount = DEFAULT_GLYPH_COUNT;
    }

    font = LoadFontEx(path, size, codepoints, codepointCount);

    if (font.texture.id != 0) {
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
        printf("[LlzFont] Loaded font: %s (%dpx)\n", path, size);
    } else {
        printf("[LlzFont] WARNING: Failed to load font: %s\n", path);
        font = GetFontDefault();
    }

    return font;
}

static CachedFont* FindCachedFont(LlzFontType type, int size) {
    for (int i = 0; i < MAX_CACHED_FONTS; i++) {
        if (g_fontState.cache[i].inUse &&
            g_fontState.cache[i].type == type &&
            g_fontState.cache[i].size == size) {
            return &g_fontState.cache[i];
        }
    }
    return NULL;
}

static CachedFont* GetFreeCacheSlot(void) {
    for (int i = 0; i < MAX_CACHED_FONTS; i++) {
        if (!g_fontState.cache[i].inUse) {
            return &g_fontState.cache[i];
        }
    }
    return NULL;
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool LlzFontInit(void) {
    if (g_fontState.initialized) {
        return true;
    }

    printf("[LlzFont] Initializing font system...\n");

    // Clear state
    memset(&g_fontState, 0, sizeof(g_fontState));

    // Find font files for each type
    bool foundAny = false;
    static const char* fontTypeNames[] = {"UI", "UI Bold", "Mono", "Display", "Accent"};
    for (int i = 0; i < LLZ_FONT_COUNT; i++) {
        if (FindFontFile((LlzFontType)i, g_fontState.fontPaths[i], sizeof(g_fontState.fontPaths[i]))) {
            foundAny = true;
            printf("[LlzFont] Found %s font: %s\n", fontTypeNames[i], g_fontState.fontPaths[i]);

            // Set directory from first found font
            if (g_fontState.fontDirectory[0] == '\0') {
                strncpy(g_fontState.fontDirectory, g_fontState.fontPaths[i], sizeof(g_fontState.fontDirectory) - 1);
                char* lastSlash = strrchr(g_fontState.fontDirectory, '/');
                if (lastSlash) {
                    *(lastSlash + 1) = '\0';
                }
            }
        }
    }

    if (!foundAny) {
        printf("[LlzFont] WARNING: No font files found, will use raylib default\n");
        strcpy(g_fontState.fontDirectory, "./fonts/");
    }

    g_fontState.initialized = true;
    return foundAny;
}

void LlzFontShutdown(void) {
    if (!g_fontState.initialized) {
        return;
    }

    printf("[LlzFont] Shutting down font system...\n");

    // Unload cached fonts
    for (int i = 0; i < MAX_CACHED_FONTS; i++) {
        if (g_fontState.cache[i].inUse) {
            if (g_fontState.cache[i].font.texture.id != GetFontDefault().texture.id) {
                UnloadFont(g_fontState.cache[i].font);
            }
            g_fontState.cache[i].inUse = false;
        }
    }

    // Unload default font if loaded
    if (g_fontState.defaultFontLoaded &&
        g_fontState.defaultFont.texture.id != GetFontDefault().texture.id) {
        UnloadFont(g_fontState.defaultFont);
    }

    memset(&g_fontState, 0, sizeof(g_fontState));
}

bool LlzFontIsReady(void) {
    return g_fontState.initialized;
}

Font LlzFontGetDefault(void) {
    if (!g_fontState.initialized) {
        LlzFontInit();
    }

    if (!g_fontState.defaultFontLoaded) {
        g_fontState.defaultFont = LoadFontInternal(
            g_fontState.fontPaths[LLZ_FONT_UI],
            DEFAULT_FONT_SIZE,
            NULL, 0
        );
        g_fontState.defaultFontLoaded = true;
    }

    return g_fontState.defaultFont;
}

Font LlzFontGet(LlzFontType type, int size) {
    if (!g_fontState.initialized) {
        LlzFontInit();
    }

    if (type < 0 || type >= LLZ_FONT_COUNT) {
        type = LLZ_FONT_UI;
    }

    // Check cache
    CachedFont* cached = FindCachedFont(type, size);
    if (cached) {
        return cached->font;
    }

    // Load new font
    Font font = LoadFontInternal(g_fontState.fontPaths[type], size, NULL, 0);

    // Cache it
    CachedFont* slot = GetFreeCacheSlot();
    if (slot) {
        slot->font = font;
        slot->type = type;
        slot->size = size;
        slot->inUse = true;
    }

    return font;
}

Font LlzFontLoadCustom(LlzFontType type, int size, int* codepoints, int codepointCount) {
    if (!g_fontState.initialized) {
        LlzFontInit();
    }

    if (type < 0 || type >= LLZ_FONT_COUNT) {
        type = LLZ_FONT_UI;
    }

    return LoadFontInternal(g_fontState.fontPaths[type], size, codepoints, codepointCount);
}

const char* LlzFontGetPath(LlzFontType type) {
    if (!g_fontState.initialized) {
        LlzFontInit();
    }

    if (type < 0 || type >= LLZ_FONT_COUNT) {
        return NULL;
    }

    if (g_fontState.fontPaths[type][0] == '\0') {
        return NULL;
    }

    return g_fontState.fontPaths[type];
}

const char* LlzFontGetDirectory(void) {
    if (!g_fontState.initialized) {
        LlzFontInit();
    }

    return g_fontState.fontDirectory;
}

// ============================================================================
// Text Drawing Helpers
// ============================================================================

void LlzDrawText(const char* text, int x, int y, int fontSize, Color color) {
    Font font = LlzFontGet(LLZ_FONT_UI, fontSize);
    float spacing = fontSize * 0.05f;
    DrawTextEx(font, text, (Vector2){x, y}, fontSize, spacing, color);
}

void LlzDrawTextCentered(const char* text, int centerX, int y, int fontSize, Color color) {
    int width = LlzMeasureText(text, fontSize);
    LlzDrawText(text, centerX - width / 2, y, fontSize, color);
}

void LlzDrawTextShadow(const char* text, int x, int y, int fontSize, Color color, Color shadowColor) {
    LlzDrawText(text, x + 1, y + 1, fontSize, shadowColor);
    LlzDrawText(text, x, y, fontSize, color);
}

int LlzMeasureText(const char* text, int fontSize) {
    Font font = LlzFontGet(LLZ_FONT_UI, fontSize);
    float spacing = fontSize * 0.05f;
    Vector2 size = MeasureTextEx(font, text, fontSize, spacing);
    return (int)size.x;
}

Vector2 LlzMeasureTextEx(const char* text, int fontSize) {
    Font font = LlzFontGet(LLZ_FONT_UI, fontSize);
    float spacing = fontSize * 0.05f;
    return MeasureTextEx(font, text, fontSize, spacing);
}
