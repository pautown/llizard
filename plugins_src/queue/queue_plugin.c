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
 * - Back button returns to Now Playing
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
#define ITEM_HEIGHT 80
#define ITEM_SPACING 4
#define ITEMS_PER_PAGE 5
#define PADDING 24
#define LIST_TOP 24

// ============================================================================
// Color Palette
// ============================================================================

static const Color COLOR_BG = {12, 12, 16, 255};
static const Color COLOR_ACCENT = {30, 215, 96, 255};        // Spotify green
static const Color COLOR_TEXT_PRIMARY = {255, 255, 255, 255};
static const Color COLOR_TEXT_SECONDARY = {180, 180, 190, 255};
static const Color COLOR_TEXT_DIM = {100, 100, 110, 255};
static const Color COLOR_CARD_BG = {24, 24, 30, 255};
static const Color COLOR_CARD_SELECTED = {36, 36, 46, 255};
static const Color COLOR_NOW_PLAYING_BG = {25, 50, 35, 255};
static const Color COLOR_NOW_PLAYING_SELECTED = {35, 65, 45, 255};

// ============================================================================
// Plugin State
// ============================================================================

static bool g_wantsClose = false;
static int g_highlightedItem = 0;
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
static const float AUTO_REFRESH_INTERVAL = 10.0f;
static float g_autoRefreshTimer = 0.0f;

// Loading state
static bool g_isLoading = false;
static float g_loadingTimer = 0.0f;

// ============================================================================
// Forward Declarations
// ============================================================================

static void RequestQueue(void);
static void DrawLoadingSpinner(float x, float y, float radius);

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
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 24;
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

    if (g_autoRefreshTimer >= AUTO_REFRESH_INTERVAL) {
        g_autoRefreshTimer = 0.0f;
        g_queueRequested = false;
        RequestQueue();
    }

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

    if (g_isLoading) {
        g_loadingTimer += deltaTime;
        if (g_loadingTimer > 5.0f) {
            g_isLoading = false;
            g_queueRequested = false;
        }
    }
}

static void SkipToQueuePosition(int index) {
    printf("[QUEUE] Skipping to queue position: %d\n", index);
    LlzMediaQueueShift(index);

    g_queueValid = false;
    g_queueRequested = false;
    g_autoRefreshTimer = 0.0f;
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

static void DrawTruncatedText(const char *text, float x, float y,
                              float maxWidth, int fontSize, Color color) {
    Font font = LlzFontGet(LLZ_FONT_UI, fontSize);
    int textWidth = LlzMeasureText(text, fontSize);

    if (textWidth <= (int)maxWidth) {
        LlzDrawText(text, (int)x, (int)y, fontSize, color);
        return;
    }

    // Truncate with ellipsis
    char truncated[256];
    int len = strlen(text);
    for (int i = len; i > 0; i--) {
        strncpy(truncated, text, i);
        truncated[i] = '\0';
        strcat(truncated, "...");
        if (LlzMeasureText(truncated, fontSize) <= (int)maxWidth) {
            LlzDrawText(truncated, (int)x, (int)y, fontSize, color);
            return;
        }
    }
    LlzDrawText("...", (int)x, (int)y, fontSize, color);
}

static void DrawQueueItem(int index, const LlzQueueTrack *track, float yPos,
                          bool isSelected, bool isNowPlaying) {
    float x = PADDING;
    float width = SCREEN_WIDTH - PADDING * 2;

    // Background color
    Color bgColor;
    if (isNowPlaying) {
        bgColor = isSelected ? COLOR_NOW_PLAYING_SELECTED : COLOR_NOW_PLAYING_BG;
    } else {
        bgColor = isSelected ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
    }

    DrawRectangleRounded((Rectangle){x, yPos, width, ITEM_HEIGHT}, 0.08f, 8, bgColor);

    // Selection indicator bar
    if (isSelected) {
        DrawRectangleRounded((Rectangle){x, yPos, 3, ITEM_HEIGHT}, 0.5f, 4, COLOR_ACCENT);
    }

    // Content layout
    float contentX = x + 16;
    float titleY = yPos + 16;
    float artistY = yPos + 46;
    float maxTextWidth = width - 100;

    // Now Playing badge or index
    if (isNowPlaying) {
        Color badgeColor = ColorAlpha(COLOR_ACCENT, 0.2f);
        DrawRectangleRounded((Rectangle){contentX, yPos + 28, 52, 24}, 0.4f, 4, badgeColor);
        LlzDrawText("NOW", (int)contentX + 8, (int)yPos + 32, 14, COLOR_ACCENT);
        contentX += 64;
        maxTextWidth -= 64;
    } else {
        // Queue position number
        char indexStr[8];
        snprintf(indexStr, sizeof(indexStr), "%d", index + 1);
        LlzDrawText(indexStr, (int)contentX, (int)yPos + 30, 18, COLOR_TEXT_DIM);
        contentX += 40;
        maxTextWidth -= 40;
    }

    // Title (larger, white)
    DrawTruncatedText(track->title, contentX, titleY, maxTextWidth, 22, COLOR_TEXT_PRIMARY);

    // Artist (smaller, gray)
    DrawTruncatedText(track->artist, contentX, artistY, maxTextWidth, 16, COLOR_TEXT_SECONDARY);

    // Duration (right-aligned)
    int durationSec = (int)(track->durationMs / 1000);
    int minutes = durationSec / 60;
    int seconds = durationSec % 60;
    char durationStr[16];
    snprintf(durationStr, sizeof(durationStr), "%d:%02d", minutes, seconds);
    int durWidth = LlzMeasureText(durationStr, 14);
    LlzDrawText(durationStr, (int)(x + width - durWidth - 16), (int)(yPos + 32), 14, COLOR_TEXT_DIM);
}

// ============================================================================
// Main Drawing
// ============================================================================

static void DrawQueueList(void) {
    if (!g_queueValid) {
        if (g_isLoading) {
            DrawLoadingSpinner(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 30);
            LlzDrawTextCentered("Loading queue...", SCREEN_WIDTH / 2,
                               SCREEN_HEIGHT / 2 + 60, 18, COLOR_TEXT_SECONDARY);
        } else {
            LlzDrawTextCentered("No queue available", SCREEN_WIDTH / 2,
                               SCREEN_HEIGHT / 2 - 20, 24, COLOR_TEXT_DIM);
            LlzDrawTextCentered("Play music on Spotify to see queue", SCREEN_WIDTH / 2,
                               SCREEN_HEIGHT / 2 + 16, 16, COLOR_TEXT_DIM);
        }
        return;
    }

    if (g_queueData.trackCount == 0 && !g_queueData.hasCurrentlyPlaying) {
        LlzDrawTextCentered("Queue is empty", SCREEN_WIDTH / 2,
                           SCREEN_HEIGHT / 2, 24, COLOR_TEXT_DIM);
        return;
    }

    int totalItems = g_queueData.trackCount;
    if (g_queueData.hasCurrentlyPlaying) totalItems++;

    float yOffset = LIST_TOP - g_smoothScrollOffset;

    // Draw "Now Playing" item if available
    int itemIndex = 0;
    if (g_queueData.hasCurrentlyPlaying) {
        if (yOffset > -ITEM_HEIGHT && yOffset < SCREEN_HEIGHT) {
            DrawQueueItem(-1, &g_queueData.currentlyPlaying, yOffset,
                         g_highlightedItem == 0, true);
        }
        yOffset += ITEM_HEIGHT + ITEM_SPACING + 16;  // Extra spacing after now playing
        itemIndex = 1;
    }

    // "Up Next" section label
    if (g_queueData.trackCount > 0 && yOffset > -30 && yOffset < SCREEN_HEIGHT) {
        LlzDrawText("Up Next", PADDING, (int)yOffset - 4, 14, COLOR_TEXT_DIM);
        yOffset += 24;
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

    memset(&g_queueData, 0, sizeof(g_queueData));
    g_queueValid = false;
    g_queueRequested = false;
    g_refreshTimer = 0.0f;
    g_autoRefreshTimer = 0.0f;
    g_isLoading = false;
    g_loadingTimer = 0.0f;

    LlzMediaInit(NULL);
    RequestQueue();
}

static void plugin_update(const LlzInputState *input, float deltaTime) {
    g_highlightPulse += deltaTime;

    PollQueueData(deltaTime);
    UpdateSmoothScroll(deltaTime);

    int totalItems = g_queueValid ? g_queueData.trackCount : 0;
    if (g_queueValid && g_queueData.hasCurrentlyPlaying) totalItems++;

    // Handle back button - return to Now Playing
    if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
        printf("[QUEUE] Back pressed - returning to Now Playing\n");
        LlzRequestOpenPlugin("Now Playing");
        g_wantsClose = true;
        return;
    }

    // Handle selection
    if (input->selectPressed && g_queueValid && totalItems > 0) {
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

    // Manual refresh on tap
    if (input->tap && g_queueValid && !g_isLoading) {
        g_queueValid = false;
        g_queueRequested = false;
        RequestQueue();
    }
}

static void plugin_draw(void) {
    ClearBackground(COLOR_BG);
    DrawQueueList();
}

static void plugin_shutdown(void) {
    printf("[QUEUE] Shutting down queue plugin\n");
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
