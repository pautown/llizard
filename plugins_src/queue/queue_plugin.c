/**
 * Queue Plugin for llizardgui-host
 *
 * Displays the current Spotify playback queue and allows skipping to
 * any track in the queue.
 *
 * Features:
 * - Shows currently playing track
 * - Shows upcoming tracks in queue
 * - Select a track to skip to it
 * - Automatic queue refresh
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
#define HEADER_HEIGHT 80
#define ITEM_HEIGHT 72
#define ITEM_SPACING 8
#define ITEMS_PER_PAGE 5
#define PADDING 32
#define LIST_TOP 100

// ============================================================================
// Color Palette (matching other plugins)
// ============================================================================

static const Color COLOR_BG_DARK = {18, 18, 22, 255};
static const Color COLOR_BG_GRADIENT = {28, 24, 38, 255};
static const Color COLOR_ACCENT = {30, 215, 96, 255};        // Spotify green
static const Color COLOR_ACCENT_DIM = {20, 140, 65, 255};
static const Color COLOR_TEXT_PRIMARY = {245, 245, 250, 255};
static const Color COLOR_TEXT_SECONDARY = {160, 160, 175, 255};
static const Color COLOR_TEXT_DIM = {100, 100, 115, 255};
static const Color COLOR_CARD_BG = {32, 30, 42, 255};
static const Color COLOR_CARD_SELECTED = {48, 42, 68, 255};
static const Color COLOR_CARD_BORDER = {60, 55, 80, 255};
static const Color COLOR_NOW_PLAYING_BG = {40, 60, 45, 255};

// ============================================================================
// Plugin State
// ============================================================================

static bool g_wantsClose = false;
static int g_highlightedItem = 0;       // -1 = now playing, 0+ = queue items
static float g_highlightPulse = 0.0f;

// Smooth scroll state
static float g_smoothScrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;

// Queue data
static LlzQueueData g_queueData;
static bool g_queueValid = false;
static bool g_queueRequested = false;
static float g_refreshTimer = 0.0f;
static const float REFRESH_INTERVAL = 0.5f;
static const float AUTO_REFRESH_INTERVAL = 10.0f;  // Auto-refresh queue every 10 seconds
static float g_autoRefreshTimer = 0.0f;

// Font
static Font g_queueFont;
static bool g_fontLoaded = false;

// Loading state
static bool g_isLoading = false;
static float g_loadingTimer = 0.0f;

// ============================================================================
// Forward Declarations
// ============================================================================

static void RequestQueue(void);
static void DrawLoadingSpinner(float x, float y, float radius);

// ============================================================================
// Font Loading
// ============================================================================

static int *BuildUnicodeCodepoints(int *outCount) {
    static const int ranges[][2] = {
        {0x0020, 0x007E},   // ASCII
        {0x00A0, 0x00FF},   // Latin-1 Supplement
        {0x0100, 0x017F},   // Latin Extended-A
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

static void LoadQueueFont(void) {
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    g_queueFont = LlzFontLoadCustom(LLZ_FONT_UI, 48, codepoints, codepointCount);

    if (g_queueFont.texture.id != 0) {
        g_fontLoaded = true;
        SetTextureFilter(g_queueFont.texture, TEXTURE_FILTER_BILINEAR);
    } else {
        g_queueFont = LlzFontGet(LLZ_FONT_UI, 48);
        g_fontLoaded = false;
    }

    if (codepoints) free(codepoints);
}

static void UnloadQueueFont(void) {
    if (g_fontLoaded && g_queueFont.texture.id != 0) {
        UnloadFont(g_queueFont);
    }
    g_fontLoaded = false;
}

// ============================================================================
// Smooth Scroll
// ============================================================================

static void UpdateSmoothScroll(float deltaTime) {
    float diff = g_targetScrollOffset - g_smoothScrollOffset;
    float speed = 12.0f;
    g_smoothScrollOffset += diff * speed * deltaTime;
    if (fabsf(diff) < 0.5f) {
        g_smoothScrollOffset = g_targetScrollOffset;
    }
}

static float CalculateTargetScroll(int selected, int totalItems, int visibleItems) {
    if (totalItems <= visibleItems) return 0.0f;

    float itemTotalHeight = ITEM_HEIGHT + ITEM_SPACING;
    float totalListHeight = totalItems * itemTotalHeight;
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 40;
    float maxScroll = totalListHeight - visibleArea;
    if (maxScroll < 0) maxScroll = 0;

    float selectedTop = selected * itemTotalHeight;
    float selectedBottom = selectedTop + ITEM_HEIGHT;

    float visibleTop = g_targetScrollOffset;
    float visibleBottom = g_targetScrollOffset + visibleArea;

    float topMargin = ITEM_HEIGHT * 0.5f;
    float bottomMargin = ITEM_HEIGHT * 1.2f;

    float newTarget = g_targetScrollOffset;

    if (selectedTop < visibleTop + topMargin) {
        newTarget = selectedTop - topMargin;
    } else if (selectedBottom > visibleBottom - bottomMargin) {
        newTarget = selectedBottom - visibleArea + bottomMargin;
    }

    if (newTarget < 0) newTarget = 0;
    if (newTarget > maxScroll) newTarget = maxScroll;

    return newTarget;
}

// ============================================================================
// Queue Management
// ============================================================================

static void RequestQueue(void) {
    if (!g_queueRequested) {
        printf("[QUEUE] Requesting playback queue\n");
        LlzMediaRequestQueue();
        g_queueRequested = true;
        g_isLoading = true;
        g_loadingTimer = 0.0f;
    }
}

static void PollQueueData(float deltaTime) {
    g_refreshTimer += deltaTime;
    g_autoRefreshTimer += deltaTime;

    // Auto-refresh queue periodically
    if (g_autoRefreshTimer >= AUTO_REFRESH_INTERVAL) {
        g_autoRefreshTimer = 0.0f;
        g_queueRequested = false;  // Allow new request
        RequestQueue();
    }

    // Poll for data
    if (g_refreshTimer >= REFRESH_INTERVAL) {
        g_refreshTimer = 0.0f;

        if (g_queueRequested && !g_queueValid) {
            LlzQueueData tempQueue;
            if (LlzMediaGetQueue(&tempQueue)) {
                memcpy(&g_queueData, &tempQueue, sizeof(LlzQueueData));
                g_queueValid = true;
                g_isLoading = false;
                printf("[QUEUE] Loaded queue: %d tracks, currently playing: %s\n",
                       g_queueData.trackCount,
                       g_queueData.hasCurrentlyPlaying ? g_queueData.currentlyPlaying.title : "(none)");
            }
        }
    }

    // Update loading timer
    if (g_isLoading) {
        g_loadingTimer += deltaTime;
        // Timeout after 5 seconds
        if (g_loadingTimer > 5.0f) {
            g_isLoading = false;
            g_queueRequested = false;
        }
    }
}

static void SkipToQueuePosition(int index) {
    printf("[QUEUE] Skipping to queue position: %d\n", index);
    LlzMediaQueueShift(index);

    // Refresh queue after skip
    g_queueValid = false;
    g_queueRequested = false;
    g_autoRefreshTimer = 0.0f;

    // Small delay then request new queue
    g_refreshTimer = REFRESH_INTERVAL - 0.3f;
}

// ============================================================================
// Drawing Helpers
// ============================================================================

static void DrawLoadingSpinner(float x, float y, float radius) {
    int segments = 8;
    for (int i = 0; i < segments; i++) {
        float angle = (float)i / segments * 2.0f * PI + g_loadingTimer * 4.0f;
        float alpha = (float)(segments - i) / segments;
        Color c = ColorAlpha(COLOR_ACCENT, alpha);

        float px = x + cosf(angle) * radius;
        float py = y + sinf(angle) * radius;
        DrawCircle((int)px, (int)py, 4.0f, c);
    }
}

static void DrawTruncatedText(Font font, const char *text, float x, float y,
                              float maxWidth, float fontSize, Color color) {
    Vector2 size = MeasureTextEx(font, text, fontSize, 1);
    if (size.x <= maxWidth) {
        DrawTextEx(font, text, (Vector2){x, y}, fontSize, 1, color);
        return;
    }

    // Truncate with ellipsis
    char truncated[256];
    int len = strlen(text);
    for (int i = len; i > 0; i--) {
        strncpy(truncated, text, i);
        truncated[i] = '\0';
        strcat(truncated, "...");
        size = MeasureTextEx(font, truncated, fontSize, 1);
        if (size.x <= maxWidth) {
            DrawTextEx(font, truncated, (Vector2){x, y}, fontSize, 1, color);
            return;
        }
    }
    DrawTextEx(font, "...", (Vector2){x, y}, fontSize, 1, color);
}

static void DrawQueueItem(int index, const LlzQueueTrack *track, float yPos,
                          bool isSelected, bool isNowPlaying) {
    float x = PADDING;
    float width = SCREEN_WIDTH - PADDING * 2;

    // Background
    Color bgColor = isNowPlaying ? COLOR_NOW_PLAYING_BG :
                    (isSelected ? COLOR_CARD_SELECTED : COLOR_CARD_BG);

    // Pulse effect for selected item
    if (isSelected) {
        float pulse = (sinf(g_highlightPulse * 3.0f) + 1.0f) * 0.5f;
        bgColor = ColorAlpha(bgColor, 0.9f + pulse * 0.1f);
    }

    DrawRectangleRounded((Rectangle){x, yPos, width, ITEM_HEIGHT}, 0.1f, 8, bgColor);

    // Selection indicator
    if (isSelected) {
        DrawRectangleRounded((Rectangle){x, yPos, 4, ITEM_HEIGHT}, 0.5f, 4, COLOR_ACCENT);
    }

    // Index or "Now Playing" indicator
    float textX = x + 20;
    float textY = yPos + 8;

    if (isNowPlaying) {
        // Draw play icon or "NOW" label
        DrawTextEx(g_queueFont, "NOW", (Vector2){textX, textY + 18}, 16, 1, COLOR_ACCENT);
    } else {
        // Draw queue position
        char indexStr[8];
        snprintf(indexStr, sizeof(indexStr), "%d", index + 1);
        DrawTextEx(g_queueFont, indexStr, (Vector2){textX, textY + 18}, 20, 1, COLOR_TEXT_DIM);
    }

    // Track info
    float infoX = textX + 50;
    float maxTextWidth = width - 90;

    // Title
    DrawTruncatedText(g_queueFont, track->title, infoX, textY + 4,
                      maxTextWidth, 22, COLOR_TEXT_PRIMARY);

    // Artist
    DrawTruncatedText(g_queueFont, track->artist, infoX, textY + 30,
                      maxTextWidth, 18, COLOR_TEXT_SECONDARY);

    // Duration
    int durationSec = (int)(track->durationMs / 1000);
    int minutes = durationSec / 60;
    int seconds = durationSec % 60;
    char durationStr[16];
    snprintf(durationStr, sizeof(durationStr), "%d:%02d", minutes, seconds);
    Vector2 durSize = MeasureTextEx(g_queueFont, durationStr, 16, 1);
    DrawTextEx(g_queueFont, durationStr, (Vector2){x + width - durSize.x - 16, textY + 26},
               16, 1, COLOR_TEXT_DIM);
}

// ============================================================================
// Main Drawing
// ============================================================================

static void DrawHeader(void) {
    // Background gradient at top
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, HEADER_HEIGHT,
                          COLOR_BG_GRADIENT, COLOR_BG_DARK);

    // Title
    const char *title = "Queue";
    Vector2 titleSize = MeasureTextEx(g_queueFont, title, 32, 1);
    DrawTextEx(g_queueFont, title, (Vector2){(SCREEN_WIDTH - titleSize.x) / 2, 24},
               32, 1, COLOR_TEXT_PRIMARY);

    // Subtitle with service info
    if (g_queueValid && g_queueData.service[0] != '\0') {
        char subtitle[64];
        snprintf(subtitle, sizeof(subtitle), "%s - %d tracks",
                 g_queueData.service, g_queueData.trackCount);
        Vector2 subSize = MeasureTextEx(g_queueFont, subtitle, 16, 1);
        DrawTextEx(g_queueFont, subtitle, (Vector2){(SCREEN_WIDTH - subSize.x) / 2, 58},
                   16, 1, COLOR_TEXT_SECONDARY);
    }
}

static void DrawQueueList(void) {
    if (!g_queueValid) {
        // Show loading state
        if (g_isLoading) {
            DrawLoadingSpinner(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 30);
            const char *loadMsg = "Loading queue...";
            Vector2 msgSize = MeasureTextEx(g_queueFont, loadMsg, 20, 1);
            DrawTextEx(g_queueFont, loadMsg,
                      (Vector2){(SCREEN_WIDTH - msgSize.x) / 2, SCREEN_HEIGHT / 2 + 50},
                      20, 1, COLOR_TEXT_SECONDARY);
        } else {
            // Show empty state
            const char *emptyMsg = "No queue available";
            Vector2 msgSize = MeasureTextEx(g_queueFont, emptyMsg, 24, 1);
            DrawTextEx(g_queueFont, emptyMsg,
                      (Vector2){(SCREEN_WIDTH - msgSize.x) / 2, SCREEN_HEIGHT / 2 - 12},
                      24, 1, COLOR_TEXT_DIM);

            const char *hintMsg = "Play music on Spotify to see queue";
            Vector2 hintSize = MeasureTextEx(g_queueFont, hintMsg, 16, 1);
            DrawTextEx(g_queueFont, hintMsg,
                      (Vector2){(SCREEN_WIDTH - hintSize.x) / 2, SCREEN_HEIGHT / 2 + 20},
                      16, 1, COLOR_TEXT_DIM);
        }
        return;
    }

    if (g_queueData.trackCount == 0 && !g_queueData.hasCurrentlyPlaying) {
        const char *emptyMsg = "Queue is empty";
        Vector2 msgSize = MeasureTextEx(g_queueFont, emptyMsg, 24, 1);
        DrawTextEx(g_queueFont, emptyMsg,
                  (Vector2){(SCREEN_WIDTH - msgSize.x) / 2, SCREEN_HEIGHT / 2 - 12},
                  24, 1, COLOR_TEXT_DIM);
        return;
    }

    // Calculate total items (now playing + queue)
    int totalItems = g_queueData.trackCount;
    if (g_queueData.hasCurrentlyPlaying) totalItems++;

    // Draw items with scroll offset
    float yOffset = LIST_TOP - g_smoothScrollOffset;

    // Draw "Now Playing" section if available
    int itemIndex = 0;
    if (g_queueData.hasCurrentlyPlaying) {
        if (yOffset > -ITEM_HEIGHT && yOffset < SCREEN_HEIGHT) {
            DrawQueueItem(-1, &g_queueData.currentlyPlaying, yOffset,
                         g_highlightedItem == 0, true);
        }
        yOffset += ITEM_HEIGHT + ITEM_SPACING + 20;  // Extra spacing after now playing
        itemIndex = 1;
    }

    // "Up Next" label
    if (g_queueData.trackCount > 0) {
        if (yOffset > -20 && yOffset < SCREEN_HEIGHT) {
            DrawTextEx(g_queueFont, "Up Next", (Vector2){PADDING, yOffset - 24},
                       18, 1, COLOR_TEXT_DIM);
        }
    }

    // Draw queue items
    for (int i = 0; i < g_queueData.trackCount; i++) {
        if (yOffset > -ITEM_HEIGHT && yOffset < SCREEN_HEIGHT) {
            bool isSelected = (g_highlightedItem == itemIndex);
            DrawQueueItem(i, &g_queueData.tracks[i], yOffset, isSelected, false);
        }
        yOffset += ITEM_HEIGHT + ITEM_SPACING;
        itemIndex++;
    }
}

static void DrawFooter(void) {
    // Hint text at bottom
    float footerY = SCREEN_HEIGHT - 40;
    const char *hint = "Select to skip  |  Back to exit";
    Vector2 hintSize = MeasureTextEx(g_queueFont, hint, 14, 1);
    DrawTextEx(g_queueFont, hint,
              (Vector2){(SCREEN_WIDTH - hintSize.x) / 2, footerY},
              14, 1, COLOR_TEXT_DIM);
}

// ============================================================================
// Plugin Callbacks
// ============================================================================

static void plugin_init(int width, int height) {
    printf("[QUEUE] Initializing queue plugin\n");

    g_wantsClose = false;
    g_highlightedItem = 0;
    g_highlightPulse = 0.0f;
    g_smoothScrollOffset = 0.0f;
    g_targetScrollOffset = 0.0f;

    // Reset queue state
    memset(&g_queueData, 0, sizeof(g_queueData));
    g_queueValid = false;
    g_queueRequested = false;
    g_refreshTimer = 0.0f;
    g_autoRefreshTimer = 0.0f;
    g_isLoading = false;
    g_loadingTimer = 0.0f;

    // Initialize media SDK
    LlzMediaInit(NULL);

    // Load font
    LoadQueueFont();

    // Request initial queue data
    RequestQueue();
}

static void plugin_update(const LlzInputState *input, float deltaTime) {
    g_highlightPulse += deltaTime;

    // Poll for queue data
    PollQueueData(deltaTime);

    // Update smooth scroll
    UpdateSmoothScroll(deltaTime);

    // Calculate total items
    int totalItems = g_queueValid ? g_queueData.trackCount : 0;
    if (g_queueValid && g_queueData.hasCurrentlyPlaying) totalItems++;

    // Handle back button
    if (input->backPressed) {
        g_wantsClose = true;
        return;
    }

    // Handle selection
    if (input->selectPressed && g_queueValid && totalItems > 0) {
        // Skip to selected track in queue
        // If now playing is selected (item 0 with hasCurrentlyPlaying), do nothing
        // Otherwise, skip to queue index
        if (g_queueData.hasCurrentlyPlaying) {
            if (g_highlightedItem > 0) {
                int queueIndex = g_highlightedItem - 1;
                SkipToQueuePosition(queueIndex);
            }
        } else if (g_highlightedItem >= 0 && g_highlightedItem < g_queueData.trackCount) {
            SkipToQueuePosition(g_highlightedItem);
        }
    }

    // Handle navigation
    if (totalItems > 0) {
        int delta = 0;
        if (input->scrollDelta != 0) {
            delta = (input->scrollDelta > 0) ? -1 : 1;
        }
        if (input->downPressed) delta = 1;
        if (input->upPressed) delta = -1;

        if (delta != 0) {
            g_highlightedItem += delta;
            if (g_highlightedItem < 0) g_highlightedItem = 0;
            if (g_highlightedItem >= totalItems) g_highlightedItem = totalItems - 1;

            g_targetScrollOffset = CalculateTargetScroll(g_highlightedItem, totalItems, ITEMS_PER_PAGE);
        }
    }

    // Manual refresh on tap (if queue is loaded)
    if (input->tap && g_queueValid && !g_isLoading) {
        g_queueValid = false;
        g_queueRequested = false;
        RequestQueue();
    }
}

static void plugin_draw(void) {
    // Background
    ClearBackground(COLOR_BG_DARK);
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                          COLOR_BG_DARK, COLOR_BG_GRADIENT);

    DrawQueueList();
    DrawHeader();
    DrawFooter();
}

static void plugin_shutdown(void) {
    printf("[QUEUE] Shutting down queue plugin\n");
    UnloadQueueFont();
}

static bool plugin_wants_close(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin API Export
// ============================================================================

static const LlzPluginAPI g_queuePluginAPI = {
    .name = "Queue",
    .description = "View and skip in the playback queue",
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
    return &g_queuePluginAPI;
}
