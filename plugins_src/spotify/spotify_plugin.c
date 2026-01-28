/**
 * Spotify Plugin for llizardgui-host
 *
 * A full-featured Spotify control interface with carousel screens matching
 * the Janus Android companion app's Spotify library tabs.
 *
 * Screens:
 * - Now Playing: Current track with album art and playback controls
 * - Queue: Upcoming tracks with skip-to functionality
 * - Liked: Saved tracks from Spotify library (placeholder)
 * - Albums: Saved albums from Spotify library (placeholder)
 * - Playlists: User playlists from Spotify library (placeholder)
 *
 * Navigation:
 * - Swipe left/right: Switch between carousel screens
 * - Scroll wheel: Navigate within screen (list items, volume)
 * - Select button: Confirm action (play track, toggle control)
 * - Back button: Return to menu
 * - Tap: Quick actions (play/pause on Now Playing)
 */

#include "llz_sdk.h"
#include "llz_sdk_navigation.h"
#include "llizard_plugin.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ============================================================================
// Display Constants
// ============================================================================

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
#define PADDING 20
#define HEADER_HEIGHT 50
#define FOOTER_HEIGHT 40
#define CONTENT_HEIGHT (SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT)

// ============================================================================
// Spotify Color Palette
// ============================================================================

static const Color SPOTIFY_GREEN = {30, 215, 96, 255};
static const Color SPOTIFY_GREEN_DARK = {20, 145, 65, 255};
static const Color SPOTIFY_BLACK = {18, 18, 18, 255};
static const Color SPOTIFY_DARK = {24, 24, 24, 255};
static const Color SPOTIFY_GRAY = {40, 40, 40, 255};
static const Color SPOTIFY_LIGHT_GRAY = {83, 83, 83, 255};
static const Color SPOTIFY_WHITE = {255, 255, 255, 255};
static const Color SPOTIFY_SUBTLE = {179, 179, 179, 255};
static const Color SPOTIFY_RED = {230, 70, 70, 255};

// ============================================================================
// Screen Types
// ============================================================================

typedef enum {
    SCREEN_NOW_PLAYING = 0,
    SCREEN_QUEUE,
    SCREEN_LIKED,
    SCREEN_ALBUMS,
    SCREEN_PLAYLISTS,
    SCREEN_COUNT
} SpotifyScreen;

static const char* SCREEN_TITLES[SCREEN_COUNT] = {
    "Now Playing",
    "Queue",
    "Liked Songs",
    "Albums",
    "Playlists"
};

static const char* SCREEN_ICONS[SCREEN_COUNT] = {
    ">",   // Now Playing
    "=",   // Queue
    "<3",  // Liked
    "[A]", // Albums
    "[P]"  // Playlists
};

// ============================================================================
// Plugin State
// ============================================================================

static bool g_wantsClose = false;
static SpotifyScreen g_currentScreen = SCREEN_NOW_PLAYING;
static float g_animTimer = 0.0f;

// Screen transition animation
static float g_screenOffset = 0.0f;
static float g_targetScreenOffset = 0.0f;
static SpotifyScreen g_fromScreen = SCREEN_NOW_PLAYING;

// Media state
static LlzMediaState g_mediaState = {0};
static bool g_mediaValid = false;
static float g_mediaRefreshTimer = 0.0f;

// Queue state
static LlzQueueData g_queueData = {0};
static bool g_queueValid = false;
static int g_queueSelectedIndex = 0;
static float g_queueScrollOffset = 0.0f;

// Album art
static Texture2D g_albumArtTexture = {0};
static bool g_albumArtValid = false;
static char g_currentArtHash[64] = {0};

// Connection status
static bool g_spotifyConnected = false;

// Controls on Now Playing screen
static int g_npControlSelected = 1;  // 0=prev, 1=play, 2=next
static bool g_showingVolume = false;
static float g_volumeShowTimer = 0.0f;

// Library data
static LlzSpotifyLibraryOverview g_libraryOverview = {0};
static bool g_libraryOverviewValid = false;

static LlzSpotifyTrackListResponse g_likedTracks = {0};
static bool g_likedTracksValid = false;
static int g_likedSelectedIndex = 0;
static float g_likedScrollOffset = 0.0f;
static bool g_likedRefreshing = false;

static LlzSpotifyAlbumListResponse g_albums = {0};
static bool g_albumsValid = false;
static int g_albumsSelectedIndex = 0;
static float g_albumsScrollOffset = 0.0f;
static bool g_albumsRefreshing = false;

static LlzSpotifyPlaylistListResponse g_playlists = {0};
static bool g_playlistsValid = false;
static int g_playlistsSelectedIndex = 0;
static float g_playlistsScrollOffset = 0.0f;
static bool g_playlistsRefreshing = false;

// List item dimensions
#define LIST_ITEM_HEIGHT 60
#define LIST_VISIBLE_ITEMS 5

// ============================================================================
// Forward Declarations
// ============================================================================

static void DrawHeader(void);
static void DrawFooter(void);
static void DrawScreenIndicator(void);
static void DrawNowPlayingScreen(float offsetX);
static void DrawQueueScreen(float offsetX);
static void DrawLikedScreen(float offsetX);
static void DrawAlbumsScreen(float offsetX);
static void DrawPlaylistsScreen(float offsetX);
static void UpdateNowPlayingScreen(const LlzInputState *input, float dt);
static void UpdateQueueScreen(const LlzInputState *input, float dt);
static void UpdateLikedScreen(const LlzInputState *input, float dt);
static void UpdateAlbumsScreen(const LlzInputState *input, float dt);
static void UpdatePlaylistsScreen(const LlzInputState *input, float dt);
static void SwitchScreen(SpotifyScreen target);
static void LoadAlbumArt(void);
static void RefreshQueue(void);
static void RefreshLibraryData(void);
static void PollLibraryData(float dt);

// ============================================================================
// Utility Functions
// ============================================================================

static void DrawTruncatedText(const char *text, float x, float y, float maxWidth, int fontSize, Color color) {
    if (!text || text[0] == '\0') return;

    int textWidth = LlzMeasureText(text, fontSize);
    if (textWidth <= (int)maxWidth) {
        LlzDrawText(text, (int)x, (int)y, fontSize, color);
        return;
    }

    char truncated[256];
    int len = strlen(text);
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

static void DrawRoundedCard(float x, float y, float w, float h, Color color) {
    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.1f, 8, color);
}

static void DrawProgressBar(float x, float y, float w, float h, float progress, Color bgColor, Color fgColor) {
    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.5f, 4, bgColor);
    if (progress > 0) {
        float fillWidth = w * progress;
        if (fillWidth < h) fillWidth = h;
        DrawRectangleRounded((Rectangle){x, y, fillWidth, h}, 0.5f, 4, fgColor);
    }
}

static const char* FormatDuration(int seconds) {
    static char buf[16];
    int mins = seconds / 60;
    int secs = seconds % 60;
    snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
    return buf;
}

// ============================================================================
// Album Art Loading
// ============================================================================

static void LoadAlbumArt(void) {
    if (!g_mediaValid || g_mediaState.albumArtPath[0] == '\0') return;

    const char *hash = LlzMediaGenerateArtHash(g_mediaState.artist, g_mediaState.album);
    if (!hash) return;

    if (strcmp(g_currentArtHash, hash) == 0 && g_albumArtValid) return;

    if (FileExists(g_mediaState.albumArtPath)) {
        if (g_albumArtValid) UnloadTexture(g_albumArtTexture);
        g_albumArtTexture = LoadTexture(g_mediaState.albumArtPath);
        g_albumArtValid = IsTextureValid(g_albumArtTexture);
        if (g_albumArtValid) {
            strncpy(g_currentArtHash, hash, sizeof(g_currentArtHash) - 1);
        }
    } else {
        LlzMediaRequestAlbumArt(hash);
        strncpy(g_currentArtHash, hash, sizeof(g_currentArtHash) - 1);
    }
}

// ============================================================================
// Queue Management
// ============================================================================

static void RefreshQueue(void) {
    LlzMediaRequestQueue();
}

static void PollQueue(float dt) {
    static float timer = 0;
    timer += dt;
    if (timer >= 0.5f) {
        timer = 0;
        LlzQueueData temp;
        if (LlzMediaGetQueue(&temp)) {
            memcpy(&g_queueData, &temp, sizeof(LlzQueueData));
            g_queueValid = true;
        }
    }
}

// ============================================================================
// Header & Footer
// ============================================================================

static void DrawHeader(void) {
    // Background
    DrawRectangle(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, SPOTIFY_BLACK);

    // Title
    LlzDrawText(SCREEN_TITLES[g_currentScreen], PADDING, 12, 24, SPOTIFY_WHITE);

    // Connection indicator
    Color connColor = g_spotifyConnected ? SPOTIFY_GREEN : SPOTIFY_RED;
    DrawCircle(SCREEN_WIDTH - PADDING - 10, HEADER_HEIGHT / 2, 6, connColor);

    // Spotify logo text
    int logoWidth = LlzMeasureText("SPOTIFY", 12);
    LlzDrawText("SPOTIFY", SCREEN_WIDTH - PADDING - 30 - logoWidth, 18, 12, SPOTIFY_GREEN);
}

static void DrawFooter(void) {
    // Background
    DrawRectangle(0, SCREEN_HEIGHT - FOOTER_HEIGHT, SCREEN_WIDTH, FOOTER_HEIGHT, SPOTIFY_BLACK);

    // Screen indicator dots
    DrawScreenIndicator();

    // Navigation hints
    LlzDrawText("< >", PADDING, SCREEN_HEIGHT - FOOTER_HEIGHT + 12, 14, SPOTIFY_LIGHT_GRAY);

    int hintWidth = LlzMeasureText("Back: Menu", 14);
    LlzDrawText("Back: Menu", SCREEN_WIDTH - PADDING - hintWidth, SCREEN_HEIGHT - FOOTER_HEIGHT + 12, 14, SPOTIFY_LIGHT_GRAY);
}

static void DrawScreenIndicator(void) {
    float dotSize = 8;
    float dotSpacing = 20;
    float totalWidth = SCREEN_COUNT * dotSpacing;
    float startX = (SCREEN_WIDTH - totalWidth) / 2;
    float y = SCREEN_HEIGHT - FOOTER_HEIGHT / 2;

    for (int i = 0; i < SCREEN_COUNT; i++) {
        Color dotColor = (i == g_currentScreen) ? SPOTIFY_GREEN : SPOTIFY_LIGHT_GRAY;
        float size = (i == g_currentScreen) ? dotSize : dotSize * 0.7f;
        DrawCircle((int)(startX + i * dotSpacing + dotSize / 2), (int)y, size / 2, dotColor);
    }
}

// ============================================================================
// Now Playing Screen
// ============================================================================

static void DrawNowPlayingScreen(float offsetX) {
    float contentY = HEADER_HEIGHT;
    float contentH = CONTENT_HEIGHT;

    // Album art (large, centered)
    float artSize = 220;
    float artX = offsetX + (SCREEN_WIDTH - artSize) / 2;
    float artY = contentY + 20;

    DrawRoundedCard(artX, artY, artSize, artSize, SPOTIFY_GRAY);

    if (g_albumArtValid) {
        float scale = artSize / (float)g_albumArtTexture.width;
        Rectangle src = {0, 0, (float)g_albumArtTexture.width, (float)g_albumArtTexture.height};
        Rectangle dest = {artX + 4, artY + 4, artSize - 8, artSize - 8};
        DrawTexturePro(g_albumArtTexture, src, dest, (Vector2){0, 0}, 0, WHITE);
    } else {
        LlzDrawTextCentered("No Art", (int)(artX + artSize / 2), (int)(artY + artSize / 2), 20, SPOTIFY_LIGHT_GRAY);
    }

    // Track info below album art
    float infoY = artY + artSize + 20;
    float infoWidth = SCREEN_WIDTH - PADDING * 4;
    float infoX = offsetX + PADDING * 2;

    // Like indicator
    Color likeColor = g_mediaState.isLiked ? SPOTIFY_GREEN : SPOTIFY_LIGHT_GRAY;
    LlzDrawText(g_mediaState.isLiked ? "<3" : "o", (int)(infoX - 30), (int)(infoY + 5), 20, likeColor);

    // Track title
    if (g_mediaValid && g_mediaState.track[0]) {
        DrawTruncatedText(g_mediaState.track, infoX, infoY, infoWidth, 24, SPOTIFY_WHITE);
    } else {
        LlzDrawText("Not Playing", (int)infoX, (int)infoY, 24, SPOTIFY_SUBTLE);
    }

    // Artist
    if (g_mediaValid && g_mediaState.artist[0]) {
        DrawTruncatedText(g_mediaState.artist, infoX, infoY + 30, infoWidth, 18, SPOTIFY_SUBTLE);
    }

    // Progress bar
    float progressY = infoY + 65;
    float progress = 0;
    if (g_mediaValid && g_mediaState.durationSeconds > 0) {
        progress = (float)g_mediaState.positionSeconds / (float)g_mediaState.durationSeconds;
    }
    DrawProgressBar(infoX, progressY, infoWidth, 4, progress, SPOTIFY_GRAY, SPOTIFY_GREEN);

    // Time labels
    if (g_mediaValid) {
        LlzDrawText(FormatDuration(g_mediaState.positionSeconds), (int)infoX, (int)(progressY + 8), 12, SPOTIFY_SUBTLE);
        const char *durStr = FormatDuration(g_mediaState.durationSeconds);
        int durWidth = LlzMeasureText(durStr, 12);
        LlzDrawText(durStr, (int)(infoX + infoWidth - durWidth), (int)(progressY + 8), 12, SPOTIFY_SUBTLE);
    }

    // Playback controls
    float controlsY = progressY + 35;
    float controlSpacing = 70;
    float controlsStartX = offsetX + SCREEN_WIDTH / 2 - controlSpacing;

    // Shuffle
    Color shuffleColor = g_mediaState.shuffleEnabled ? SPOTIFY_GREEN : SPOTIFY_LIGHT_GRAY;
    LlzDrawTextCentered("S", (int)(controlsStartX - controlSpacing), (int)controlsY, 18, shuffleColor);

    // Previous
    Color prevColor = (g_npControlSelected == 0) ? SPOTIFY_WHITE : SPOTIFY_SUBTLE;
    LlzDrawTextCentered("<<", (int)controlsStartX, (int)controlsY, 24, prevColor);

    // Play/Pause
    Color playBg = (g_npControlSelected == 1) ? SPOTIFY_GREEN : SPOTIFY_WHITE;
    Color playFg = SPOTIFY_BLACK;
    DrawCircle((int)(controlsStartX + controlSpacing), (int)(controlsY + 8), 24, playBg);
    const char *playIcon = (g_mediaValid && g_mediaState.isPlaying) ? "||" : ">";
    LlzDrawTextCentered(playIcon, (int)(controlsStartX + controlSpacing), (int)(controlsY + 4), 20, playFg);

    // Next
    Color nextColor = (g_npControlSelected == 2) ? SPOTIFY_WHITE : SPOTIFY_SUBTLE;
    LlzDrawTextCentered(">>", (int)(controlsStartX + controlSpacing * 2), (int)controlsY, 24, nextColor);

    // Repeat
    Color repeatColor = (g_mediaState.repeatMode != LLZ_REPEAT_OFF) ? SPOTIFY_GREEN : SPOTIFY_LIGHT_GRAY;
    const char *repeatIcon = (g_mediaState.repeatMode == LLZ_REPEAT_TRACK) ? "R1" : "R";
    LlzDrawTextCentered(repeatIcon, (int)(controlsStartX + controlSpacing * 3), (int)controlsY, 18, repeatColor);

    // Volume overlay
    if (g_showingVolume) {
        float volY = controlsY + 40;
        LlzDrawTextCentered("Volume", (int)(offsetX + SCREEN_WIDTH / 2), (int)volY, 14, SPOTIFY_SUBTLE);
        DrawProgressBar(infoX + 50, volY + 18, infoWidth - 100, 6, g_mediaState.volumePercent / 100.0f, SPOTIFY_GRAY, SPOTIFY_GREEN);
        char volStr[16];
        snprintf(volStr, sizeof(volStr), "%d%%", g_mediaState.volumePercent);
        LlzDrawTextCentered(volStr, (int)(offsetX + SCREEN_WIDTH / 2), (int)(volY + 32), 14, SPOTIFY_WHITE);
    }
}

static void UpdateNowPlayingScreen(const LlzInputState *input, float dt) {
    // Volume adjustment with scroll
    if (input->scrollDelta != 0) {
        int volDelta = (input->scrollDelta > 0) ? 5 : -5;
        int newVol = g_mediaState.volumePercent + volDelta;
        if (newVol < 0) newVol = 0;
        if (newVol > 100) newVol = 100;
        if (newVol != g_mediaState.volumePercent) {
            LlzMediaSendCommand(LLZ_PLAYBACK_SET_VOLUME, newVol);
            g_mediaState.volumePercent = newVol;
        }
        g_showingVolume = true;
        g_volumeShowTimer = 2.0f;
    }

    // Hide volume after timeout
    if (g_showingVolume) {
        g_volumeShowTimer -= dt;
        if (g_volumeShowTimer <= 0) {
            g_showingVolume = false;
        }
    }

    // Control selection with up/down
    if (input->upPressed || input->downPressed) {
        // Navigate controls: prev, play, next
        if (input->downPressed) {
            g_npControlSelected = (g_npControlSelected + 1) % 3;
        } else {
            g_npControlSelected = (g_npControlSelected + 2) % 3;
        }
    }

    // Tap to toggle playback
    if (input->tap) {
        LlzMediaSendCommand(LLZ_PLAYBACK_TOGGLE, 0);
    }

    // Select to activate control
    if (input->selectPressed) {
        switch (g_npControlSelected) {
            case 0: LlzMediaSendCommand(LLZ_PLAYBACK_PREVIOUS, 0); break;
            case 1: LlzMediaSendCommand(LLZ_PLAYBACK_TOGGLE, 0); break;
            case 2: LlzMediaSendCommand(LLZ_PLAYBACK_NEXT, 0); break;
        }
    }
}

// ============================================================================
// Queue Screen
// ============================================================================

#define QUEUE_ITEM_HEIGHT 60
#define QUEUE_VISIBLE_ITEMS 5

static void DrawQueueScreen(float offsetX) {
    float contentY = HEADER_HEIGHT + 10;
    float listX = offsetX + PADDING;
    float listWidth = SCREEN_WIDTH - PADDING * 2;

    if (!g_queueValid) {
        LlzDrawTextCentered("Loading...", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2), 20, SPOTIFY_SUBTLE);
        return;
    }

    int totalItems = g_queueData.trackCount + (g_queueData.hasCurrentlyPlaying ? 1 : 0);
    if (totalItems == 0) {
        LlzDrawTextCentered("Queue is empty", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 15), 20, SPOTIFY_SUBTLE);
        LlzDrawTextCentered("Play something on Spotify", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 + 15), 16, SPOTIFY_LIGHT_GRAY);
        return;
    }

    // Draw items
    float yOffset = contentY - g_queueScrollOffset;
    int itemIndex = 0;

    // Currently playing
    if (g_queueData.hasCurrentlyPlaying) {
        if (yOffset > -QUEUE_ITEM_HEIGHT && yOffset < SCREEN_HEIGHT) {
            bool selected = (g_queueSelectedIndex == 0);
            Color bgColor = selected ? SPOTIFY_GREEN_DARK : SPOTIFY_GRAY;

            DrawRoundedCard(listX, yOffset, listWidth, QUEUE_ITEM_HEIGHT - 4, bgColor);

            // Now Playing badge
            DrawRectangleRounded((Rectangle){listX + 8, yOffset + 6, 50, 16}, 0.3f, 4, SPOTIFY_GREEN);
            LlzDrawText("NOW", (int)(listX + 14), (int)(yOffset + 8), 10, SPOTIFY_BLACK);

            // Track info
            DrawTruncatedText(g_queueData.currentlyPlaying.title, listX + 65, yOffset + 8, listWidth - 150, 16, SPOTIFY_WHITE);
            DrawTruncatedText(g_queueData.currentlyPlaying.artist, listX + 65, yOffset + 28, listWidth - 150, 14, SPOTIFY_SUBTLE);

            // Duration
            int durSec = (int)(g_queueData.currentlyPlaying.durationMs / 1000);
            const char *durStr = FormatDuration(durSec);
            int durWidth = LlzMeasureText(durStr, 12);
            LlzDrawText(durStr, (int)(listX + listWidth - durWidth - 12), (int)(yOffset + 22), 12, SPOTIFY_LIGHT_GRAY);

            if (selected) {
                DrawRectangle((int)listX, (int)yOffset, 3, QUEUE_ITEM_HEIGHT - 4, SPOTIFY_GREEN);
            }
        }
        yOffset += QUEUE_ITEM_HEIGHT + 8;
        itemIndex = 1;
    }

    // Queue tracks
    for (int i = 0; i < g_queueData.trackCount && yOffset < SCREEN_HEIGHT; i++) {
        if (yOffset > -QUEUE_ITEM_HEIGHT) {
            bool selected = (g_queueSelectedIndex == itemIndex);
            Color bgColor = selected ? SPOTIFY_GRAY : SPOTIFY_DARK;

            DrawRoundedCard(listX, yOffset, listWidth, QUEUE_ITEM_HEIGHT - 4, bgColor);

            // Track number
            char numStr[8];
            snprintf(numStr, sizeof(numStr), "%d", i + 1);
            LlzDrawText(numStr, (int)(listX + 12), (int)(yOffset + 20), 14, SPOTIFY_LIGHT_GRAY);

            // Track info
            DrawTruncatedText(g_queueData.tracks[i].title, listX + 40, yOffset + 8, listWidth - 120, 16, SPOTIFY_WHITE);
            DrawTruncatedText(g_queueData.tracks[i].artist, listX + 40, yOffset + 28, listWidth - 120, 14, SPOTIFY_SUBTLE);

            // Duration
            int durSec = (int)(g_queueData.tracks[i].durationMs / 1000);
            const char *durStr = FormatDuration(durSec);
            int durWidth = LlzMeasureText(durStr, 12);
            LlzDrawText(durStr, (int)(listX + listWidth - durWidth - 12), (int)(yOffset + 22), 12, SPOTIFY_LIGHT_GRAY);

            if (selected) {
                DrawRectangle((int)listX, (int)yOffset, 3, QUEUE_ITEM_HEIGHT - 4, SPOTIFY_GREEN);
            }
        }
        yOffset += QUEUE_ITEM_HEIGHT;
        itemIndex++;
    }
}

static void UpdateQueueScreen(const LlzInputState *input, float dt) {
    int totalItems = g_queueData.trackCount + (g_queueData.hasCurrentlyPlaying ? 1 : 0);

    // Navigation
    int delta = 0;
    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? -1 : 1;
    }
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    if (delta != 0 && totalItems > 0) {
        g_queueSelectedIndex += delta;
        if (g_queueSelectedIndex < 0) g_queueSelectedIndex = 0;
        if (g_queueSelectedIndex >= totalItems) g_queueSelectedIndex = totalItems - 1;

        // Update scroll
        float itemTop = g_queueSelectedIndex * QUEUE_ITEM_HEIGHT;
        float visibleHeight = CONTENT_HEIGHT - 20;

        if (itemTop < g_queueScrollOffset) {
            g_queueScrollOffset = itemTop;
        } else if (itemTop + QUEUE_ITEM_HEIGHT > g_queueScrollOffset + visibleHeight) {
            g_queueScrollOffset = itemTop + QUEUE_ITEM_HEIGHT - visibleHeight;
        }
        if (g_queueScrollOffset < 0) g_queueScrollOffset = 0;
    }

    // Select to skip to track
    if (input->selectPressed && g_queueValid && totalItems > 0) {
        int queueIndex = g_queueSelectedIndex;
        if (g_queueData.hasCurrentlyPlaying) queueIndex--;

        if (queueIndex >= 0) {
            LlzMediaQueueShift(queueIndex);
            g_queueValid = false;
            RefreshQueue();
        }
    }

    // Tap to refresh
    if (input->tap) {
        g_queueValid = false;
        RefreshQueue();
    }
}

// ============================================================================
// Library Data Management
// ============================================================================

static void RefreshLibraryData(void) {
    // Request overview
    LlzMediaRequestLibraryOverview();
}

static void PollLibraryData(float dt) {
    static float overviewTimer = 0;
    static float likedTimer = 0;
    static float albumsTimer = 0;
    static float playlistsTimer = 0;

    // Poll overview
    overviewTimer += dt;
    if (overviewTimer >= 1.0f) {
        overviewTimer = 0;
        LlzSpotifyLibraryOverview temp;
        if (LlzMediaGetLibraryOverview(&temp) && temp.valid) {
            memcpy(&g_libraryOverview, &temp, sizeof(temp));
            g_libraryOverviewValid = true;
        }
    }

    // Poll liked tracks when on that screen
    if (g_currentScreen == SCREEN_LIKED) {
        likedTimer += dt;
        if (likedTimer >= 0.5f) {
            likedTimer = 0;
            LlzSpotifyTrackListResponse temp;
            if (LlzMediaGetLibraryTracks("liked", &temp) && temp.valid) {
                memcpy(&g_likedTracks, &temp, sizeof(temp));
                g_likedTracksValid = true;
                g_likedRefreshing = false;
            }
        }
    }

    // Poll albums when on that screen
    if (g_currentScreen == SCREEN_ALBUMS) {
        albumsTimer += dt;
        if (albumsTimer >= 0.5f) {
            albumsTimer = 0;
            LlzSpotifyAlbumListResponse temp;
            if (LlzMediaGetLibraryAlbums(&temp) && temp.valid) {
                memcpy(&g_albums, &temp, sizeof(temp));
                g_albumsValid = true;
                g_albumsRefreshing = false;
            }
        }
    }

    // Poll playlists when on that screen
    if (g_currentScreen == SCREEN_PLAYLISTS) {
        playlistsTimer += dt;
        if (playlistsTimer >= 0.5f) {
            playlistsTimer = 0;
            LlzSpotifyPlaylistListResponse temp;
            if (LlzMediaGetLibraryPlaylists(&temp) && temp.valid) {
                memcpy(&g_playlists, &temp, sizeof(temp));
                g_playlistsValid = true;
                g_playlistsRefreshing = false;
            }
        }
    }
}

// ============================================================================
// Liked Songs Screen
// ============================================================================

static void DrawLikedScreen(float offsetX) {
    float contentY = HEADER_HEIGHT + 10;
    float listX = offsetX + PADDING;
    float listWidth = SCREEN_WIDTH - PADDING * 2;

    // Header with count
    if (g_libraryOverviewValid) {
        char countStr[64];
        snprintf(countStr, sizeof(countStr), "%d songs saved", g_libraryOverview.likedCount);
        LlzDrawText(countStr, (int)listX, (int)contentY, 14, SPOTIFY_SUBTLE);
        contentY += 25;
    }

    if (g_likedRefreshing && !g_likedTracksValid) {
        LlzDrawTextCentered("Loading...", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 40), 20, SPOTIFY_SUBTLE);
        return;
    }

    if (!g_likedTracksValid || g_likedTracks.itemCount == 0) {
        LlzDrawTextCentered("No liked songs", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 40), 20, SPOTIFY_SUBTLE);
        LlzDrawTextCentered("Like songs on Spotify to see them here", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 10), 14, SPOTIFY_LIGHT_GRAY);
        LlzDrawTextCentered("Tap to refresh", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 + 30), 14, SPOTIFY_LIGHT_GRAY);
        return;
    }

    // Draw tracks
    float yOffset = contentY - g_likedScrollOffset;

    for (int i = 0; i < g_likedTracks.itemCount && yOffset < SCREEN_HEIGHT - FOOTER_HEIGHT; i++) {
        if (yOffset > HEADER_HEIGHT - LIST_ITEM_HEIGHT) {
            bool selected = (g_likedSelectedIndex == i);
            Color bgColor = selected ? SPOTIFY_GREEN_DARK : SPOTIFY_DARK;

            DrawRoundedCard(listX, yOffset, listWidth, LIST_ITEM_HEIGHT - 4, bgColor);

            // Track number
            char numStr[8];
            snprintf(numStr, sizeof(numStr), "%d", i + 1);
            LlzDrawText(numStr, (int)(listX + 12), (int)(yOffset + 20), 14, SPOTIFY_LIGHT_GRAY);

            // Track info
            DrawTruncatedText(g_likedTracks.items[i].name, listX + 45, yOffset + 8, listWidth - 120, 16, SPOTIFY_WHITE);
            DrawTruncatedText(g_likedTracks.items[i].artist, listX + 45, yOffset + 28, listWidth - 120, 14, SPOTIFY_SUBTLE);

            // Duration
            int durSec = (int)(g_likedTracks.items[i].durationMs / 1000);
            const char *durStr = FormatDuration(durSec);
            int durWidth = LlzMeasureText(durStr, 12);
            LlzDrawText(durStr, (int)(listX + listWidth - durWidth - 12), (int)(yOffset + 22), 12, SPOTIFY_LIGHT_GRAY);

            if (selected) {
                DrawRectangle((int)listX, (int)yOffset, 3, LIST_ITEM_HEIGHT - 4, SPOTIFY_GREEN);
            }
        }
        yOffset += LIST_ITEM_HEIGHT;
    }

    // Pagination info
    if (g_likedTracks.hasMore) {
        char moreStr[32];
        snprintf(moreStr, sizeof(moreStr), "Showing %d of %d", g_likedTracks.itemCount, g_likedTracks.total);
        int moreWidth = LlzMeasureText(moreStr, 12);
        LlzDrawText(moreStr, (int)(offsetX + SCREEN_WIDTH / 2 - moreWidth / 2), (int)(SCREEN_HEIGHT - FOOTER_HEIGHT - 20), 12, SPOTIFY_LIGHT_GRAY);
    }
}

static void UpdateLikedScreen(const LlzInputState *input, float dt) {
    if (!g_likedTracksValid) return;

    int totalItems = g_likedTracks.itemCount;
    if (totalItems == 0) return;

    // Navigation
    int delta = 0;
    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? -1 : 1;
    }
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    if (delta != 0) {
        g_likedSelectedIndex += delta;
        if (g_likedSelectedIndex < 0) g_likedSelectedIndex = 0;
        if (g_likedSelectedIndex >= totalItems) g_likedSelectedIndex = totalItems - 1;

        // Update scroll
        float itemTop = g_likedSelectedIndex * LIST_ITEM_HEIGHT;
        float visibleHeight = CONTENT_HEIGHT - 45;

        if (itemTop < g_likedScrollOffset) {
            g_likedScrollOffset = itemTop;
        } else if (itemTop + LIST_ITEM_HEIGHT > g_likedScrollOffset + visibleHeight) {
            g_likedScrollOffset = itemTop + LIST_ITEM_HEIGHT - visibleHeight;
        }
        if (g_likedScrollOffset < 0) g_likedScrollOffset = 0;
    }

    // Select to play track
    if (input->selectPressed && totalItems > 0) {
        const char *uri = g_likedTracks.items[g_likedSelectedIndex].uri;
        if (uri[0] != '\0') {
            LlzMediaPlaySpotifyUri(uri);
        }
    }

    // Tap to refresh
    if (input->tap) {
        g_likedRefreshing = true;
        LlzMediaRequestLibraryLiked(0, 20);
    }
}

// ============================================================================
// Albums Screen
// ============================================================================

static void DrawAlbumsScreen(float offsetX) {
    float contentY = HEADER_HEIGHT + 10;
    float listX = offsetX + PADDING;
    float listWidth = SCREEN_WIDTH - PADDING * 2;

    // Header with count
    if (g_libraryOverviewValid) {
        char countStr[64];
        snprintf(countStr, sizeof(countStr), "%d albums saved", g_libraryOverview.albumsCount);
        LlzDrawText(countStr, (int)listX, (int)contentY, 14, SPOTIFY_SUBTLE);
        contentY += 25;
    }

    if (g_albumsRefreshing && !g_albumsValid) {
        LlzDrawTextCentered("Loading...", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 40), 20, SPOTIFY_SUBTLE);
        return;
    }

    if (!g_albumsValid || g_albums.itemCount == 0) {
        LlzDrawTextCentered("No saved albums", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 40), 20, SPOTIFY_SUBTLE);
        LlzDrawTextCentered("Save albums on Spotify to see them here", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 10), 14, SPOTIFY_LIGHT_GRAY);
        LlzDrawTextCentered("Tap to refresh", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 + 30), 14, SPOTIFY_LIGHT_GRAY);
        return;
    }

    // Draw albums
    float yOffset = contentY - g_albumsScrollOffset;

    for (int i = 0; i < g_albums.itemCount && yOffset < SCREEN_HEIGHT - FOOTER_HEIGHT; i++) {
        if (yOffset > HEADER_HEIGHT - LIST_ITEM_HEIGHT) {
            bool selected = (g_albumsSelectedIndex == i);
            Color bgColor = selected ? SPOTIFY_GREEN_DARK : SPOTIFY_DARK;

            DrawRoundedCard(listX, yOffset, listWidth, LIST_ITEM_HEIGHT - 4, bgColor);

            // Album icon placeholder
            DrawRectangle((int)(listX + 8), (int)(yOffset + 8), 40, 40, SPOTIFY_GRAY);
            LlzDrawText("[A]", (int)(listX + 18), (int)(yOffset + 20), 12, SPOTIFY_LIGHT_GRAY);

            // Album info
            DrawTruncatedText(g_albums.items[i].name, listX + 58, yOffset + 8, listWidth - 140, 16, SPOTIFY_WHITE);
            DrawTruncatedText(g_albums.items[i].artist, listX + 58, yOffset + 28, listWidth - 140, 14, SPOTIFY_SUBTLE);

            // Track count and year
            char infoStr[32];
            if (g_albums.items[i].year[0]) {
                snprintf(infoStr, sizeof(infoStr), "%d tracks - %s", g_albums.items[i].trackCount, g_albums.items[i].year);
            } else {
                snprintf(infoStr, sizeof(infoStr), "%d tracks", g_albums.items[i].trackCount);
            }
            int infoWidth = LlzMeasureText(infoStr, 12);
            LlzDrawText(infoStr, (int)(listX + listWidth - infoWidth - 12), (int)(yOffset + 22), 12, SPOTIFY_LIGHT_GRAY);

            if (selected) {
                DrawRectangle((int)listX, (int)yOffset, 3, LIST_ITEM_HEIGHT - 4, SPOTIFY_GREEN);
            }
        }
        yOffset += LIST_ITEM_HEIGHT;
    }

    // Pagination info
    if (g_albums.hasMore) {
        char moreStr[32];
        snprintf(moreStr, sizeof(moreStr), "Showing %d of %d", g_albums.itemCount, g_albums.total);
        int moreWidth = LlzMeasureText(moreStr, 12);
        LlzDrawText(moreStr, (int)(offsetX + SCREEN_WIDTH / 2 - moreWidth / 2), (int)(SCREEN_HEIGHT - FOOTER_HEIGHT - 20), 12, SPOTIFY_LIGHT_GRAY);
    }
}

static void UpdateAlbumsScreen(const LlzInputState *input, float dt) {
    if (!g_albumsValid) return;

    int totalItems = g_albums.itemCount;
    if (totalItems == 0) return;

    // Navigation
    int delta = 0;
    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? -1 : 1;
    }
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    if (delta != 0) {
        g_albumsSelectedIndex += delta;
        if (g_albumsSelectedIndex < 0) g_albumsSelectedIndex = 0;
        if (g_albumsSelectedIndex >= totalItems) g_albumsSelectedIndex = totalItems - 1;

        // Update scroll
        float itemTop = g_albumsSelectedIndex * LIST_ITEM_HEIGHT;
        float visibleHeight = CONTENT_HEIGHT - 45;

        if (itemTop < g_albumsScrollOffset) {
            g_albumsScrollOffset = itemTop;
        } else if (itemTop + LIST_ITEM_HEIGHT > g_albumsScrollOffset + visibleHeight) {
            g_albumsScrollOffset = itemTop + LIST_ITEM_HEIGHT - visibleHeight;
        }
        if (g_albumsScrollOffset < 0) g_albumsScrollOffset = 0;
    }

    // Select to play album
    if (input->selectPressed && totalItems > 0) {
        const char *uri = g_albums.items[g_albumsSelectedIndex].uri;
        if (uri[0] != '\0') {
            LlzMediaPlaySpotifyUri(uri);
        }
    }

    // Tap to refresh
    if (input->tap) {
        g_albumsRefreshing = true;
        LlzMediaRequestLibraryAlbums(0, 20);
    }
}

// ============================================================================
// Playlists Screen
// ============================================================================

static void DrawPlaylistsScreen(float offsetX) {
    float contentY = HEADER_HEIGHT + 10;
    float listX = offsetX + PADDING;
    float listWidth = SCREEN_WIDTH - PADDING * 2;

    // Header with count
    if (g_libraryOverviewValid) {
        char countStr[64];
        snprintf(countStr, sizeof(countStr), "%d playlists", g_libraryOverview.playlistsCount);
        LlzDrawText(countStr, (int)listX, (int)contentY, 14, SPOTIFY_SUBTLE);
        contentY += 25;
    }

    if (g_playlistsRefreshing && !g_playlistsValid) {
        LlzDrawTextCentered("Loading...", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 40), 20, SPOTIFY_SUBTLE);
        return;
    }

    if (!g_playlistsValid || g_playlists.itemCount == 0) {
        LlzDrawTextCentered("No playlists", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 40), 20, SPOTIFY_SUBTLE);
        LlzDrawTextCentered("Create playlists on Spotify to see them here", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 - 10), 14, SPOTIFY_LIGHT_GRAY);
        LlzDrawTextCentered("Tap to refresh", (int)(offsetX + SCREEN_WIDTH / 2), (int)(contentY + CONTENT_HEIGHT / 2 + 30), 14, SPOTIFY_LIGHT_GRAY);
        return;
    }

    // Draw playlists
    float yOffset = contentY - g_playlistsScrollOffset;

    for (int i = 0; i < g_playlists.itemCount && yOffset < SCREEN_HEIGHT - FOOTER_HEIGHT; i++) {
        if (yOffset > HEADER_HEIGHT - LIST_ITEM_HEIGHT) {
            bool selected = (g_playlistsSelectedIndex == i);
            Color bgColor = selected ? SPOTIFY_GREEN_DARK : SPOTIFY_DARK;

            DrawRoundedCard(listX, yOffset, listWidth, LIST_ITEM_HEIGHT - 4, bgColor);

            // Playlist icon placeholder
            DrawRectangle((int)(listX + 8), (int)(yOffset + 8), 40, 40, SPOTIFY_GRAY);
            LlzDrawText("[P]", (int)(listX + 18), (int)(yOffset + 20), 12, SPOTIFY_LIGHT_GRAY);

            // Playlist info
            DrawTruncatedText(g_playlists.items[i].name, listX + 58, yOffset + 8, listWidth - 140, 16, SPOTIFY_WHITE);

            // Owner and track count
            char ownerStr[128];
            if (g_playlists.items[i].owner[0]) {
                snprintf(ownerStr, sizeof(ownerStr), "by %s", g_playlists.items[i].owner);
            } else {
                ownerStr[0] = '\0';
            }
            DrawTruncatedText(ownerStr, listX + 58, yOffset + 28, listWidth - 140, 14, SPOTIFY_SUBTLE);

            // Track count
            char countStr[16];
            snprintf(countStr, sizeof(countStr), "%d", g_playlists.items[i].trackCount);
            int countWidth = LlzMeasureText(countStr, 12);
            LlzDrawText(countStr, (int)(listX + listWidth - countWidth - 12), (int)(yOffset + 22), 12, SPOTIFY_LIGHT_GRAY);

            if (selected) {
                DrawRectangle((int)listX, (int)yOffset, 3, LIST_ITEM_HEIGHT - 4, SPOTIFY_GREEN);
            }
        }
        yOffset += LIST_ITEM_HEIGHT;
    }

    // Pagination info
    if (g_playlists.hasMore) {
        char moreStr[32];
        snprintf(moreStr, sizeof(moreStr), "Showing %d of %d", g_playlists.itemCount, g_playlists.total);
        int moreWidth = LlzMeasureText(moreStr, 12);
        LlzDrawText(moreStr, (int)(offsetX + SCREEN_WIDTH / 2 - moreWidth / 2), (int)(SCREEN_HEIGHT - FOOTER_HEIGHT - 20), 12, SPOTIFY_LIGHT_GRAY);
    }
}

static void UpdatePlaylistsScreen(const LlzInputState *input, float dt) {
    if (!g_playlistsValid) return;

    int totalItems = g_playlists.itemCount;
    if (totalItems == 0) return;

    // Navigation
    int delta = 0;
    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? -1 : 1;
    }
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    if (delta != 0) {
        g_playlistsSelectedIndex += delta;
        if (g_playlistsSelectedIndex < 0) g_playlistsSelectedIndex = 0;
        if (g_playlistsSelectedIndex >= totalItems) g_playlistsSelectedIndex = totalItems - 1;

        // Update scroll
        float itemTop = g_playlistsSelectedIndex * LIST_ITEM_HEIGHT;
        float visibleHeight = CONTENT_HEIGHT - 45;

        if (itemTop < g_playlistsScrollOffset) {
            g_playlistsScrollOffset = itemTop;
        } else if (itemTop + LIST_ITEM_HEIGHT > g_playlistsScrollOffset + visibleHeight) {
            g_playlistsScrollOffset = itemTop + LIST_ITEM_HEIGHT - visibleHeight;
        }
        if (g_playlistsScrollOffset < 0) g_playlistsScrollOffset = 0;
    }

    // Select to play playlist
    if (input->selectPressed && totalItems > 0) {
        const char *uri = g_playlists.items[g_playlistsSelectedIndex].uri;
        if (uri[0] != '\0') {
            LlzMediaPlaySpotifyUri(uri);
        }
    }

    // Tap to refresh
    if (input->tap) {
        g_playlistsRefreshing = true;
        LlzMediaRequestLibraryPlaylists(0, 20);
    }
}

// ============================================================================
// Screen Management
// ============================================================================

static void SwitchScreen(SpotifyScreen target) {
    if (target < 0 || target >= SCREEN_COUNT) return;
    if (target == g_currentScreen) return;

    g_fromScreen = g_currentScreen;
    g_currentScreen = target;

    // Calculate transition direction
    int direction = (target > g_fromScreen) ? 1 : -1;
    g_screenOffset = -direction * SCREEN_WIDTH;
    g_targetScreenOffset = 0;

    // Reset screen-specific state and request data
    switch (target) {
        case SCREEN_NOW_PLAYING:
            g_npControlSelected = 1;
            break;
        case SCREEN_QUEUE:
            g_queueSelectedIndex = 0;
            g_queueScrollOffset = 0;
            RefreshQueue();
            break;
        case SCREEN_LIKED:
            g_likedSelectedIndex = 0;
            g_likedScrollOffset = 0;
            if (!g_likedTracksValid) {
                g_likedRefreshing = true;
                LlzMediaRequestLibraryLiked(0, 20);
            }
            break;
        case SCREEN_ALBUMS:
            g_albumsSelectedIndex = 0;
            g_albumsScrollOffset = 0;
            if (!g_albumsValid) {
                g_albumsRefreshing = true;
                LlzMediaRequestLibraryAlbums(0, 20);
            }
            break;
        case SCREEN_PLAYLISTS:
            g_playlistsSelectedIndex = 0;
            g_playlistsScrollOffset = 0;
            if (!g_playlistsValid) {
                g_playlistsRefreshing = true;
                LlzMediaRequestLibraryPlaylists(0, 20);
            }
            break;
        default:
            break;
    }
}

// ============================================================================
// Plugin Callbacks
// ============================================================================

static void plugin_init(int width, int height) {
    g_wantsClose = false;
    g_currentScreen = SCREEN_NOW_PLAYING;
    g_animTimer = 0;
    g_screenOffset = 0;
    g_targetScreenOffset = 0;

    memset(&g_mediaState, 0, sizeof(g_mediaState));
    g_mediaValid = false;
    g_mediaRefreshTimer = 0;

    memset(&g_queueData, 0, sizeof(g_queueData));
    g_queueValid = false;
    g_queueSelectedIndex = 0;
    g_queueScrollOffset = 0;

    g_albumArtValid = false;
    memset(g_currentArtHash, 0, sizeof(g_currentArtHash));

    g_spotifyConnected = false;
    g_npControlSelected = 1;
    g_showingVolume = false;

    // Reset library state
    memset(&g_libraryOverview, 0, sizeof(g_libraryOverview));
    g_libraryOverviewValid = false;

    memset(&g_likedTracks, 0, sizeof(g_likedTracks));
    g_likedTracksValid = false;
    g_likedSelectedIndex = 0;
    g_likedScrollOffset = 0;
    g_likedRefreshing = false;

    memset(&g_albums, 0, sizeof(g_albums));
    g_albumsValid = false;
    g_albumsSelectedIndex = 0;
    g_albumsScrollOffset = 0;
    g_albumsRefreshing = false;

    memset(&g_playlists, 0, sizeof(g_playlists));
    g_playlistsValid = false;
    g_playlistsSelectedIndex = 0;
    g_playlistsScrollOffset = 0;
    g_playlistsRefreshing = false;

    LlzMediaInit(NULL);
    LlzConnectionsInit(NULL);
    RefreshQueue();
    RefreshLibraryData();
    LlzMediaRequestSpotifyState();
}

static void plugin_update(const LlzInputState *input, float deltaTime) {
    g_animTimer += deltaTime;

    // Update media state
    g_mediaRefreshTimer += deltaTime;
    if (g_mediaRefreshTimer >= 0.25f) {
        g_mediaRefreshTimer = 0;
        if (LlzMediaGetState(&g_mediaState)) {
            g_mediaValid = true;
        }
    }

    // Update connection status
    LlzConnectionsUpdate(deltaTime);
    g_spotifyConnected = LlzConnectionsIsConnected(LLZ_SERVICE_SPOTIFY);

    // Load album art
    LoadAlbumArt();

    // Poll queue
    PollQueue(deltaTime);

    // Poll library data
    PollLibraryData(deltaTime);

    // Handle back button
    if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
        g_wantsClose = true;
        return;
    }

    // Screen transition animation
    if (g_screenOffset != g_targetScreenOffset) {
        float diff = g_targetScreenOffset - g_screenOffset;
        g_screenOffset += diff * 10.0f * deltaTime;
        if (fabsf(diff) < 1.0f) {
            g_screenOffset = g_targetScreenOffset;
        }
    }

    // Swipe navigation between screens
    if (input->swipeLeft) {
        if (g_currentScreen < SCREEN_COUNT - 1) {
            SwitchScreen((SpotifyScreen)(g_currentScreen + 1));
        }
    }
    if (input->swipeRight) {
        if (g_currentScreen > 0) {
            SwitchScreen((SpotifyScreen)(g_currentScreen - 1));
        }
    }

    // Update current screen
    switch (g_currentScreen) {
        case SCREEN_NOW_PLAYING:
            UpdateNowPlayingScreen(input, deltaTime);
            break;
        case SCREEN_QUEUE:
            UpdateQueueScreen(input, deltaTime);
            break;
        case SCREEN_LIKED:
            UpdateLikedScreen(input, deltaTime);
            break;
        case SCREEN_ALBUMS:
            UpdateAlbumsScreen(input, deltaTime);
            break;
        case SCREEN_PLAYLISTS:
            UpdatePlaylistsScreen(input, deltaTime);
            break;
        default:
            break;
    }
}

static void plugin_draw(void) {
    ClearBackground(SPOTIFY_BLACK);

    // Draw current screen with offset for transitions
    float offset = g_screenOffset;

    switch (g_currentScreen) {
        case SCREEN_NOW_PLAYING:
            DrawNowPlayingScreen(offset);
            break;
        case SCREEN_QUEUE:
            DrawQueueScreen(offset);
            break;
        case SCREEN_LIKED:
            DrawLikedScreen(offset);
            break;
        case SCREEN_ALBUMS:
            DrawAlbumsScreen(offset);
            break;
        case SCREEN_PLAYLISTS:
            DrawPlaylistsScreen(offset);
            break;
        default:
            break;
    }

    // Always draw header and footer on top
    DrawHeader();
    DrawFooter();
}

static void plugin_shutdown(void) {
    if (g_albumArtValid) {
        UnloadTexture(g_albumArtTexture);
        g_albumArtValid = false;
    }
}

static bool plugin_wants_close(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin API Export
// ============================================================================

static const LlzPluginAPI g_spotifyPluginAPI = {
    .name = "Spotify",
    .description = "Browse and control your Spotify library",
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
    return &g_spotifyPluginAPI;
}
