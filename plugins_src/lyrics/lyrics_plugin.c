/**
 * Lyrics Plugin - Apple Music-inspired lyrics display for llizardgui-host
 *
 * Displays synced and unsynced lyrics with smooth animations,
 * album art backgrounds, and dynamic color theming.
 *
 * Controls:
 *   SELECT     - Cycle display style (Centered, Full Screen, Minimalist, Karaoke)
 *   Button 2   - Cycle background modes (Off, Album Art, Animated styles...)
 *   Button 3   - Cycle text visibility modes
 *   DOWN       - Cycle font size emphasis
 *   SCROLL     - Adjust volume
 *   DRAG       - Seek through synced lyrics
 *   BACK       - Return to Now Playing
 */

#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "llz_sdk_navigation.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <webp/decode.h>

// ============================================================================
// Display Style Definitions
// ============================================================================

typedef enum {
    LYRICS_STYLE_CENTERED = 0,    // Classic centered scrolling (Apple Music default)
    LYRICS_STYLE_FULL_SCREEN,     // All visible lines shown
    LYRICS_STYLE_MINIMALIST,      // Just current line, very clean
    LYRICS_STYLE_KARAOKE,         // Current line at bottom, upcoming above
    LYRICS_STYLE_COUNT
} LyricsDisplayStyle;

static const char *STYLE_NAMES[] = {
    "Centered",
    "Full Screen",
    "Minimalist",
    "Karaoke"
};

// ============================================================================
// Background Mode Definitions
// ============================================================================

typedef enum {
    BG_MODE_OFF = 0,              // Solid dark background
    BG_MODE_ALBUM_ART,            // Blurred album art (Apple Music style)
    BG_MODE_ANIMATED_START,       // Start of animated styles
    // Animated styles follow from here
} BackgroundMode;

// ============================================================================
// Text Visibility Modes
// ============================================================================

typedef enum {
    VISIBILITY_ALL = 0,           // All lines visible with fade
    VISIBILITY_CURRENT_ONLY,      // Only current line visible
    VISIBILITY_CURRENT_NEXT,      // Current and next 2 lines
    VISIBILITY_SPOTLIGHT,         // Spotlight effect - bright current, very dim others
    VISIBILITY_COUNT
} TextVisibilityMode;

static const char *VISIBILITY_NAMES[] = {
    "All Lines",
    "Current Only",
    "Current + Next",
    "Spotlight"
};

// ============================================================================
// Font Size Emphasis
// ============================================================================

typedef enum {
    SIZE_RATIO_SUBTLE = 0,        // +6pt for current
    SIZE_RATIO_NORMAL,            // +10pt for current
    SIZE_RATIO_BOLD,              // +16pt for current
    SIZE_RATIO_EXTREME,           // +24pt for current
    SIZE_RATIO_COUNT
} FontSizeRatio;

static const float SIZE_DIFF[] = {6.0f, 10.0f, 16.0f, 24.0f};
static const char *SIZE_NAMES[] = {
    "Subtle",
    "Normal",
    "Bold",
    "Extreme"
};

// ============================================================================
// Configuration Constants (Apple Music inspired)
// ============================================================================

#define LYRICS_BASE_LINE_HEIGHT 48.0f     // Base height per line of text
#define LYRICS_LINE_SPACING 1.3f          // Multiplier for space between logical lyrics
#define LYRICS_BASE_FONT_SIZE 26.0f
#define LYRICS_SCROLL_SPEED 6.0f          // Smoother scroll
#define LYRICS_SCROLL_EASE_FACTOR 0.12f   // Easing for smooth deceleration
#define LYRICS_FADE_DISTANCE 4
#define LYRICS_VISIBLE_LINES_BEFORE 6
#define LYRICS_VISIBLE_LINES_AFTER 6

#define LYRICS_HORIZONTAL_PADDING 40.0f   // Padding on each side for text wrapping
#define LYRICS_MAX_WRAPPED_LINES 4        // Max lines a single lyric can wrap to

#define INDICATOR_SHOW_DURATION 2.0f
#define ALBUM_ART_FADE_SPEED 2.5f         // Crossfade speed

// Apple Music-inspired colors
static const Color COLOR_BG = {10, 10, 14, 255};
static const Color COLOR_TEXT_PRIMARY = {255, 255, 255, 255};
static const Color COLOR_TEXT_SECONDARY = {160, 160, 175, 255};
static const Color COLOR_TEXT_DIM = {100, 100, 115, 255};
static const Color COLOR_ACCENT_DEFAULT = {255, 45, 85, 255};  // Apple Music pink/red

// ============================================================================
// Text Wrapping Structures
// ============================================================================

#define MAX_WRAP_LINE_LEN 256
#define MAX_WRAP_LINES 4

typedef struct {
    char lines[MAX_WRAP_LINES][MAX_WRAP_LINE_LEN];
    int lineCount;
    float totalHeight;
} WrappedText;

// ============================================================================
// Album Art & Color State
// ============================================================================

typedef struct {
    Texture2D texture;
    Texture2D blurred;
    bool loaded;
    char loadedPath[256];
} AlbumArtState;

typedef struct {
    Texture2D texture;
    Texture2D blurred;
    float alpha;
} AlbumArtTransition;

typedef struct {
    Color primary;
    Color accent;
    Color textPrimary;
    Color textSecondary;
    Color glow;
    bool hasColors;
} DynamicColors;

// ============================================================================
// Plugin State
// ============================================================================

static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;

// Display settings
static LyricsDisplayStyle g_displayStyle = LYRICS_STYLE_CENTERED;
static TextVisibilityMode g_visibilityMode = VISIBILITY_ALL;
static FontSizeRatio g_sizeRatio = SIZE_RATIO_NORMAL;

// Background state
static BackgroundMode g_bgMode = BG_MODE_ALBUM_ART;
static int g_animatedBgIndex = 0;  // Which animated style when in animated mode

// Album art state
static AlbumArtState g_albumArt = {0};
static AlbumArtTransition g_prevAlbumArt = {0};
static float g_currentAlbumArtAlpha = 1.0f;
static bool g_inAlbumArtTransition = false;

// Dynamic colors from album art
static DynamicColors g_colors = {0};

// Indicator overlay
static float g_indicatorTimer = 0.0f;
static char g_indicatorText[128] = {0};

// Lyrics state
static LlzLyricsData g_lyrics = {0};
static bool g_lyricsLoaded = false;
static char g_currentHash[64] = {0};      // Hash of currently loaded lyrics
static char g_priorTrackHash[64] = {0};   // Hash of lyrics from previous track (before track change)
static bool g_lyricsStale = false;        // True if loaded lyrics are from previous track
static int g_currentLineIndex = 0;
static float g_scrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;
static float g_scrollVelocity = 0.0f;
static bool g_hasLyrics = false;
static bool g_isSynced = false;

// Pre-calculated line positions for variable height scrolling
static float *g_lineYPositions = NULL;  // Y position of each line
static float g_totalLyricsHeight = 0.0f;

// Line highlighting animation
static float g_lineHighlightProgress = 0.0f;  // 0 to 1 for current line emphasis
static int g_lastHighlightedLine = -1;

// Track info
static char g_trackTitle[256] = {0};
static char g_trackArtist[256] = {0};
static char g_trackAlbumArtPath[256] = {0};

// Font
static Font g_font = {0};
static bool g_fontLoaded = false;

// Scrubbing/Seeking state (for synced lyrics only)
static bool g_isScrubbing = false;
static float g_scrubStartY = 0.0f;
static float g_scrubStartScrollOffset = 0.0f;
static int g_scrubTargetLine = 0;
static float g_scrubTargetSeconds = 0.0f;
static float g_trackDuration = 0.0f;

// Seek cooldown to prevent snap-back and accidental actions after seeking
// Needs to be long enough for seek to propagate through Redis/BLE to phone and back
static bool g_justSeeked = false;
static float g_justSeekedTimer = 0.0f;
static const float JUST_SEEKED_COOLDOWN = 1.5f;

// Plugin config for persistent settings
static LlzPluginConfig g_pluginConfig;
static bool g_pluginConfigInitialized = false;

// Volume state
static int g_currentVolume = 50;  // 0-100, synced from media state

// Volume overlay animation
static float g_volumeOverlayTimer = 0.0f;
static float g_volumeOverlayAlpha = 0.0f;
#define VOLUME_OVERLAY_DURATION 1.5f

// ============================================================================
// Helper Functions
// ============================================================================

static float Clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int Clampi(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static float Lerpf(float a, float b, float t) {
    return a + (b - a) * Clampf(t, 0.0f, 1.0f);
}

// Smooth step function for easing
static float SmoothStep(float t) {
    t = Clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static Color ColorWithAlpha(Color c, float alpha) {
    return (Color){c.r, c.g, c.b, (unsigned char)(Clampf(alpha, 0.0f, 1.0f) * c.a)};
}

static void ShowIndicator(const char *text) {
    strncpy(g_indicatorText, text, sizeof(g_indicatorText) - 1);
    g_indicatorText[sizeof(g_indicatorText) - 1] = '\0';
    g_indicatorTimer = INDICATOR_SHOW_DURATION;
}

// ============================================================================
// Config Save/Load Functions
// ============================================================================

// Save all appearance settings to config
static void SavePluginSettings(void) {
    if (!g_pluginConfigInitialized) return;

    LlzPluginConfigSetInt(&g_pluginConfig, "display_style", (int)g_displayStyle);
    LlzPluginConfigSetInt(&g_pluginConfig, "visibility_mode", (int)g_visibilityMode);
    LlzPluginConfigSetInt(&g_pluginConfig, "size_ratio", (int)g_sizeRatio);
    LlzPluginConfigSetInt(&g_pluginConfig, "bg_mode", (int)g_bgMode);
    LlzPluginConfigSetInt(&g_pluginConfig, "animated_bg_index", g_animatedBgIndex);

    LlzPluginConfigSave(&g_pluginConfig);
    printf("[LYRICS] Settings saved\n");
}

// Load settings from config (call after defaults are set)
static void LoadPluginSettings(void) {
    if (!g_pluginConfigInitialized) return;

    // Load display style
    int displayStyle = LlzPluginConfigGetInt(&g_pluginConfig, "display_style", LYRICS_STYLE_CENTERED);
    if (displayStyle >= 0 && displayStyle < LYRICS_STYLE_COUNT) {
        g_displayStyle = (LyricsDisplayStyle)displayStyle;
    }

    // Load visibility mode
    int visibilityMode = LlzPluginConfigGetInt(&g_pluginConfig, "visibility_mode", VISIBILITY_ALL);
    if (visibilityMode >= 0 && visibilityMode < VISIBILITY_COUNT) {
        g_visibilityMode = (TextVisibilityMode)visibilityMode;
    }

    // Load font size ratio
    int sizeRatio = LlzPluginConfigGetInt(&g_pluginConfig, "size_ratio", SIZE_RATIO_NORMAL);
    if (sizeRatio >= 0 && sizeRatio < SIZE_RATIO_COUNT) {
        g_sizeRatio = (FontSizeRatio)sizeRatio;
    }

    // Load background mode
    int bgMode = LlzPluginConfigGetInt(&g_pluginConfig, "bg_mode", BG_MODE_ALBUM_ART);
    if (bgMode >= BG_MODE_OFF && bgMode <= BG_MODE_ANIMATED_START + LLZ_BG_STYLE_COUNT) {
        g_bgMode = (BackgroundMode)bgMode;
    }

    // Load animated background index
    int animatedBgIndex = LlzPluginConfigGetInt(&g_pluginConfig, "animated_bg_index", 0);
    if (animatedBgIndex >= 0 && animatedBgIndex < LLZ_BG_STYLE_COUNT) {
        g_animatedBgIndex = animatedBgIndex;
    }

    // Apply background settings
    if (g_bgMode >= BG_MODE_ANIMATED_START) {
        LlzBackgroundSetEnabled(true);
        LlzBackgroundSetStyle((LlzBackgroundStyle)g_animatedBgIndex, false);
        if (g_colors.hasColors) {
            LlzBackgroundSetColors(g_colors.primary, g_colors.accent);
        }
    } else {
        LlzBackgroundSetEnabled(false);
    }

    printf("[LYRICS] Loaded settings: display_style=%d, visibility=%d, size_ratio=%d, bg_mode=%d, animated_bg=%d\n",
           g_displayStyle, g_visibilityMode, g_sizeRatio, g_bgMode, g_animatedBgIndex);
}

// ============================================================================
// Text Wrapping Functions
// ============================================================================

static float GetMaxTextWidth(void) {
    return (float)g_screenWidth - (LYRICS_HORIZONTAL_PADDING * 2.0f);
}

static void WrapText(const char *text, float fontSize, float spacing, WrappedText *out) {
    memset(out, 0, sizeof(WrappedText));

    if (!text || text[0] == '\0') {
        out->lineCount = 0;
        out->totalHeight = 0;
        return;
    }

    float maxWidth = GetMaxTextWidth();
    float lineHeight = fontSize * 1.3f;  // Line height for wrapped text

    // Check if text fits on one line
    Vector2 fullMeasure = MeasureTextEx(g_font, text, fontSize, spacing);
    if (fullMeasure.x <= maxWidth) {
        strncpy(out->lines[0], text, MAX_WRAP_LINE_LEN - 1);
        out->lineCount = 1;
        out->totalHeight = lineHeight;
        return;
    }

    // Need to wrap - split by words
    char tempLine[MAX_WRAP_LINE_LEN] = {0};
    char testLine[MAX_WRAP_LINE_LEN] = {0};
    const char *ptr = text;
    int currentLine = 0;

    while (*ptr && currentLine < MAX_WRAP_LINES) {
        // Skip leading spaces
        while (*ptr == ' ') ptr++;
        if (*ptr == '\0') break;

        tempLine[0] = '\0';

        while (*ptr && currentLine < MAX_WRAP_LINES) {
            // Find next word
            const char *wordStart = ptr;
            while (*ptr && *ptr != ' ') ptr++;
            int wordLen = (int)(ptr - wordStart);

            if (wordLen == 0) break;

            // Test if adding this word fits
            if (tempLine[0] != '\0') {
                snprintf(testLine, sizeof(testLine), "%s %.*s", tempLine, wordLen, wordStart);
            } else {
                snprintf(testLine, sizeof(testLine), "%.*s", wordLen, wordStart);
            }

            Vector2 testMeasure = MeasureTextEx(g_font, testLine, fontSize, spacing);

            if (testMeasure.x <= maxWidth) {
                // Word fits, add it
                strcpy(tempLine, testLine);
            } else {
                // Word doesn't fit
                if (tempLine[0] != '\0') {
                    // Save current line and start new one with this word
                    strncpy(out->lines[currentLine], tempLine, MAX_WRAP_LINE_LEN - 1);
                    currentLine++;

                    if (currentLine < MAX_WRAP_LINES) {
                        snprintf(tempLine, sizeof(tempLine), "%.*s", wordLen, wordStart);
                    }
                } else {
                    // Single word too long - force it and truncate if needed
                    snprintf(out->lines[currentLine], MAX_WRAP_LINE_LEN - 1, "%.*s", wordLen, wordStart);
                    currentLine++;
                    tempLine[0] = '\0';
                }
            }

            // Skip spaces after word
            while (*ptr == ' ') ptr++;
        }

        // Save any remaining text in tempLine
        if (tempLine[0] != '\0' && currentLine < MAX_WRAP_LINES) {
            strncpy(out->lines[currentLine], tempLine, MAX_WRAP_LINE_LEN - 1);
            currentLine++;
        }

        break;  // Only process once
    }

    out->lineCount = currentLine;
    out->totalHeight = currentLine * lineHeight;
}

// Calculate the height a lyrics line will take at a given font size
static float CalculateLineHeight(const char *text, float fontSize, float spacing) {
    if (!text || text[0] == '\0') return LYRICS_BASE_LINE_HEIGHT;

    float maxWidth = GetMaxTextWidth();
    Vector2 measure = MeasureTextEx(g_font, text, fontSize, spacing);

    if (measure.x <= maxWidth) {
        // Single line
        return fontSize * LYRICS_LINE_SPACING;
    }

    // Wrapped text - estimate line count
    WrappedText wrapped;
    WrapText(text, fontSize, spacing, &wrapped);
    return wrapped.totalHeight + (fontSize * 0.3f);  // Extra padding for wrapped text
}

// ============================================================================
// Color Extraction from Album Art
// ============================================================================

static void ExtractColorsFromImage(Image img) {
    if (img.data == NULL || img.width <= 0 || img.height <= 0) {
        g_colors.hasColors = false;
        return;
    }

    // Sample colors from the image
    int sampleCount = 0;
    float totalR = 0, totalG = 0, totalB = 0;
    float maxSat = 0;
    Color vibrantColor = {0};

    int sampleStep = (img.width * img.height > 10000) ? 8 : 4;

    for (int y = 0; y < img.height; y += sampleStep) {
        for (int x = 0; x < img.width; x += sampleStep) {
            Color pixel = GetImageColor(img, x, y);

            // Skip very dark or very bright pixels
            float brightness = (pixel.r + pixel.g + pixel.b) / 3.0f / 255.0f;
            if (brightness < 0.1f || brightness > 0.95f) continue;

            totalR += pixel.r;
            totalG += pixel.g;
            totalB += pixel.b;
            sampleCount++;

            // Track most saturated color for accent
            Vector3 hsv = ColorToHSV(pixel);
            if (hsv.y > maxSat && hsv.z > 0.3f) {
                maxSat = hsv.y;
                vibrantColor = pixel;
            }
        }
    }

    if (sampleCount == 0) {
        g_colors.hasColors = false;
        return;
    }

    // Calculate average color
    Color avgColor = {
        (unsigned char)(totalR / sampleCount),
        (unsigned char)(totalG / sampleCount),
        (unsigned char)(totalB / sampleCount),
        255
    };

    // Create accent color (boosted saturation from vibrant or average)
    Vector3 accentHSV;
    if (maxSat > 0.2f) {
        accentHSV = ColorToHSV(vibrantColor);
    } else {
        accentHSV = ColorToHSV(avgColor);
    }

    // Boost saturation and brightness for accent
    accentHSV.y = fminf(accentHSV.y + 0.35f, 1.0f);
    accentHSV.z = fminf(accentHSV.z + 0.15f, 0.95f);
    Color accent = ColorFromHSV(accentHSV.x, accentHSV.y, accentHSV.z);

    // Create glow color (lighter, more transparent)
    Vector3 glowHSV = accentHSV;
    glowHSV.z = fminf(glowHSV.z + 0.2f, 1.0f);
    Color glow = ColorFromHSV(glowHSV.x, glowHSV.y * 0.8f, glowHSV.z);

    g_colors.primary = avgColor;
    g_colors.accent = accent;
    g_colors.textPrimary = COLOR_TEXT_PRIMARY;
    g_colors.textSecondary = COLOR_TEXT_SECONDARY;
    g_colors.glow = glow;
    g_colors.hasColors = true;

    // Update background system colors
    if (g_bgMode >= BG_MODE_ANIMATED_START) {
        LlzBackgroundSetColors(avgColor, accent);
    }

    printf("[LYRICS] Extracted colors - Primary: (%d,%d,%d) Accent: (%d,%d,%d)\n",
           avgColor.r, avgColor.g, avgColor.b, accent.r, accent.g, accent.b);
}

// ============================================================================
// Album Art Loading
// ============================================================================

static void CleanupPrevAlbumArt(void) {
    if (g_prevAlbumArt.texture.id != 0) {
        UnloadTexture(g_prevAlbumArt.texture);
        memset(&g_prevAlbumArt.texture, 0, sizeof(Texture2D));
    }
    if (g_prevAlbumArt.blurred.id != 0) {
        UnloadTexture(g_prevAlbumArt.blurred);
        memset(&g_prevAlbumArt.blurred, 0, sizeof(Texture2D));
    }
}

static void UnloadAlbumArt(void) {
    if (g_albumArt.loaded && g_albumArt.texture.id != 0) {
        // Move current to prev for crossfade
        CleanupPrevAlbumArt();
        g_prevAlbumArt.texture = g_albumArt.texture;
        g_prevAlbumArt.blurred = g_albumArt.blurred;
        g_prevAlbumArt.alpha = g_currentAlbumArtAlpha;
        g_currentAlbumArtAlpha = 0.0f;
        g_inAlbumArtTransition = true;

        memset(&g_albumArt.texture, 0, sizeof(Texture2D));
        memset(&g_albumArt.blurred, 0, sizeof(Texture2D));
    }
    g_albumArt.loaded = false;
    g_albumArt.loadedPath[0] = '\0';
}

// Check if file path has WebP extension
static bool IsWebPFile(const char *path) {
    if (!path) return false;
    size_t len = strlen(path);
    if (len < 5) return false;
    const char *ext = path + len - 5;
    return (strcmp(ext, ".webp") == 0 || strcmp(ext, ".WEBP") == 0);
}

// Load WebP image file and convert to raylib Image format
static Image LoadImageWebP(const char *path) {
    Image image = {0};

    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("[LYRICS] LoadImageWebP: failed to open file '%s'\n", path);
        return image;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file data
    uint8_t *fileData = (uint8_t *)malloc(fileSize);
    if (!fileData) {
        printf("[LYRICS] LoadImageWebP: failed to allocate %ld bytes\n", fileSize);
        fclose(file);
        return image;
    }

    size_t bytesRead = fread(fileData, 1, fileSize, file);
    fclose(file);

    if (bytesRead != (size_t)fileSize) {
        printf("[LYRICS] LoadImageWebP: read %zu bytes, expected %ld\n", bytesRead, fileSize);
        free(fileData);
        return image;
    }

    // Decode WebP
    int width = 0, height = 0;
    uint8_t *rgbaData = WebPDecodeRGBA(fileData, fileSize, &width, &height);
    free(fileData);

    if (!rgbaData) {
        printf("[LYRICS] LoadImageWebP: WebPDecodeRGBA failed\n");
        return image;
    }

    // Create raylib Image from RGBA data
    // We need to copy the data because WebP uses its own allocator
    size_t dataSize = width * height * 4;
    void *imageCopy = RL_MALLOC(dataSize);
    if (!imageCopy) {
        printf("[LYRICS] LoadImageWebP: failed to allocate image data\n");
        WebPFree(rgbaData);
        return image;
    }
    memcpy(imageCopy, rgbaData, dataSize);
    WebPFree(rgbaData);

    image.data = imageCopy;
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    printf("[LYRICS] LoadImageWebP: decoded %dx%d image\n", width, height);
    return image;
}

static void LoadAlbumArt(const char *path) {
    if (!path || path[0] == '\0') {
        printf("[LYRICS] LoadAlbumArt: path is NULL or empty\n");
        return;
    }

    // Already loaded
    if (g_albumArt.loaded && strcmp(path, g_albumArt.loadedPath) == 0) {
        return;
    }

    printf("[LYRICS] LoadAlbumArt: attempting to load '%s'\n", path);

    // Check file exists using stat (more reliable)
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("[LYRICS] LoadAlbumArt: FILE NOT FOUND '%s'\n", path);
        return;
    }
    printf("[LYRICS] LoadAlbumArt: file exists, size=%ld bytes\n", (long)st.st_size);

    // Load image - use WebP decoder for WebP files
    Image img;
    if (IsWebPFile(path)) {
        printf("[LYRICS] LoadAlbumArt: using WebP decoder\n");
        img = LoadImageWebP(path);
    } else {
        img = LoadImage(path);
    }

    if (img.data == NULL) {
        printf("[LYRICS] LoadAlbumArt: LoadImage FAILED for '%s'\n", path);
        return;
    }
    printf("[LYRICS] LoadAlbumArt: image loaded %dx%d\n", img.width, img.height);

    // Extract colors before creating texture
    ExtractColorsFromImage(img);

    // Create texture
    Texture2D newTexture = LoadTextureFromImage(img);
    if (newTexture.id == 0) {
        UnloadImage(img);
        printf("[LYRICS] LoadAlbumArt: LoadTextureFromImage FAILED\n");
        return;
    }

    // Generate blurred version for background effect
    // Using same parameters as nowplaying: blurRadius=15, darkenAmount=0.4
    Texture2D blurred = LlzTextureBlur(newTexture, 15, 0.4f);
    if (blurred.id != 0) {
        printf("[LYRICS] LoadAlbumArt: Blurred texture generated, id=%u\n", blurred.id);
    } else {
        printf("[LYRICS] LoadAlbumArt: Warning - failed to generate blurred texture\n");
    }

    UnloadImage(img);

    // Setup crossfade transition
    CleanupPrevAlbumArt();
    if (g_albumArt.loaded && g_albumArt.texture.id != 0) {
        g_prevAlbumArt.texture = g_albumArt.texture;
        g_prevAlbumArt.blurred = g_albumArt.blurred;
        g_prevAlbumArt.alpha = g_currentAlbumArtAlpha;
    } else {
        g_prevAlbumArt.alpha = 0.0f;
    }

    g_albumArt.texture = newTexture;
    g_albumArt.blurred = blurred;
    g_albumArt.loaded = true;
    strncpy(g_albumArt.loadedPath, path, sizeof(g_albumArt.loadedPath) - 1);

    g_currentAlbumArtAlpha = 0.0f;
    g_inAlbumArtTransition = true;

    printf("[LYRICS] LoadAlbumArt: SUCCESS texture_id=%u loaded='%s'\n",
           newTexture.id, path);
}

static void UpdateAlbumArtTransition(float deltaTime) {
    if (!g_inAlbumArtTransition) return;

    float fadeStep = deltaTime * ALBUM_ART_FADE_SPEED;

    // Fade in current
    if (g_albumArt.loaded) {
        g_currentAlbumArtAlpha += fadeStep;
        if (g_currentAlbumArtAlpha >= 1.0f) {
            g_currentAlbumArtAlpha = 1.0f;
        }
    }

    // Fade out previous
    g_prevAlbumArt.alpha -= fadeStep;
    if (g_prevAlbumArt.alpha <= 0.0f) {
        g_prevAlbumArt.alpha = 0.0f;
    }

    // Transition complete
    if (g_currentAlbumArtAlpha >= 1.0f && g_prevAlbumArt.alpha <= 0.0f) {
        g_inAlbumArtTransition = false;
        CleanupPrevAlbumArt();
    }
}

// ============================================================================
// Visibility & Fade Calculations
// ============================================================================

static float CalculateLineFade(int lineIndex, int currentLine, int totalLines) {
    int distance = abs(lineIndex - currentLine);

    switch (g_visibilityMode) {
        case VISIBILITY_CURRENT_ONLY:
            return (distance == 0) ? 1.0f : 0.0f;

        case VISIBILITY_CURRENT_NEXT:
            if (distance == 0) return 1.0f;
            if (distance <= 2 && lineIndex > currentLine) return 0.55f;
            return 0.0f;

        case VISIBILITY_SPOTLIGHT:
            if (distance == 0) return 1.0f;
            if (distance == 1) return 0.12f;
            if (distance == 2) return 0.06f;
            return 0.02f;

        case VISIBILITY_ALL:
        default:
            if (distance == 0) return 1.0f;
            if (distance == 1) return 0.65f;
            if (distance == 2) return 0.45f;
            if (distance <= LYRICS_FADE_DISTANCE) return 0.3f;
            return Clampf(0.2f - (distance - LYRICS_FADE_DISTANCE) * 0.04f, 0.08f, 0.2f);
    }
}

static float GetCurrentFontSize(void) {
    return LYRICS_BASE_FONT_SIZE + SIZE_DIFF[g_sizeRatio];
}

static float GetOtherFontSize(void) {
    return LYRICS_BASE_FONT_SIZE;
}

// Get accent color (from album art or default)
static Color GetAccentColor(void) {
    return g_colors.hasColors ? g_colors.accent : COLOR_ACCENT_DEFAULT;
}

static Color GetGlowColor(void) {
    return g_colors.hasColors ? g_colors.glow : COLOR_ACCENT_DEFAULT;
}

// ============================================================================
// Line Position Calculation
// ============================================================================

static void RecalculateLinePositions(void) {
    if (!g_hasLyrics || g_lyrics.lineCount == 0) {
        if (g_lineYPositions) {
            free(g_lineYPositions);
            g_lineYPositions = NULL;
        }
        g_totalLyricsHeight = 0;
        return;
    }

    // Allocate position array
    if (g_lineYPositions) {
        free(g_lineYPositions);
    }
    g_lineYPositions = (float *)malloc(sizeof(float) * g_lyrics.lineCount);

    // Calculate positions based on current font sizes
    // Use the "other" (non-current) font size for layout to keep things stable
    float fontSize = GetOtherFontSize();
    float spacing = 1.2f;
    float currentY = 0.0f;

    for (int i = 0; i < g_lyrics.lineCount; i++) {
        g_lineYPositions[i] = currentY;

        // Calculate height of this line (with wrapping)
        float lineHeight = CalculateLineHeight(g_lyrics.lines[i].text, fontSize, spacing);

        // Add spacing between lines
        currentY += lineHeight + (fontSize * 0.4f);
    }

    g_totalLyricsHeight = currentY;
}

// Get the Y position for a line index in the scroll coordinate space
static float GetLineYPosition(int lineIndex) {
    if (!g_lineYPositions || lineIndex < 0 || lineIndex >= g_lyrics.lineCount) {
        return 0.0f;
    }
    return g_lineYPositions[lineIndex];
}

// ============================================================================
// Lyrics Loading
// ============================================================================

static void LoadLyricsFromHash(const char *hash) {
    printf("[LYRICS] LoadLyricsFromHash: '%s'\n", hash ? hash : "(null)");

    if (g_lyricsLoaded) {
        LlzLyricsFree(&g_lyrics);
        g_lyricsLoaded = false;
    }
    memset(&g_lyrics, 0, sizeof(g_lyrics));

    if (LlzLyricsGet(&g_lyrics)) {
        g_lyricsLoaded = true;
        g_hasLyrics = g_lyrics.lineCount > 0;
        g_isSynced = g_lyrics.synced;

        // Get the new hash from loaded lyrics or parameter
        char newLoadedHash[64] = {0};
        if (g_lyrics.hash[0] != '\0') {
            strncpy(newLoadedHash, g_lyrics.hash, sizeof(newLoadedHash) - 1);
        } else if (hash) {
            strncpy(newLoadedHash, hash, sizeof(newLoadedHash) - 1);
        }
        newLoadedHash[sizeof(newLoadedHash) - 1] = '\0';

        // Clear stale flag if this is a different hash than what we had before track change
        // This means we've received new lyrics for the new track
        if (g_lyricsStale && newLoadedHash[0] != '\0' &&
            strcmp(newLoadedHash, g_priorTrackHash) != 0) {
            g_lyricsStale = false;
            printf("[LYRICS] Lyrics loaded for new track (hash changed: %s -> %s)\n",
                   g_priorTrackHash, newLoadedHash);
        }

        strncpy(g_currentHash, newLoadedHash, sizeof(g_currentHash) - 1);
        g_currentHash[sizeof(g_currentHash) - 1] = '\0';

        printf("[LYRICS] Loaded %d lines, synced: %s\n",
               g_lyrics.lineCount, g_isSynced ? "yes" : "no");

        g_currentLineIndex = 0;
        g_scrollOffset = 0.0f;
        g_targetScrollOffset = 0.0f;
        g_scrollVelocity = 0.0f;

        // Recalculate line positions for variable height scrolling
        RecalculateLinePositions();
    } else {
        g_hasLyrics = false;
        g_isSynced = false;
        if (hash) {
            // Clear stale flag if this is a different hash than what we had before track change
            // This means we tried to load for a new song but there are no lyrics
            if (g_lyricsStale && strcmp(hash, g_priorTrackHash) != 0) {
                g_lyricsStale = false;
                printf("[LYRICS] No lyrics available for new track (hash changed: %s -> %s)\n",
                       g_priorTrackHash, hash);
            }
            strncpy(g_currentHash, hash, sizeof(g_currentHash) - 1);
            g_currentHash[sizeof(g_currentHash) - 1] = '\0';
        }
    }
}

static void LoadLyrics(void) {
    LoadLyricsFromHash(NULL);
}

static void CheckForLyricsUpdate(void) {
    char newHash[64] = {0};
    bool gotHash = LlzLyricsGetHash(newHash, sizeof(newHash));

    if (gotHash) {
        if (strcmp(newHash, g_currentHash) != 0) {
            LoadLyricsFromHash(newHash);
        }
    } else if (!g_lyricsLoaded) {
        LoadLyrics();
    }

    // Retry if we have hash but no lyrics
    if (!g_hasLyrics && g_currentHash[0] != '\0') {
        static float retryTimer = 0.0f;
        retryTimer += GetFrameTime();
        if (retryTimer > 1.0f) {
            LoadLyricsFromHash(g_currentHash);
            retryTimer = 0.0f;
        }
    }
}

// ============================================================================
// Drawing Functions
// ============================================================================

// Draw a single wrapped line of text with glow effect
static void DrawWrappedLineWithGlow(const char *text, float x, float y, float fontSize,
                                     float spacing, Color textColor, Color glowColor,
                                     bool hasGlow, float glowIntensity) {
    // Draw glow layers
    if (hasGlow && glowIntensity > 0.3f) {
        // Soft shadow/glow beneath
        for (int layer = 4; layer >= 1; layer--) {
            float layerAlpha = glowIntensity * 0.15f * (5 - layer) / 4.0f;
            Color shadowColor = ColorWithAlpha(glowColor, layerAlpha);
            DrawTextEx(g_font, text, (Vector2){x, y + layer * 1.5f}, fontSize, spacing, shadowColor);
        }

        // Subtle horizontal glow
        for (int i = 1; i <= 2; i++) {
            float offsetAlpha = glowIntensity * 0.1f / i;
            Color offsetGlow = ColorWithAlpha(glowColor, offsetAlpha);
            DrawTextEx(g_font, text, (Vector2){x - i, y}, fontSize, spacing, offsetGlow);
            DrawTextEx(g_font, text, (Vector2){x + i, y}, fontSize, spacing, offsetGlow);
        }
    }

    // Draw main text
    DrawTextEx(g_font, text, (Vector2){x, y}, fontSize, spacing, textColor);
}

static void DrawLyricsLine(const char *text, float y, bool isCurrent, float fadeAlpha, float highlightProgress) {
    if (!text || text[0] == '\0') return;
    if (fadeAlpha <= 0.01f) return;

    float fontSize = isCurrent ? Lerpf(GetOtherFontSize(), GetCurrentFontSize(), highlightProgress) : GetOtherFontSize();
    float spacing = 1.2f;

    // Determine colors - always use white as base
    Color textColor;
    Color glowColor;

    if (isCurrent) {
        // Current line: pure white with subtle glow
        textColor = WHITE;
        Color glow = GetGlowColor();
        glowColor = ColorWithAlpha(glow, fadeAlpha * highlightProgress * 0.6f);
    } else {
        // Other lines: white with reduced opacity based on distance
        textColor = WHITE;
        glowColor = (Color){0, 0, 0, 0};
    }

    Color finalColor = ColorWithAlpha(textColor, fadeAlpha);
    bool hasGlow = isCurrent && highlightProgress > 0.3f;
    float glowIntensity = highlightProgress * fadeAlpha;

    // Wrap text if necessary
    WrappedText wrapped;
    WrapText(text, fontSize, spacing, &wrapped);

    if (wrapped.lineCount <= 1) {
        // Single line - center and draw
        Vector2 measure = MeasureTextEx(g_font, text, fontSize, spacing);
        float x = (g_screenWidth - measure.x) / 2.0f;
        DrawWrappedLineWithGlow(text, x, y, fontSize, spacing, finalColor, glowColor, hasGlow, glowIntensity);
    } else {
        // Multiple lines - draw each centered
        float lineHeight = fontSize * 1.3f;
        float totalHeight = wrapped.lineCount * lineHeight;

        // Center the block vertically around y
        float startY = y - (totalHeight / 2.0f) + (lineHeight / 2.0f);

        for (int i = 0; i < wrapped.lineCount; i++) {
            Vector2 measure = MeasureTextEx(g_font, wrapped.lines[i], fontSize, spacing);
            float x = (g_screenWidth - measure.x) / 2.0f;
            float lineY = startY + i * lineHeight;

            DrawWrappedLineWithGlow(wrapped.lines[i], x, lineY, fontSize, spacing,
                                   finalColor, glowColor, hasGlow, glowIntensity);
        }
    }
}

// Calculate the display height of a line at current settings (for proper spacing)
static float GetLineDisplayHeight(int lineIndex, bool isCurrent, float highlightProgress) {
    if (lineIndex < 0 || lineIndex >= g_lyrics.lineCount) return LYRICS_BASE_LINE_HEIGHT;

    float fontSize = isCurrent ? Lerpf(GetOtherFontSize(), GetCurrentFontSize(), highlightProgress) : GetOtherFontSize();
    float spacing = 1.2f;

    WrappedText wrapped;
    WrapText(g_lyrics.lines[lineIndex].text, fontSize, spacing, &wrapped);

    if (wrapped.lineCount <= 1) {
        return fontSize * LYRICS_LINE_SPACING;
    } else {
        return wrapped.totalHeight + (fontSize * 0.2f);
    }
}

static void DrawLoadingMessage(void) {
    const char *message = "Loading lyrics...";

    float fontSize = 26.0f;
    float spacing = 1.2f;

    Vector2 measure = MeasureTextEx(g_font, message, fontSize, spacing);

    float x = (g_screenWidth - measure.x) / 2.0f;
    float y = g_screenHeight / 2.0f - 10.0f;

    // Pulsing animation for loading
    float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 3.0f);

    DrawTextEx(g_font, message, (Vector2){x, y}, fontSize, spacing,
               ColorWithAlpha(WHITE, pulse));
}

static void DrawNoLyricsMessage(void) {
    const char *message = "No lyrics available";
    const char *subMessage = "Long-press SELECT on Now Playing to request lyrics";

    float fontSize = 26.0f;
    float subFontSize = 18.0f;
    float spacing = 1.2f;

    Vector2 measure = MeasureTextEx(g_font, message, fontSize, spacing);
    Vector2 subMeasure = MeasureTextEx(g_font, subMessage, subFontSize, spacing);

    float x = (g_screenWidth - measure.x) / 2.0f;
    float y = g_screenHeight / 2.0f - 30.0f;

    float subX = (g_screenWidth - subMeasure.x) / 2.0f;
    float subY = y + 45.0f;

    DrawTextEx(g_font, message, (Vector2){x, y}, fontSize, spacing,
               ColorWithAlpha(WHITE, 0.9f));
    DrawTextEx(g_font, subMessage, (Vector2){subX, subY}, subFontSize, spacing,
               ColorWithAlpha(WHITE, 0.5f));
}

static void DrawUnsyncedIndicator(void) {
    const char *message = "Drag to navigate";
    float fontSize = 15.0f;
    float spacing = 1.0f;

    Vector2 measure = MeasureTextEx(g_font, message, fontSize, spacing);
    float x = (g_screenWidth - measure.x) / 2.0f;
    float y = g_screenHeight - 35.0f;

    // Draw with pill background
    float padding = 12.0f;
    Rectangle pill = {x - padding, y - 4, measure.x + padding * 2, measure.y + 8};
    DrawRectangleRounded(pill, 0.5f, 8, ColorWithAlpha((Color){0, 0, 0, 180}, 0.7f));

    DrawTextEx(g_font, message, (Vector2){x, y}, fontSize, spacing,
               ColorWithAlpha(WHITE, 0.6f));
}

// Calculate font size that fits text within maxWidth
static float CalculateFittingFontSize(const char *text, float maxFontSize, float minFontSize,
                                       float maxWidth, float spacing) {
    if (!text || text[0] == '\0') return maxFontSize;

    float fontSize = maxFontSize;

    while (fontSize >= minFontSize) {
        Vector2 measure = MeasureTextEx(g_font, text, fontSize, spacing);
        if (measure.x <= maxWidth) {
            return fontSize;
        }
        fontSize -= 1.0f;
    }

    return minFontSize;
}

static void DrawTrackInfo(void) {
    if (g_trackTitle[0] == '\0') return;
    if (g_displayStyle == LYRICS_STYLE_MINIMALIST) return;

    float spacing = 1.0f;
    float leftPadding = 24.0f;  // Left padding for alignment
    float maxWidth = g_screenWidth * 0.80f;  // 80% of screen width max

    // Font sizes
    float titleMaxSize = 28.0f;
    float titleMinSize = 16.0f;
    float artistMaxSize = 20.0f;
    float artistMinSize = 14.0f;

    // Calculate fitting font sizes
    float titleFontSize = CalculateFittingFontSize(g_trackTitle, titleMaxSize, titleMinSize, maxWidth, spacing);
    float artistFontSize = artistMaxSize;
    if (g_trackArtist[0] != '\0') {
        artistFontSize = CalculateFittingFontSize(g_trackArtist, artistMaxSize, artistMinSize, maxWidth, spacing);
    }

    // Left-aligned positions
    float titleX = leftPadding;
    float titleY = 18.0f;

    float artistY = titleY + titleFontSize + 4.0f;
    float bgHeight = artistY + artistFontSize + 12.0f;

    // Semi-transparent gradient background for readability
    DrawRectangleGradientV(0, 0, g_screenWidth, (int)(bgHeight + 30.0f),
                           ColorWithAlpha((Color){0, 0, 0, 200}, 0.9f),
                           ColorWithAlpha((Color){0, 0, 0, 0}, 0.0f));

    // Draw title - left aligned, white text
    DrawTextEx(g_font, g_trackTitle, (Vector2){titleX, titleY}, titleFontSize, spacing, WHITE);

    // Draw artist - left aligned, slightly dimmed white
    if (g_trackArtist[0] != '\0') {
        Color artistColor = ColorWithAlpha(WHITE, 0.7f);
        DrawTextEx(g_font, g_trackArtist, (Vector2){leftPadding, artistY}, artistFontSize, spacing, artistColor);
    }
}

static void DrawLyricsCentered(void) {
    float centerY = g_screenHeight / 2.0f;

    // Calculate cumulative heights for proper positioning
    float baseOffset = g_scrollOffset;

    // Draw lines around current
    for (int i = 0; i < g_lyrics.lineCount; i++) {
        // Get Y position from pre-calculated positions
        float lineYInScroll = GetLineYPosition(i);
        float lineY = centerY + lineYInScroll - baseOffset;

        // Get line height for culling
        bool isCurrent = (i == g_currentLineIndex);
        float highlight = isCurrent ? g_lineHighlightProgress : 0.0f;
        float lineHeight = GetLineDisplayHeight(i, isCurrent, highlight);

        // Skip lines outside viewport (with margin for wrapped text)
        if (lineY < -lineHeight - 50.0f || lineY > g_screenHeight + lineHeight + 50.0f) {
            continue;
        }

        float distanceFade = CalculateLineFade(i, g_currentLineIndex, g_lyrics.lineCount);

        // Smooth edge fade
        float edgeFadeTop = SmoothStep((lineY - 40.0f) / 80.0f);
        float edgeFadeBottom = SmoothStep((g_screenHeight - 40.0f - lineY) / 80.0f);
        float edgeFade = fminf(edgeFadeTop, edgeFadeBottom);

        float finalFade = distanceFade * edgeFade;

        DrawLyricsLine(g_lyrics.lines[i].text, lineY, isCurrent, finalFade, highlight);
    }
}

static void DrawLyricsFullScreen(void) {
    float startY = 70.0f;
    float endY = g_screenHeight - 50.0f;
    float availableHeight = endY - startY;

    // Calculate how many lines fit on screen
    float currentY = startY;
    int startLine = 0;

    // Find starting line for current page
    float accumulatedHeight = 0;
    for (int i = 0; i < g_lyrics.lineCount; i++) {
        float lineHeight = GetLineDisplayHeight(i, i == g_currentLineIndex, g_lineHighlightProgress);
        if (accumulatedHeight + lineHeight > availableHeight) {
            if (i <= g_currentLineIndex) {
                startLine = i;
                accumulatedHeight = 0;
            } else {
                break;
            }
        }
        accumulatedHeight += lineHeight + 8.0f;
    }

    // Draw lines from startLine
    currentY = startY;
    for (int i = startLine; i < g_lyrics.lineCount; i++) {
        bool isCurrent = (i == g_currentLineIndex);
        float highlight = isCurrent ? g_lineHighlightProgress : 0.0f;
        float lineHeight = GetLineDisplayHeight(i, isCurrent, highlight);

        if (currentY + lineHeight > endY) break;

        float fade = CalculateLineFade(i, g_currentLineIndex, g_lyrics.lineCount);
        DrawLyricsLine(g_lyrics.lines[i].text, currentY + lineHeight / 2.0f, isCurrent, fade, highlight);

        currentY += lineHeight + 8.0f;
    }
}

static void DrawLyricsMinimalist(void) {
    float centerY = g_screenHeight / 2.0f;

    if (g_currentLineIndex >= 0 && g_currentLineIndex < g_lyrics.lineCount) {
        DrawLyricsLine(g_lyrics.lines[g_currentLineIndex].text, centerY, true, 1.0f, g_lineHighlightProgress);
    }
}

static void DrawLyricsKaraoke(void) {
    float baseY = g_screenHeight - 100.0f;

    if (g_currentLineIndex >= 0 && g_currentLineIndex < g_lyrics.lineCount) {
        DrawLyricsLine(g_lyrics.lines[g_currentLineIndex].text, baseY, true, 1.0f, g_lineHighlightProgress);
    }

    // Upcoming lines above
    float currentUpY = baseY;
    for (int i = 1; i <= 4; i++) {
        int lineIdx = g_currentLineIndex + i;
        if (lineIdx >= g_lyrics.lineCount) break;

        float lineHeight = GetLineDisplayHeight(lineIdx, false, 0.0f);
        currentUpY -= lineHeight + 12.0f;

        if (currentUpY < 60.0f) break;

        float fade = CalculateLineFade(lineIdx, g_currentLineIndex, g_lyrics.lineCount);
        DrawLyricsLine(g_lyrics.lines[lineIdx].text, currentUpY, false, fade, 0.0f);
    }

    // Past line below (very faded)
    if (g_currentLineIndex > 0) {
        float lineHeight = GetLineDisplayHeight(g_currentLineIndex, true, g_lineHighlightProgress);
        float pastY = baseY + lineHeight + 20.0f;

        if (pastY < g_screenHeight - 20.0f) {
            DrawLyricsLine(g_lyrics.lines[g_currentLineIndex - 1].text, pastY, false, 0.15f, 0.0f);
        }
    }
}

static void DrawLyrics(void) {
    if (!g_hasLyrics || !g_lyricsLoaded || g_lyrics.lineCount == 0) {
        // If lyrics are stale (from previous song), show loading message
        // Otherwise show no lyrics available
        if (g_lyricsStale) {
            DrawLoadingMessage();
        } else {
            DrawNoLyricsMessage();
        }
        return;
    }

    // If we have lyrics but they're stale, still show loading
    // (lyrics data exists but is from the previous song)
    if (g_lyricsStale) {
        DrawLoadingMessage();
        return;
    }

    switch (g_displayStyle) {
        case LYRICS_STYLE_FULL_SCREEN:
            DrawLyricsFullScreen();
            break;
        case LYRICS_STYLE_MINIMALIST:
            DrawLyricsMinimalist();
            break;
        case LYRICS_STYLE_KARAOKE:
            DrawLyricsKaraoke();
            break;
        case LYRICS_STYLE_CENTERED:
        default:
            DrawLyricsCentered();
            break;
    }

    if (!g_isSynced && g_displayStyle != LYRICS_STYLE_MINIMALIST) {
        DrawUnsyncedIndicator();
    }
}

static void DrawBackground(void) {
    if (g_bgMode == BG_MODE_OFF) {
        ClearBackground(COLOR_BG);
        return;
    }

    if (g_bgMode == BG_MODE_ALBUM_ART) {
        // Draw album art blurred background
        bool hasPrev = g_prevAlbumArt.blurred.id != 0 && g_prevAlbumArt.alpha > 0.01f;
        bool hasCurrent = g_albumArt.loaded && g_albumArt.blurred.id != 0;

        if (!hasPrev && !hasCurrent) {
            ClearBackground(COLOR_BG);
        } else {
            // Draw previous (fading out)
            if (hasPrev) {
                Color tint = ColorAlpha(WHITE, g_prevAlbumArt.alpha);
                Rectangle dest = {0, 0, (float)g_screenWidth, (float)g_screenHeight};
                LlzDrawTextureCover(g_prevAlbumArt.blurred, dest, tint);
            }

            // Draw current (fading in)
            if (hasCurrent && g_currentAlbumArtAlpha > 0.01f) {
                Color tint = ColorAlpha(WHITE, g_currentAlbumArtAlpha);
                Rectangle dest = {0, 0, (float)g_screenWidth, (float)g_screenHeight};
                LlzDrawTextureCover(g_albumArt.blurred, dest, tint);
            }

            // Fallback if nothing visible yet
            if ((!hasPrev || g_prevAlbumArt.alpha < 0.01f) &&
                (!hasCurrent || g_currentAlbumArtAlpha < 0.01f)) {
                ClearBackground(COLOR_BG);
            }
        }

        // Subtle dark vignette overlay for readability
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight, ColorWithAlpha((Color){0, 0, 0, 80}, 0.3f));
        return;
    }

    // Animated background modes
    if (g_bgMode >= BG_MODE_ANIMATED_START && LlzBackgroundIsEnabled()) {
        LlzBackgroundDraw();
    } else {
        ClearBackground(COLOR_BG);
    }
}

static void DrawTopGradient(void) {
    // Skip gradient for album art mode (already handled)
    if (g_bgMode == BG_MODE_ALBUM_ART) return;
    if (g_bgMode >= BG_MODE_ANIMATED_START) return;

    for (int i = 0; i < 80; i++) {
        float alpha = 1.0f - (float)i / 80.0f;
        Color gradColor = ColorWithAlpha(COLOR_BG, alpha);
        DrawRectangle(0, i, g_screenWidth, 1, gradColor);
    }
}

static void DrawBottomGradient(void) {
    if (g_bgMode == BG_MODE_ALBUM_ART) return;
    if (g_bgMode >= BG_MODE_ANIMATED_START) return;

    for (int i = 0; i < 80; i++) {
        float alpha = (float)i / 80.0f;
        Color gradColor = ColorWithAlpha(COLOR_BG, alpha);
        DrawRectangle(0, g_screenHeight - 80 + i, g_screenWidth, 1, gradColor);
    }
}

static void DrawIndicatorOverlay(void) {
    if (g_indicatorTimer <= 0.0f || g_indicatorText[0] == '\0') return;

    float alpha = Clampf(g_indicatorTimer / 0.5f, 0.0f, 1.0f);
    float fontSize = 18.0f;
    float padding = 14.0f;

    Vector2 measure = MeasureTextEx(g_font, g_indicatorText, fontSize, 1.0f);
    float boxWidth = measure.x + padding * 2;
    float boxHeight = measure.y + padding * 2;
    float x = (g_screenWidth - boxWidth) / 2.0f;
    float y = g_screenHeight - 75.0f;

    // Rounded background
    Rectangle box = {x, y, boxWidth, boxHeight};
    DrawRectangleRounded(box, 0.3f, 8, ColorWithAlpha((Color){20, 20, 25, 230}, alpha));

    // Accent border
    Color accentColor = GetAccentColor();
    DrawRectangleRoundedLinesEx(box, 0.3f, 8, 1.0f, ColorWithAlpha(accentColor, alpha * 0.6f));

    DrawTextEx(g_font, g_indicatorText,
               (Vector2){x + padding, y + padding},
               fontSize, 1.0f, ColorWithAlpha(WHITE, alpha));
}

static void DrawScrubIndicator(void) {
    if (!g_isScrubbing) return;

    // Show current scrub position
    int mins = (int)g_scrubTargetSeconds / 60;
    int secs = (int)g_scrubTargetSeconds % 60;

    char timeText[64];
    snprintf(timeText, sizeof(timeText), "%d:%02d", mins, secs);

    float fontSize = 24.0f;
    float padding = 16.0f;

    Vector2 measure = MeasureTextEx(g_font, timeText, fontSize, 1.0f);
    float boxWidth = measure.x + padding * 2;
    float boxHeight = measure.y + padding * 2;
    float x = (g_screenWidth - boxWidth) / 2.0f;
    float y = 100.0f;

    // Rounded background with accent color
    Color accentColor = GetAccentColor();
    Rectangle box = {x, y, boxWidth, boxHeight};
    DrawRectangleRounded(box, 0.4f, 8, ColorWithAlpha((Color){0, 0, 0, 200}, 0.9f));
    DrawRectangleRoundedLinesEx(box, 0.4f, 8, 2.0f, ColorWithAlpha(accentColor, 0.8f));

    // Time text - white for readability
    DrawTextEx(g_font, timeText,
               (Vector2){x + padding, y + padding},
               fontSize, 1.0f, WHITE);

    // "Drag to seek" label below
    const char *label = "Drag to seek";
    float labelSize = 14.0f;
    Vector2 labelMeasure = MeasureTextEx(g_font, label, labelSize, 1.0f);
    float labelX = (g_screenWidth - labelMeasure.x) / 2.0f;
    DrawTextEx(g_font, label, (Vector2){labelX, y + boxHeight + 8.0f}, labelSize, 1.0f,
               ColorWithAlpha(WHITE, 0.6f));
}

static void DrawVolumeOverlay(void) {
    if (g_volumeOverlayAlpha <= 0.01f) return;

    // Minimal volume bar at top of screen (same style as nowplaying)
    float barHeight = 6.0f;
    float margin = 24.0f;
    Rectangle bar = {margin, 16.0f, (float)g_screenWidth - margin * 2.0f, barHeight};

    // Use dynamic colors if available from album art, otherwise use defaults
    Color fillColor;
    Color barBg;
    if (g_colors.hasColors) {
        fillColor = ColorWithAlpha(g_colors.accent, g_volumeOverlayAlpha);
        barBg = ColorWithAlpha((Color){30, 30, 35, 255}, g_volumeOverlayAlpha * 0.7f);
    } else {
        fillColor = ColorWithAlpha(COLOR_ACCENT_DEFAULT, g_volumeOverlayAlpha);
        barBg = ColorWithAlpha((Color){60, 60, 70, 255}, g_volumeOverlayAlpha * 0.7f);
    }

    // Draw background track
    DrawRectangleRounded(bar, 0.5f, 8, barBg);

    // Draw fill
    Rectangle fill = bar;
    fill.width *= (float)g_currentVolume / 100.0f;
    if (fill.width > 0.0f) {
        DrawRectangleRounded(fill, 0.5f, 8, fillColor);
    }

    // Draw volume percentage text
    char volText[16];
    snprintf(volText, sizeof(volText), "%d%%", g_currentVolume);
    float fontSize = 14.0f;
    Vector2 textSize = MeasureTextEx(g_font, volText, fontSize, 1.0f);
    float textX = (float)g_screenWidth / 2.0f - textSize.x / 2.0f;
    float textY = bar.y + barHeight + 6.0f;
    DrawTextEx(g_font, volText, (Vector2){textX, textY}, fontSize, 1.0f,
               ColorWithAlpha(WHITE, g_volumeOverlayAlpha * 0.8f));
}

static void DrawLineCounter(void) {
    if (!g_hasLyrics || g_lyrics.lineCount == 0) return;
    if (g_displayStyle == LYRICS_STYLE_MINIMALIST) return;

    char lineInfo[64];
    snprintf(lineInfo, sizeof(lineInfo), "%d / %d", g_currentLineIndex + 1, g_lyrics.lineCount);

    float fontSize = 13.0f;
    Vector2 measure = MeasureTextEx(g_font, lineInfo, fontSize, 1.0f);
    float x = g_screenWidth - measure.x - 16.0f;
    float y = g_screenHeight - 22.0f;

    DrawTextEx(g_font, lineInfo, (Vector2){x, y}, fontSize, 1.0f,
               ColorWithAlpha(WHITE, 0.35f));
}

// ============================================================================
// Plugin API Implementation
// ============================================================================

static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;

    printf("[LYRICS] ===== PluginInit START =====\n");

    // Initialize plugin config with defaults
    LlzPluginConfigEntry defaults[] = {
        {"display_style", "0"},
        {"visibility_mode", "0"},
        {"size_ratio", "1"},
        {"bg_mode", "1"},
        {"animated_bg_index", "0"}
    };
    g_pluginConfigInitialized = LlzPluginConfigInit(&g_pluginConfig, "lyrics", defaults, 5);

    // Initialize media system
    bool mediaOk = LlzMediaInit(NULL);
    printf("[LYRICS] LlzMediaInit: %s\n", mediaOk ? "OK" : "FAILED");

    // Initialize background system
    LlzBackgroundInit(width, height);
    LlzBackgroundSetEnabled(false);

    // Load font
    g_font = LlzFontGet(LLZ_FONT_UI, 36);
    g_fontLoaded = g_font.texture.id != 0;
    if (!g_fontLoaded) {
        g_font = GetFontDefault();
    }

    // Set default state values (before loading from config)
    g_displayStyle = LYRICS_STYLE_CENTERED;
    g_visibilityMode = VISIBILITY_ALL;
    g_sizeRatio = SIZE_RATIO_NORMAL;
    g_bgMode = BG_MODE_ALBUM_ART;
    g_animatedBgIndex = 0;
    g_indicatorTimer = 0.0f;
    g_lineHighlightProgress = 0.0f;
    g_lastHighlightedLine = -1;
    g_scrollVelocity = 0.0f;

    // Reset scrub state
    g_isScrubbing = false;
    g_scrubStartY = 0.0f;
    g_scrubStartScrollOffset = 0.0f;
    g_scrubTargetLine = 0;
    g_scrubTargetSeconds = 0.0f;
    g_trackDuration = 0.0f;
    g_justSeeked = false;
    g_justSeekedTimer = 0.0f;

    memset(&g_colors, 0, sizeof(g_colors));
    memset(&g_albumArt, 0, sizeof(g_albumArt));
    memset(&g_prevAlbumArt, 0, sizeof(g_prevAlbumArt));
    g_currentAlbumArtAlpha = 1.0f;
    g_inAlbumArtTransition = false;

    g_lineYPositions = NULL;
    g_totalLyricsHeight = 0.0f;

    // Load saved settings (overrides defaults)
    LoadPluginSettings();

    // Load initial lyrics
    LoadLyrics();

    // Get track info and album art
    LlzMediaState state;
    if (LlzMediaGetState(&state)) {
        strncpy(g_trackTitle, state.track, sizeof(g_trackTitle) - 1);
        strncpy(g_trackArtist, state.artist, sizeof(g_trackArtist) - 1);
        strncpy(g_trackAlbumArtPath, state.albumArtPath, sizeof(g_trackAlbumArtPath) - 1);

        // Load album art
        if (state.albumArtPath[0] != '\0') {
            LoadAlbumArt(state.albumArtPath);
        } else if (state.artist[0] != '\0' || state.album[0] != '\0') {
            const char *hash = LlzMediaGenerateArtHash(state.artist, state.album);
            if (hash && hash[0] != '\0') {
                char generatedPath[512];
                snprintf(generatedPath, sizeof(generatedPath),
                         "/var/mediadash/album_art_cache/%s.webp", hash);
                LoadAlbumArt(generatedPath);
            }
        }
    }

    printf("[LYRICS] Initialized (%dx%d)\n", width, height);
}

// Find the line index closest to a given scroll offset
static int FindLineAtScrollOffset(float scrollOffset) {
    if (!g_lineYPositions || g_lyrics.lineCount == 0) return 0;

    int closestLine = 0;
    float closestDistance = fabsf(g_lineYPositions[0] - scrollOffset);

    for (int i = 1; i < g_lyrics.lineCount; i++) {
        float distance = fabsf(g_lineYPositions[i] - scrollOffset);
        if (distance < closestDistance) {
            closestDistance = distance;
            closestLine = i;
        }
    }

    return closestLine;
}

// Get the timestamp for a lyrics line (in seconds)
static float GetLineTimestamp(int lineIndex) {
    if (lineIndex < 0 || lineIndex >= g_lyrics.lineCount) return 0.0f;
    return (float)g_lyrics.lines[lineIndex].timestampMs / 1000.0f;
}

static void PluginUpdate(const LlzInputState *input, float deltaTime) {
    // Handle back button - return to Now Playing plugin
    if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
        printf("[LYRICS] Back pressed - returning to Now Playing\n");
        LlzRequestOpenPlugin("Now Playing");
        g_wantsClose = true;
        return;
    }

    // Update timers
    if (g_indicatorTimer > 0.0f) {
        g_indicatorTimer -= deltaTime;
    }

    if (g_justSeekedTimer > 0.0f) {
        g_justSeekedTimer -= deltaTime;
        if (g_justSeekedTimer <= 0.0f) {
            g_justSeeked = false;
        }
    }

    // Update volume overlay animation
    if (g_volumeOverlayTimer > 0.0f) {
        g_volumeOverlayTimer -= deltaTime;
        if (g_volumeOverlayTimer < 0.0f) g_volumeOverlayTimer = 0.0f;
    }
    // Smooth fade in/out for volume overlay
    float targetAlpha = (g_volumeOverlayTimer > 0.0f) ? 1.0f : 0.0f;
    float fadeSpeed = (targetAlpha > g_volumeOverlayAlpha) ? 8.0f : 3.0f;
    g_volumeOverlayAlpha += (targetAlpha - g_volumeOverlayAlpha) * fminf(1.0f, deltaTime * fadeSpeed);
    if (g_volumeOverlayAlpha < 0.01f && targetAlpha == 0.0f) g_volumeOverlayAlpha = 0.0f;

    // Update album art transition
    UpdateAlbumArtTransition(deltaTime);

    // Check for lyrics updates
    CheckForLyricsUpdate();

    // Update track info and album art
    LlzMediaState state;
    bool gotState = LlzMediaGetState(&state);

    if (gotState) {
        // Store track duration for seek calculations
        g_trackDuration = state.durationSeconds;

        // Track changed?
        if (strcmp(state.track, g_trackTitle) != 0 || strcmp(state.artist, g_trackArtist) != 0) {
            strncpy(g_trackTitle, state.track, sizeof(g_trackTitle) - 1);
            strncpy(g_trackArtist, state.artist, sizeof(g_trackArtist) - 1);
            // Reset scrubbing on track change
            g_isScrubbing = false;

            // Save current hash as prior track hash and mark lyrics as stale
            // LoadLyricsFromHash will clear stale when new lyrics arrive with different hash
            if (g_currentHash[0] != '\0') {
                strncpy(g_priorTrackHash, g_currentHash, sizeof(g_priorTrackHash) - 1);
                g_priorTrackHash[sizeof(g_priorTrackHash) - 1] = '\0';
                g_lyricsStale = true;
                printf("[LYRICS] Track changed - marking lyrics stale (prior hash: %s)\n",
                       g_priorTrackHash);
            }
        }

        // Always try to load album art (LoadAlbumArt returns early if already loaded)
        // This handles cases where:
        // 1. albumArtPath changes
        // 2. Track changes but albumArtPath hasn't updated yet
        // 3. albumArtPath is empty but we can generate from artist/album
        if (state.albumArtPath[0] != '\0') {
            LoadAlbumArt(state.albumArtPath);
        } else if (state.artist[0] != '\0' || state.album[0] != '\0') {
            // Generate path from artist/album hash (same as nowplaying)
            const char *hash = LlzMediaGenerateArtHash(state.artist, state.album);
            if (hash && hash[0] != '\0') {
                char generatedPath[512];
                snprintf(generatedPath, sizeof(generatedPath),
                         "/var/mediadash/album_art_cache/%s.webp", hash);
                LoadAlbumArt(generatedPath);
            }
        }

        // Update stored path for reference
        if (strcmp(state.albumArtPath, g_trackAlbumArtPath) != 0) {
            strncpy(g_trackAlbumArtPath, state.albumArtPath, sizeof(g_trackAlbumArtPath) - 1);
        }

        // Update current line for synced lyrics (only when NOT scrubbing and NOT just seeked)
        // After seeking, wait for cooldown before syncing to media position again
        // This prevents the lyrics from jumping back before the seek propagates
        if (g_hasLyrics && g_isSynced && g_lyricsLoaded && !g_isScrubbing && !g_justSeeked) {
            int64_t positionMs = (int64_t)state.positionSeconds * 1000;
            int newLineIndex = LlzLyricsFindCurrentLine(positionMs, &g_lyrics);
            if (newLineIndex >= 0 && newLineIndex != g_currentLineIndex) {
                g_currentLineIndex = newLineIndex;
                g_targetScrollOffset = GetLineYPosition(newLineIndex);

                // Reset highlight animation for new line
                if (newLineIndex != g_lastHighlightedLine) {
                    g_lineHighlightProgress = 0.0f;
                    g_lastHighlightedLine = newLineIndex;
                }
            }
        }

        // Update background energy
        if (g_bgMode >= BG_MODE_ANIMATED_START) {
            LlzBackgroundSetEnergy(state.isPlaying ? 1.0f : 0.3f);
        }

        // Sync volume from media state
        if (state.volumePercent >= 0) {
            g_currentVolume = state.volumePercent;
        }
    }

    // =========================================================================
    // DRAG-TO-SEEK HANDLING (synced lyrics only, centered mode)
    // =========================================================================

    bool canScrub = g_hasLyrics && g_isSynced && g_lyricsLoaded &&
                    g_displayStyle == LYRICS_STYLE_CENTERED && g_lyrics.lineCount > 0;

    // Detect drag start
    if (canScrub && input->mouseJustPressed && !g_isScrubbing) {
        // Start scrubbing anywhere on screen (lyrics area)
        Rectangle lyricsArea = {0, 80, (float)g_screenWidth, (float)g_screenHeight - 120};
        if (CheckCollisionPointRec(input->mousePos, lyricsArea)) {
            g_isScrubbing = true;
            g_scrubStartY = input->mousePos.y;
            g_scrubStartScrollOffset = g_scrollOffset;
            g_scrubTargetLine = g_currentLineIndex;
            g_scrubTargetSeconds = GetLineTimestamp(g_currentLineIndex);
            printf("[LYRICS] Scrub started at line %d (%.1fs)\n", g_scrubTargetLine, g_scrubTargetSeconds);
        }
    }

    // Handle ongoing scrub
    if (g_isScrubbing && input->mousePressed) {
        // Calculate drag delta (inverted - drag up = scroll forward in song)
        float dragDelta = g_scrubStartY - input->mousePos.y;

        // Sensitivity: pixels per scroll unit
        float sensitivity = 1.5f;

        // Calculate new scroll offset
        float newScrollOffset = g_scrubStartScrollOffset + dragDelta * sensitivity;

        // Clamp to valid range
        if (newScrollOffset < 0.0f) newScrollOffset = 0.0f;
        if (g_totalLyricsHeight > 0.0f && newScrollOffset > g_totalLyricsHeight) {
            newScrollOffset = g_totalLyricsHeight;
        }

        // Update scroll directly during scrub (no easing)
        g_scrollOffset = newScrollOffset;
        g_targetScrollOffset = newScrollOffset;

        // Find which line we're now at
        g_scrubTargetLine = FindLineAtScrollOffset(newScrollOffset);
        g_scrubTargetLine = Clampi(g_scrubTargetLine, 0, g_lyrics.lineCount - 1);

        // Update current line to match (for visual highlighting)
        if (g_scrubTargetLine != g_currentLineIndex) {
            g_currentLineIndex = g_scrubTargetLine;
            g_lineHighlightProgress = 1.0f;  // Instant highlight during scrub
            g_lastHighlightedLine = g_scrubTargetLine;
        }

        // Calculate target time for this line
        g_scrubTargetSeconds = GetLineTimestamp(g_scrubTargetLine);
    }

    // Handle scrub release - send seek command
    if (g_isScrubbing && input->mouseJustReleased) {
        g_isScrubbing = false;

        // Send seek command to Redis
        int targetSeconds = (int)roundf(g_scrubTargetSeconds);
        if (targetSeconds < 0) targetSeconds = 0;
        if (g_trackDuration > 0 && targetSeconds > (int)g_trackDuration) {
            targetSeconds = (int)g_trackDuration;
        }

        printf("[LYRICS] Seek to line %d at %ds\n", g_scrubTargetLine, targetSeconds);
        LlzMediaSeekSeconds(targetSeconds);

        // Set cooldown to prevent accidental actions
        g_justSeeked = true;
        g_justSeekedTimer = JUST_SEEKED_COOLDOWN;

        // Show indicator
        char timeStr[32];
        int mins = targetSeconds / 60;
        int secs = targetSeconds % 60;
        snprintf(timeStr, sizeof(timeStr), "Seek to %d:%02d", mins, secs);
        ShowIndicator(timeStr);
    }

    // UPDATE LINE HIGHLIGHT ANIMATION
    if (g_lineHighlightProgress < 1.0f) {
        g_lineHighlightProgress += deltaTime * 4.0f;  // ~0.25s to full highlight
        if (g_lineHighlightProgress > 1.0f) g_lineHighlightProgress = 1.0f;
    }

    // SELECT - Cycle display style
    if (input->selectPressed || IsKeyPressed(KEY_ENTER)) {
        g_displayStyle = (g_displayStyle + 1) % LYRICS_STYLE_COUNT;
        char indicator[64];
        snprintf(indicator, sizeof(indicator), "Style: %s", STYLE_NAMES[g_displayStyle]);
        ShowIndicator(indicator);
        g_isScrubbing = false;  // Cancel scrub on mode change
        SavePluginSettings();
    }

    // Button 2 (displayModeNext) - Cycle background modes
    if (input->displayModeNext || IsKeyPressed(KEY_B)) {
        if (g_bgMode == BG_MODE_OFF) {
            g_bgMode = BG_MODE_ALBUM_ART;
            ShowIndicator("Background: Album Art");
        } else if (g_bgMode == BG_MODE_ALBUM_ART) {
            g_bgMode = BG_MODE_ANIMATED_START;
            g_animatedBgIndex = 0;
            LlzBackgroundSetEnabled(true);
            LlzBackgroundSetStyle(LLZ_BG_STYLE_PULSE, true);
            if (g_colors.hasColors) {
                LlzBackgroundSetColors(g_colors.primary, g_colors.accent);
            }
            ShowIndicator("Background: Pulse");
        } else {
            // Cycle through animated styles
            g_animatedBgIndex++;
            if (g_animatedBgIndex >= LLZ_BG_STYLE_COUNT) {
                // Back to off
                g_bgMode = BG_MODE_OFF;
                g_animatedBgIndex = 0;
                LlzBackgroundSetEnabled(false);
                ShowIndicator("Background: Off");
            } else {
                LlzBackgroundCycleNext();
                char indicator[64];
                snprintf(indicator, sizeof(indicator), "Background: %s",
                         LlzBackgroundGetStyleName(LlzBackgroundGetStyle()));
                ShowIndicator(indicator);
            }
        }
        SavePluginSettings();
    }

    // Button 3 (styleCyclePressed) - Cycle text visibility
    if (input->styleCyclePressed || IsKeyPressed(KEY_V)) {
        g_visibilityMode = (g_visibilityMode + 1) % VISIBILITY_COUNT;
        char indicator[64];
        snprintf(indicator, sizeof(indicator), "Visibility: %s", VISIBILITY_NAMES[g_visibilityMode]);
        ShowIndicator(indicator);
        SavePluginSettings();
    }

    // DOWN - Cycle font size ratio
    if (input->downPressed || IsKeyPressed(KEY_S)) {
        g_sizeRatio = (g_sizeRatio + 1) % SIZE_RATIO_COUNT;
        char indicator[64];
        snprintf(indicator, sizeof(indicator), "Size: %s", SIZE_NAMES[g_sizeRatio]);
        ShowIndicator(indicator);
        SavePluginSettings();
    }

    // Volume control (wheel/rotary - same as nowplaying plugin)
    if (input->scrollDelta != 0) {
        int volumeDelta = (int)(input->scrollDelta * 5.0f);
        g_currentVolume += volumeDelta;
        if (g_currentVolume < 0) g_currentVolume = 0;
        if (g_currentVolume > 100) g_currentVolume = 100;

        LlzMediaSetVolume(g_currentVolume);

        // Trigger volume overlay bar
        g_volumeOverlayTimer = VOLUME_OVERLAY_DURATION;
        g_volumeOverlayAlpha = 1.0f;
    }

    // SMOOTH SCROLL ANIMATION with easing (only when not scrubbing)
    if (!g_isScrubbing) {
        float scrollDiff = g_targetScrollOffset - g_scrollOffset;
        if (fabsf(scrollDiff) > 0.1f) {
            // Smooth exponential easing
            g_scrollOffset += scrollDiff * LYRICS_SCROLL_EASE_FACTOR * 60.0f * deltaTime;
        } else {
            g_scrollOffset = g_targetScrollOffset;
        }
    }

    // Update background animation
    if (g_bgMode >= BG_MODE_ANIMATED_START) {
        LlzBackgroundUpdate(deltaTime);
    }
}

static void PluginDraw(void) {
    // Background
    DrawBackground();

    // Draw lyrics
    DrawLyrics();

    // Draw gradients
    DrawTopGradient();
    DrawBottomGradient();

    // Track info
    DrawTrackInfo();

    // Line counter
    DrawLineCounter();

    // Indicator overlay
    DrawIndicatorOverlay();

    // Scrub indicator (during drag-to-seek)
    DrawScrubIndicator();

    // Volume overlay bar
    DrawVolumeOverlay();

    // Background style indicator
    if (g_bgMode >= BG_MODE_ANIMATED_START) {
        LlzBackgroundDrawIndicator();
    }
}

static void PluginShutdown(void) {
    if (g_lyricsLoaded) {
        LlzLyricsFree(&g_lyrics);
        g_lyricsLoaded = false;
    }

    // Free line positions array
    if (g_lineYPositions) {
        free(g_lineYPositions);
        g_lineYPositions = NULL;
    }

    // Unload album art
    if (g_albumArt.texture.id != 0) UnloadTexture(g_albumArt.texture);
    if (g_albumArt.blurred.id != 0) UnloadTexture(g_albumArt.blurred);
    CleanupPrevAlbumArt();

    LlzBackgroundShutdown();

    // Free plugin config (saves any pending changes)
    if (g_pluginConfigInitialized) {
        LlzPluginConfigFree(&g_pluginConfig);
        g_pluginConfigInitialized = false;
    }

    // Reset scrub state
    g_isScrubbing = false;
    g_justSeeked = false;

    g_wantsClose = false;
    printf("[LYRICS] Shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin Export
// ============================================================================

static LlzPluginAPI g_api = {
    .name = "Lyrics",
    .description = "Display synced lyrics for current track",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .handles_back_button = false
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
