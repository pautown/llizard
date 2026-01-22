#include "llizard_plugin.h"
#include "llz_sdk_input.h"
#include "llz_sdk.h"
#include "llz_sdk_image.h"
#include "llz_sdk_config.h"
#include "llz_sdk_navigation.h"
#include "raylib.h"

#include "nowplaying/core/np_theme.h"
#include "nowplaying/core/np_effects.h"
#include "nowplaying/screens/np_screen_now_playing.h"
#include "nowplaying/overlays/np_overlay_manager.h"
#include "nowplaying/overlays/np_overlay_clock.h"
#include "nowplaying/overlays/np_overlay_colorpicker.h"
#include "nowplaying/overlays/np_overlay_lyrics.h"
#include "nowplaying/overlays/np_overlay_media_channels.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <webp/decode.h>

// Screen dimensions
static int g_screenWidth = 800;
static int g_screenHeight = 480;

// State
static bool g_wantsClose = false;
static NpNowPlayingScreen g_nowPlayingScreen;
static NpOverlayManager g_overlayManager;
static NpDisplayMode g_displayMode = NP_DISPLAY_NORMAL;
static float g_volumeOverlayTimer = 0.0f;
static float g_volumeOverlayAlpha = 0.0f;
static int g_volumeOverlayValue = 0;
static const float VOLUME_OVERLAY_DURATION = 2.0f;

// Color picker overlay state
static NpColorPickerOverlay g_colorPicker;

// Media channels overlay state
static NpMediaChannelsOverlay g_mediaChannelsOverlay;

static char g_trackTitle[LLZ_MEDIA_TEXT_MAX] = "No track";
static char g_trackArtist[LLZ_MEDIA_TEXT_MAX] = "No artist";
static char g_trackAlbum[LLZ_MEDIA_TEXT_MAX] = "No album";
static char g_mediaChannel[LLZ_MEDIA_TEXT_MAX] = "";

// Media bridge state
static bool g_mediaInitialized = false;
static bool g_mediaStateValid = false;
static float g_mediaRefreshTimer = 0.0f;
static const float MEDIA_REFRESH_INTERVAL = 0.25f;
static LlzMediaState g_mediaState;
static bool g_scrubActive = false;

// Seek cooldown to prevent accidental track skip after scrubbing
static bool g_justSeeked = false;
static float g_justSeekedTimer = 0.0f;
static const float JUST_SEEKED_COOLDOWN = 0.5f;

// Play/pause grace period - ignore Redis isPlaying updates briefly after toggle
// to prevent "flicker" while BLE client propagates the state change
static float g_playPauseGracePeriod = 0.0f;
static const float PLAY_PAUSE_GRACE_DURATION = 0.5f;

// Album art texture state
static Texture2D g_albumArtTexture = {0};
static Texture2D g_albumArtBlurred = {0};
static bool g_albumArtLoaded = false;
static char g_albumArtLoadedPath[LLZ_MEDIA_PATH_MAX] = {0};

// Album art crossfade transition state
typedef struct {
    Texture2D prevTexture;      // Previous album art (for crossfade out)
    Texture2D prevBlurred;      // Previous blurred (for crossfade out)
    float currentAlpha;         // Alpha for current textures (fade in)
    float prevAlpha;            // Alpha for previous textures (fade out)
    bool inTransition;          // Currently transitioning
    bool fadingOut;             // Fading out with no replacement
} AlbumArtTransition;

static AlbumArtTransition g_albumArtTransition = {0};
static const float ALBUM_ART_FADE_SPEED = 3.0f;  // Fade duration ~0.33s

// Album art extracted colors for UI elements
typedef struct {
    Color primary;          // Dominant color from album art
    Color accent;           // Vibrant/saturated variant
    Color complementary;    // Complementary color for contrast
    bool hasColors;         // Whether colors have been extracted
} AlbumArtColors;

static AlbumArtColors g_albumArtColors = {0};

// Playback state used by rendering layer
static NpPlaybackState g_playback = {
    .isPlaying = false,
    .volume = 60,
    .trackPosition = 0.0f,
    .trackDuration = 0.0f,
    .trackTitle = g_trackTitle,
    .trackArtist = g_trackArtist,
    .trackAlbum = g_trackAlbum,
    .mediaChannel = g_mediaChannel,
    .shuffleEnabled = false,
    .repeatEnabled = false
};

static void MediaInitialize(void);
static void MediaPoll(float deltaTime);
static void MediaApplyState(const LlzMediaState *state);
static void TogglePlayback(void);
static void SkipTrack(bool next);
static void HandleScrubState(const NpPlaybackActions *actions);
static void LoadAlbumArtTexture(const char *path);
static void UnloadAlbumArtTexture(void);
static void UpdateAlbumArtTransition(float deltaTime);
static void CleanupPrevAlbumArt(void);

// Background system now uses SDK - see llz_sdk_background.h
// Local state for tracking if background was enabled in this session
static bool g_bgStyleEnabled = false;
static int g_bgStyleIndex = 0;

typedef struct {
    bool active;
    bool isNext;          // true = next track, false = previous
    float timer;          // countdown timer
    float alpha;          // current alpha for fading
} SwipeIndicatorState;

static SwipeIndicatorState g_swipeIndicator = {0};
static const float SWIPE_INDICATOR_DURATION = 0.8f;

// Plugin config for persistent settings
static LlzPluginConfig g_pluginConfig;
static bool g_pluginConfigInitialized = false;

// Save all settings to config
static void SavePluginSettings(void) {
    if (!g_pluginConfigInitialized) return;

    LlzPluginConfigSetInt(&g_pluginConfig, "display_mode", (int)g_displayMode);
    LlzPluginConfigSetInt(&g_pluginConfig, "bg_style", (int)LlzBackgroundGetStyle());
    LlzPluginConfigSetInt(&g_pluginConfig, "theme_variant", (int)NpThemeGetVariant());
    LlzPluginConfigSetBool(&g_pluginConfig, "bg_style_enabled", LlzBackgroundIsEnabled());

    // Save custom color if set
    if (NpThemeHasCustomBackgroundColor()) {
        Color customBg = NpThemeGetColor(NP_COLOR_BG_DARK);
        char colorStr[32];
        snprintf(colorStr, sizeof(colorStr), "%d,%d,%d", customBg.r, customBg.g, customBg.b);
        LlzPluginConfigSetString(&g_pluginConfig, "custom_color", colorStr);
        LlzPluginConfigSetBool(&g_pluginConfig, "has_custom_color", true);
    } else {
        LlzPluginConfigSetBool(&g_pluginConfig, "has_custom_color", false);
    }

    LlzPluginConfigSave(&g_pluginConfig);
}

// Parse RGB color string "r,g,b" to Color
static bool ParseColorString(const char *str, Color *outColor) {
    if (!str || !outColor) return false;

    int r, g, b;
    if (sscanf(str, "%d,%d,%d", &r, &g, &b) == 3) {
        outColor->r = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
        outColor->g = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
        outColor->b = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
        outColor->a = 255;
        return true;
    }
    return false;
}

// Load settings from config (call after theme and screen init)
static void LoadPluginSettings(void) {
    if (!g_pluginConfigInitialized) return;

    // Load display mode
    int displayMode = LlzPluginConfigGetInt(&g_pluginConfig, "display_mode", NP_DISPLAY_NORMAL);
    if (displayMode >= 0 && displayMode < NP_DISPLAY_MODE_COUNT) {
        g_displayMode = (NpDisplayMode)displayMode;
    }

    // Load theme variant
    int themeVariant = LlzPluginConfigGetInt(&g_pluginConfig, "theme_variant", NP_THEME_VARIANT_ZUNE);
    if (themeVariant >= 0 && themeVariant < NP_THEME_VARIANT_COUNT) {
        NpThemeSetVariant((NpThemeVariant)themeVariant);
    }

    // Load background style using SDK
    bool bgStyleEnabled = LlzPluginConfigGetBool(&g_pluginConfig, "bg_style_enabled", false);
    int bgStyle = LlzPluginConfigGetInt(&g_pluginConfig, "bg_style", LLZ_BG_STYLE_PULSE);
    if (bgStyleEnabled && bgStyle >= 0 && bgStyle < LlzBackgroundGetStyleCount()) {
        g_bgStyleEnabled = true;
        g_bgStyleIndex = bgStyle;
        LlzBackgroundSetStyle((LlzBackgroundStyle)bgStyle, false);
        // Set colors from album art if available
        if (g_albumArtColors.hasColors) {
            LlzBackgroundSetColors(g_albumArtColors.primary, g_albumArtColors.accent);
        }
    }

    // Load custom color if set
    bool hasCustomColor = LlzPluginConfigGetBool(&g_pluginConfig, "has_custom_color", false);
    if (hasCustomColor) {
        const char *colorStr = LlzPluginConfigGetString(&g_pluginConfig, "custom_color");
        Color customColor;
        if (colorStr && ParseColorString(colorStr, &customColor)) {
            NpThemeSetCustomBackgroundColor(customColor);
            printf("[NOWPLAYING] Restored custom color: RGB(%d,%d,%d)\n",
                   customColor.r, customColor.g, customColor.b);
        }
    }

    printf("[NOWPLAYING] Loaded settings: display_mode=%d, theme=%d, bg_style=%d (enabled=%d)\n",
           g_displayMode, NpThemeGetVariant(), LlzBackgroundGetStyle(), LlzBackgroundIsEnabled());
}

static float Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

// Helper to update SDK background colors when album art changes
static void UpdateBackgroundColors(void)
{
    if (g_albumArtColors.hasColors) {
        LlzBackgroundSetColors(g_albumArtColors.primary, g_albumArtColors.accent);
    } else {
        LlzBackgroundClearColors();
    }
}

// Helper to draw background (SDK or fallback to theme)
static void DrawBackground(void)
{
    if (LlzBackgroundIsEnabled()) {
        // Update blur texture state for SDK
        LlzBackgroundSetBlurTexture(g_albumArtBlurred, g_albumArtTransition.prevBlurred,
                                     g_albumArtTransition.currentAlpha, g_albumArtTransition.prevAlpha);
        LlzBackgroundDraw();
    } else {
        NpThemeDrawBackground();
    }
}


static void TriggerSwipeIndicator(bool isNext)
{
    g_swipeIndicator.active = true;
    g_swipeIndicator.isNext = isNext;
    g_swipeIndicator.timer = SWIPE_INDICATOR_DURATION;
    g_swipeIndicator.alpha = 1.0f;
}

static void UpdateSwipeIndicator(float deltaTime)
{
    if (!g_swipeIndicator.active) return;

    g_swipeIndicator.timer -= deltaTime;
    if (g_swipeIndicator.timer <= 0.0f) {
        g_swipeIndicator.active = false;
        g_swipeIndicator.timer = 0.0f;
        g_swipeIndicator.alpha = 0.0f;
    } else {
        // Fade out in last 0.3 seconds
        float fadeStart = 0.3f;
        if (g_swipeIndicator.timer < fadeStart) {
            g_swipeIndicator.alpha = g_swipeIndicator.timer / fadeStart;
        }
    }
}

static void DrawSwipeIndicator(void)
{
    if (!g_swipeIndicator.active || g_swipeIndicator.alpha <= 0.01f) return;

    float alpha = g_swipeIndicator.alpha;
    const char *label = g_swipeIndicator.isNext ? ">> Next" : "<< Previous";

    // Panel dimensions - centered horizontally, positioned at top
    float width = 200.0f;
    float height = 56.0f;
    Rectangle panel = {
        (float)g_screenWidth * 0.5f - width * 0.5f,
        32.0f,
        width,
        height
    };

    // Use contextual colors if available
    Color accentColor = g_albumArtColors.hasColors ?
                        g_albumArtColors.accent : NpThemeGetColor(NP_COLOR_ACCENT);

    // Draw panel background with accent border
    Color panelColor = ColorAlpha(NpThemeGetColor(NP_COLOR_BG_DARK), 0.9f * alpha);
    Color borderColor = ColorAlpha(accentColor, alpha);
    Color textColor = ColorAlpha(accentColor, alpha);

    DrawRectangleRounded(panel, 0.4f, 16, panelColor);
    DrawRectangleRoundedLines(panel, 0.4f, 16, borderColor);

    // Center text in panel
    float textWidth = NpThemeMeasureTextWidth(NP_TYPO_BODY, label);
    float textX = panel.x + (panel.width - textWidth) * 0.5f;
    float textY = panel.y + (panel.height - NpThemeGetLineHeight(NP_TYPO_BODY)) * 0.5f;
    NpThemeDrawTextColored(NP_TYPO_BODY, label, (Vector2){textX, textY}, textColor);
}

static void PluginInit(int width, int height)
{
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;
    g_displayMode = NP_DISPLAY_NORMAL;
    g_volumeOverlayTimer = 0.0f;
    g_volumeOverlayAlpha = 0.0f;
    g_volumeOverlayValue = g_playback.volume;

    // Initialize plugin config with defaults
    LlzPluginConfigEntry defaults[] = {
        {"display_mode", "0"},
        {"theme_variant", "0"},
        {"bg_style", "0"},
        {"bg_style_enabled", "false"},
        {"has_custom_color", "false"},
        {"custom_color", "0,0,0"}
    };
    g_pluginConfigInitialized = LlzPluginConfigInit(&g_pluginConfig, "nowplaying", defaults, 6);

    // Initialize theme system
    NpThemeInit(width, height);

    // Initialize now playing screen
    Rectangle viewport = {0, 0, (float)width, (float)height};
    NpNowPlayingInit(&g_nowPlayingScreen, viewport);

    // Initialize overlay manager
    NpOverlayManagerInit(&g_overlayManager);

    // Initialize color picker
    NpColorPickerOverlayInit(&g_colorPicker);

    // Initialize media channels overlay
    NpMediaChannelsOverlayInit(&g_mediaChannelsOverlay);

    // Note: Don't call LlzBackgroundInit() - host manages the lifecycle

    // Load saved settings (after theme and screen init)
    LoadPluginSettings();

    // Apply loaded display mode to screen
    NpNowPlayingSetDisplayMode(&g_nowPlayingScreen, g_displayMode);
    NpNowPlayingSetPlayback(&g_nowPlayingScreen, &g_playback);

    MediaInitialize();

    printf("NowPlaying plugin initialized\n");
    printf("Controls:\n");
    printf("  Back         - Exit plugin\n");
    printf("  Screenshot   - Toggle clock overlay\n");
    printf("  Space/Select - Play/Pause\n");
    printf("  Up/Down      - Volume\n");
    printf("  Tap clock    - Cycle theme\n");
}

static const char *DisplayModeName(NpDisplayMode mode)
{
    switch (mode) {
        case NP_DISPLAY_NORMAL: return "Normal";
        case NP_DISPLAY_BAREBONES: return "Barebones";
        case NP_DISPLAY_ALBUM_ART: return "Album art";
        default: return "Unknown";
    }
}

static void CycleDisplayMode(void)
{
    g_displayMode = (g_displayMode + 1) % NP_DISPLAY_MODE_COUNT;
    NpNowPlayingSetDisplayMode(&g_nowPlayingScreen, g_displayMode);
    printf("NowPlaying display mode: %s\n", DisplayModeName(g_displayMode));
    SavePluginSettings();
}

static void TriggerVolumeOverlay(void)
{
    g_volumeOverlayTimer = VOLUME_OVERLAY_DURATION;
    g_volumeOverlayValue = g_playback.volume;
}

// Check if a color is considered "light" (for contrast decisions)
static bool IsColorLight(Color c)
{
    // Use relative luminance formula (perceived brightness)
    float luminance = (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) / 255.0f;
    return luminance > 0.5f;
}

// Get appropriate contrasting background for a fill color
static Color GetContrastingBarBackground(Color fillColor, float alpha)
{
    if (IsColorLight(fillColor)) {
        // Light fill needs dark background
        return ColorAlpha((Color){30, 30, 35, 255}, alpha);
    } else {
        // Dark fill needs light background
        return ColorAlpha((Color){200, 200, 210, 255}, alpha);
    }
}

static void DrawVolumeOverlay(void)
{
    if (g_volumeOverlayAlpha <= 0.01f) return;

    // Minimal volume bar at top of screen
    float barHeight = 6.0f;
    float margin = 24.0f;
    Rectangle bar = {margin, 16.0f, (float)g_screenWidth - margin * 2.0f, barHeight};

    // Use album art colors if available, otherwise fall back to theme
    Color fillColor;
    Color barBg;
    if (g_albumArtColors.hasColors) {
        fillColor = ColorAlpha(g_albumArtColors.accent, g_volumeOverlayAlpha);
        barBg = GetContrastingBarBackground(g_albumArtColors.accent, g_volumeOverlayAlpha * 0.5f);
    } else {
        fillColor = ColorAlpha(NpThemeGetColor(NP_COLOR_ACCENT), g_volumeOverlayAlpha);
        barBg = ColorAlpha(NpThemeGetColor(NP_COLOR_PANEL), g_volumeOverlayAlpha * 0.5f);
    }

    DrawRectangleRounded(bar, 0.5f, 8, barBg);

    Rectangle fill = bar;
    fill.width *= (float)g_volumeOverlayValue / 100.0f;
    if (fill.width > 0.0f) {
        DrawRectangleRounded(fill, 0.5f, 8, fillColor);
    }
}

// Load WebP image file and convert to raylib Image format
static Image LoadImageWebP(const char *path)
{
    Image image = {0};

    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("[ALBUMART] LoadImageWebP: failed to open file '%s'\n", path);
        return image;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file data
    uint8_t *fileData = (uint8_t *)malloc(fileSize);
    if (!fileData) {
        printf("[ALBUMART] LoadImageWebP: failed to allocate %ld bytes\n", fileSize);
        fclose(file);
        return image;
    }

    size_t bytesRead = fread(fileData, 1, fileSize, file);
    fclose(file);

    if (bytesRead != (size_t)fileSize) {
        printf("[ALBUMART] LoadImageWebP: read %zu bytes, expected %ld\n", bytesRead, fileSize);
        free(fileData);
        return image;
    }

    // Decode WebP
    int width = 0, height = 0;
    uint8_t *rgbaData = WebPDecodeRGBA(fileData, fileSize, &width, &height);
    free(fileData);

    if (!rgbaData) {
        printf("[ALBUMART] LoadImageWebP: WebPDecodeRGBA failed\n");
        return image;
    }

    // Create raylib Image from RGBA data
    // We need to copy the data because WebP uses its own allocator
    size_t dataSize = width * height * 4;
    void *imageCopy = RL_MALLOC(dataSize);
    if (!imageCopy) {
        printf("[ALBUMART] LoadImageWebP: failed to allocate image data\n");
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

    printf("[ALBUMART] LoadImageWebP: decoded %dx%d image\n", width, height);
    return image;
}

// Check if file path has WebP extension
static bool IsWebPFile(const char *path)
{
    if (!path) return false;
    size_t len = strlen(path);
    if (len < 5) return false;
    const char *ext = path + len - 5;
    return (strcmp(ext, ".webp") == 0 || strcmp(ext, ".WEBP") == 0);
}

// Cleanup previous album art textures after crossfade completes
static void CleanupPrevAlbumArt(void)
{
    if (g_albumArtTransition.prevTexture.id != 0) {
        UnloadTexture(g_albumArtTransition.prevTexture);
        memset(&g_albumArtTransition.prevTexture, 0, sizeof(Texture2D));
    }
    if (g_albumArtTransition.prevBlurred.id != 0) {
        UnloadTexture(g_albumArtTransition.prevBlurred);
        memset(&g_albumArtTransition.prevBlurred, 0, sizeof(Texture2D));
    }
}

// Update album art transition (call each frame)
static void UpdateAlbumArtTransition(float deltaTime)
{
    if (!g_albumArtTransition.inTransition) return;

    float fadeStep = deltaTime * ALBUM_ART_FADE_SPEED;

    if (g_albumArtTransition.fadingOut) {
        // Fading out with no replacement
        g_albumArtTransition.prevAlpha -= fadeStep;
        if (g_albumArtTransition.prevAlpha <= 0.0f) {
            g_albumArtTransition.prevAlpha = 0.0f;
            g_albumArtTransition.inTransition = false;
            g_albumArtTransition.fadingOut = false;
            CleanupPrevAlbumArt();
        }
    } else {
        // Crossfade: fade in new, fade out old
        g_albumArtTransition.currentAlpha += fadeStep;
        g_albumArtTransition.prevAlpha -= fadeStep;

        if (g_albumArtTransition.currentAlpha >= 1.0f) {
            g_albumArtTransition.currentAlpha = 1.0f;
        }
        if (g_albumArtTransition.prevAlpha <= 0.0f) {
            g_albumArtTransition.prevAlpha = 0.0f;
        }

        // Transition complete when new is fully visible and old is gone
        if (g_albumArtTransition.currentAlpha >= 1.0f && g_albumArtTransition.prevAlpha <= 0.0f) {
            g_albumArtTransition.inTransition = false;
            CleanupPrevAlbumArt();
        }
    }
}

// Convert RGB to HSV
static Vector3 RGBToHSV(Color c)
{
    float r = c.r / 255.0f;
    float g = c.g / 255.0f;
    float b = c.b / 255.0f;

    float max = fmaxf(r, fmaxf(g, b));
    float min = fminf(r, fminf(g, b));
    float delta = max - min;

    Vector3 hsv = {0, 0, max};

    if (delta > 0.0001f) {
        hsv.y = delta / max;
        if (r >= max) hsv.x = (g - b) / delta;
        else if (g >= max) hsv.x = 2.0f + (b - r) / delta;
        else hsv.x = 4.0f + (r - g) / delta;
        hsv.x *= 60.0f;
        if (hsv.x < 0.0f) hsv.x += 360.0f;
    }
    return hsv;
}

// Convert HSV to RGB
static Color HSVToRGB(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }

    return (Color){
        (unsigned char)((r + m) * 255),
        (unsigned char)((g + m) * 255),
        (unsigned char)((b + m) * 255),
        255
    };
}

// Extract dominant color from image using sampling
static void ExtractAlbumArtColors(Image img)
{
    if (img.data == NULL || img.width <= 0 || img.height <= 0) {
        g_albumArtColors.hasColors = false;
        return;
    }

    // Sample colors from the image
    int sampleCount = 0;
    float avgR = 0, avgG = 0, avgB = 0;
    float maxSat = 0;
    Color mostVibrant = {128, 128, 128, 255};

    int stepX = img.width / 8;
    int stepY = img.height / 8;
    if (stepX < 1) stepX = 1;
    if (stepY < 1) stepY = 1;

    for (int y = stepY / 2; y < img.height; y += stepY) {
        for (int x = stepX / 2; x < img.width; x += stepX) {
            Color pixel = GetImageColor(img, x, y);

            // Skip very dark or very light pixels
            float brightness = (pixel.r + pixel.g + pixel.b) / 3.0f;
            if (brightness < 30 || brightness > 240) continue;

            avgR += pixel.r;
            avgG += pixel.g;
            avgB += pixel.b;
            sampleCount++;

            // Track most vibrant/saturated color
            Vector3 hsv = RGBToHSV(pixel);
            if (hsv.y > maxSat && hsv.z > 0.2f) {
                maxSat = hsv.y;
                mostVibrant = pixel;
            }
        }
    }

    if (sampleCount == 0) {
        g_albumArtColors.hasColors = false;
        return;
    }

    // Calculate average color
    Color avgColor = {
        (unsigned char)(avgR / sampleCount),
        (unsigned char)(avgG / sampleCount),
        (unsigned char)(avgB / sampleCount),
        255
    };

    // Generate complementary color (opposite hue)
    Vector3 hsv = RGBToHSV(mostVibrant);
    float compHue = fmodf(hsv.x + 180.0f, 360.0f);
    Color complementary = HSVToRGB(compHue, fminf(hsv.y * 0.8f, 0.7f), fminf(hsv.z + 0.2f, 0.9f));

    // Create accent color (boosted saturation)
    Color accent = HSVToRGB(hsv.x, fminf(hsv.y + 0.3f, 1.0f), fminf(hsv.z + 0.1f, 1.0f));

    g_albumArtColors.primary = avgColor;
    g_albumArtColors.accent = accent;
    g_albumArtColors.complementary = complementary;
    g_albumArtColors.hasColors = true;

    // Update SDK background colors with album art colors
    UpdateBackgroundColors();

    printf("[ALBUMART] Extracted colors - Primary: (%d,%d,%d) Accent: (%d,%d,%d) Comp: (%d,%d,%d)\n",
           avgColor.r, avgColor.g, avgColor.b,
           accent.r, accent.g, accent.b,
           complementary.r, complementary.g, complementary.b);
}

static void UnloadAlbumArtTexture(void)
{
    // If we have textures, move them to prev for fade-out
    if (g_albumArtLoaded && g_albumArtTexture.id != 0) {
        // Cleanup any existing prev textures first
        CleanupPrevAlbumArt();

        // Move current to prev for crossfade
        g_albumArtTransition.prevTexture = g_albumArtTexture;
        g_albumArtTransition.prevBlurred = g_albumArtBlurred;
        g_albumArtTransition.prevAlpha = g_albumArtTransition.currentAlpha;
        g_albumArtTransition.currentAlpha = 0.0f;
        g_albumArtTransition.inTransition = true;
        g_albumArtTransition.fadingOut = true;

        // Clear current (but don't unload - they're now in prev)
        memset(&g_albumArtTexture, 0, sizeof(g_albumArtTexture));
        memset(&g_albumArtBlurred, 0, sizeof(g_albumArtBlurred));
    }
    g_albumArtLoaded = false;
    g_albumArtLoadedPath[0] = '\0';
}

static void LoadAlbumArtTexture(const char *path)
{
    if (!path || path[0] == '\0') {
        printf("[ALBUMART] LoadAlbumArtTexture: path is NULL or empty\n");
        return;
    }

    // Already loaded this path
    if (g_albumArtLoaded && strcmp(path, g_albumArtLoadedPath) == 0) {
        return;  // Skip logging for already loaded
    }

    printf("[ALBUMART] LoadAlbumArtTexture: attempting to load '%s'\n", path);

    // Check if file exists before doing anything
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("[ALBUMART] LoadAlbumArtTexture: FILE NOT FOUND '%s'\n", path);
        return;
    }
    printf("[ALBUMART] LoadAlbumArtTexture: file exists, size=%ld bytes\n", (long)st.st_size);

    Image img;
    if (IsWebPFile(path)) {
        // Use custom WebP loader (raylib doesn't support WebP natively)
        printf("[ALBUMART] LoadAlbumArtTexture: using WebP decoder\n");
        img = LoadImageWebP(path);
    } else {
        // Use raylib's built-in image loader for other formats
        img = LoadImage(path);
    }

    if (img.data == NULL) {
        printf("[ALBUMART] LoadAlbumArtTexture: LoadImage FAILED for '%s'\n", path);
        return;
    }
    printf("[ALBUMART] LoadAlbumArtTexture: image loaded %dx%d\n", img.width, img.height);

    // Extract colors from image before converting to texture
    ExtractAlbumArtColors(img);

    Texture2D newTexture = LoadTextureFromImage(img);
    UnloadImage(img);

    if (newTexture.id == 0) {
        printf("[ALBUMART] LoadAlbumArtTexture: LoadTextureFromImage FAILED\n");
        return;
    }

    // Move current textures to prev for crossfade (if we have any)
    CleanupPrevAlbumArt();  // Clean up any previous transition first
    if (g_albumArtLoaded && g_albumArtTexture.id != 0) {
        g_albumArtTransition.prevTexture = g_albumArtTexture;
        g_albumArtTransition.prevBlurred = g_albumArtBlurred;
        g_albumArtTransition.prevAlpha = g_albumArtTransition.currentAlpha;
    } else {
        g_albumArtTransition.prevAlpha = 0.0f;
    }

    // Set new textures
    g_albumArtTexture = newTexture;
    g_albumArtLoaded = true;
    strncpy(g_albumArtLoadedPath, path, sizeof(g_albumArtLoadedPath) - 1);
    g_albumArtLoadedPath[sizeof(g_albumArtLoadedPath) - 1] = '\0';
    printf("[ALBUMART] LoadAlbumArtTexture: SUCCESS texture_id=%u loaded='%s'\n",
           g_albumArtTexture.id, g_albumArtLoadedPath);

    // Generate blurred version for background effect
    // blurRadius=15 gives good blur, darkenAmount=0.4 darkens to make text readable
    g_albumArtBlurred = LlzTextureBlur(g_albumArtTexture, 15, 0.4f);
    if (g_albumArtBlurred.id != 0) {
        printf("[ALBUMART] LoadAlbumArtTexture: Blurred texture generated, id=%u\n", g_albumArtBlurred.id);
    } else {
        printf("[ALBUMART] LoadAlbumArtTexture: Warning - failed to generate blurred texture\n");
    }

    // Start fade-in transition for new album art
    g_albumArtTransition.currentAlpha = 0.0f;
    g_albumArtTransition.inTransition = true;
    g_albumArtTransition.fadingOut = false;
}

static void MediaApplyState(const LlzMediaState *state)
{
    if (!state) return;

    const char *title = (state->track[0] != '\0') ? state->track : "Unknown Track";
    strncpy(g_trackTitle, title, sizeof(g_trackTitle) - 1);
    g_trackTitle[sizeof(g_trackTitle) - 1] = '\0';

    const char *artist = (state->artist[0] != '\0') ? state->artist : "Unknown Artist";
    strncpy(g_trackArtist, artist, sizeof(g_trackArtist) - 1);
    g_trackArtist[sizeof(g_trackArtist) - 1] = '\0';

    const char *album = (state->album[0] != '\0') ? state->album : "Unknown Album";
    strncpy(g_trackAlbum, album, sizeof(g_trackAlbum) - 1);
    g_trackAlbum[sizeof(g_trackAlbum) - 1] = '\0';

    g_playback.trackTitle = g_trackTitle;
    g_playback.trackArtist = g_trackArtist;
    g_playback.trackAlbum = g_trackAlbum;

    // Get the controlled media channel (e.g., "Spotify", "YouTube Music")
    if (LlzMediaGetControlledChannel(g_mediaChannel, sizeof(g_mediaChannel))) {
        g_playback.mediaChannel = g_mediaChannel;
    } else {
        g_mediaChannel[0] = '\0';
        g_playback.mediaChannel = g_mediaChannel;
    }

    // Only update isPlaying from Redis if grace period has expired
    // This prevents "flicker" when toggling play/pause (local state is correct,
    // but Redis may briefly return old state while BLE propagates the change)
    if (g_playPauseGracePeriod <= 0.0f) {
        g_playback.isPlaying = state->isPlaying;
    }
    g_playback.trackDuration = (state->durationSeconds >= 0) ? (float)state->durationSeconds : g_playback.trackDuration;
    g_playback.trackPosition = (state->positionSeconds >= 0) ? (float)state->positionSeconds : g_playback.trackPosition;

    if (g_playback.trackDuration < 0.0f) g_playback.trackDuration = 0.0f;
    if (g_playback.trackPosition < 0.0f) g_playback.trackPosition = 0.0f;
    if (g_playback.trackDuration > 0.0f && g_playback.trackPosition > g_playback.trackDuration) {
        g_playback.trackPosition = g_playback.trackDuration;
    }

    if (state->volumePercent >= 0) {
        int volume = state->volumePercent;
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        g_playback.volume = volume;
    }

    // Load album art texture - try albumArtPath first, then generate from artist/album
    if (state->albumArtPath[0] != '\0') {
        LoadAlbumArtTexture(state->albumArtPath);
    } else if (state->artist[0] != '\0' || state->album[0] != '\0') {
        // Generate path from artist/album hash (same as album_art_viewer)
        const char *hash = LlzMediaGenerateArtHash(state->artist, state->album);
        if (hash && hash[0] != '\0') {
            static char generatedPath[512];
            snprintf(generatedPath, sizeof(generatedPath),
                     "/var/mediadash/album_art_cache/%s.webp", hash);
            printf("[ALBUMART] MediaApplyState: albumArtPath empty, trying generated path '%s'\n", generatedPath);
            LoadAlbumArtTexture(generatedPath);
        }
    }
}

static void MediaPoll(float deltaTime)
{
    if (!g_mediaInitialized) return;

    g_mediaRefreshTimer += deltaTime;
    if (g_mediaRefreshTimer < MEDIA_REFRESH_INTERVAL) return;
    g_mediaRefreshTimer = 0.0f;

    LlzMediaState latest;
    if (LlzMediaGetState(&latest)) {
        g_mediaState = latest;
        g_mediaStateValid = true;
        MediaApplyState(&g_mediaState);
    }
}

static void MediaInitialize(void)
{
    if (g_mediaInitialized) return;

    LlzMediaConfig cfg = {0};
    bool ok = LlzMediaInit(&cfg);
    g_mediaInitialized = true;
    g_mediaRefreshTimer = 0.0f;

    if (!ok) {
        printf("NowPlaying plugin: Redis media init failed (retry background)\n");
    }

    if (LlzMediaGetState(&g_mediaState)) {
        g_mediaStateValid = true;
        MediaApplyState(&g_mediaState);
    }
}

static void TogglePlayback(void)
{
    bool success = false;
    if (g_mediaInitialized) {
        success = LlzMediaSendCommand(LLZ_PLAYBACK_TOGGLE, 0);
    }
    if (!g_mediaInitialized || success) {
        g_playback.isPlaying = !g_playback.isPlaying;
        // Start grace period to prevent Redis from overwriting local state
        // while BLE propagates the change
        g_playPauseGracePeriod = PLAY_PAUSE_GRACE_DURATION;
    }
}

static void SkipTrack(bool next)
{
    bool success = false;
    if (g_mediaInitialized) {
        success = LlzMediaSendCommand(next ? LLZ_PLAYBACK_NEXT : LLZ_PLAYBACK_PREVIOUS, 0);
    }

    if (next) {
        g_playback.trackPosition = 0.0f;
        return;
    }

    if (!g_mediaInitialized || !success) {
        if (g_playback.trackPosition < 5.0f) g_playback.trackPosition = 0.0f;
        else g_playback.trackPosition -= 5.0f;
    } else {
        g_playback.trackPosition = 0.0f;
    }
}

static void HandleScrubState(const NpPlaybackActions *actions)
{
    if (!actions) return;

    if (actions->isScrubbing) {
        g_scrubActive = true;
        g_playback.trackPosition = actions->scrubPosition;
        if (g_playback.trackPosition < 0.0f) g_playback.trackPosition = 0.0f;
        if (g_playback.trackDuration > 0.0f && g_playback.trackPosition > g_playback.trackDuration) {
            g_playback.trackPosition = g_playback.trackDuration;
        }
    } else if (g_scrubActive) {
        g_scrubActive = false;
        int targetSeconds = (int)roundf(g_playback.trackPosition);
        if (targetSeconds < 0) targetSeconds = 0;
        if (g_mediaInitialized) {
            LlzMediaSeekSeconds(targetSeconds);
        }
        // Set cooldown to prevent accidental swipe-to-skip after seeking
        g_justSeeked = true;
        g_justSeekedTimer = JUST_SEEKED_COOLDOWN;
    }
}

static void PluginUpdate(const LlzInputState *hostInput, float deltaTime)
{
    LlzInputState emptyInput = {0};
    const LlzInputState *input = hostInput ? hostInput : &emptyInput;

    // Update seek cooldown timer
    if (g_justSeeked) {
        g_justSeekedTimer -= deltaTime;
        if (g_justSeekedTimer <= 0.0f) {
            g_justSeekedTimer = 0.0f;
            g_justSeeked = false;
        }
    }

    // Update play/pause grace period timer
    if (g_playPauseGracePeriod > 0.0f) {
        g_playPauseGracePeriod -= deltaTime;
        if (g_playPauseGracePeriod < 0.0f) {
            g_playPauseGracePeriod = 0.0f;
        }
    }

    MediaPoll(deltaTime);

    if (input->displayModeNext) {
        CycleDisplayMode();
    }

    // Cycle backgrounds only on quick click (button4Pressed), not on hold
    if (input->button4Pressed) {
        LlzBackgroundCycleNext();
        UpdateBackgroundColors();
        SavePluginSettings();
    }

    // Update SDK background animations and energy level
    LlzBackgroundUpdate(deltaTime);
    LlzBackgroundSetEnergy(g_playback.isPlaying ? 1.0f : 0.0f);
    UpdateSwipeIndicator(deltaTime);
    UpdateAlbumArtTransition(deltaTime);

    // Color picker - toggle on button4 hold event (button code 5 / mapped 6)
    if (input->button4Hold) {
        if (g_colorPicker.visible) {
            NpColorPickerOverlayHide(&g_colorPicker);
            printf("Color picker closed (button4 hold)\n");
        } else if (!NpColorPickerOverlayIsActive(&g_colorPicker)) {
            NpColorPickerOverlayShow(&g_colorPicker);
            printf("Color picker opened (button4 hold detected: %.2fs)\n", input->button4HoldTime);
        }
    }

    // When color picker is active (visible or animating), update it
    if (NpColorPickerOverlayIsActive(&g_colorPicker)) {
        bool wasVisible = g_colorPicker.visible;
        NpColorPickerOverlayUpdate(&g_colorPicker, input, deltaTime);

        // Check if overlay just closed (was visible, now not)
        if (wasVisible && !g_colorPicker.visible) {
            // Only apply color if user selected (not cancelled)
            if (NpColorPickerOverlayWasColorSelected(&g_colorPicker)) {
                const Color *selected = NpColorPickerOverlayGetSelectedColor(&g_colorPicker);
                if (selected) {
                    NpThemeSetCustomBackgroundColor(*selected);
                    printf("Custom background color applied: RGB(%d, %d, %d)\n",
                           selected->r, selected->g, selected->b);
                    SavePluginSettings();
                }
            } else {
                printf("Color picker cancelled\n");
            }
        }

        // Block all other input processing while color picker is active
        if (g_colorPicker.visible) {
            return;
        }
    }

    // Media channels overlay - toggle on back button long press
    if (input->backHold) {
        if (g_mediaChannelsOverlay.visible) {
            NpMediaChannelsOverlayHide(&g_mediaChannelsOverlay);
            printf("[MEDIA_CHANNELS] Overlay closed (back hold)\n");
        } else if (!NpMediaChannelsOverlayIsActive(&g_mediaChannelsOverlay)) {
            NpMediaChannelsOverlayShow(&g_mediaChannelsOverlay);
            printf("[MEDIA_CHANNELS] Overlay opened (back hold detected: %.2fs)\n", input->backHoldTime);
        }
    }

    // When media channels overlay is active (visible or animating), update it
    if (NpMediaChannelsOverlayIsActive(&g_mediaChannelsOverlay)) {
        bool wasVisible = g_mediaChannelsOverlay.visible;
        NpMediaChannelsOverlayUpdate(&g_mediaChannelsOverlay, input, deltaTime);

        // Check if overlay just closed (was visible, now not)
        if (wasVisible && !g_mediaChannelsOverlay.visible) {
            // Apply channel selection if user selected one
            if (NpMediaChannelsOverlayWasChannelSelected(&g_mediaChannelsOverlay)) {
                const char *selected = NpMediaChannelsOverlayGetSelectedChannel(&g_mediaChannelsOverlay);
                if (selected) {
                    printf("[MEDIA_CHANNELS] Selected channel: %s\n", selected);
                    LlzMediaSelectChannel(selected);
                }
            } else {
                printf("[MEDIA_CHANNELS] Overlay cancelled\n");
            }
            // Consume the input event that closed the overlay (prevent backClick from closing plugin)
            return;
        }

        // Block all other input processing while media channels overlay is active
        if (g_mediaChannelsOverlay.visible) {
            return;
        }
    }

    // Back button quick click - close overlay if visible, otherwise exit plugin
    if (input->backClick) {
        // If media channels overlay is visible, close it instead of exiting
        if (g_mediaChannelsOverlay.visible) {
            NpMediaChannelsOverlayHide(&g_mediaChannelsOverlay);
            printf("[MEDIA_CHANNELS] Overlay closed via back click\n");
            return;
        }
        // No overlay visible, exit plugin
        printf("[NOWPLAYING] Back click detected, closing plugin\n");
        g_wantsClose = true;
        return;
    }

    if (input->playPausePressed) {
        TogglePlayback();
    }

    if (g_volumeOverlayTimer > 0.0f) {
        g_volumeOverlayTimer -= deltaTime;
        if (g_volumeOverlayTimer < 0.0f) g_volumeOverlayTimer = 0.0f;
    }
    float targetAlpha = (g_volumeOverlayTimer > 0.0f) ? 1.0f : 0.0f;
    float fadeSpeed = (targetAlpha > g_volumeOverlayAlpha) ? 8.0f : 3.0f;
    g_volumeOverlayAlpha += (targetAlpha - g_volumeOverlayAlpha) * fminf(1.0f, deltaTime * fadeSpeed);
    if (g_volumeOverlayAlpha < 0.01f && targetAlpha == 0.0f) g_volumeOverlayAlpha = 0.0f;

    NpOverlayType currentOverlay = NpOverlayManagerGetCurrent(&g_overlayManager);
    bool overlayVisible = NpOverlayManagerIsVisible(&g_overlayManager);

    // Screenshot button / F1 - toggle clock overlay
    if (input->screenshotPressed) {
        if (currentOverlay == NP_OVERLAY_CLOCK) {
            NpOverlayManagerHide(&g_overlayManager);
        } else {
            NpOverlayManagerShow(&g_overlayManager, NP_OVERLAY_CLOCK);
        }
    }

    // Select long press - open Lyrics plugin and request lyrics when lyrics are enabled
    if (input->selectHold) {
        bool lyricsEnabled = LlzLyricsIsEnabled();
        printf("[LYRICS] Long-press select detected (lyrics enabled=%d)\n", lyricsEnabled);

        if (lyricsEnabled) {
            // Request lyrics for current track via SDK before switching plugins
            // This queues a request to the golang_ble_client to fetch lyrics from Android
            if (g_trackArtist[0] != '\0' && g_trackTitle[0] != '\0') {
                printf("[LYRICS] Requesting lyrics for: '%s' - '%s'\n", g_trackArtist, g_trackTitle);
                if (LlzLyricsRequest(g_trackArtist, g_trackTitle)) {
                    printf("[LYRICS] Lyrics request queued successfully\n");
                } else {
                    printf("[LYRICS] Failed to queue lyrics request\n");
                }
            } else {
                printf("[LYRICS] Cannot request lyrics - missing artist or track title\n");
            }

            // Open the Lyrics plugin
            printf("[LYRICS] Opening Lyrics plugin\n");
            LlzRequestOpenPlugin("Lyrics");
            g_wantsClose = true;
        } else {
            printf("[LYRICS] Long-press ignored - lyrics feature is disabled\n");
        }
    }

    // Down key cycles clock style when clock is visible
    if (currentOverlay == NP_OVERLAY_CLOCK && input->downPressed) {
        NpClockOverlay *clock = NpOverlayManagerGetClock(&g_overlayManager);
        NpClockOverlayCycleStyle(clock);
    }

    // Tap/click on clock overlay cycles theme
    if (currentOverlay == NP_OVERLAY_CLOCK && NpOverlayManagerGetAlpha(&g_overlayManager) >= 1.0f) {
        bool tapped = false;
#ifdef PLATFORM_DRM
        if (input->mouseJustReleased) {
            tapped = true;
        }
#else
        if (input->mouseJustPressed) {
            tapped = true;
        }
#endif
        if (tapped) {
            NpThemeVariant current = NpThemeGetVariant();
            NpThemeVariant next = (current + 1) % NP_THEME_VARIANT_COUNT;
            NpThemeSetVariant(next);
            printf("Theme: %s\n", NpThemeGetVariantName(next));
            SavePluginSettings();
        }
    }

    // Theme cycling with T key (desktop only)
    if (IsKeyPressed(KEY_T)) {
        NpThemeVariant current = NpThemeGetVariant();
        NpThemeVariant next = (current + 1) % NP_THEME_VARIANT_COUNT;
        NpThemeSetVariant(next);
        printf("Theme: %s\n", NpThemeGetVariantName(next));
        SavePluginSettings();
    }

    // Update overlay
    NpOverlayManagerUpdate(&g_overlayManager, deltaTime);

    // Update lyrics overlay with current playback position (for synced scrolling)
    int64_t positionMs = (int64_t)(g_playback.trackPosition * 1000.0f);
    NpOverlayManagerUpdateLyrics(&g_overlayManager, deltaTime, positionMs);

    // Only update now playing screen if overlay is not fully visible
    if (!overlayVisible || NpOverlayManagerGetAlpha(&g_overlayManager) < 1.0f) {
        NpNowPlayingUpdate(&g_nowPlayingScreen, input, deltaTime);

        // Handle playback actions
        NpPlaybackActions *actions = NpNowPlayingGetActions(&g_nowPlayingScreen);

        if (actions->playPausePressed) {
            TogglePlayback();
        }
        if (actions->shufflePressed) {
            g_playback.shuffleEnabled = !g_playback.shuffleEnabled;
        }
        if (actions->repeatPressed) {
            g_playback.repeatEnabled = !g_playback.repeatEnabled;
        }
        if (actions->previousPressed) {
            SkipTrack(false);
        }
        if (actions->nextPressed) {
            SkipTrack(true);
        }
        // Handle swipe gestures for track skipping
        // Only allow if not recently seeking (prevents accidental skip after scrubbing)
        if (!g_justSeeked) {
            if (actions->swipePreviousTriggered) {
                SkipTrack(false);  // Previous
                TriggerSwipeIndicator(false);
            }
            if (actions->swipeNextTriggered) {
                SkipTrack(true);   // Next
                TriggerSwipeIndicator(true);
            }
        }

        // Desktop keyboard controls
        if (IsKeyPressed(KEY_SPACE)) {
            TogglePlayback();
        }
        if (IsKeyPressed(KEY_S)) {
            g_playback.shuffleEnabled = !g_playback.shuffleEnabled;
        }
        if (IsKeyPressed(KEY_R)) {
            g_playback.repeatEnabled = !g_playback.repeatEnabled;
        }

        // Handle volume
        if (actions->volumeDelta != 0 || input->upPressed || input->downPressed) {
            int delta = actions->volumeDelta;
            if (input->upPressed) delta += 5;
            if (input->downPressed) delta -= 5;

            g_playback.volume += delta;
            if (g_playback.volume < 0) g_playback.volume = 0;
            if (g_playback.volume > 100) g_playback.volume = 100;
            TriggerVolumeOverlay();

            if (g_mediaInitialized) {
                LlzMediaSetVolume(g_playback.volume);
            }

            // Show volume on clock overlay if visible
            if (currentOverlay == NP_OVERLAY_CLOCK) {
                NpClockOverlay *clock = NpOverlayManagerGetClock(&g_overlayManager);
                NpClockOverlayShowVolume(clock, g_playback.volume);
            }
        }

        HandleScrubState(actions);

        // Advance track position if playing
        if (g_playback.isPlaying && !actions->isScrubbing) {
            g_playback.trackPosition += deltaTime;
            if (g_playback.trackPosition < 0.0f) g_playback.trackPosition = 0.0f;
            if (g_playback.trackDuration > 0.0f && g_playback.trackPosition > g_playback.trackDuration) {
                g_playback.trackPosition = g_playback.trackDuration;
            }
        }

        // Update screen with new playback state
        NpNowPlayingSetPlayback(&g_nowPlayingScreen, &g_playback);
    }
}

static void PluginDraw(void)
{
    Rectangle viewport = {0, 0, (float)g_screenWidth, (float)g_screenHeight};

    // Draw themed background or animated cycle
    DrawBackground();

    // Draw now playing screen
    LlzInputState drawInput = {0};
    const LlzInputState *currentInput = LlzInputGetState();
    if (currentInput) {
        drawInput = *currentInput;
    }
    // Log texture state periodically (once per second)
    static float logTimer = 0.0f;
    logTimer += GetFrameTime();
    if (logTimer >= 5.0f) {
        logTimer = 0.0f;
        printf("[ALBUMART] PluginDraw: g_albumArtLoaded=%d texture_id=%u path='%s'\n",
               g_albumArtLoaded, g_albumArtTexture.id, g_albumArtLoadedPath);
    }

    // Prepare album art transition state for drawing
    NpAlbumArtTransition artTransition = {
        .prevTexture = g_albumArtTransition.prevTexture.id != 0 ? &g_albumArtTransition.prevTexture : NULL,
        .prevBlurred = g_albumArtTransition.prevBlurred.id != 0 ? &g_albumArtTransition.prevBlurred : NULL,
        .currentAlpha = g_albumArtTransition.currentAlpha,
        .prevAlpha = g_albumArtTransition.prevAlpha
    };

    // Prepare UI colors from album art
    NpAlbumArtUIColors uiColors = {
        .accent = g_albumArtColors.accent,
        .complementary = g_albumArtColors.complementary,
        .trackBackground = g_albumArtColors.hasColors ?
            GetContrastingBarBackground(g_albumArtColors.accent, 1.0f) :
            NpThemeGetColor(NP_COLOR_PANEL_HOVER),
        .hasColors = g_albumArtColors.hasColors
    };

    NpNowPlayingDraw(&g_nowPlayingScreen, &drawInput, LlzBackgroundIsEnabled(),
                      g_albumArtLoaded ? &g_albumArtTexture : NULL,
                      g_albumArtBlurred.id != 0 ? &g_albumArtBlurred : NULL,
                      &artTransition, &uiColors);

    // Draw overlay on top (pass uiColors for contextual coloring)
    NpOverlayManagerDraw(&g_overlayManager, viewport, &uiColors);

    LlzBackgroundDrawIndicator();
    DrawSwipeIndicator();

    bool overlayAllowed = (g_displayMode == NP_DISPLAY_NORMAL ||
                           g_displayMode == NP_DISPLAY_ALBUM_ART ||
                           g_displayMode == NP_DISPLAY_BAREBONES);
    if (overlayAllowed && g_volumeOverlayAlpha > 0.01f) {
        DrawVolumeOverlay();
    }

    // Draw color picker overlay on top of everything
    NpColorPickerOverlayDraw(&g_colorPicker, &uiColors);

    // Draw media channels overlay on top of everything
    NpMediaChannelsOverlayDraw(&g_mediaChannelsOverlay, &uiColors);
}

static void PluginShutdown(void)
{
    // Save settings and cleanup config
    if (g_pluginConfigInitialized) {
        SavePluginSettings();
        LlzPluginConfigFree(&g_pluginConfig);
        g_pluginConfigInitialized = false;
    }

    // Unload album art texture
    UnloadAlbumArtTexture();

    if (g_mediaInitialized) {
        LlzMediaShutdown();
        g_mediaInitialized = false;
    }
    g_mediaStateValid = false;
    g_mediaRefreshTimer = 0.0f;
    g_scrubActive = false;
    g_justSeeked = false;
    g_justSeekedTimer = 0.0f;

    NpColorPickerOverlayShutdown(&g_colorPicker);
    NpMediaChannelsOverlayShutdown(&g_mediaChannelsOverlay);
    NpThemeShutdown();
    g_wantsClose = false;
    printf("NowPlaying plugin shutdown\n");
}

static bool PluginWantsClose(void)
{
    return g_wantsClose;
}

static LlzPluginAPI g_api = {
    .name = "Now Playing",
    .description = "Now playing screen with clock overlay and theming",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose
};

const LlzPluginAPI *LlzGetPlugin(void)
{
    return &g_api;
}
