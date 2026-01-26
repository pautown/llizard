/**
 * Albums Plugin for llizardgui-host
 *
 * Displays saved albums from Spotify library in a smooth carousel view.
 * Albums are shown as larger cards that can be scrolled horizontally.
 * Album art is loaded from either the preview cache or full art cache.
 *
 * Navigation:
 * - Scroll/Swipe: Navigate through albums
 * - Select: Play selected album
 * - Back: Return to menu
 */

#include "llz_sdk.h"
#include "llz_sdk_image.h"
#include "llz_sdk_navigation.h"
#include "llizard_plugin.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <webp/decode.h>

// ============================================================================
// Display Constants
// ============================================================================

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
#define PADDING 20

// Album card dimensions - LARGER for better visibility
#define ALBUM_SIZE 200
#define ALBUM_SPACING 30
#define SELECTED_SCALE 1.1f  // Selected album is 10% bigger

// Layout - centered vertically
#define CAROUSEL_Y 90
#define CAROUSEL_HEIGHT (ALBUM_SIZE + 100)  // Room for text below

// Text sizes - MUCH LARGER for readability
#define TITLE_FONT_SIZE 42
#define ALBUM_NAME_FONT_SIZE 24
#define ARTIST_FONT_SIZE 20
#define INFO_FONT_SIZE 17
#define HINT_FONT_SIZE 18

// Album art cache paths - separate preview and full art folders
#define MAX_ALBUM_ART_CACHE 50
#define ALBUM_ART_PREVIEW_DIR "/var/mediadash/album_art_previews"
#define ALBUM_ART_CACHE_DIR "/var/mediadash/album_art_cache"

// Smooth scrolling physics - optimized for buttery smooth feel
#define SCROLL_LERP_SPEED 8.0f          // Lower = smoother deceleration
#define SCROLL_SNAP_THRESHOLD 0.001f    // Very small for smooth settling
#define SCROLL_VELOCITY_DECAY 0.88f     // Slower decay = more momentum
#define SCROLL_IMPULSE 0.4f             // Smaller impulse for finer control
#define SCROLL_SPRING_STIFFNESS 6.0f    // Spring constant for smooth snapping
#define SCROLL_DAMPING 0.85f            // Damping for spring oscillation

// ============================================================================
// Spotify Color Palette
// ============================================================================

static const Color SPOTIFY_GREEN = {30, 215, 96, 255};
static const Color SPOTIFY_GREEN_DARK = {20, 145, 65, 255};
static const Color SPOTIFY_BLACK = {18, 18, 18, 255};
static const Color SPOTIFY_DARK = {24, 24, 24, 255};
static const Color SPOTIFY_GRAY = {40, 40, 40, 255};
static const Color SPOTIFY_LIGHT_GRAY = {120, 120, 120, 255};
static const Color SPOTIFY_WHITE = {255, 255, 255, 255};
static const Color SPOTIFY_SUBTLE = {179, 179, 179, 255};

// ============================================================================
// Album Art Cache Entry
// ============================================================================

typedef struct {
    char hash[64];           // Art hash (artist|album CRC32)
    Texture2D texture;       // Loaded texture
    bool loaded;             // True if texture is valid
    bool requested;          // True if art has been requested via BLE
    float requestTime;       // Time when art was requested (for retry logic)
} AlbumArtCacheEntry;

// ============================================================================
// Plugin State
// ============================================================================

static bool g_wantsClose = false;
static float g_animTimer = 0.0f;

// Albums data
static LlzSpotifyAlbumListResponse g_albums = {0};
static bool g_albumsValid = false;
static bool g_albumsLoading = false;
static float g_pollTimer = 0.0f;

// Album art cache
static AlbumArtCacheEntry g_artCache[MAX_ALBUM_ART_CACHE];
static int g_artCacheCount = 0;

// Carousel state - smooth scrolling
static int g_selectedIndex = 0;
static float g_visualOffset = 0.0f;      // Current visual position (smooth)
static float g_targetOffset = 0.0f;      // Target position (discrete)
static float g_scrollVelocity = 0.0f;    // Current scroll velocity

// Art check timer
static float g_artCheckTimer = 0.0f;

// ============================================================================
// Forward Declarations
// ============================================================================

static void DrawHeader(void);
static void DrawCarousel(void);
static void DrawAlbumCard(int index, float centerX, float y, float scale, float alpha);
static void DrawFooter(void);
static void RefreshAlbums(void);
static void UpdateCarousel(const LlzInputState *input, float dt);
static void InitAlbumArtCache(void);
static void CleanupAlbumArtCache(void);
static AlbumArtCacheEntry* GetOrCreateArtCacheEntry(const char *artist, const char *album);
static void CheckAndLoadAlbumArt(int albumIndex);
static void UpdateAlbumArtLoading(float dt);

// ============================================================================
// Utility Functions
// ============================================================================

static float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float Clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static void DrawTruncatedText(const char *text, float x, float y, float maxWidth, int fontSize, Color color) {
    if (!text || text[0] == '\0') return;

    int textWidth = LlzMeasureText(text, fontSize);
    if (textWidth <= (int)maxWidth) {
        LlzDrawText(text, (int)x, (int)y, fontSize, color);
        return;
    }

    char truncated[128];
    int len = strlen(text);
    if (len > 120) len = 120;

    for (int i = len; i > 0; i--) {
        strncpy(truncated, text, i);
        truncated[i] = '\0';
        strcat(truncated, "..");
        if (LlzMeasureText(truncated, fontSize) <= (int)maxWidth) {
            LlzDrawText(truncated, (int)x, (int)y, fontSize, color);
            return;
        }
    }
    LlzDrawText("..", (int)x, (int)y, fontSize, color);
}

static void DrawCenteredTruncatedText(const char *text, float centerX, float y, float maxWidth, int fontSize, Color color) {
    if (!text || text[0] == '\0') return;

    char truncated[128];
    int textWidth = LlzMeasureText(text, fontSize);

    if (textWidth <= (int)maxWidth) {
        LlzDrawText(text, (int)(centerX - textWidth / 2), (int)y, fontSize, color);
        return;
    }

    int len = strlen(text);
    if (len > 120) len = 120;

    for (int i = len; i > 0; i--) {
        strncpy(truncated, text, i);
        truncated[i] = '\0';
        strcat(truncated, "..");
        textWidth = LlzMeasureText(truncated, fontSize);
        if (textWidth <= (int)maxWidth) {
            LlzDrawText(truncated, (int)(centerX - textWidth / 2), (int)y, fontSize, color);
            return;
        }
    }
    textWidth = LlzMeasureText("..", fontSize);
    LlzDrawText("..", (int)(centerX - textWidth / 2), (int)y, fontSize, color);
}

// Check if a file is WebP format
static bool IsWebPFile(const char *path) {
    if (!path) return false;
    size_t len = strlen(path);
    if (len < 5) return false;
    return (strcmp(path + len - 5, ".webp") == 0);
}

// Load WebP image file and convert to raylib Image format
static Image LoadImageWebP(const char *path) {
    Image image = {0};

    FILE *file = fopen(path, "rb");
    if (!file) {
        return image;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *fileData = (uint8_t *)malloc(fileSize);
    if (!fileData) {
        fclose(file);
        return image;
    }

    size_t bytesRead = fread(fileData, 1, fileSize, file);
    fclose(file);

    if (bytesRead != (size_t)fileSize) {
        free(fileData);
        return image;
    }

    int width, height;
    uint8_t *rgbaData = WebPDecodeRGBA(fileData, fileSize, &width, &height);
    free(fileData);

    if (!rgbaData) {
        return image;
    }

    int dataSize = width * height * 4;
    void *imageCopy = malloc(dataSize);
    if (!imageCopy) {
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

    return image;
}

// ============================================================================
// Album Art Cache Management
// ============================================================================

static void InitAlbumArtCache(void) {
    memset(g_artCache, 0, sizeof(g_artCache));
    g_artCacheCount = 0;
}

static void CleanupAlbumArtCache(void) {
    for (int i = 0; i < g_artCacheCount; i++) {
        if (g_artCache[i].loaded && g_artCache[i].texture.id != 0) {
            UnloadTexture(g_artCache[i].texture);
        }
    }
    memset(g_artCache, 0, sizeof(g_artCache));
    g_artCacheCount = 0;
}

static AlbumArtCacheEntry* FindArtCacheEntry(const char *hash) {
    for (int i = 0; i < g_artCacheCount; i++) {
        if (strcmp(g_artCache[i].hash, hash) == 0) {
            return &g_artCache[i];
        }
    }
    return NULL;
}

static AlbumArtCacheEntry* GetOrCreateArtCacheEntry(const char *artist, const char *album) {
    const char *hash = LlzMediaGenerateArtHash(artist, album);
    if (!hash || hash[0] == '\0') return NULL;

    AlbumArtCacheEntry *entry = FindArtCacheEntry(hash);
    if (entry) return entry;

    if (g_artCacheCount >= MAX_ALBUM_ART_CACHE) {
        if (g_artCache[0].loaded && g_artCache[0].texture.id != 0) {
            UnloadTexture(g_artCache[0].texture);
        }
        memmove(&g_artCache[0], &g_artCache[1], sizeof(AlbumArtCacheEntry) * (MAX_ALBUM_ART_CACHE - 1));
        g_artCacheCount = MAX_ALBUM_ART_CACHE - 1;
    }

    entry = &g_artCache[g_artCacheCount];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->hash, hash, sizeof(entry->hash) - 1);
    g_artCacheCount++;

    return entry;
}

// Try to load album art from either preview or full cache folder
static bool TryLoadAlbumArt(AlbumArtCacheEntry *entry, const char *albumName) {
    char artPath[512];
    struct stat st;

    // First try the preview folder (smaller 150x150 images for library browsing)
    snprintf(artPath, sizeof(artPath), "%s/%s.webp", ALBUM_ART_PREVIEW_DIR, entry->hash);
    if (stat(artPath, &st) == 0 && st.st_size > 0) {
        printf("[ALBUMS] Loading preview art for '%s' from %s\n", albumName, artPath);
        Image img = LoadImageWebP(artPath);
        if (img.data != NULL) {
            entry->texture = LoadTextureFromImage(img);
            UnloadImage(img);
            if (entry->texture.id != 0) {
                entry->loaded = true;
                printf("[ALBUMS] Preview art loaded: %s (%dx%d)\n", albumName, entry->texture.width, entry->texture.height);
                return true;
            }
        }
    }

    // Fallback to full album art cache (larger 250x250 images)
    snprintf(artPath, sizeof(artPath), "%s/%s.webp", ALBUM_ART_CACHE_DIR, entry->hash);
    if (stat(artPath, &st) == 0 && st.st_size > 0) {
        printf("[ALBUMS] Loading full art for '%s' from %s\n", albumName, artPath);
        Image img = LoadImageWebP(artPath);
        if (img.data != NULL) {
            entry->texture = LoadTextureFromImage(img);
            UnloadImage(img);
            if (entry->texture.id != 0) {
                entry->loaded = true;
                printf("[ALBUMS] Full art loaded: %s (%dx%d)\n", albumName, entry->texture.width, entry->texture.height);
                return true;
            }
        }
    }

    return false;
}

static void CheckAndLoadAlbumArt(int albumIndex) {
    if (albumIndex < 0 || albumIndex >= g_albums.itemCount) return;

    LlzSpotifyAlbumItem *album = &g_albums.items[albumIndex];
    AlbumArtCacheEntry *entry = GetOrCreateArtCacheEntry(album->artist, album->name);
    if (!entry) return;

    if (entry->loaded) return;

    // Try to load from disk (preview or full cache)
    if (TryLoadAlbumArt(entry, album->name)) {
        return;
    }

    // File doesn't exist in either location, request it if not already requested (or retry after timeout)
    float timeSinceRequest = g_animTimer - entry->requestTime;
    if (!entry->requested || timeSinceRequest > 10.0f) {
        printf("[ALBUMS] Requesting album art for '%s' (hash: %s)\n", album->name, entry->hash);
        LlzMediaRequestAlbumArt(entry->hash);
        entry->requested = true;
        entry->requestTime = g_animTimer;
    }
}

static void UpdateAlbumArtLoading(float dt) {
    g_artCheckTimer += dt;
    if (g_artCheckTimer < 0.3f) return;  // Check every 0.3 seconds
    g_artCheckTimer = 0;

    if (!g_albumsValid || g_albums.itemCount == 0) return;

    // Check art for visible albums (selected +/- 3)
    for (int offset = -3; offset <= 3; offset++) {
        int idx = g_selectedIndex + offset;
        if (idx >= 0 && idx < g_albums.itemCount) {
            CheckAndLoadAlbumArt(idx);
        }
    }
}

// ============================================================================
// Header & Footer
// ============================================================================

static void DrawHeader(void) {
    // Title
    LlzDrawText("Albums", PADDING, 15, TITLE_FONT_SIZE, SPOTIFY_WHITE);

    // Album count
    if (g_albumsValid && g_albums.total > 0) {
        char countStr[64];
        snprintf(countStr, sizeof(countStr), "%d albums", g_albums.total);
        int countWidth = LlzMeasureText(countStr, 22);
        LlzDrawText(countStr, SCREEN_WIDTH - PADDING - countWidth, 24, 22, SPOTIFY_SUBTLE);
    }

    // Loading indicator
    if (g_albumsLoading) {
        int dots = ((int)(g_animTimer * 4)) % 4;
        char loadStr[32] = "Loading";
        for (int i = 0; i < dots; i++) strcat(loadStr, ".");
        int loadWidth = LlzMeasureText(loadStr, 22);
        LlzDrawText(loadStr, SCREEN_WIDTH / 2 - loadWidth / 2, 24, 22, SPOTIFY_GREEN);
    }
}

static void DrawFooter(void) {
    int footerY = SCREEN_HEIGHT - 40;

    // Navigation hints
    LlzDrawText("Scroll: Browse", PADDING, footerY, HINT_FONT_SIZE, SPOTIFY_LIGHT_GRAY);

    const char *selectHint = "Select: Play Album";
    int selectWidth = LlzMeasureText(selectHint, HINT_FONT_SIZE);
    LlzDrawText(selectHint, SCREEN_WIDTH / 2 - selectWidth / 2, footerY, HINT_FONT_SIZE, SPOTIFY_LIGHT_GRAY);

    const char *backHint = "Back: Menu";
    int backWidth = LlzMeasureText(backHint, HINT_FONT_SIZE);
    LlzDrawText(backHint, SCREEN_WIDTH - PADDING - backWidth, footerY, HINT_FONT_SIZE, SPOTIFY_LIGHT_GRAY);

    // Page indicator - larger and more prominent
    if (g_albumsValid && g_albums.itemCount > 0) {
        char pageStr[32];
        snprintf(pageStr, sizeof(pageStr), "%d / %d", g_selectedIndex + 1, g_albums.itemCount);
        int pageWidth = LlzMeasureText(pageStr, 24);
        LlzDrawText(pageStr, SCREEN_WIDTH / 2 - pageWidth / 2, SCREEN_HEIGHT - 70, 24, SPOTIFY_WHITE);
    }
}

// ============================================================================
// Album Card Drawing
// ============================================================================

static void DrawAlbumCard(int index, float centerX, float y, float scale, float alpha) {
    if (index < 0 || index >= g_albums.itemCount) return;

    LlzSpotifyAlbumItem *album = &g_albums.items[index];
    bool isSelected = (index == g_selectedIndex);

    // Calculate scaled size
    float cardSize = ALBUM_SIZE * scale;
    float cardX = centerX - cardSize / 2;
    float cardY = y;

    // Alpha for fading distant cards
    Color alphaWhite = {255, 255, 255, (unsigned char)(255 * alpha)};
    Color alphaSubtle = {179, 179, 179, (unsigned char)(179 * alpha)};
    Color alphaGray = {120, 120, 120, (unsigned char)(120 * alpha)};

    // Card background
    Color cardBg = isSelected ? SPOTIFY_GREEN_DARK : SPOTIFY_GRAY;
    cardBg.a = (unsigned char)(cardBg.a * alpha);

    // Shadow for depth
    if (scale > 1.0f) {
        DrawRectangleRounded((Rectangle){cardX + 6, cardY + 6, cardSize, cardSize}, 0.1f, 8, (Color){0, 0, 0, (unsigned char)(80 * alpha)});
    }

    // Draw card background
    DrawRectangleRounded((Rectangle){cardX, cardY, cardSize, cardSize}, 0.1f, 8, cardBg);

    // Album art area
    float artPadding = 10 * scale;
    float artSize = cardSize - artPadding * 2;
    float artX = cardX + artPadding;
    float artY = cardY + artPadding;

    // Try to get album art
    AlbumArtCacheEntry *artEntry = GetOrCreateArtCacheEntry(album->artist, album->name);
    bool hasArt = artEntry && artEntry->loaded && artEntry->texture.id != 0;

    if (hasArt) {
        Rectangle artBounds = {artX, artY, artSize, artSize};
        Color tint = {255, 255, 255, (unsigned char)(255 * alpha)};
        LlzDrawTextureRounded(artEntry->texture, artBounds, 0.08f, 8, tint);
    } else {
        // Gradient placeholder
        Color gradTop = {(unsigned char)(60 + (index * 17) % 60), (unsigned char)(60 + (index * 23) % 60), (unsigned char)(80 + (index * 31) % 60), (unsigned char)(255 * alpha)};
        Color gradBot = {(unsigned char)(30 + (index * 13) % 40), (unsigned char)(30 + (index * 19) % 40), (unsigned char)(50 + (index * 29) % 40), (unsigned char)(255 * alpha)};

        DrawRectangleRounded((Rectangle){artX, artY, artSize, artSize}, 0.08f, 8, gradTop);
        DrawRectangleRounded((Rectangle){artX, artY + artSize/2, artSize, artSize/2}, 0.08f, 8, gradBot);

        // Album initial
        char initial[2] = {album->name[0], '\0'};
        if (initial[0] >= 'a' && initial[0] <= 'z') initial[0] -= 32;
        Color initColor = {255, 255, 255, (unsigned char)(180 * alpha)};
        int initSize = (int)(48 * scale);
        LlzDrawTextCentered(initial, (int)(artX + artSize/2), (int)(artY + artSize/2 - initSize/3), initSize, initColor);

        // Loading dots
        if (artEntry && artEntry->requested && !artEntry->loaded) {
            int dotCount = ((int)(g_animTimer * 4)) % 4;
            char dots[5] = "";
            for (int i = 0; i < dotCount; i++) strcat(dots, ".");
            Color dotColor = SPOTIFY_GREEN;
            dotColor.a = (unsigned char)(255 * alpha);
            LlzDrawTextCentered(dots, (int)(artX + artSize/2), (int)(artY + artSize - 20 * scale), (int)(14 * scale), dotColor);
        }
    }

    // Selection indicator
    if (isSelected) {
        // Green accent bar
        Color accentColor = SPOTIFY_GREEN;
        accentColor.a = (unsigned char)(255 * alpha);
        DrawRectangleRounded((Rectangle){cardX, cardY, 5, cardSize}, 0.5f, 4, accentColor);

        // Play icon
        float iconX = artX + artSize - 24 * scale;
        float iconY = artY + artSize - 24 * scale;
        float iconRadius = 18 * scale;
        DrawCircle((int)iconX, (int)iconY, iconRadius, accentColor);
        Color playColor = SPOTIFY_BLACK;
        playColor.a = (unsigned char)(255 * alpha);
        // Draw triangle play icon
        Vector2 v1 = {iconX - 5 * scale, iconY - 8 * scale};
        Vector2 v2 = {iconX - 5 * scale, iconY + 8 * scale};
        Vector2 v3 = {iconX + 8 * scale, iconY};
        DrawTriangle(v1, v2, v3, playColor);
    }

    // Text below card
    float textY = cardY + cardSize + 15;
    float textMaxWidth = cardSize + 40;  // Allow text wider than card for longer names

    // Album name - larger when selected
    int nameSize = isSelected ? ALBUM_NAME_FONT_SIZE + 4 : ALBUM_NAME_FONT_SIZE;
    DrawCenteredTruncatedText(album->name, centerX, textY, textMaxWidth, nameSize, alphaWhite);

    // Artist name
    DrawCenteredTruncatedText(album->artist, centerX, textY + nameSize + 6, textMaxWidth, ARTIST_FONT_SIZE, alphaGray);

    // Year and track count (for selected)
    if (isSelected && (album->year[0] || album->trackCount > 0)) {
        char infoStr[48];
        if (album->year[0] && album->trackCount > 0) {
            snprintf(infoStr, sizeof(infoStr), "%s  •  %d tracks", album->year, album->trackCount);
        } else if (album->year[0]) {
            snprintf(infoStr, sizeof(infoStr), "%s", album->year);
        } else {
            snprintf(infoStr, sizeof(infoStr), "%d tracks", album->trackCount);
        }
        DrawCenteredTruncatedText(infoStr, centerX, textY + nameSize + ARTIST_FONT_SIZE + 12, textMaxWidth, INFO_FONT_SIZE, alphaSubtle);
    }
}

// ============================================================================
// Carousel Drawing & Update
// ============================================================================

static void DrawCarousel(void) {
    if (!g_albumsValid || g_albums.itemCount == 0) {
        if (g_albumsLoading) {
            LlzDrawTextCentered("Loading albums...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 25, 32, SPOTIFY_SUBTLE);
            int dots = ((int)(g_animTimer * 3)) % 4;
            char dotsStr[8] = "";
            for (int i = 0; i < dots; i++) strcat(dotsStr, ".");
            LlzDrawTextCentered(dotsStr, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 32, SPOTIFY_GREEN);
        } else {
            LlzDrawTextCentered("No saved albums", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 35, 32, SPOTIFY_SUBTLE);
            LlzDrawTextCentered("Save albums on Spotify to see them here", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15, 22, SPOTIFY_LIGHT_GRAY);
            LlzDrawTextCentered("Press Select to refresh", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 55, 20, SPOTIFY_LIGHT_GRAY);
        }
        return;
    }

    float centerX = SCREEN_WIDTH / 2.0f;
    float cardSpacing = ALBUM_SIZE + ALBUM_SPACING;

    // Draw albums from back to front for proper layering
    // First pass: draw non-selected (distant) albums
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < g_albums.itemCount; i++) {
            float offset = (float)i - g_visualOffset;
            bool isSelected = (i == g_selectedIndex);

            // Skip based on pass (0 = non-selected, 1 = selected)
            if (pass == 0 && isSelected) continue;
            if (pass == 1 && !isSelected) continue;

            float posX = centerX + offset * cardSpacing;

            // Cull off-screen cards (with margin)
            if (posX < -ALBUM_SIZE * 1.5f || posX > SCREEN_WIDTH + ALBUM_SIZE * 0.5f) continue;

            // Calculate scale based on distance from center
            float distFromCenter = fabsf(offset);
            float scale = 1.0f;
            float alpha = 1.0f;

            if (isSelected) {
                scale = SELECTED_SCALE;
            } else {
                // Gradually scale down distant cards
                scale = 1.0f - distFromCenter * 0.1f;
                if (scale < 0.7f) scale = 0.7f;

                // Fade distant cards
                alpha = 1.0f - (distFromCenter - 1.0f) * 0.3f;
                if (alpha < 0.4f) alpha = 0.4f;
                if (alpha > 1.0f) alpha = 1.0f;
            }

            // Slight Y offset for 3D depth effect
            float yOffset = distFromCenter * distFromCenter * 5;

            DrawAlbumCard(i, posX, CAROUSEL_Y + yOffset, scale, alpha);
        }
    }

    // Draw navigation arrows - larger and more visible
    Color arrowColor = {255, 255, 255, 200};
    if (g_selectedIndex > 0) {
        LlzDrawTextCentered("<", 30, (int)(CAROUSEL_Y + ALBUM_SIZE / 2), 52, arrowColor);
    }
    if (g_selectedIndex < g_albums.itemCount - 1) {
        LlzDrawTextCentered(">", SCREEN_WIDTH - 30, (int)(CAROUSEL_Y + ALBUM_SIZE / 2), 52, arrowColor);
    }
}

static void UpdateCarousel(const LlzInputState *input, float dt) {
    // Clamp dt to prevent physics explosions on lag spikes
    if (dt > 0.1f) dt = 0.1f;

    // Navigation input
    int delta = 0;

    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? -1 : 1;
    }
    if (input->swipeLeft) delta = 1;
    if (input->swipeRight) delta = -1;
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    // Apply input
    if (delta != 0 && g_albums.itemCount > 0) {
        int newIndex = g_selectedIndex + delta;
        if (newIndex < 0) newIndex = 0;
        if (newIndex >= g_albums.itemCount) newIndex = g_albums.itemCount - 1;

        if (newIndex != g_selectedIndex) {
            g_selectedIndex = newIndex;
            // Add velocity impulse for smooth feel
            g_scrollVelocity += delta * SCROLL_IMPULSE;
        }
    }

    // Target is the selected index
    g_targetOffset = (float)g_selectedIndex;

    // Spring-based physics for buttery smooth scrolling
    float diff = g_targetOffset - g_visualOffset;

    // Spring force: F = -k * displacement
    float springForce = diff * SCROLL_SPRING_STIFFNESS;

    // Apply spring force to velocity
    g_scrollVelocity += springForce * dt;

    // Apply damping
    g_scrollVelocity *= SCROLL_DAMPING;

    // Also apply velocity decay for natural deceleration
    g_scrollVelocity *= (1.0f - (1.0f - SCROLL_VELOCITY_DECAY) * dt * 60.0f);

    // Apply velocity to position
    g_visualOffset += g_scrollVelocity;

    // Additional smooth lerp for extra smoothness
    g_visualOffset = Lerp(g_visualOffset, g_targetOffset, dt * SCROLL_LERP_SPEED);

    // Snap when very close and nearly stopped
    if (fabsf(diff) < SCROLL_SNAP_THRESHOLD && fabsf(g_scrollVelocity) < 0.001f) {
        g_visualOffset = g_targetOffset;
        g_scrollVelocity = 0;
    }

    // Select to play album
    if (input->selectPressed) {
        if (g_albumsValid && g_albums.itemCount > 0 && g_selectedIndex < g_albums.itemCount) {
            const char *uri = g_albums.items[g_selectedIndex].uri;
            if (uri[0] != '\0') {
                printf("[ALBUMS] Playing album: %s\n", g_albums.items[g_selectedIndex].name);
                LlzMediaPlaySpotifyUri(uri);
                // Navigate to Now Playing after starting playback
                LlzRequestOpenPlugin("Now Playing");
                g_wantsClose = true;
            }
        } else {
            RefreshAlbums();
        }
    }

    // Tap to refresh if no albums
    if (input->tap && !g_albumsValid) {
        RefreshAlbums();
    }
}

// ============================================================================
// Data Management
// ============================================================================

static void RefreshAlbums(void) {
    printf("[ALBUMS] Requesting saved albums from Spotify...\n");
    g_albumsLoading = true;
    LlzMediaRequestLibraryAlbums(0, 50);
}

static void PollAlbums(float dt) {
    g_pollTimer += dt;
    if (g_pollTimer >= 0.5f) {
        g_pollTimer = 0;

        LlzSpotifyAlbumListResponse temp;
        if (LlzMediaGetLibraryAlbums(&temp) && temp.valid) {
            memcpy(&g_albums, &temp, sizeof(temp));
            g_albumsValid = true;
            g_albumsLoading = false;
            printf("[ALBUMS] Got %d albums (total: %d)\n", g_albums.itemCount, g_albums.total);
        }
    }
}

// ============================================================================
// Plugin Callbacks
// ============================================================================

static void plugin_init(int width, int height) {
    g_wantsClose = false;
    g_animTimer = 0;

    memset(&g_albums, 0, sizeof(g_albums));
    g_albumsValid = false;
    g_albumsLoading = false;
    g_pollTimer = 0;
    g_artCheckTimer = 0;

    g_selectedIndex = 0;
    g_visualOffset = 0;
    g_targetOffset = 0;
    g_scrollVelocity = 0;

    InitAlbumArtCache();
    LlzMediaInit(NULL);
    RefreshAlbums();
}

static void plugin_update(const LlzInputState *input, float deltaTime) {
    g_animTimer += deltaTime;

    PollAlbums(deltaTime);
    UpdateAlbumArtLoading(deltaTime);

    if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
        g_wantsClose = true;
        return;
    }

    UpdateCarousel(input, deltaTime);
}

static void plugin_draw(void) {
    ClearBackground(SPOTIFY_BLACK);

    DrawHeader();
    DrawCarousel();
    DrawFooter();
}

static void plugin_shutdown(void) {
    CleanupAlbumArtCache();
}

static bool plugin_wants_close(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin API Export
// ============================================================================

static const LlzPluginAPI g_albumsPluginAPI = {
    .name = "Albums",
    .description = "Browse your saved Spotify albums",
    .init = plugin_init,
    .update = plugin_update,
    .draw = plugin_draw,
    .shutdown = plugin_shutdown,
    .wants_close = plugin_wants_close,
    .handles_back_button = false,
    .category = LLZ_CATEGORY_MEDIA,
    .wants_refresh = NULL
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_albumsPluginAPI;
}
