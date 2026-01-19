/**
 * Clock Plugin - Modern Apple/Spotify-inspired clock for llizardgui-host
 *
 * Features:
 *   - Multiple clock faces (Digital, Analog, Minimal, Flip)
 *   - Time and Stopwatch modes (swipe to switch)
 *   - Multiple color schemes
 *   - Animated backgrounds including blurred album art
 *   - Smooth animations and transitions
 *   - Configurable sizes
 *
 * Controls:
 *   SWIPE LEFT/RIGHT - Switch between Time and Stopwatch modes
 *   SELECT           - Start/Stop/Reset stopwatch (double-tap to reset)
 *   UP/DOWN          - Cycle clock face style
 *   SCROLL           - Cycle color scheme
 *   Button 2         - Cycle background mode
 *   Button 3         - Cycle clock size
 *   BACK             - Exit plugin
 */

#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <webp/decode.h>

// ============================================================================
// Mode Definitions
// ============================================================================

typedef enum {
    CLOCK_MODE_TIME = 0,
    CLOCK_MODE_STOPWATCH,
    CLOCK_MODE_COUNT
} ClockMode;

typedef enum {
    CLOCK_FACE_DIGITAL = 0,
    CLOCK_FACE_ANALOG,
    CLOCK_FACE_MINIMAL,
    CLOCK_FACE_FLIP,
    CLOCK_FACE_COUNT
} ClockFace;

typedef enum {
    CLOCK_SIZE_SMALL = 0,
    CLOCK_SIZE_MEDIUM,
    CLOCK_SIZE_LARGE,
    CLOCK_SIZE_FULLSCREEN,
    CLOCK_SIZE_COUNT
} ClockSize;

typedef enum {
    COLOR_SCHEME_SPOTIFY = 0,
    COLOR_SCHEME_APPLE,
    COLOR_SCHEME_MIDNIGHT,
    COLOR_SCHEME_SUNSET,
    COLOR_SCHEME_OCEAN,
    COLOR_SCHEME_MONO,
    COLOR_SCHEME_COUNT
} ColorScheme;

typedef enum {
    BG_MODE_SOLID = 0,
    BG_MODE_GRADIENT,
    BG_MODE_ALBUM_ART,
    BG_MODE_ANIMATED_START,
    // Animated styles follow...
} BackgroundMode;

static const char *FACE_NAMES[] = {"Digital", "Analog", "Minimal", "Flip"};
static const char *SIZE_NAMES[] = {"Small", "Medium", "Large", "Fullscreen"};
static const char *SCHEME_NAMES[] = {"Spotify", "Apple", "Midnight", "Sunset", "Ocean", "Mono"};
static const char *MODE_NAMES[] = {"Clock", "Stopwatch"};

// ============================================================================
// Color Schemes
// ============================================================================

typedef struct {
    Color background;
    Color backgroundAlt;
    Color primary;
    Color secondary;
    Color accent;
    Color accentSoft;
    Color textPrimary;
    Color textSecondary;
} ClockColorScheme;

static const ClockColorScheme COLOR_SCHEMES[] = {
    // Spotify
    {
        {18, 18, 18, 255}, {25, 25, 25, 255},
        {30, 215, 96, 255}, {29, 185, 84, 255},
        {30, 215, 96, 255}, {30, 215, 96, 60},
        {255, 255, 255, 255}, {179, 179, 179, 255}
    },
    // Apple
    {
        {0, 0, 0, 255}, {28, 28, 30, 255},
        {255, 69, 58, 255}, {255, 55, 95, 255},
        {10, 132, 255, 255}, {10, 132, 255, 60},
        {255, 255, 255, 255}, {142, 142, 147, 255}
    },
    // Midnight
    {
        {15, 15, 35, 255}, {25, 25, 55, 255},
        {138, 43, 226, 255}, {148, 87, 235, 255},
        {138, 43, 226, 255}, {138, 43, 226, 60},
        {255, 255, 255, 255}, {160, 160, 200, 255}
    },
    // Sunset
    {
        {30, 15, 20, 255}, {45, 25, 35, 255},
        {255, 94, 77, 255}, {255, 154, 139, 255},
        {255, 183, 77, 255}, {255, 183, 77, 60},
        {255, 255, 255, 255}, {255, 200, 180, 255}
    },
    // Ocean
    {
        {10, 25, 35, 255}, {15, 40, 55, 255},
        {0, 188, 212, 255}, {77, 208, 225, 255},
        {0, 150, 136, 255}, {0, 150, 136, 60},
        {255, 255, 255, 255}, {176, 190, 197, 255}
    },
    // Mono
    {
        {12, 12, 12, 255}, {24, 24, 24, 255},
        {255, 255, 255, 255}, {200, 200, 200, 255},
        {255, 255, 255, 255}, {255, 255, 255, 40},
        {255, 255, 255, 255}, {128, 128, 128, 255}
    }
};

// ============================================================================
// Configuration Constants
// ============================================================================

#define INDICATOR_DURATION 2.0f
#define TRANSITION_SPEED 8.0f
#define PULSE_SPEED 2.0f
#define SWIPE_THRESHOLD 80.0f
#define DOUBLE_TAP_THRESHOLD 0.4f

// Size multipliers for clock display
static const float SIZE_MULTIPLIERS[] = {0.5f, 0.75f, 1.0f, 1.3f};

// ============================================================================
// Album Art State
// ============================================================================

typedef struct {
    Texture2D texture;
    Texture2D blurred;
    bool loaded;
    char loadedPath[256];
    float alpha;
} AlbumArtState;

// ============================================================================
// Plugin State
// ============================================================================

static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;

// Mode and style state
static ClockMode g_mode = CLOCK_MODE_TIME;
static ClockFace g_face = CLOCK_FACE_DIGITAL;
static ClockSize g_size = CLOCK_SIZE_LARGE;
static ColorScheme g_colorScheme = COLOR_SCHEME_SPOTIFY;
static BackgroundMode g_bgMode = BG_MODE_GRADIENT;

// Animation state
static float g_animTime = 0.0f;
static float g_modeTransition = 0.0f;  // 0 = time, 1 = stopwatch
static float g_indicatorTimer = 0.0f;
static char g_indicatorText[64] = {0};
static float g_pulsePhase = 0.0f;
static float g_secondsAngle = 0.0f;

// Smooth analog clock animation state
static double g_prevSecondAngle = 0.0;      // Previous second hand angle for spring effect
static double g_currentSecondAngle = 0.0;   // Current animated second hand angle
static double g_secondVelocity = 0.0;       // Velocity for spring physics
static float g_tickGlowPhase = 0.0f;        // Phase for tick mark glow animation
static float g_centerPulsePhase = 0.0f;     // Phase for center dot pulse
static int g_lastSecond = -1;               // Track second changes for spring effect

// Swipe detection
static bool g_isSwiping = false;
static float g_swipeStartX = 0.0f;
static float g_swipeOffset = 0.0f;

// Stopwatch state
static bool g_stopwatchRunning = false;
static double g_stopwatchTime = 0.0;  // In seconds
static float g_lastTapTime = 0.0f;
static bool g_waitingForDoubleTap = false;

// Album art
static AlbumArtState g_albumArt = {0};
static AlbumArtState g_prevAlbumArt = {0};
static bool g_inTransition = false;
static char g_trackAlbumArtPath[256] = {0};  // Track current album art path for change detection

// Flip clock digit animation
// g_flipProgress: 1.0 = idle (showing current digit), 0->1 = animating flip
// During animation: 0-0.5 = top flap flipping down, 0.5-1.0 = bottom flap settling
static float g_flipProgress[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};  // HH:MM:SS - start at 1 (idle)
static int g_currentDigits[6] = {-1, -1, -1, -1, -1, -1};   // Current target digits
static int g_previousDigits[6] = {-1, -1, -1, -1, -1, -1};  // Previous digits (shown during flip)

// Flip animation speed (time in seconds to complete one flip)
#define FLIP_ANIMATION_DURATION 0.35f

// Font
static Font g_font = {0};

// Config
static LlzPluginConfig g_config;
static bool g_configInit = false;

// ============================================================================
// Helper Functions
// ============================================================================

static float Clampf(float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
}

static float Lerpf(float a, float b, float t) {
    return a + (b - a) * Clampf(t, 0.0f, 1.0f);
}

static float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

static float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

static float EaseInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

// Spring easing for overshoot effect
static float EaseOutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    float c4 = (2.0f * PI) / 3.0f;
    return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
}

static Color ColorWithAlpha(Color c, float a) {
    return (Color){c.r, c.g, c.b, (unsigned char)(Clampf(a, 0.0f, 1.0f) * 255.0f)};
}

static const ClockColorScheme* GetScheme(void) {
    return &COLOR_SCHEMES[g_colorScheme];
}

static void ShowIndicator(const char *text) {
    strncpy(g_indicatorText, text, sizeof(g_indicatorText) - 1);
    g_indicatorText[sizeof(g_indicatorText) - 1] = '\0';
    g_indicatorTimer = INDICATOR_DURATION;
}

// ============================================================================
// Config Functions
// ============================================================================

static void SaveConfig(void) {
    if (!g_configInit) return;
    LlzPluginConfigSetInt(&g_config, "face", (int)g_face);
    LlzPluginConfigSetInt(&g_config, "size", (int)g_size);
    LlzPluginConfigSetInt(&g_config, "scheme", (int)g_colorScheme);
    LlzPluginConfigSetInt(&g_config, "bg_mode", (int)g_bgMode);
    LlzPluginConfigSave(&g_config);
}

static void LoadConfig(void) {
    if (!g_configInit) return;

    int face = LlzPluginConfigGetInt(&g_config, "face", CLOCK_FACE_DIGITAL);
    if (face >= 0 && face < CLOCK_FACE_COUNT) g_face = (ClockFace)face;

    int size = LlzPluginConfigGetInt(&g_config, "size", CLOCK_SIZE_LARGE);
    if (size >= 0 && size < CLOCK_SIZE_COUNT) g_size = (ClockSize)size;

    int scheme = LlzPluginConfigGetInt(&g_config, "scheme", COLOR_SCHEME_SPOTIFY);
    if (scheme >= 0 && scheme < COLOR_SCHEME_COUNT) g_colorScheme = (ColorScheme)scheme;

    int bg = LlzPluginConfigGetInt(&g_config, "bg_mode", BG_MODE_GRADIENT);
    g_bgMode = (BackgroundMode)bg;
}

// ============================================================================
// Album Art Functions
// ============================================================================

static void UnloadArt(AlbumArtState *art) {
    if (art->texture.id != 0) UnloadTexture(art->texture);
    if (art->blurred.id != 0) UnloadTexture(art->blurred);
    memset(art, 0, sizeof(AlbumArtState));
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
        printf("[CLOCK] LoadImageWebP: failed to open file '%s'\n", path);
        return image;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file data
    uint8_t *fileData = (uint8_t *)malloc(fileSize);
    if (!fileData) {
        printf("[CLOCK] LoadImageWebP: failed to allocate %ld bytes\n", fileSize);
        fclose(file);
        return image;
    }

    size_t bytesRead = fread(fileData, 1, fileSize, file);
    fclose(file);

    if (bytesRead != (size_t)fileSize) {
        printf("[CLOCK] LoadImageWebP: read %zu bytes, expected %ld\n", bytesRead, fileSize);
        free(fileData);
        return image;
    }

    // Decode WebP
    int width = 0, height = 0;
    uint8_t *rgbaData = WebPDecodeRGBA(fileData, fileSize, &width, &height);
    free(fileData);

    if (!rgbaData) {
        printf("[CLOCK] LoadImageWebP: WebPDecodeRGBA failed\n");
        return image;
    }

    // Create raylib Image from RGBA data
    // We need to copy the data because WebP uses its own allocator
    size_t dataSize = width * height * 4;
    void *imageCopy = RL_MALLOC(dataSize);
    if (!imageCopy) {
        printf("[CLOCK] LoadImageWebP: failed to allocate image data\n");
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

    printf("[CLOCK] LoadImageWebP: decoded %dx%d image\n", width, height);
    return image;
}

static void LoadAlbumArt(const char *path) {
    if (!path || path[0] == '\0') {
        printf("[CLOCK] LoadAlbumArt: path is NULL or empty\n");
        return;
    }

    // Already loaded this exact path
    if (g_albumArt.loaded && strcmp(path, g_albumArt.loadedPath) == 0) {
        return;
    }

    printf("[CLOCK] LoadAlbumArt: attempting to load '%s'\n", path);

    // Check file exists
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("[CLOCK] LoadAlbumArt: FILE NOT FOUND '%s'\n", path);
        return;
    }
    printf("[CLOCK] LoadAlbumArt: file exists, size=%ld bytes\n", (long)st.st_size);

    // Load image - use WebP decoder for WebP files
    Image img;
    if (IsWebPFile(path)) {
        printf("[CLOCK] LoadAlbumArt: using WebP decoder\n");
        img = LoadImageWebP(path);
    } else {
        img = LoadImage(path);
    }

    if (img.data == NULL) {
        printf("[CLOCK] LoadAlbumArt: LoadImage FAILED for '%s'\n", path);
        return;
    }
    printf("[CLOCK] LoadAlbumArt: image loaded %dx%d\n", img.width, img.height);

    // Create texture
    Texture2D tex = LoadTextureFromImage(img);
    if (tex.id == 0) {
        UnloadImage(img);
        printf("[CLOCK] LoadAlbumArt: LoadTextureFromImage FAILED\n");
        return;
    }

    // Create blurred version for background
    Texture2D blur = LlzTextureBlur(tex, 20, 0.5f);
    if (blur.id != 0) {
        printf("[CLOCK] LoadAlbumArt: Blurred texture generated, id=%u\n", blur.id);
    } else {
        printf("[CLOCK] LoadAlbumArt: Warning - failed to generate blurred texture\n");
    }

    UnloadImage(img);

    // Setup crossfade transition
    UnloadArt(&g_prevAlbumArt);
    if (g_albumArt.loaded && g_albumArt.texture.id != 0) {
        g_prevAlbumArt = g_albumArt;
        g_prevAlbumArt.alpha = 1.0f;
    } else {
        g_prevAlbumArt.alpha = 0.0f;
    }

    g_albumArt.texture = tex;
    g_albumArt.blurred = blur;
    g_albumArt.loaded = true;
    g_albumArt.alpha = 0.0f;
    strncpy(g_albumArt.loadedPath, path, sizeof(g_albumArt.loadedPath) - 1);
    g_inTransition = true;

    printf("[CLOCK] LoadAlbumArt: SUCCESS texture_id=%u loaded='%s'\n", tex.id, path);
}

static void UpdateAlbumArtTransition(float dt) {
    if (!g_inTransition) return;

    float speed = 2.5f * dt;

    if (g_albumArt.loaded) {
        g_albumArt.alpha += speed;
        if (g_albumArt.alpha >= 1.0f) g_albumArt.alpha = 1.0f;
    }

    if (g_prevAlbumArt.loaded) {
        g_prevAlbumArt.alpha -= speed;
        if (g_prevAlbumArt.alpha <= 0.0f) {
            UnloadArt(&g_prevAlbumArt);
        }
    }

    if (g_albumArt.alpha >= 1.0f && !g_prevAlbumArt.loaded) {
        g_inTransition = false;
    }
}

// ============================================================================
// Time Functions
// ============================================================================

static void GetCurrentTime(int *hours, int *minutes, int *seconds) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (hours) *hours = t->tm_hour;
    if (minutes) *minutes = t->tm_min;
    if (seconds) *seconds = t->tm_sec;
}

// Get precise time with sub-second accuracy
static void GetPreciseTime(int *hours, int *minutes, int *seconds, double *fractionalSecond) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *t = localtime(&tv.tv_sec);
    if (hours) *hours = t->tm_hour;
    if (minutes) *minutes = t->tm_min;
    if (seconds) *seconds = t->tm_sec;
    if (fractionalSecond) *fractionalSecond = tv.tv_usec / 1000000.0;
}

static void FormatTime(double seconds, char *out, size_t size, bool showMs) {
    int total = (int)seconds;
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    int ms = (int)((seconds - total) * 100);

    if (h > 0) {
        if (showMs) {
            snprintf(out, size, "%d:%02d:%02d.%02d", h, m, s, ms);
        } else {
            snprintf(out, size, "%d:%02d:%02d", h, m, s);
        }
    } else {
        if (showMs) {
            snprintf(out, size, "%02d:%02d.%02d", m, s, ms);
        } else {
            snprintf(out, size, "%02d:%02d", m, s);
        }
    }
}

// ============================================================================
// Drawing Functions
// ============================================================================

static void DrawBackground(void) {
    const ClockColorScheme *scheme = GetScheme();

    if (g_bgMode == BG_MODE_SOLID) {
        ClearBackground(scheme->background);
    } else if (g_bgMode == BG_MODE_GRADIENT) {
        ClearBackground(scheme->background);
        DrawRectangleGradientV(0, 0, g_screenWidth, g_screenHeight,
                               scheme->background, scheme->backgroundAlt);

        // Animated glow
        float pulse = 0.5f + 0.5f * sinf(g_pulsePhase * 0.5f);
        Color glow = ColorWithAlpha(scheme->accentSoft, 0.3f * pulse);
        DrawCircleGradient(g_screenWidth / 2, g_screenHeight / 2, 400, glow, BLANK);
    } else if (g_bgMode == BG_MODE_ALBUM_ART) {
        // Draw blurred album art
        bool hasPrev = g_prevAlbumArt.loaded && g_prevAlbumArt.alpha > 0.01f;
        bool hasCurrent = g_albumArt.loaded && g_albumArt.blurred.id != 0;

        if (!hasPrev && !hasCurrent) {
            ClearBackground(scheme->background);
        } else {
            ClearBackground(BLACK);

            if (hasPrev && g_prevAlbumArt.blurred.id != 0) {
                Color tint = ColorAlpha(WHITE, g_prevAlbumArt.alpha);
                Rectangle dest = {0, 0, (float)g_screenWidth, (float)g_screenHeight};
                LlzDrawTextureCover(g_prevAlbumArt.blurred, dest, tint);
            }

            if (hasCurrent && g_albumArt.alpha > 0.01f) {
                Color tint = ColorAlpha(WHITE, g_albumArt.alpha);
                Rectangle dest = {0, 0, (float)g_screenWidth, (float)g_screenHeight};
                LlzDrawTextureCover(g_albumArt.blurred, dest, tint);
            }
        }

        // Dark overlay for readability
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 100});
    } else if (g_bgMode >= BG_MODE_ANIMATED_START) {
        LlzBackgroundDraw();
    }
}

static void DrawDigitalClock(int h, int m, int s, float centerX, float centerY, float scale) {
    const ClockColorScheme *scheme = GetScheme();

    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", h, m);

    // Main time
    float fontSize = 120.0f * scale;
    Vector2 size = MeasureTextEx(g_font, timeStr, fontSize, 2.0f);
    float x = centerX - size.x / 2.0f;
    float y = centerY - size.y / 2.0f - 20.0f * scale;

    // Glow effect
    Color glow = ColorWithAlpha(scheme->accent, 0.3f);
    for (int i = 3; i >= 1; i--) {
        DrawTextEx(g_font, timeStr, (Vector2){x, y + i * 2}, fontSize, 2.0f, glow);
    }

    DrawTextEx(g_font, timeStr, (Vector2){x, y}, fontSize, 2.0f, scheme->textPrimary);

    // Seconds
    char secStr[8];
    snprintf(secStr, sizeof(secStr), ":%02d", s);
    float secSize = 48.0f * scale;
    Vector2 secMeasure = MeasureTextEx(g_font, secStr, secSize, 1.0f);
    float secX = centerX + size.x / 2.0f + 8.0f * scale;
    float secY = y + size.y - secMeasure.y - 8.0f * scale;

    DrawTextEx(g_font, secStr, (Vector2){secX, secY}, secSize, 1.0f, scheme->textSecondary);

    // Blinking colon animation
    float colonAlpha = 0.5f + 0.5f * sinf(g_animTime * 3.0f);
    if ((int)(g_animTime * 2.0f) % 2 == 0) {
        // The colons in timeStr are already visible
    }

    // Date
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char dateStr[64];
    strftime(dateStr, sizeof(dateStr), "%A, %B %d", t);

    float dateSize = 24.0f * scale;
    Vector2 dateMeasure = MeasureTextEx(g_font, dateStr, dateSize, 1.0f);
    float dateX = centerX - dateMeasure.x / 2.0f;
    float dateY = y + size.y + 20.0f * scale;

    DrawTextEx(g_font, dateStr, (Vector2){dateX, dateY}, dateSize, 1.0f, scheme->textSecondary);
}

static void DrawAnalogClock(int h, int m, int s, float centerX, float centerY, float radius) {
    const ClockColorScheme *scheme = GetScheme();
    (void)h; (void)m; (void)s; // We use precise time instead

    // Get precise time with sub-second accuracy
    double fractionalSecond = 0.0;
    int preciseH, preciseM, preciseS;
    GetPreciseTime(&preciseH, &preciseM, &preciseS, &fractionalSecond);

    // Calculate total seconds with fractional part for smooth animation
    double totalSeconds = (double)preciseS + fractionalSecond;
    double totalMinutes = (double)preciseM + totalSeconds / 60.0;
    double totalHours = (double)(preciseH % 12) + totalMinutes / 60.0;

    // Face background with subtle outer glow
    DrawCircle((int)centerX, (int)centerY, radius + 8, ColorWithAlpha(scheme->accent, 0.15f));
    DrawCircle((int)centerX, (int)centerY, radius + 4, ColorWithAlpha(scheme->accent, 0.3f));
    DrawCircle((int)centerX, (int)centerY, radius, scheme->backgroundAlt);

    // Subtle tick mark glow animation - travels around the clock following the second hand
    float glowAngle = (float)(totalSeconds * 6.0 - 90.0) * DEG2RAD;

    // Hour markers with subtle glow effect
    for (int i = 0; i < 12; i++) {
        float angle = (i * 30.0f - 90.0f) * DEG2RAD;
        float innerR = radius * 0.85f;
        float outerR = radius * 0.92f;

        if (i % 3 == 0) {
            innerR = radius * 0.78f;
        }

        Vector2 inner = {centerX + cosf(angle) * innerR, centerY + sinf(angle) * innerR};
        Vector2 outer = {centerX + cosf(angle) * outerR, centerY + sinf(angle) * outerR};

        // Calculate glow based on proximity to the current second position
        float angleDiff = fabsf(fmodf(angle - glowAngle + PI, 2.0f * PI) - PI);
        float glowIntensity = expf(-angleDiff * angleDiff * 8.0f) * 0.5f; // Gaussian falloff

        float thickness = (i % 3 == 0) ? 4.0f : 2.0f;

        // Draw glow layer first
        if (glowIntensity > 0.01f) {
            Color glowColor = ColorWithAlpha(scheme->accent, glowIntensity);
            DrawLineEx(inner, outer, thickness + 3.0f, glowColor);
        }

        DrawLineEx(inner, outer, thickness, scheme->textSecondary);
    }

    // Minute markers with glow
    for (int i = 0; i < 60; i++) {
        if (i % 5 == 0) continue;
        float angle = (i * 6.0f - 90.0f) * DEG2RAD;
        float r = radius * 0.90f;

        // Calculate glow for minute markers too
        float angleDiff = fabsf(fmodf(angle - glowAngle + PI, 2.0f * PI) - PI);
        float glowIntensity = expf(-angleDiff * angleDiff * 12.0f) * 0.4f;

        float dotRadius = 2.0f;

        // Draw glow
        if (glowIntensity > 0.01f) {
            DrawCircle((int)(centerX + cosf(angle) * r), (int)(centerY + sinf(angle) * r),
                       dotRadius + 2.0f, ColorWithAlpha(scheme->accent, glowIntensity));
        }

        DrawCircle((int)(centerX + cosf(angle) * r), (int)(centerY + sinf(angle) * r),
                   dotRadius, ColorWithAlpha(scheme->textSecondary, 0.4f));
    }

    // Shadow offset for hand shadows
    float shadowOffsetX = 3.0f;
    float shadowOffsetY = 4.0f;
    Color shadowColor = ColorWithAlpha(BLACK, 0.25f);

    // Hour hand - smooth continuous movement
    float hourAngle = (float)(totalHours * 30.0 - 90.0) * DEG2RAD;
    float hourLen = radius * 0.5f;
    Vector2 hourCenter = {centerX, centerY};
    Vector2 hourEnd = {centerX + cosf(hourAngle) * hourLen, centerY + sinf(hourAngle) * hourLen};

    // Hour hand shadow
    Vector2 hourShadowCenter = {centerX + shadowOffsetX, centerY + shadowOffsetY};
    Vector2 hourShadowEnd = {hourEnd.x + shadowOffsetX, hourEnd.y + shadowOffsetY};
    DrawLineEx(hourShadowCenter, hourShadowEnd, 8.0f, shadowColor);

    // Hour hand
    DrawLineEx(hourCenter, hourEnd, 8.0f, scheme->textPrimary);

    // Minute hand - smooth continuous movement
    float minAngle = (float)(totalMinutes * 6.0 - 90.0) * DEG2RAD;
    float minLen = radius * 0.7f;
    Vector2 minEnd = {centerX + cosf(minAngle) * minLen, centerY + sinf(minAngle) * minLen};

    // Minute hand shadow
    Vector2 minShadowEnd = {minEnd.x + shadowOffsetX, minEnd.y + shadowOffsetY};
    DrawLineEx(hourShadowCenter, minShadowEnd, 5.0f, shadowColor);

    // Minute hand
    DrawLineEx(hourCenter, minEnd, 5.0f, scheme->textPrimary);

    // Second hand with smooth sweeping and spring overshoot effect
    // Target angle based on precise time (smooth continuous sweep)
    double targetSecAngle = totalSeconds * 6.0 - 90.0;

    // Detect second change for spring effect
    if (preciseS != g_lastSecond && g_lastSecond >= 0) {
        // Add a small velocity boost for spring overshoot when second changes
        g_secondVelocity += 12.0; // degrees per second boost
    }
    g_lastSecond = preciseS;

    // Spring physics for smooth animation with overshoot
    double secAngleDiff = targetSecAngle - g_currentSecondAngle;

    // Handle wrap-around at 60 seconds (360 degrees)
    while (secAngleDiff > 180.0) secAngleDiff -= 360.0;
    while (secAngleDiff < -180.0) secAngleDiff += 360.0;

    // Spring constants for relaxed feel
    double springStiffness = 80.0;   // How quickly it follows (lower = more relaxed)
    double damping = 12.0;           // How quickly oscillation settles

    // Apply spring force
    double springForce = secAngleDiff * springStiffness;
    double dampingForce = g_secondVelocity * damping;
    double acceleration = springForce - dampingForce;

    // Update velocity and position (using frame time approximation)
    double dt = 1.0 / 60.0; // Approximate frame time
    g_secondVelocity += acceleration * dt;
    g_currentSecondAngle += g_secondVelocity * dt;

    // Normalize angle to prevent overflow
    while (g_currentSecondAngle > 360.0) g_currentSecondAngle -= 360.0;
    while (g_currentSecondAngle < -360.0) g_currentSecondAngle += 360.0;

    // Subtle pendulum oscillation at the tip (very gentle)
    float pendulumOscillation = sinf(g_animTime * 3.5f) * 0.5f;
    float secAngle = (float)(g_currentSecondAngle + pendulumOscillation) * DEG2RAD;

    float secLen = radius * 0.8f;
    float secTailLen = radius * 0.15f; // Small tail behind center

    Vector2 secEnd = {centerX + cosf(secAngle) * secLen, centerY + sinf(secAngle) * secLen};
    Vector2 secTail = {centerX - cosf(secAngle) * secTailLen, centerY - sinf(secAngle) * secTailLen};

    // Second hand shadow (lighter than other hands)
    Vector2 secShadowEnd = {secEnd.x + shadowOffsetX * 0.5f, secEnd.y + shadowOffsetY * 0.5f};
    Vector2 secShadowTail = {secTail.x + shadowOffsetX * 0.5f, secTail.y + shadowOffsetY * 0.5f};
    DrawLineEx(secShadowTail, secShadowEnd, 2.5f, ColorWithAlpha(shadowColor, 0.4f));

    // Second hand (with tail extending past center)
    DrawLineEx(secTail, secEnd, 2.0f, scheme->accent);

    // Small circle at the tip of second hand for emphasis
    DrawCircle((int)secEnd.x, (int)secEnd.y, 3.0f, scheme->accent);

    // Center dot with subtle pulsing synchronized with seconds
    float centerPulse = 1.0f + 0.06f * sinf((float)totalSeconds * 2.0f * PI); // Subtle 6% size variation per second
    float centerDotSize = 8.0f * centerPulse;
    float innerDotSize = 4.0f * centerPulse;

    // Center dot shadow
    DrawCircle((int)(centerX + shadowOffsetX * 0.5f), (int)(centerY + shadowOffsetY * 0.5f),
               centerDotSize, ColorWithAlpha(shadowColor, 0.4f));

    // Outer center (accent color)
    DrawCircle((int)centerX, (int)centerY, centerDotSize, scheme->accent);
    // Inner center (primary color)
    DrawCircle((int)centerX, (int)centerY, innerDotSize, scheme->textPrimary);
}

static void DrawMinimalClock(int h, int m, int s, float centerX, float centerY, float scale) {
    const ClockColorScheme *scheme = GetScheme();
    (void)s;  // Minimal doesn't show seconds prominently

    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", h, m);

    float fontSize = 160.0f * scale;
    Vector2 size = MeasureTextEx(g_font, timeStr, fontSize, 4.0f);
    float x = centerX - size.x / 2.0f;
    float y = centerY - size.y / 2.0f;

    // Subtle shadow
    DrawTextEx(g_font, timeStr, (Vector2){x + 2, y + 4}, fontSize, 4.0f,
               ColorWithAlpha(BLACK, 0.3f));

    // Main text
    DrawTextEx(g_font, timeStr, (Vector2){x, y}, fontSize, 4.0f, scheme->textPrimary);

    // Pulsing colon
    float colonPulse = 0.6f + 0.4f * sinf(g_animTime * PI);
    char colonStr[] = ":";
    float colonSize = fontSize;
    Vector2 colonMeasure = MeasureTextEx(g_font, "00", colonSize, 4.0f);
    float colonX = centerX - MeasureTextEx(g_font, colonStr, colonSize, 4.0f).x / 2.0f;
    DrawTextEx(g_font, colonStr, (Vector2){colonX, y}, colonSize, 4.0f,
               ColorWithAlpha(scheme->accent, colonPulse));
}

// Helper to draw a half-card with text clipped to that half
static void DrawFlipHalf(int digit, float x, float y, float width, float height,
                         bool isTopHalf, float scaleY, float pivotOffsetY,
                         const ClockColorScheme *scheme, float shadowIntensity) {
    if (digit < 0) return;

    float halfHeight = height / 2.0f;
    float cardY = isTopHalf ? y : y + halfHeight;

    // Apply vertical scaling for 3D flip effect
    // scaleY: 1.0 = normal, 0.0 = flat (edge-on)
    float scaledHalfHeight = halfHeight * fabsf(scaleY);
    float actualY = cardY;

    if (isTopHalf) {
        // Top half pivots from bottom edge, so it scales upward from the middle
        actualY = y + halfHeight - scaledHalfHeight + pivotOffsetY;
    } else {
        // Bottom half pivots from top edge, scales downward from the middle
        actualY = y + halfHeight + pivotOffsetY;
    }

    // Skip drawing if scaled to nothing
    if (scaledHalfHeight < 1.0f) return;

    // Card background color - darken based on flip angle for 3D shading
    Color cardColor = scheme->backgroundAlt;
    if (shadowIntensity > 0.0f) {
        cardColor.r = (unsigned char)(cardColor.r * (1.0f - shadowIntensity * 0.4f));
        cardColor.g = (unsigned char)(cardColor.g * (1.0f - shadowIntensity * 0.4f));
        cardColor.b = (unsigned char)(cardColor.b * (1.0f - shadowIntensity * 0.4f));
    }

    // Draw the card background
    Rectangle cardRect = {x, actualY, width, scaledHalfHeight};
    DrawRectangleRounded(cardRect, 0.08f, 6, cardColor);

    // Draw subtle border
    DrawRectangleRoundedLinesEx(cardRect, 0.08f, 6, 1.0f, ColorWithAlpha(BLACK, 0.2f));

    // Calculate text position
    char digitStr[2] = {(char)('0' + digit), '\0'};
    float fontSize = height * 0.75f;
    Vector2 measure = MeasureTextEx(g_font, digitStr, fontSize, 1.0f);

    // Text is centered in the full card, then we clip to the half we're drawing
    float textX = x + (width - measure.x) / 2.0f;
    float fullTextY = y + (height - measure.y) / 2.0f;

    // Apply scissor to clip text to this half
    BeginScissorMode((int)x, (int)actualY, (int)width, (int)scaledHalfHeight);

    // Scale text position to match the card scaling
    float scaledTextY;
    if (isTopHalf) {
        // For top half, text needs to scale with the card
        float textOffsetFromMiddle = fullTextY - (y + halfHeight);
        scaledTextY = actualY + scaledHalfHeight + textOffsetFromMiddle * scaleY;
    } else {
        // For bottom half
        float textOffsetFromMiddle = fullTextY - (y + halfHeight);
        scaledTextY = actualY + textOffsetFromMiddle * scaleY;
    }

    DrawTextEx(g_font, digitStr, (Vector2){textX, scaledTextY}, fontSize, 1.0f, scheme->textPrimary);

    EndScissorMode();

    // Top half gets a subtle highlight
    if (isTopHalf && scaleY > 0.5f) {
        Rectangle highlightRect = {x + 2, actualY + 2, width - 4, scaledHalfHeight * 0.3f};
        DrawRectangleRounded(highlightRect, 0.1f, 4, ColorWithAlpha(WHITE, 0.06f * scaleY));
    }
}

static void DrawFlipDigit(int digit, int prevDigit, float progress,
                          float x, float y, float width, float height,
                          const ClockColorScheme *scheme) {
    float halfHeight = height / 2.0f;
    float midY = y + halfHeight;

    // progress: 0.0 = start of flip, 1.0 = end of flip
    // Phase 1 (0.0 - 0.5): Top flap with OLD digit flips down past horizontal
    // Phase 2 (0.5 - 1.0): Bottom flap with NEW digit settles into place

    bool isAnimating = (progress > 0.0f && progress < 1.0f && prevDigit >= 0);

    // --- STATIC HALVES (always visible) ---

    // Bottom half: shows NEW digit (static, revealed as top flap moves away)
    DrawFlipHalf(digit, x, y, width, height, false, 1.0f, 0.0f, scheme, 0.0f);

    // Top half: shows NEW digit (static, visible after animation or when not animating)
    if (!isAnimating || progress >= 0.5f) {
        DrawFlipHalf(digit, x, y, width, height, true, 1.0f, 0.0f, scheme, 0.0f);
    } else {
        // During first half of animation, show OLD digit on static top half
        DrawFlipHalf(prevDigit, x, y, width, height, true, 1.0f, 0.0f, scheme, 0.0f);
    }

    // --- ANIMATED FLAPS ---
    if (isAnimating) {
        if (progress < 0.5f) {
            // Phase 1: Top flap (OLD digit) flipping down
            // Map progress 0->0.5 to flip angle 0->90 degrees (top of card rotating toward viewer)
            float flipPhase = progress * 2.0f;  // 0 to 1
            float easedPhase = EaseInOutQuad(flipPhase);

            // scaleY goes from 1.0 (flat) to 0.0 (edge-on) as we reach 90 degrees
            float scaleY = 1.0f - easedPhase;

            // Shadow increases as flap tilts
            float shadow = easedPhase * 0.6f;

            // Small downward movement as flap rotates
            float pivotOffset = easedPhase * 4.0f;

            // Draw the flipping top half with OLD digit
            DrawFlipHalf(prevDigit, x, y, width, height, true, scaleY, pivotOffset, scheme, shadow);

        } else {
            // Phase 2: Bottom flap (NEW digit) settling down
            // Map progress 0.5->1.0 to flip settling from 90 degrees to flat
            float flipPhase = (progress - 0.5f) * 2.0f;  // 0 to 1
            float easedPhase = EaseOutBack(flipPhase);  // Overshoot for bounce effect

            // scaleY goes from 0.0 (edge-on) to 1.0 (flat)
            float scaleY = Clampf(easedPhase, 0.0f, 1.0f);

            // Shadow decreases as flap settles
            float shadow = (1.0f - easedPhase) * 0.4f;

            // Small upward movement as flap settles
            float pivotOffset = (1.0f - Clampf(easedPhase, 0.0f, 1.0f)) * -4.0f;

            // Draw the settling bottom half with NEW digit on top of the static one
            DrawFlipHalf(digit, x, y, width, height, false, scaleY, pivotOffset, scheme, shadow);
        }

        // Draw drop shadow under the flipping card for depth
        float shadowAlpha = 0.15f;
        if (progress < 0.5f) {
            shadowAlpha *= progress * 2.0f;
        } else {
            shadowAlpha *= (1.0f - progress) * 2.0f;
        }
        Rectangle shadowRect = {x + 4, midY + 2, width - 8, 6};
        DrawRectangleRounded(shadowRect, 0.5f, 4, ColorWithAlpha(BLACK, shadowAlpha));
    }

    // Horizontal divider line at the middle
    DrawLineEx((Vector2){x, midY}, (Vector2){x + width, midY}, 2.0f,
               ColorWithAlpha(BLACK, 0.4f));

    // Card frame/border
    Rectangle cardRect = {x, y, width, height};
    DrawRectangleRoundedLinesEx(cardRect, 0.08f, 6, 2.0f, ColorWithAlpha(BLACK, 0.3f));
}

static void DrawFlipClock(int h, int m, int s, float centerX, float centerY, float scale) {
    const ClockColorScheme *scheme = GetScheme();
    (void)h; (void)m; (void)s;  // We use g_currentDigits instead

    float digitW = 80.0f * scale;
    float digitH = 120.0f * scale;
    float gap = 12.0f * scale;
    float colonW = 24.0f * scale;

    // HH:MM:SS = 6 digits + 2 colons
    float totalWidth = digitW * 6 + colonW * 2 + gap * 7;
    float startX = centerX - totalWidth / 2.0f;
    float baseY = centerY - digitH / 2.0f;

    float x = startX;
    for (int i = 0; i < 6; i++) {
        // Use g_currentDigits (target) and g_previousDigits (old value during animation)
        DrawFlipDigit(g_currentDigits[i], g_previousDigits[i], g_flipProgress[i],
                      x, baseY, digitW, digitH, scheme);
        x += digitW + gap;

        // Draw colon after position 1 (hours) and 3 (minutes)
        if (i == 1 || i == 3) {
            float colonY = baseY + digitH * 0.3f;
            float colonGap = digitH * 0.25f;

            // Colon between hours:minutes is always solid
            // Colon between minutes:seconds pulses
            float pulse = 0.5f + 0.5f * sinf(g_animTime * PI * 2.0f);
            Color colonColor = (i == 1) ? scheme->textPrimary : ColorWithAlpha(scheme->textPrimary, 0.4f + 0.6f * pulse);

            // Draw colon dots with slight 3D effect
            float dotRadius = 5.0f * scale;
            DrawCircle((int)(x + colonW / 2.0f), (int)colonY, dotRadius + 1, ColorWithAlpha(BLACK, 0.3f));
            DrawCircle((int)(x + colonW / 2.0f), (int)(colonY + colonGap), dotRadius + 1, ColorWithAlpha(BLACK, 0.3f));
            DrawCircle((int)(x + colonW / 2.0f), (int)colonY, dotRadius, colonColor);
            DrawCircle((int)(x + colonW / 2.0f), (int)(colonY + colonGap), dotRadius, colonColor);

            x += colonW + gap;
        }
    }
}

static void DrawStopwatch(float centerX, float centerY, float scale) {
    const ClockColorScheme *scheme = GetScheme();

    char timeStr[32];
    FormatTime(g_stopwatchTime, timeStr, sizeof(timeStr), true);

    float fontSize = 100.0f * scale;
    Vector2 size = MeasureTextEx(g_font, timeStr, fontSize, 2.0f);
    float x = centerX - size.x / 2.0f;
    float y = centerY - size.y / 2.0f - 30.0f * scale;

    // Glow when running
    if (g_stopwatchRunning) {
        float pulse = 0.5f + 0.5f * sinf(g_pulsePhase * 3.0f);
        Color glow = ColorWithAlpha(scheme->accent, 0.4f * pulse);
        for (int i = 4; i >= 1; i--) {
            DrawTextEx(g_font, timeStr, (Vector2){x, y + i * 1.5f}, fontSize, 2.0f, glow);
        }
    }

    DrawTextEx(g_font, timeStr, (Vector2){x, y}, fontSize, 2.0f, scheme->textPrimary);

    // Status and instructions
    const char *status = g_stopwatchRunning ? "Running" : (g_stopwatchTime > 0 ? "Paused" : "Ready");
    Color statusColor = g_stopwatchRunning ? scheme->accent : scheme->textSecondary;

    float statusSize = 24.0f * scale;
    Vector2 statusMeasure = MeasureTextEx(g_font, status, statusSize, 1.0f);
    float statusX = centerX - statusMeasure.x / 2.0f;
    float statusY = y + size.y + 20.0f * scale;

    // Status pill
    Rectangle pill = {statusX - 16, statusY - 4, statusMeasure.x + 32, statusMeasure.y + 8};
    DrawRectangleRounded(pill, 0.5f, 8, ColorWithAlpha(statusColor, 0.2f));
    DrawTextEx(g_font, status, (Vector2){statusX, statusY}, statusSize, 1.0f, statusColor);

    // Instructions
    const char *instr = g_stopwatchRunning ? "Tap to pause" :
                        (g_stopwatchTime > 0 ? "Tap to resume, double-tap to reset" : "Tap to start");
    float instrSize = 16.0f * scale;
    Vector2 instrMeasure = MeasureTextEx(g_font, instr, instrSize, 1.0f);
    float instrX = centerX - instrMeasure.x / 2.0f;
    float instrY = statusY + statusMeasure.y + 24.0f * scale;

    DrawTextEx(g_font, instr, (Vector2){instrX, instrY}, instrSize, 1.0f,
               ColorWithAlpha(scheme->textSecondary, 0.7f));

    // Lap indicator ring when running
    if (g_stopwatchRunning) {
        float ringRadius = 180.0f * scale;
        float ringThickness = 4.0f;
        float progress = (float)fmod(g_stopwatchTime, 60.0) / 60.0f;
        float startAngle = -90.0f;
        float endAngle = startAngle + progress * 360.0f;

        // Background ring
        DrawRing((Vector2){centerX, centerY}, ringRadius - ringThickness/2,
                 ringRadius + ringThickness/2, 0, 360, 64, ColorWithAlpha(scheme->textSecondary, 0.2f));

        // Progress ring
        DrawRing((Vector2){centerX, centerY}, ringRadius - ringThickness/2,
                 ringRadius + ringThickness/2, startAngle, endAngle, 64, scheme->accent);
    }
}

static void DrawModeIndicator(void) {
    const ClockColorScheme *scheme = GetScheme();

    float dotRadius = 6.0f;
    float spacing = 20.0f;
    float y = g_screenHeight - 40.0f;
    float startX = g_screenWidth / 2.0f - spacing / 2.0f;

    for (int i = 0; i < CLOCK_MODE_COUNT; i++) {
        float x = startX + (i - 0.5f) * spacing;
        Color dotColor = (i == (int)g_mode) ? scheme->accent : ColorWithAlpha(scheme->textSecondary, 0.4f);
        float radius = (i == (int)g_mode) ? dotRadius : dotRadius * 0.7f;
        DrawCircle((int)x, (int)y, radius, dotColor);
    }
}

static void DrawSwipeHint(float progress) {
    if (fabsf(progress) < 0.01f) return;

    const ClockColorScheme *scheme = GetScheme();
    float alpha = Clampf(fabsf(progress) / (SWIPE_THRESHOLD * 2.0f), 0.0f, 0.8f);

    // Arrow indicator
    float arrowX = (progress > 0) ? g_screenWidth - 60 : 60;
    float arrowY = g_screenHeight / 2.0f;

    const char *arrow = (progress > 0) ? ">" : "<";
    float fontSize = 48.0f;
    Vector2 measure = MeasureTextEx(g_font, arrow, fontSize, 1.0f);

    DrawTextEx(g_font, arrow, (Vector2){arrowX - measure.x/2, arrowY - measure.y/2},
               fontSize, 1.0f, ColorWithAlpha(scheme->accent, alpha));
}

static void DrawIndicatorOverlay(void) {
    if (g_indicatorTimer <= 0.0f || g_indicatorText[0] == '\0') return;

    const ClockColorScheme *scheme = GetScheme();
    float alpha = Clampf(g_indicatorTimer / 0.5f, 0.0f, 1.0f);

    float fontSize = 20.0f;
    float padding = 16.0f;
    Vector2 measure = MeasureTextEx(g_font, g_indicatorText, fontSize, 1.0f);

    float x = (g_screenWidth - measure.x - padding * 2) / 2.0f;
    float y = g_screenHeight - 100.0f;

    Rectangle box = {x, y, measure.x + padding * 2, measure.y + padding};
    DrawRectangleRounded(box, 0.4f, 8, ColorWithAlpha((Color){0, 0, 0, 200}, alpha));
    DrawRectangleRoundedLinesEx(box, 0.4f, 8, 1.0f, ColorWithAlpha(scheme->accent, alpha * 0.6f));

    DrawTextEx(g_font, g_indicatorText, (Vector2){x + padding, y + padding/2}, fontSize, 1.0f,
               ColorWithAlpha(WHITE, alpha));
}

// ============================================================================
// Plugin API
// ============================================================================

static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;

    // Initialize config
    LlzPluginConfigEntry defaults[] = {
        {"face", "0"},
        {"size", "2"},
        {"scheme", "0"},
        {"bg_mode", "1"}
    };
    g_configInit = LlzPluginConfigInit(&g_config, "clock", defaults, 4);
    LoadConfig();

    // Initialize media for album art
    LlzMediaInit(NULL);

    // Initialize background system
    LlzBackgroundInit(width, height);
    if (g_bgMode >= BG_MODE_ANIMATED_START) {
        LlzBackgroundSetEnabled(true);
        LlzBackgroundSetStyle((LlzBackgroundStyle)(g_bgMode - BG_MODE_ANIMATED_START), false);
    } else {
        LlzBackgroundSetEnabled(false);
    }

    // Load font
    g_font = LlzFontGet(LLZ_FONT_UI, 48);
    if (g_font.texture.id == 0) {
        g_font = GetFontDefault();
    }

    // Reset state
    g_animTime = 0.0f;
    g_mode = CLOCK_MODE_TIME;
    g_modeTransition = 0.0f;
    g_stopwatchRunning = false;
    g_stopwatchTime = 0.0;
    g_pulsePhase = 0.0f;

    // Initialize flip clock state - start with current time, no animation
    for (int i = 0; i < 6; i++) {
        g_flipProgress[i] = 1.0f;  // Start at 1.0 (idle, not animating)
        g_currentDigits[i] = -1;
        g_previousDigits[i] = -1;
    }

    // Initialize analog clock smooth animation state
    // Start the second hand at the current position to avoid initial animation jump
    double fractionalSecond = 0.0;
    int initH, initM, initS;
    GetPreciseTime(&initH, &initM, &initS, &fractionalSecond);
    double initTotalSeconds = (double)initS + fractionalSecond;
    g_currentSecondAngle = initTotalSeconds * 6.0 - 90.0; // Current angle in degrees
    g_secondVelocity = 0.0;
    g_lastSecond = initS;
    g_tickGlowPhase = 0.0f;
    g_centerPulsePhase = 0.0f;

    // Load album art if available
    LlzMediaState state;
    if (LlzMediaGetState(&state)) {
        if (state.albumArtPath[0] != '\0') {
            strncpy(g_trackAlbumArtPath, state.albumArtPath, sizeof(g_trackAlbumArtPath) - 1);
            LoadAlbumArt(state.albumArtPath);
        } else if (state.artist[0] != '\0' || state.album[0] != '\0') {
            // Generate path from artist/album hash if direct path not available
            const char *hash = LlzMediaGenerateArtHash(state.artist, state.album);
            if (hash && hash[0] != '\0') {
                char generatedPath[512];
                snprintf(generatedPath, sizeof(generatedPath),
                         "/var/mediadash/album_art_cache/%s.webp", hash);
                LoadAlbumArt(generatedPath);
            }
        }
    }

    printf("[CLOCK] Initialized (%dx%d)\n", width, height);
}

static void PluginUpdate(const LlzInputState *input, float dt) {
    // Back button
    if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
        g_wantsClose = true;
        return;
    }

    // Update timers
    g_animTime += dt;
    g_pulsePhase += dt;
    if (g_indicatorTimer > 0) g_indicatorTimer -= dt;

    // Update stopwatch
    if (g_stopwatchRunning) {
        g_stopwatchTime += dt;
    }

    // Double-tap detection
    if (g_waitingForDoubleTap) {
        g_lastTapTime += dt;
        if (g_lastTapTime > DOUBLE_TAP_THRESHOLD) {
            g_waitingForDoubleTap = false;
        }
    }

    // Mode transition animation
    float targetTransition = (g_mode == CLOCK_MODE_STOPWATCH) ? 1.0f : 0.0f;
    g_modeTransition = Lerpf(g_modeTransition, targetTransition, dt * TRANSITION_SPEED);

    // Update flip clock animations
    int h, m, s;
    GetCurrentTime(&h, &m, &s);
    int timeDigits[6] = {h/10, h%10, m/10, m%10, s/10, s%10};

    for (int i = 0; i < 6; i++) {
        // Check if digit changed
        if (timeDigits[i] != g_currentDigits[i]) {
            // Only start animation if we have a previous digit to animate from
            if (g_currentDigits[i] >= 0) {
                // Store the previous digit for animation
                g_previousDigits[i] = g_currentDigits[i];
                // Reset animation progress to start the flip
                g_flipProgress[i] = 0.0f;
            }
            // Update current digit to the new value
            g_currentDigits[i] = timeDigits[i];
        }

        // Advance flip animation
        if (g_flipProgress[i] < 1.0f) {
            g_flipProgress[i] += dt / FLIP_ANIMATION_DURATION;
            if (g_flipProgress[i] >= 1.0f) {
                g_flipProgress[i] = 1.0f;
                // Animation complete - clear previous digit
                g_previousDigits[i] = -1;
            }
        }
    }

    // Album art transitions
    UpdateAlbumArtTransition(dt);

    // Check for album art updates (matches lyrics plugin logic)
    LlzMediaState state;
    if (LlzMediaGetState(&state)) {
        // Always try to load album art - LoadAlbumArt returns early if already loaded
        // This handles:
        // 1. albumArtPath changes
        // 2. Track changes but albumArtPath hasn't updated yet
        // 3. albumArtPath is empty but we can generate from artist/album
        if (state.albumArtPath[0] != '\0') {
            LoadAlbumArt(state.albumArtPath);
        } else if (state.artist[0] != '\0' || state.album[0] != '\0') {
            // Generate path from artist/album hash
            const char *hash = LlzMediaGenerateArtHash(state.artist, state.album);
            if (hash && hash[0] != '\0') {
                char generatedPath[512];
                snprintf(generatedPath, sizeof(generatedPath),
                         "/var/mediadash/album_art_cache/%s.webp", hash);
                LoadAlbumArt(generatedPath);
            }
        }

        // Track path changes
        if (strcmp(state.albumArtPath, g_trackAlbumArtPath) != 0) {
            strncpy(g_trackAlbumArtPath, state.albumArtPath, sizeof(g_trackAlbumArtPath) - 1);
        }
    }

    // Background update
    if (g_bgMode >= BG_MODE_ANIMATED_START) {
        LlzBackgroundUpdate(dt);
    }

    // === INPUT HANDLING ===

    // Swipe detection for mode switching
    if (input->swipeLeft) {
        g_mode = (ClockMode)((g_mode + 1) % CLOCK_MODE_COUNT);
        ShowIndicator(MODE_NAMES[g_mode]);
    }
    if (input->swipeRight) {
        g_mode = (ClockMode)((g_mode - 1 + CLOCK_MODE_COUNT) % CLOCK_MODE_COUNT);
        ShowIndicator(MODE_NAMES[g_mode]);
    }

    // Drag for swipe feedback
    if (input->mouseJustPressed) {
        g_isSwiping = true;
        g_swipeStartX = input->mousePos.x;
        g_swipeOffset = 0.0f;
    }
    if (g_isSwiping && input->mousePressed) {
        g_swipeOffset = input->mousePos.x - g_swipeStartX;
    }
    if (input->mouseJustReleased) {
        g_isSwiping = false;
        g_swipeOffset = 0.0f;
    }

    // Tap in stopwatch mode
    if (g_mode == CLOCK_MODE_STOPWATCH) {
        if (input->tap || (input->selectPressed)) {
            if (g_waitingForDoubleTap && g_lastTapTime < DOUBLE_TAP_THRESHOLD) {
                // Double tap - reset
                g_stopwatchRunning = false;
                g_stopwatchTime = 0.0;
                g_waitingForDoubleTap = false;
                ShowIndicator("Reset");
            } else {
                // Single tap - toggle run
                g_stopwatchRunning = !g_stopwatchRunning;
                g_waitingForDoubleTap = true;
                g_lastTapTime = 0.0f;
                ShowIndicator(g_stopwatchRunning ? "Started" : "Paused");
            }
        }
    }

    // UP/DOWN - cycle face
    if (input->upPressed || IsKeyPressed(KEY_UP)) {
        g_face = (ClockFace)((g_face + 1) % CLOCK_FACE_COUNT);
        ShowIndicator(FACE_NAMES[g_face]);
        SaveConfig();
    }
    if (input->downPressed || IsKeyPressed(KEY_DOWN)) {
        g_face = (ClockFace)((g_face - 1 + CLOCK_FACE_COUNT) % CLOCK_FACE_COUNT);
        ShowIndicator(FACE_NAMES[g_face]);
        SaveConfig();
    }

    // Scroll - cycle color scheme
    if (input->scrollDelta != 0) {
        if (input->scrollDelta > 0) {
            g_colorScheme = (ColorScheme)((g_colorScheme + 1) % COLOR_SCHEME_COUNT);
        } else {
            g_colorScheme = (ColorScheme)((g_colorScheme - 1 + COLOR_SCHEME_COUNT) % COLOR_SCHEME_COUNT);
        }
        ShowIndicator(SCHEME_NAMES[g_colorScheme]);

        // Update background colors
        if (g_bgMode >= BG_MODE_ANIMATED_START) {
            const ClockColorScheme *scheme = GetScheme();
            LlzBackgroundSetColors(scheme->background, scheme->accent);
        }
        SaveConfig();
    }

    // Button 2 (displayModeNext) - cycle background
    if (input->displayModeNext || IsKeyPressed(KEY_B)) {
        g_bgMode++;
        if (g_bgMode == BG_MODE_ALBUM_ART + 1) {
            g_bgMode = BG_MODE_ANIMATED_START;
            LlzBackgroundSetEnabled(true);
            LlzBackgroundSetStyle(LLZ_BG_STYLE_PULSE, true);
            const ClockColorScheme *scheme = GetScheme();
            LlzBackgroundSetColors(scheme->background, scheme->accent);
            ShowIndicator("Background: Pulse");
        } else if (g_bgMode >= BG_MODE_ANIMATED_START + LLZ_BG_STYLE_COUNT) {
            g_bgMode = BG_MODE_SOLID;
            LlzBackgroundSetEnabled(false);
            ShowIndicator("Background: Solid");
        } else if (g_bgMode >= BG_MODE_ANIMATED_START) {
            LlzBackgroundCycleNext();
            char buf[64];
            snprintf(buf, sizeof(buf), "Background: %s", LlzBackgroundGetStyleName(LlzBackgroundGetStyle()));
            ShowIndicator(buf);
        } else if (g_bgMode == BG_MODE_GRADIENT) {
            ShowIndicator("Background: Gradient");
        } else if (g_bgMode == BG_MODE_ALBUM_ART) {
            ShowIndicator("Background: Album Art");
        }
        SaveConfig();
    }

    // Button 3 (styleCyclePressed) - cycle size
    if (input->styleCyclePressed || IsKeyPressed(KEY_S)) {
        g_size = (ClockSize)((g_size + 1) % CLOCK_SIZE_COUNT);
        ShowIndicator(SIZE_NAMES[g_size]);
        SaveConfig();
    }
}

static void PluginDraw(void) {
    DrawBackground();

    float centerX = g_screenWidth / 2.0f;
    float centerY = g_screenHeight / 2.0f;
    float scale = SIZE_MULTIPLIERS[g_size];

    // Offset for mode transition
    float offset = g_swipeOffset * 0.3f;

    // Draw based on mode (with crossfade)
    if (g_modeTransition < 0.99f) {
        int h, m, s;
        GetCurrentTime(&h, &m, &s);

        float clockCenterX = centerX + offset + g_modeTransition * g_screenWidth;
        float alpha = 1.0f - g_modeTransition;

        BeginBlendMode(BLEND_ALPHA);

        switch (g_face) {
            case CLOCK_FACE_DIGITAL:
                DrawDigitalClock(h, m, s, clockCenterX, centerY, scale);
                break;
            case CLOCK_FACE_ANALOG:
                DrawAnalogClock(h, m, s, clockCenterX, centerY, 150.0f * scale);
                break;
            case CLOCK_FACE_MINIMAL:
                DrawMinimalClock(h, m, s, clockCenterX, centerY, scale);
                break;
            case CLOCK_FACE_FLIP:
                DrawFlipClock(h, m, s, clockCenterX, centerY, scale);
                break;
            default:
                DrawDigitalClock(h, m, s, clockCenterX, centerY, scale);
                break;
        }

        EndBlendMode();
    }

    if (g_modeTransition > 0.01f) {
        float swCenterX = centerX + offset - (1.0f - g_modeTransition) * g_screenWidth;
        DrawStopwatch(swCenterX, centerY, scale);
    }

    // UI overlays
    DrawModeIndicator();
    DrawSwipeHint(g_swipeOffset);
    DrawIndicatorOverlay();

    // Background indicator
    if (g_bgMode >= BG_MODE_ANIMATED_START) {
        LlzBackgroundDrawIndicator();
    }
}

static void PluginShutdown(void) {
    UnloadArt(&g_albumArt);
    UnloadArt(&g_prevAlbumArt);

    LlzBackgroundShutdown();

    if (g_configInit) {
        LlzPluginConfigFree(&g_config);
        g_configInit = false;
    }

    g_wantsClose = false;
    printf("[CLOCK] Shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin Export
// ============================================================================

static LlzPluginAPI g_api = {
    .name = "Clock",
    .description = "Modern clock with multiple styles",
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
