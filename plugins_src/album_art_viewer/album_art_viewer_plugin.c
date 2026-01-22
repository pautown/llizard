/**
 * Album Art Viewer Plugin
 *
 * Displays album art from the MediaDash album art cache.
 * The cache is populated by the golang_ble_client daemon which stores
 * album art at /var/mediadash/album_art_cache/<hash>.jpg
 *
 * Features:
 * - Shows current playing track's album art
 * - Browse all cached album art with up/down navigation
 * - Display album art hash and file info
 * - Fit-to-screen with aspect ratio preservation
 * - Request album art from Android companion app (SELECT button, TAP, or F1/Screenshot)
 *
 * Album Art Request Flow:
 * 1. Plugin generates CRC32 hash from artist|album (matching Android's algorithm)
 * 2. Request is written to Redis key "mediadash:albumart:request"
 * 3. golang_ble_client daemon reads request and sends BLE command to Android
 * 4. Android companion fetches and transmits album art via BLE
 * 5. golang_ble_client caches the art and updates "media:album_art_path"
 */

#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "llz_sdk_background.h"
#include "llz_sdk_image.h"
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <webp/decode.h>

// Album art cache directory (matches golang_ble_client)
#define AAV_CACHE_DIR "/var/mediadash/album_art_cache"
#define AAV_MAX_CACHE_ENTRIES 256
#define AAV_HASH_LEN 16
#define AAV_PATH_LEN 256

// Colors
#define AAV_BG_COLOR       (Color){12, 12, 18, 255}
#define AAV_PANEL_COLOR    (Color){28, 30, 42, 255}
#define AAV_ACCENT_COLOR   (Color){120, 180, 255, 255}
#define AAV_TEXT_PRIMARY   (Color){240, 240, 245, 255}
#define AAV_TEXT_SECONDARY (Color){160, 165, 180, 255}
#define AAV_TEXT_MUTED     (Color){100, 105, 120, 255}
#define AAV_SUCCESS_COLOR  (Color){72, 199, 142, 255}
#define AAV_WARNING_COLOR  (Color){255, 184, 76, 255}

typedef struct {
    char hash[AAV_HASH_LEN];
    char path[AAV_PATH_LEN];
    long fileSize;
} AavCacheEntry;

typedef struct {
    // Display
    int screenWidth;
    int screenHeight;
    bool wantsClose;

    // Media SDK
    LlzMediaState media;
    bool mediaValid;
    bool mediaInitDone;
    float refreshTimer;

    // Album art texture
    Texture2D texture;
    bool textureLoaded;
    char loadedPath[AAV_PATH_LEN];

    // Carousel state
    Texture2D prevTexture;
    bool prevTextureLoaded;
    float carouselOffset;      // -1 to 1, animation progress
    float carouselTarget;      // Target offset (0 = done)
    int slideDirection;        // -1 = left, 1 = right, 0 = none

    // Cache browsing
    AavCacheEntry cache[AAV_MAX_CACHE_ENTRIES];
    int cacheCount;
    int currentIndex;
    bool browseMode;

    // UI state
    float fadeAlpha;
    char statusText[128];

    // Album art request state
    bool requestPending;
    float requestCooldown;
    char lastRequestedHash[AAV_HASH_LEN];
    float requestIndicatorTimer;

    // Blurred background textures
    Texture2D blurTexture;
    Texture2D prevBlurTexture;
    bool blurTextureLoaded;
    bool prevBlurTextureLoaded;
    float blurAlpha;
    float prevBlurAlpha;
} AavState;

static AavState g_state;

// Custom font for international character support
static Font g_customFont;
static bool g_fontLoaded = false;

// Build Unicode codepoints for international character support
static int *BuildUnicodeCodepoints(int *outCount) {
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

static void LoadCustomFont(void) {
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    // Use SDK font loading with custom codepoints for international character support
    g_customFont = LlzFontLoadCustom(LLZ_FONT_UI, 48, codepoints, codepointCount);
    if (g_customFont.texture.id != 0) {
        g_fontLoaded = true;
        SetTextureFilter(g_customFont.texture, TEXTURE_FILTER_BILINEAR);
        printf("[AAV] Loaded font via SDK with %d Unicode codepoints\n", codepointCount);
    } else {
        g_customFont = GetFontDefault();
        printf("[AAV] Using default font (SDK font load failed)\n");
    }

    if (codepoints) free(codepoints);
}

static void UnloadCustomFont(void) {
    Font defaultFont = GetFontDefault();
    if (g_fontLoaded && g_customFont.texture.id != 0 && g_customFont.texture.id != defaultFont.texture.id) {
        UnloadFont(g_customFont);
    }
    g_fontLoaded = false;
}

// Forward declarations
static void AavLoadCacheDirectory(void);
static void AavLoadTextureFromPath(const char *path);
static void AavLoadTextureWithTransition(const char *path, int direction);
static void AavUnloadPrevTexture(void);
static void AavUnloadTexture(void);
static void AavDrawImage(void);
static void AavDrawInfo(void);
static void AavDrawControls(void);
static void AavRequestCurrentArt(void);
static void AavDrawRequestIndicator(void);

// Load WebP image file and convert to raylib Image format
// (raylib doesn't support WebP natively, so we use libwebp)
static Image LoadImageWebP(const char *path)
{
    Image image = {0};

    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("[AAV] LoadImageWebP: failed to open file '%s'\n", path);
        return image;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file data
    uint8_t *fileData = (uint8_t *)malloc(fileSize);
    if (!fileData) {
        printf("[AAV] LoadImageWebP: failed to allocate %ld bytes\n", fileSize);
        fclose(file);
        return image;
    }

    size_t bytesRead = fread(fileData, 1, fileSize, file);
    fclose(file);

    if (bytesRead != (size_t)fileSize) {
        printf("[AAV] LoadImageWebP: read %zu bytes, expected %ld\n", bytesRead, fileSize);
        free(fileData);
        return image;
    }

    // Decode WebP
    int width, height;
    uint8_t *rgbaData = WebPDecodeRGBA(fileData, fileSize, &width, &height);
    free(fileData);

    if (!rgbaData) {
        printf("[AAV] LoadImageWebP: WebPDecodeRGBA failed\n");
        return image;
    }

    // Create raylib Image from RGBA data
    // We need to copy the data because WebP uses its own allocator
    size_t dataSize = width * height * 4;
    void *imageCopy = RL_MALLOC(dataSize);
    if (!imageCopy) {
        printf("[AAV] LoadImageWebP: failed to allocate image data\n");
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

    printf("[AAV] LoadImageWebP: decoded %dx%d image\n", width, height);
    return image;
}

// Check if a file path has a .webp extension
static bool IsWebPFile(const char *path)
{
    if (!path) return false;
    size_t len = strlen(path);
    if (len < 5) return false;
    const char *ext = path + len - 5;
    return (strcmp(ext, ".webp") == 0 || strcmp(ext, ".WEBP") == 0);
}

static void PluginInit(int width, int height)
{
    g_state.screenWidth = width;
    g_state.screenHeight = height;
    g_state.wantsClose = false;
    g_state.textureLoaded = false;
    g_state.loadedPath[0] = '\0';
    g_state.cacheCount = 0;
    g_state.currentIndex = 0;
    g_state.browseMode = false;
    g_state.fadeAlpha = 0.0f;
    g_state.statusText[0] = '\0';
    memset(&g_state.texture, 0, sizeof(g_state.texture));
    memset(&g_state.prevTexture, 0, sizeof(g_state.prevTexture));
    g_state.prevTextureLoaded = false;
    g_state.carouselOffset = 0.0f;
    g_state.carouselTarget = 0.0f;
    g_state.slideDirection = 0;
    g_state.requestPending = false;
    g_state.requestCooldown = 0.0f;
    g_state.lastRequestedHash[0] = '\0';
    g_state.requestIndicatorTimer = 0.0f;

    // Initialize blur texture state
    memset(&g_state.blurTexture, 0, sizeof(g_state.blurTexture));
    memset(&g_state.prevBlurTexture, 0, sizeof(g_state.prevBlurTexture));
    g_state.blurTextureLoaded = false;
    g_state.prevBlurTextureLoaded = false;
    g_state.blurAlpha = 0.0f;
    g_state.prevBlurAlpha = 0.0f;

    // Load custom font
    LoadCustomFont();

    // Configure background system for blur style (host manages init/shutdown)
    LlzBackgroundSetStyle(LLZ_BG_STYLE_BLUR, false);
    LlzBackgroundSetEnabled(true);

    // Initialize media SDK
    LlzMediaConfig cfg = {0};
    if (LlzMediaInit(&cfg)) {
        g_state.mediaInitDone = true;
        g_state.mediaValid = LlzMediaGetState(&g_state.media);
    } else {
        snprintf(g_state.statusText, sizeof(g_state.statusText), "Redis connection failed");
    }

    // Load cache directory listing
    AavLoadCacheDirectory();

    // Try to load current album art
    if (g_state.mediaValid && g_state.media.albumArtPath[0]) {
        AavLoadTextureFromPath(g_state.media.albumArtPath);
    } else if (g_state.cacheCount > 0) {
        // Load first cached image
        AavLoadTextureFromPath(g_state.cache[0].path);
        g_state.browseMode = true;
    }
}

static void PluginShutdown(void)
{
    AavUnloadTexture();

    // Cleanup blur textures
    if (g_state.blurTextureLoaded && g_state.blurTexture.id != 0) {
        UnloadTexture(g_state.blurTexture);
    }
    if (g_state.prevBlurTextureLoaded && g_state.prevBlurTexture.id != 0) {
        UnloadTexture(g_state.prevBlurTexture);
    }

    // Note: Don't call LlzBackgroundShutdown() - host manages the lifecycle
    UnloadCustomFont();
    if (g_state.mediaInitDone) {
        LlzMediaShutdown();
    }
    memset(&g_state, 0, sizeof(g_state));
}

static bool PluginWantsClose(void)
{
    return g_state.wantsClose;
}

static void AavLoadCacheDirectory(void)
{
    g_state.cacheCount = 0;

    DIR *dir = opendir(AAV_CACHE_DIR);
    if (!dir) {
        snprintf(g_state.statusText, sizeof(g_state.statusText), "Cache dir not found: %s", AAV_CACHE_DIR);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && g_state.cacheCount < AAV_MAX_CACHE_ENTRIES) {
        // Skip hidden files and directories
        if (entry->d_name[0] == '.') continue;

        // Check for valid image extensions (webp is primary format from golang_ble_client)
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext) continue;
        if (strcmp(ext, ".webp") != 0 && strcmp(ext, ".WEBP") != 0 &&
            strcmp(ext, ".jpg") != 0 && strcmp(ext, ".jpeg") != 0 && strcmp(ext, ".png") != 0) continue;

        // Extract hash (filename without extension)
        AavCacheEntry *e = &g_state.cache[g_state.cacheCount];
        size_t hashLen = ext - entry->d_name;
        if (hashLen >= AAV_HASH_LEN) hashLen = AAV_HASH_LEN - 1;
        strncpy(e->hash, entry->d_name, hashLen);
        e->hash[hashLen] = '\0';

        // Validate hash format (8-10 decimal digits as per Android CRC32)
        bool validHash = (hashLen >= 8 && hashLen <= 10);
        for (size_t i = 0; i < hashLen && validHash; i++) {
            if (e->hash[i] < '0' || e->hash[i] > '9') {
                validHash = false;
            }
        }
        if (!validHash) continue;

        // Build full path
        snprintf(e->path, sizeof(e->path), "%s/%s", AAV_CACHE_DIR, entry->d_name);

        // Get file size
        struct stat st;
        if (stat(e->path, &st) == 0) {
            e->fileSize = st.st_size;
        } else {
            e->fileSize = 0;
        }

        g_state.cacheCount++;
    }

    closedir(dir);
}

// Load texture with optional carousel transition
static void AavLoadTextureWithTransition(const char *path, int direction)
{
    if (!path || path[0] == '\0') return;
    if (strcmp(path, g_state.loadedPath) == 0 && g_state.textureLoaded) return;

    // Check if file exists
    struct stat st;
    if (stat(path, &st) != 0) {
        return;
    }

    // Use WebP decoder for .webp files, raylib's LoadImage for others
    Image img;
    if (IsWebPFile(path)) {
        img = LoadImageWebP(path);
    } else {
        img = LoadImage(path);
    }

    if (img.data == NULL) {
        return;
    }

    Texture2D newTexture = LoadTextureFromImage(img);

    // Create blurred background texture (blur and darken)
    Image blurredImg = LlzImageBlur(img, 12, 0.4f);
    Texture2D newBlurTexture = LoadTextureFromImage(blurredImg);
    UnloadImage(blurredImg);
    UnloadImage(img);

    if (newTexture.id == 0) {
        return;
    }

    // Setup carousel transition if we have an existing texture
    if (g_state.textureLoaded && g_state.texture.id != 0 && direction != 0) {
        // Unload any previous transition texture
        AavUnloadPrevTexture();

        // Move current to prev for transition
        g_state.prevTexture = g_state.texture;
        g_state.prevTextureLoaded = true;

        // Move blur textures for transition
        if (g_state.prevBlurTextureLoaded && g_state.prevBlurTexture.id != 0) {
            UnloadTexture(g_state.prevBlurTexture);
        }
        g_state.prevBlurTexture = g_state.blurTexture;
        g_state.prevBlurTextureLoaded = g_state.blurTextureLoaded;
        g_state.prevBlurAlpha = g_state.blurAlpha;

        // Start carousel animation
        g_state.slideDirection = direction;
        g_state.carouselOffset = (float)direction;  // Start offscreen
        g_state.carouselTarget = 0.0f;              // Animate to center
    } else {
        // No transition, just unload old
        AavUnloadTexture();
        if (g_state.blurTextureLoaded && g_state.blurTexture.id != 0) {
            UnloadTexture(g_state.blurTexture);
        }
        g_state.prevBlurAlpha = 0.0f;
    }

    g_state.texture = newTexture;
    g_state.textureLoaded = true;
    g_state.blurTexture = newBlurTexture;
    g_state.blurTextureLoaded = (newBlurTexture.id != 0);
    g_state.blurAlpha = 1.0f;
    strncpy(g_state.loadedPath, path, sizeof(g_state.loadedPath) - 1);
    g_state.loadedPath[sizeof(g_state.loadedPath) - 1] = '\0';
}

static void AavLoadTextureFromPath(const char *path)
{
    AavLoadTextureWithTransition(path, 0);  // No transition
}

static void AavUnloadPrevTexture(void)
{
    if (g_state.prevTextureLoaded && g_state.prevTexture.id != 0) {
        UnloadTexture(g_state.prevTexture);
    }
    g_state.prevTextureLoaded = false;
    memset(&g_state.prevTexture, 0, sizeof(g_state.prevTexture));
}

static void AavUnloadTexture(void)
{
    AavUnloadPrevTexture();
    if (g_state.textureLoaded && g_state.texture.id != 0) {
        UnloadTexture(g_state.texture);
    }
    g_state.textureLoaded = false;
    g_state.loadedPath[0] = '\0';
    memset(&g_state.texture, 0, sizeof(g_state.texture));
}

static void PluginUpdate(const LlzInputState *input, float deltaTime)
{
    // Update background system
    LlzBackgroundUpdate(deltaTime);

    // Update blur textures for SDK background
    float currentBlurAlpha = g_state.blurTextureLoaded ? (1.0f - fabsf(g_state.carouselOffset) * 0.5f) : 0.0f;
    float prevBlurAlpha = g_state.prevBlurTextureLoaded ? fabsf(g_state.carouselOffset) * 0.5f : 0.0f;
    LlzBackgroundSetBlurTexture(g_state.blurTexture, g_state.prevBlurTexture, currentBlurAlpha, prevBlurAlpha);

    // Fade in effect
    if (g_state.fadeAlpha < 1.0f) {
        g_state.fadeAlpha += deltaTime * 4.0f;
        if (g_state.fadeAlpha > 1.0f) g_state.fadeAlpha = 1.0f;
    }

    // Animate carousel - faster and smoother
    if (g_state.carouselOffset != g_state.carouselTarget) {
        float diff = g_state.carouselTarget - g_state.carouselOffset;
        float speed = 18.0f;  // Much faster scrolling
        g_state.carouselOffset += diff * speed * deltaTime;

        // Snap when close
        if (fabsf(diff) < 0.01f) {
            g_state.carouselOffset = g_state.carouselTarget;
            // Animation complete, cleanup prev texture
            if (g_state.carouselTarget == 0.0f) {
                AavUnloadPrevTexture();
                g_state.slideDirection = 0;
                // Also cleanup prev blur texture
                if (g_state.prevBlurTextureLoaded && g_state.prevBlurTexture.id != 0) {
                    UnloadTexture(g_state.prevBlurTexture);
                    g_state.prevBlurTextureLoaded = false;
                    memset(&g_state.prevBlurTexture, 0, sizeof(g_state.prevBlurTexture));
                }
            }
        }
    }

    // Update request cooldown timer
    if (g_state.requestCooldown > 0.0f) {
        g_state.requestCooldown -= deltaTime;
        if (g_state.requestCooldown < 0.0f) g_state.requestCooldown = 0.0f;
    }

    // Update request indicator timer
    if (g_state.requestIndicatorTimer > 0.0f) {
        g_state.requestIndicatorTimer -= deltaTime;
        if (g_state.requestIndicatorTimer < 0.0f) g_state.requestIndicatorTimer = 0.0f;
    }

    // Handle input
    if (input) {
        // Back button exits (on release)
        if (input->backReleased) {
            g_state.wantsClose = true;
            return;
        }

        // Select button (code=28): Request album art from Android companion
        if (input->selectPressed) {
            AavRequestCurrentArt();
        }

        // Screenshot/F1 button: Also request album art
        if (input->screenshotPressed) {
            AavRequestCurrentArt();
        }

        // Tap (when not browsing): Also request art
        if (input->tap && !g_state.browseMode) {
            AavRequestCurrentArt();
        }

        // Navigation in browse mode (only if not currently animating)
        // Use up/down for prev/next, swipe gestures, or scroll wheel
        bool canNavigate = (g_state.cacheCount > 0) &&
                           (fabsf(g_state.carouselOffset - g_state.carouselTarget) < 0.1f);

        if (canNavigate) {
            int newIndex = g_state.currentIndex;
            int direction = 0;  // -1 = slide from left, 1 = slide from right

            // Up button or swipe right = previous image (slides from left)
            if (input->upPressed || input->swipeRight) {
                newIndex = (g_state.currentIndex - 1 + g_state.cacheCount) % g_state.cacheCount;
                direction = -1;
            }
            // Down button or swipe left = next image (slides from right)
            else if (input->downPressed || input->swipeLeft) {
                newIndex = (g_state.currentIndex + 1) % g_state.cacheCount;
                direction = 1;
            }
            // Scroll wheel/rotary encoder navigation
            else if (input->scrollDelta != 0) {
                if (input->scrollDelta > 0) {
                    newIndex = (g_state.currentIndex + 1) % g_state.cacheCount;
                    direction = 1;
                } else {
                    newIndex = (g_state.currentIndex - 1 + g_state.cacheCount) % g_state.cacheCount;
                    direction = -1;
                }
            }

            if (direction != 0 && newIndex != g_state.currentIndex) {
                g_state.currentIndex = newIndex;
                g_state.browseMode = true;
                AavLoadTextureWithTransition(g_state.cache[g_state.currentIndex].path, direction);
                snprintf(g_state.statusText, sizeof(g_state.statusText),
                    "Image %d/%d", g_state.currentIndex + 1, g_state.cacheCount);
            }
        }

    }

    // Periodic refresh of media state
    g_state.refreshTimer += deltaTime;
    if (g_state.refreshTimer >= 2.0f) {
        g_state.refreshTimer = 0.0f;
        if (g_state.mediaInitDone) {
            g_state.mediaValid = LlzMediaGetState(&g_state.media);

            // Auto-update if not in browse mode and track changed
            if (!g_state.browseMode && g_state.mediaValid && g_state.media.albumArtPath[0]) {
                if (strcmp(g_state.media.albumArtPath, g_state.loadedPath) != 0) {
                    AavLoadTextureFromPath(g_state.media.albumArtPath);
                    // Clear pending request if art arrived
                    if (g_state.requestPending) {
                        g_state.requestPending = false;
                        snprintf(g_state.statusText, sizeof(g_state.statusText), "Album art received!");
                    }
                }
            }

            // Reload cache directory to pick up new art
            AavLoadCacheDirectory();
        }
    }
}

// Helper to draw a texture centered with fixed size
static void AavDrawTextureFixedSize(Texture2D tex, float offsetX, float alpha, float centerX, float centerY, float targetSize)
{
    if (tex.id == 0) return;

    float texW = (float)tex.width;
    float texH = (float)tex.height;

    // Scale to fit within targetSize x targetSize
    float scale = targetSize / ((texW > texH) ? texW : texH);
    float drawW = texW * scale;
    float drawH = texH * scale;
    float drawX = centerX - drawW / 2.0f + offsetX;
    float drawY = centerY - drawH / 2.0f;

    Rectangle destRect = {drawX, drawY, drawW, drawH};
    Rectangle srcRect = {0, 0, texW, texH};

    Color tint = ColorAlpha(WHITE, alpha);
    DrawTexturePro(tex, srcRect, destRect, (Vector2){0, 0}, 0.0f, tint);

    // Subtle rounded shadow/border effect
    DrawRectangleRoundedLinesEx(destRect, 0.05f, 8, 2.0f, ColorAlpha(WHITE, 0.15f * alpha));
}

static void AavDrawImage(void)
{
    // Center of screen for album art
    float centerX = (float)g_state.screenWidth / 2.0f;
    float centerY = (float)g_state.screenHeight / 2.0f - 20.0f;
    float artSize = 200.0f;  // Fixed 200x200

    // Carousel slide width
    float slideWidth = g_state.screenWidth * 0.6f;

    // Draw previous texture (sliding out)
    if (g_state.prevTextureLoaded && g_state.prevTexture.id != 0) {
        float prevOffset = -g_state.slideDirection * slideWidth * (1.0f - fabsf(g_state.carouselOffset));
        float prevAlpha = g_state.fadeAlpha * fabsf(g_state.carouselOffset);
        AavDrawTextureFixedSize(g_state.prevTexture, prevOffset, prevAlpha, centerX, centerY, artSize);
    }

    // Draw current texture (sliding in)
    if (g_state.textureLoaded && g_state.texture.id != 0) {
        float currentOffset = g_state.carouselOffset * slideWidth;
        float currentAlpha = g_state.fadeAlpha * (1.0f - fabsf(g_state.carouselOffset) * 0.3f);
        AavDrawTextureFixedSize(g_state.texture, currentOffset, currentAlpha, centerX, centerY, artSize);
    } else if (!g_state.prevTextureLoaded) {
        // Draw placeholder
        const char *msg = "No Album Art";
        Vector2 textSize = MeasureTextEx(g_customFont, msg, 24, 1.5f);
        DrawTextEx(g_customFont, msg,
            (Vector2){centerX - textSize.x / 2.0f, centerY - 12.0f},
            24, 1.5f, ColorAlpha(AAV_TEXT_MUTED, g_state.fadeAlpha));
    }

    // Simple dot indicators at bottom center
    if (g_state.cacheCount > 1) {
        int dotSpacing = 10;
        int maxDots = 8;
        int numDots = g_state.cacheCount < maxDots ? g_state.cacheCount : maxDots;
        float startX = centerX - (float)(numDots * dotSpacing) / 2.0f;
        float dotY = (float)g_state.screenHeight - 40.0f;

        for (int i = 0; i < numDots; i++) {
            int idx = i;
            if (g_state.cacheCount > maxDots) {
                int startIdx = g_state.currentIndex - numDots / 2;
                if (startIdx < 0) startIdx = 0;
                if (startIdx + numDots > g_state.cacheCount) startIdx = g_state.cacheCount - numDots;
                idx = startIdx + i;
            }

            Color dotColor = (idx == g_state.currentIndex) ? WHITE : ColorAlpha(WHITE, 0.3f);
            float radius = (idx == g_state.currentIndex) ? 4.0f : 3.0f;
            DrawCircle((int)(startX + i * dotSpacing + 4), (int)dotY, radius, dotColor);
        }
    }
}

static void AavDrawInfo(void)
{
    // Simple centered index counter below album art
    if (g_state.cacheCount > 0) {
        char indexText[32];
        snprintf(indexText, sizeof(indexText), "%d / %d", g_state.currentIndex + 1, g_state.cacheCount);
        Vector2 textSize = MeasureTextEx(g_customFont, indexText, 20, 1.3f);
        float centerX = (float)g_state.screenWidth / 2.0f;
        float y = (float)g_state.screenHeight / 2.0f + 130.0f;
        DrawTextEx(g_customFont, indexText,
            (Vector2){centerX - textSize.x / 2.0f, y},
            20, 1.3f, ColorAlpha(WHITE, 0.7f * g_state.fadeAlpha));
    }
}

static void AavRequestCurrentArt(void)
{
    // Check cooldown
    if (g_state.requestCooldown > 0.0f) {
        snprintf(g_state.statusText, sizeof(g_state.statusText),
            "Please wait %.0fs...", g_state.requestCooldown);
        return;
    }

    // Need valid media state with artist/album info
    if (!g_state.mediaValid) {
        snprintf(g_state.statusText, sizeof(g_state.statusText), "No media state available");
        return;
    }

    if (g_state.media.artist[0] == '\0' && g_state.media.album[0] == '\0') {
        snprintf(g_state.statusText, sizeof(g_state.statusText), "No artist/album info");
        return;
    }

    // Generate hash from current track
    const char *hash = LlzMediaGenerateArtHash(g_state.media.artist, g_state.media.album);
    if (!hash || hash[0] == '\0') {
        snprintf(g_state.statusText, sizeof(g_state.statusText), "Failed to generate hash");
        return;
    }

    // Send request to Android companion via Redis
    if (LlzMediaRequestAlbumArt(hash)) {
        strncpy(g_state.lastRequestedHash, hash, sizeof(g_state.lastRequestedHash) - 1);
        g_state.lastRequestedHash[sizeof(g_state.lastRequestedHash) - 1] = '\0';
        g_state.requestPending = true;
        g_state.requestCooldown = 5.0f;  // 5 second cooldown between requests
        g_state.requestIndicatorTimer = 2.0f;  // Show indicator for 2 seconds
        snprintf(g_state.statusText, sizeof(g_state.statusText),
            "Requesting art (hash: %s)...", hash);
    } else {
        snprintf(g_state.statusText, sizeof(g_state.statusText), "Failed to send request");
    }
}

static void AavDrawRequestIndicator(void)
{
    if (g_state.requestIndicatorTimer <= 0.0f) return;

    float alpha = g_state.requestIndicatorTimer / 2.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    // Draw pulsing indicator at top-center
    float width = 280.0f;
    float height = 50.0f;
    Rectangle panel = {
        (float)g_state.screenWidth * 0.5f - width * 0.5f,
        60.0f,
        width,
        height
    };

    float pulse = 0.7f + 0.3f * sinf(g_state.requestIndicatorTimer * 8.0f);
    Color panelColor = ColorAlpha(AAV_PANEL_COLOR, 0.95f * alpha);
    Color borderColor = ColorAlpha(AAV_ACCENT_COLOR, alpha * pulse);

    DrawRectangleRounded(panel, 0.3f, 12, panelColor);
    DrawRectangleRoundedLinesEx(panel, 0.3f, 12, 2.0f, borderColor);

    const char *msg = "Requesting from Android...";
    Vector2 textSize = MeasureTextEx(g_customFont, msg, 18, 1.2f);
    float textX = panel.x + (panel.width - textSize.x) / 2.0f;
    float textY = panel.y + (panel.height - 18) / 2.0f;
    DrawTextEx(g_customFont, msg, (Vector2){textX, textY}, 18, 1.2f, ColorAlpha(AAV_ACCENT_COLOR, alpha));
}

static void AavDrawControls(void)
{
    // Minimal title at top
    const char *title = "Album Art";
    Vector2 titleSize = MeasureTextEx(g_customFont, title, 20, 1.3f);
    float centerX = (float)g_state.screenWidth / 2.0f;
    DrawTextEx(g_customFont, title,
        (Vector2){centerX - titleSize.x / 2.0f, 20.0f},
        20, 1.3f, ColorAlpha(WHITE, 0.6f * g_state.fadeAlpha));
}

static void PluginDraw(void)
{
    ClearBackground(BLACK);

    // Draw blurred album art background
    LlzBackgroundDraw();

    // Dark overlay for readability
    DrawRectangle(0, 0, g_state.screenWidth, g_state.screenHeight, ColorAlpha(BLACK, 0.3f));

    AavDrawControls();
    AavDrawImage();
    AavDrawInfo();
    AavDrawRequestIndicator();
}

static LlzPluginAPI g_plugin = {
    .name = "Album Art Viewer",
    .description = "Browse cached album art",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .category = LLZ_CATEGORY_MEDIA
};

const LlzPluginAPI *LlzGetPlugin(void)
{
    return &g_plugin;
}
