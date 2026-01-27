/**
 * Artists Plugin for llizardgui-host
 *
 * Displays followed artists from Spotify library in a smooth carousel view.
 * Artists are shown as circular cards that can be scrolled horizontally.
 * Artist art is loaded from either the preview cache or full art cache.
 *
 * Navigation:
 * - Scroll/Swipe: Navigate through artists
 * - Select: Play artist (shuffle their top tracks)
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

// Artist card dimensions - circular for artist profile aesthetic
#define ARTIST_SIZE 180
#define ARTIST_SPACING 35
#define SELECTED_SCALE 1.15f  // Selected artist is 15% bigger

// Layout - centered vertically
#define CAROUSEL_Y 80
#define CAROUSEL_HEIGHT (ARTIST_SIZE + 120)  // Room for text below

// Text sizes - LARGER for readability
#define TITLE_FONT_SIZE 42
#define ARTIST_NAME_FONT_SIZE 26
#define GENRE_FONT_SIZE 18
#define FOLLOWERS_FONT_SIZE 16
#define HINT_FONT_SIZE 18

// Artist art cache paths
#define MAX_ARTIST_ART_CACHE 50
#define ALBUM_ART_PREVIEW_DIR "/var/mediadash/album_art_previews"
#define ALBUM_ART_CACHE_DIR "/var/mediadash/album_art_cache"

// Smooth scrolling physics
#define SCROLL_LERP_SPEED 8.0f
#define SCROLL_SNAP_THRESHOLD 0.001f
#define SCROLL_VELOCITY_DECAY 0.88f
#define SCROLL_IMPULSE 0.4f
#define SCROLL_SPRING_STIFFNESS 6.0f
#define SCROLL_DAMPING 0.85f

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
// Artist Art Cache Entry
// ============================================================================

typedef struct {
    char hash[64];           // Art hash (artist name CRC32)
    Texture2D texture;       // Loaded texture
    bool loaded;             // True if texture is valid
    bool requested;          // True if art has been requested via BLE
    float requestTime;       // Time when art was requested (for retry logic)
} ArtistArtCacheEntry;

// ============================================================================
// Loading State Machine
// ============================================================================

typedef enum {
    LOAD_STATE_INIT,        // Initial state, about to request data
    LOAD_STATE_REQUESTING,  // Data request sent, waiting for response
    LOAD_STATE_LOADED,      // Data loaded successfully
    LOAD_STATE_EMPTY,       // Data loaded but empty
    LOAD_STATE_ERROR        // Error occurred
} LoadState;

// ============================================================================
// Plugin State
// ============================================================================

static bool g_wantsClose = false;
static float g_animTimer = 0.0f;

// Artists data
static LlzSpotifyArtistListResponse g_artists = {0};
static LoadState g_loadState = LOAD_STATE_INIT;
static float g_pollTimer = 0.0f;
static int g_dataVersion = 0;  // Incremented when data changes

// Artist art cache
static ArtistArtCacheEntry g_artCache[MAX_ARTIST_ART_CACHE];
static int g_artCacheCount = 0;

// Carousel state - smooth scrolling
static int g_selectedIndex = 0;
static float g_visualOffset = 0.0f;
static float g_targetOffset = 0.0f;
static float g_scrollVelocity = 0.0f;

// Art check timer
static float g_artCheckTimer = 0.0f;
static float g_initDelay = 0.0f;  // Delay before starting art loading

// ============================================================================
// Helper: Safe Item Count Access
// ============================================================================

static inline int SafeItemCount(void) {
    return (g_loadState == LOAD_STATE_LOADED) ? g_artists.itemCount : 0;
}

static inline bool HasValidData(void) {
    return g_loadState == LOAD_STATE_LOADED && g_artists.itemCount > 0;
}

static inline void ClampSelectedIndex(void) {
    int count = SafeItemCount();
    if (count == 0) {
        g_selectedIndex = 0;
    } else if (g_selectedIndex >= count) {
        g_selectedIndex = count - 1;
    } else if (g_selectedIndex < 0) {
        g_selectedIndex = 0;
    }
}

// ============================================================================
// Forward Declarations
// ============================================================================

static void DrawHeader(void);
static void DrawCarousel(void);
static void DrawArtistCard(int index, float centerX, float y, float scale, float alpha);
static void DrawFooter(void);
static void RefreshArtists(void);
static void UpdateCarousel(const LlzInputState *input, float dt);
static void InitArtistArtCache(void);
static void CleanupArtistArtCache(void);
static ArtistArtCacheEntry* GetOrCreateArtCacheEntry(const char *artistName);
static void CheckAndLoadArtistArt(int artistIndex);
static void UpdateArtistArtLoading(float dt);

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

// Format follower count nicely (e.g., 1.2M, 45K)
static void FormatFollowers(int followers, char *out, size_t outSize) {
    if (followers >= 1000000) {
        snprintf(out, outSize, "%.1fM followers", followers / 1000000.0f);
    } else if (followers >= 1000) {
        snprintf(out, outSize, "%.1fK followers", followers / 1000.0f);
    } else {
        snprintf(out, outSize, "%d followers", followers);
    }
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
// Artist Art Cache Management
// ============================================================================

static void InitArtistArtCache(void) {
    memset(g_artCache, 0, sizeof(g_artCache));
    g_artCacheCount = 0;
}

static void CleanupArtistArtCache(void) {
    for (int i = 0; i < g_artCacheCount; i++) {
        if (g_artCache[i].loaded && g_artCache[i].texture.id != 0) {
            UnloadTexture(g_artCache[i].texture);
        }
    }
    memset(g_artCache, 0, sizeof(g_artCache));
    g_artCacheCount = 0;
}

static ArtistArtCacheEntry* FindArtCacheEntry(const char *hash) {
    for (int i = 0; i < g_artCacheCount; i++) {
        if (strcmp(g_artCache[i].hash, hash) == 0) {
            return &g_artCache[i];
        }
    }
    return NULL;
}

static ArtistArtCacheEntry* GetOrCreateArtCacheEntry(const char *artistName) {
    // For artists, we use just the artist name for the hash
    // The artHash field in LlzSpotifyArtistItem should contain this
    if (!artistName || artistName[0] == '\0') return NULL;

    // Use the SDK's hash generation with empty album for artist-only hash
    const char *hash = LlzMediaGenerateArtHash(artistName, "");
    if (!hash || hash[0] == '\0') return NULL;

    ArtistArtCacheEntry *entry = FindArtCacheEntry(hash);
    if (entry) return entry;

    if (g_artCacheCount >= MAX_ARTIST_ART_CACHE) {
        if (g_artCache[0].loaded && g_artCache[0].texture.id != 0) {
            UnloadTexture(g_artCache[0].texture);
        }
        memmove(&g_artCache[0], &g_artCache[1], sizeof(ArtistArtCacheEntry) * (MAX_ARTIST_ART_CACHE - 1));
        g_artCacheCount = MAX_ARTIST_ART_CACHE - 1;
    }

    entry = &g_artCache[g_artCacheCount];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->hash, hash, sizeof(entry->hash) - 1);
    g_artCacheCount++;

    return entry;
}

// Try to load artist art from cache folders
static bool TryLoadArtistArt(ArtistArtCacheEntry *entry, const char *artistName) {
    char artPath[512];
    struct stat st;

    // First try the preview folder
    snprintf(artPath, sizeof(artPath), "%s/%s.webp", ALBUM_ART_PREVIEW_DIR, entry->hash);
    if (stat(artPath, &st) == 0 && st.st_size > 0) {
        printf("[ARTISTS] Loading preview art for '%s' from %s\n", artistName, artPath);
        Image img = LoadImageWebP(artPath);
        if (img.data != NULL) {
            entry->texture = LoadTextureFromImage(img);
            UnloadImage(img);
            if (entry->texture.id != 0) {
                entry->loaded = true;
                printf("[ARTISTS] Preview art loaded: %s (%dx%d)\n", artistName, entry->texture.width, entry->texture.height);
                return true;
            }
        }
    }

    // Fallback to full art cache
    snprintf(artPath, sizeof(artPath), "%s/%s.webp", ALBUM_ART_CACHE_DIR, entry->hash);
    if (stat(artPath, &st) == 0 && st.st_size > 0) {
        printf("[ARTISTS] Loading full art for '%s' from %s\n", artistName, artPath);
        Image img = LoadImageWebP(artPath);
        if (img.data != NULL) {
            entry->texture = LoadTextureFromImage(img);
            UnloadImage(img);
            if (entry->texture.id != 0) {
                entry->loaded = true;
                printf("[ARTISTS] Full art loaded: %s (%dx%d)\n", artistName, entry->texture.width, entry->texture.height);
                return true;
            }
        }
    }

    return false;
}

static void CheckAndLoadArtistArt(int artistIndex) {
    // Defensive bounds check with safe accessor
    int count = SafeItemCount();
    if (artistIndex < 0 || artistIndex >= count || count == 0) return;

    LlzSpotifyArtistItem *artist = &g_artists.items[artistIndex];

    // Validate artist data before proceeding
    if (!artist->name[0]) return;

    ArtistArtCacheEntry *entry = GetOrCreateArtCacheEntry(artist->name);
    if (!entry) return;

    if (entry->loaded) return;

    // Try to load from disk
    if (TryLoadArtistArt(entry, artist->name)) {
        return;
    }

    // Request art if not available
    float timeSinceRequest = g_animTimer - entry->requestTime;
    if (!entry->requested || timeSinceRequest > 10.0f) {
        printf("[ARTISTS] Requesting artist art for '%s' (hash: %s)\n", artist->name, entry->hash);
        LlzMediaRequestAlbumArt(entry->hash);
        entry->requested = true;
        entry->requestTime = g_animTimer;
    }
}

static void UpdateArtistArtLoading(float dt) {
    // Wait for init delay before loading art (prevents overwhelming system)
    if (g_initDelay < 0.5f) {
        g_initDelay += dt;
        return;
    }

    g_artCheckTimer += dt;
    if (g_artCheckTimer < 0.3f) return;
    g_artCheckTimer = 0;

    if (!HasValidData()) return;

    // Clamp selected index before using it
    ClampSelectedIndex();
    int count = SafeItemCount();

    // Check art for visible artists (selected +/- 3), staggered to prevent burst loading
    static int loadOffset = -3;
    int idx = g_selectedIndex + loadOffset;
    if (idx >= 0 && idx < count) {
        CheckAndLoadArtistArt(idx);
    }
    loadOffset++;
    if (loadOffset > 3) loadOffset = -3;
}

// ============================================================================
// Header & Footer
// ============================================================================

static void DrawHeader(void) {
    // Title
    LlzDrawText("Artists", PADDING, 15, TITLE_FONT_SIZE, SPOTIFY_WHITE);

    // Artist count
    if (g_loadState == LOAD_STATE_LOADED && g_artists.total > 0) {
        char countStr[64];
        snprintf(countStr, sizeof(countStr), "%d artists", g_artists.total);
        int countWidth = LlzMeasureText(countStr, 22);
        LlzDrawText(countStr, SCREEN_WIDTH - PADDING - countWidth, 24, 22, SPOTIFY_SUBTLE);
    }

    // Loading indicator
    if (g_loadState == LOAD_STATE_INIT || g_loadState == LOAD_STATE_REQUESTING) {
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

    const char *selectHint = "Select: Play Artist";
    int selectWidth = LlzMeasureText(selectHint, HINT_FONT_SIZE);
    LlzDrawText(selectHint, SCREEN_WIDTH / 2 - selectWidth / 2, footerY, HINT_FONT_SIZE, SPOTIFY_LIGHT_GRAY);

    const char *backHint = "Back: Menu";
    int backWidth = LlzMeasureText(backHint, HINT_FONT_SIZE);
    LlzDrawText(backHint, SCREEN_WIDTH - PADDING - backWidth, footerY, HINT_FONT_SIZE, SPOTIFY_LIGHT_GRAY);

    // Page indicator
    int count = SafeItemCount();
    if (count > 0) {
        // Ensure selectedIndex is valid before displaying
        int displayIndex = g_selectedIndex;
        if (displayIndex >= count) displayIndex = count - 1;
        if (displayIndex < 0) displayIndex = 0;

        char pageStr[32];
        snprintf(pageStr, sizeof(pageStr), "%d / %d", displayIndex + 1, count);
        int pageWidth = LlzMeasureText(pageStr, 24);
        LlzDrawText(pageStr, SCREEN_WIDTH / 2 - pageWidth / 2, SCREEN_HEIGHT - 70, 24, SPOTIFY_WHITE);
    }
}

// ============================================================================
// Artist Card Drawing
// ============================================================================

static void DrawArtistCard(int index, float centerX, float y, float scale, float alpha) {
    // Double-check bounds with safe accessor
    int count = SafeItemCount();
    if (index < 0 || index >= count || count == 0) return;

    LlzSpotifyArtistItem *artist = &g_artists.items[index];

    // Validate artist has minimum required data
    if (!artist->name[0]) return;

    bool isSelected = (index == g_selectedIndex);

    // Calculate scaled size
    float cardSize = ARTIST_SIZE * scale;
    float cardX = centerX - cardSize / 2;
    float cardY = y;

    // Alpha for fading distant cards
    Color alphaWhite = {255, 255, 255, (unsigned char)(255 * alpha)};
    Color alphaSubtle = {179, 179, 179, (unsigned char)(179 * alpha)};
    Color alphaGray = {120, 120, 120, (unsigned char)(120 * alpha)};

    // Card background - circular for artist aesthetic
    Color cardBg = isSelected ? SPOTIFY_GREEN_DARK : SPOTIFY_GRAY;
    cardBg.a = (unsigned char)(cardBg.a * alpha);

    // Shadow for depth
    if (scale > 1.0f) {
        DrawCircle((int)(centerX + 4), (int)(y + cardSize/2 + 4), cardSize/2, (Color){0, 0, 0, (unsigned char)(60 * alpha)});
    }

    // Draw circular card background
    DrawCircle((int)centerX, (int)(y + cardSize/2), cardSize/2, cardBg);

    // Artist art area (slightly smaller circle inside)
    float artRadius = (cardSize / 2) - 8 * scale;
    float artCenterY = y + cardSize / 2;

    // Try to get artist art
    ArtistArtCacheEntry *artEntry = GetOrCreateArtCacheEntry(artist->name);
    bool hasArt = artEntry && artEntry->loaded && artEntry->texture.id != 0;

    if (hasArt) {
        // Draw circular artist image
        float artSize = artRadius * 2;
        Rectangle artBounds = {centerX - artRadius, artCenterY - artRadius, artSize, artSize};
        Color tint = {255, 255, 255, (unsigned char)(255 * alpha)};
        // Use very high roundness for circular effect
        LlzDrawTextureRounded(artEntry->texture, artBounds, 0.5f, 32, tint);
    } else {
        // Gradient placeholder circle
        Color gradTop = {(unsigned char)(80 + (index * 17) % 80), (unsigned char)(60 + (index * 23) % 60), (unsigned char)(100 + (index * 31) % 80), (unsigned char)(255 * alpha)};
        DrawCircle((int)centerX, (int)artCenterY, artRadius, gradTop);

        // Artist initial
        char initial[2] = {artist->name[0], '\0'};
        if (initial[0] >= 'a' && initial[0] <= 'z') initial[0] -= 32;
        Color initColor = {255, 255, 255, (unsigned char)(180 * alpha)};
        int initSize = (int)(56 * scale);
        LlzDrawTextCentered(initial, (int)centerX, (int)(artCenterY - initSize/3), initSize, initColor);

        // Loading dots
        if (artEntry && artEntry->requested && !artEntry->loaded) {
            int dotCount = ((int)(g_animTimer * 4)) % 4;
            char dots[5] = "";
            for (int i = 0; i < dotCount; i++) strcat(dots, ".");
            Color dotColor = SPOTIFY_GREEN;
            dotColor.a = (unsigned char)(255 * alpha);
            LlzDrawTextCentered(dots, (int)centerX, (int)(artCenterY + artRadius - 15 * scale), (int)(14 * scale), dotColor);
        }
    }

    // Selection indicator - green ring
    if (isSelected) {
        Color accentColor = SPOTIFY_GREEN;
        accentColor.a = (unsigned char)(255 * alpha);
        DrawCircleLines((int)centerX, (int)artCenterY, artRadius + 4, accentColor);
        DrawCircleLines((int)centerX, (int)artCenterY, artRadius + 5, accentColor);
        DrawCircleLines((int)centerX, (int)artCenterY, artRadius + 6, accentColor);

        // Play icon in bottom right
        float iconX = centerX + artRadius * 0.6f;
        float iconY = artCenterY + artRadius * 0.6f;
        float iconRadius = 16 * scale;
        DrawCircle((int)iconX, (int)iconY, iconRadius, accentColor);
        Color playColor = SPOTIFY_BLACK;
        playColor.a = (unsigned char)(255 * alpha);
        Vector2 v1 = {iconX - 4 * scale, iconY - 6 * scale};
        Vector2 v2 = {iconX - 4 * scale, iconY + 6 * scale};
        Vector2 v3 = {iconX + 6 * scale, iconY};
        DrawTriangle(v1, v2, v3, playColor);
    }

    // Text below card
    float textY = cardY + cardSize + 12;
    float textMaxWidth = cardSize + 60;

    // Artist name - larger when selected
    int nameSize = isSelected ? ARTIST_NAME_FONT_SIZE + 4 : ARTIST_NAME_FONT_SIZE;
    DrawCenteredTruncatedText(artist->name, centerX, textY, textMaxWidth, nameSize, alphaWhite);

    // Genre (show first genre if available)
    if (artist->genreCount > 0 && artist->genres[0][0] != '\0') {
        DrawCenteredTruncatedText(artist->genres[0], centerX, textY + nameSize + 6, textMaxWidth, GENRE_FONT_SIZE, alphaGray);
    }

    // Follower count (for selected)
    if (isSelected && artist->followers > 0) {
        char followersStr[48];
        FormatFollowers(artist->followers, followersStr, sizeof(followersStr));
        float followersY = (artist->genreCount > 0) ? textY + nameSize + GENRE_FONT_SIZE + 14 : textY + nameSize + 10;
        DrawCenteredTruncatedText(followersStr, centerX, followersY, textMaxWidth, FOLLOWERS_FONT_SIZE, alphaSubtle);
    }
}

// ============================================================================
// Carousel Drawing & Update
// ============================================================================

static void DrawCarousel(void) {
    int count = SafeItemCount();

    // Show loading/empty state
    if (count == 0) {
        if (g_loadState == LOAD_STATE_INIT || g_loadState == LOAD_STATE_REQUESTING) {
            LlzDrawTextCentered("Loading artists...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 25, 32, SPOTIFY_SUBTLE);
            int dots = ((int)(g_animTimer * 3)) % 4;
            char dotsStr[8] = "";
            for (int i = 0; i < dots; i++) strcat(dotsStr, ".");
            LlzDrawTextCentered(dotsStr, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 32, SPOTIFY_GREEN);
        } else {
            LlzDrawTextCentered("No followed artists", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 35, 32, SPOTIFY_SUBTLE);
            LlzDrawTextCentered("Follow artists on Spotify to see them here", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15, 22, SPOTIFY_LIGHT_GRAY);
            LlzDrawTextCentered("Press Select to refresh", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 55, 20, SPOTIFY_LIGHT_GRAY);
        }
        return;
    }

    // Ensure selectedIndex is valid before drawing
    ClampSelectedIndex();

    float centerX = SCREEN_WIDTH / 2.0f;
    float cardSpacing = ARTIST_SIZE + ARTIST_SPACING;

    // Draw artists from back to front
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < count; i++) {
            float offset = (float)i - g_visualOffset;
            bool isSelected = (i == g_selectedIndex);

            if (pass == 0 && isSelected) continue;
            if (pass == 1 && !isSelected) continue;

            float posX = centerX + offset * cardSpacing;

            if (posX < -ARTIST_SIZE * 1.5f || posX > SCREEN_WIDTH + ARTIST_SIZE * 0.5f) continue;

            float distFromCenter = fabsf(offset);
            float scale = 1.0f;
            float alpha = 1.0f;

            if (isSelected) {
                scale = SELECTED_SCALE;
            } else {
                scale = 1.0f - distFromCenter * 0.1f;
                if (scale < 0.7f) scale = 0.7f;

                alpha = 1.0f - (distFromCenter - 1.0f) * 0.3f;
                if (alpha < 0.4f) alpha = 0.4f;
                if (alpha > 1.0f) alpha = 1.0f;
            }

            float yOffset = distFromCenter * distFromCenter * 5;

            DrawArtistCard(i, posX, CAROUSEL_Y + yOffset, scale, alpha);
        }
    }

    // Draw navigation arrows
    Color arrowColor = {255, 255, 255, 200};
    if (g_selectedIndex > 0) {
        LlzDrawTextCentered("<", 30, (int)(CAROUSEL_Y + ARTIST_SIZE / 2), 52, arrowColor);
    }
    if (g_selectedIndex < count - 1) {
        LlzDrawTextCentered(">", SCREEN_WIDTH - 30, (int)(CAROUSEL_Y + ARTIST_SIZE / 2), 52, arrowColor);
    }
}

static void UpdateCarousel(const LlzInputState *input, float dt) {
    if (dt > 0.1f) dt = 0.1f;

    int count = SafeItemCount();

    // Ensure selectedIndex stays valid if data changed
    ClampSelectedIndex();

    int delta = 0;

    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? -1 : 1;
    }
    if (input->swipeLeft) delta = 1;
    if (input->swipeRight) delta = -1;
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    if (delta != 0 && count > 0) {
        int newIndex = g_selectedIndex + delta;
        if (newIndex < 0) newIndex = 0;
        if (newIndex >= count) newIndex = count - 1;

        if (newIndex != g_selectedIndex) {
            g_selectedIndex = newIndex;
            g_scrollVelocity += delta * SCROLL_IMPULSE;
        }
    }

    g_targetOffset = (float)g_selectedIndex;

    // Spring-based physics
    float diff = g_targetOffset - g_visualOffset;
    float springForce = diff * SCROLL_SPRING_STIFFNESS;
    g_scrollVelocity += springForce * dt;
    g_scrollVelocity *= SCROLL_DAMPING;
    g_scrollVelocity *= (1.0f - (1.0f - SCROLL_VELOCITY_DECAY) * dt * 60.0f);
    g_visualOffset += g_scrollVelocity;
    g_visualOffset = Lerp(g_visualOffset, g_targetOffset, dt * SCROLL_LERP_SPEED);

    if (fabsf(diff) < SCROLL_SNAP_THRESHOLD && fabsf(g_scrollVelocity) < 0.001f) {
        g_visualOffset = g_targetOffset;
        g_scrollVelocity = 0;
    }

    // Select to play artist
    if (input->selectPressed) {
        if (HasValidData() && g_selectedIndex >= 0 && g_selectedIndex < count) {
            const char *uri = g_artists.items[g_selectedIndex].uri;
            if (uri[0] != '\0') {
                printf("[ARTISTS] Playing artist: %s\n", g_artists.items[g_selectedIndex].name);
                LlzMediaPlaySpotifyUri(uri);
                LlzRequestOpenPlugin("Now Playing");
                g_wantsClose = true;
            }
        } else if (!HasValidData()) {
            RefreshArtists();
        }
    }

    // Tap to refresh if no artists
    if (input->tap && !HasValidData()) {
        RefreshArtists();
    }
}

// ============================================================================
// Data Management
// ============================================================================

static void RefreshArtists(void) {
    printf("[ARTISTS] Requesting followed artists from Spotify...\n");
    g_loadState = LOAD_STATE_REQUESTING;
    LlzMediaRequestLibraryArtists(50, NULL);
}

static void PollArtists(float dt) {
    // Only poll if we're still loading
    if (g_loadState != LOAD_STATE_INIT && g_loadState != LOAD_STATE_REQUESTING) {
        return;
    }

    g_pollTimer += dt;
    if (g_pollTimer >= 0.5f) {
        g_pollTimer = 0;

        LlzSpotifyArtistListResponse temp = {0};
        if (LlzMediaGetLibraryArtists(&temp) && temp.valid) {
            int oldCount = g_artists.itemCount;
            memcpy(&g_artists, &temp, sizeof(temp));

            if (g_artists.itemCount > 0) {
                g_loadState = LOAD_STATE_LOADED;
                printf("[ARTISTS] Got %d artists (total: %d)\n", g_artists.itemCount, g_artists.total);
            } else {
                g_loadState = LOAD_STATE_EMPTY;
                printf("[ARTISTS] No artists found\n");
            }

            // Clamp selection if data changed
            if (oldCount != g_artists.itemCount) {
                g_dataVersion++;
                ClampSelectedIndex();
            }
        }
    }
}

// ============================================================================
// Plugin Callbacks
// ============================================================================

static void plugin_init(int width, int height) {
    g_wantsClose = false;
    g_animTimer = 0;

    memset(&g_artists, 0, sizeof(g_artists));
    g_loadState = LOAD_STATE_INIT;
    g_pollTimer = 0;
    g_artCheckTimer = 0;
    g_initDelay = 0;
    g_dataVersion = 0;

    g_selectedIndex = 0;
    g_visualOffset = 0;
    g_targetOffset = 0;
    g_scrollVelocity = 0;

    InitArtistArtCache();
    LlzMediaInit(NULL);
    RefreshArtists();
}

static void plugin_update(const LlzInputState *input, float deltaTime) {
    g_animTimer += deltaTime;

    PollArtists(deltaTime);
    UpdateArtistArtLoading(deltaTime);

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
    CleanupArtistArtCache();
}

static bool plugin_wants_close(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin API Export
// ============================================================================

static const LlzPluginAPI g_artistsPluginAPI = {
    .name = "Artists",
    .description = "Browse your followed Spotify artists",
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
    return &g_artistsPluginAPI;
}
