#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

// Layout constants
#define RS_SPACING_XS     8.0f
#define RS_SPACING_SM    16.0f
#define RS_SPACING_MD    24.0f
#define RS_SPACING_LG    32.0f

// Colors
#define RS_BG_COLOR       (Color){18, 18, 24, 255}
#define RS_PANEL_COLOR    (Color){32, 34, 48, 255}
#define RS_ACCENT_COLOR   (Color){88, 166, 255, 255}
#define RS_SUCCESS_COLOR  (Color){72, 199, 142, 255}
#define RS_WARNING_COLOR  (Color){255, 184, 76, 255}
#define RS_ERROR_COLOR    (Color){255, 107, 107, 255}
#define RS_TEXT_PRIMARY   (Color){240, 240, 245, 255}
#define RS_TEXT_SECONDARY (Color){160, 165, 180, 255}
#define RS_TEXT_MUTED     (Color){100, 105, 120, 255}

typedef struct {
    LlzMediaState media;
    LlzConnectionStatus conn;
    bool mediaValid;
    bool connValid;
    float refreshTimer;
    float refreshInterval;
    bool wantsClose;
    bool mediaInitDone;
    char lastError[128];
    // Reconnect button state
    bool reconnectButtonHover;
    float reconnectFeedbackTimer;
    bool reconnectSuccess;
} RedisStatusState;

static RedisStatusState g_state;
static int g_screenWidth = 800;
static int g_screenHeight = 480;

static void FetchState(void)
{
    g_state.mediaValid = LlzMediaGetState(&g_state.media);
    g_state.connValid = LlzMediaGetConnection(&g_state.conn);
    if (!g_state.mediaValid) {
        snprintf(g_state.lastError, sizeof(g_state.lastError), "Media fetch failed");
    }
    if (!g_state.connValid) {
        snprintf(g_state.lastError, sizeof(g_state.lastError), "Connection fetch failed");
    }
    if (g_state.mediaValid || g_state.connValid) {
        g_state.lastError[0] = '\0';
    }
}

static void PluginInit(int width, int height)
{
    g_screenWidth = width;
    g_screenHeight = height;
    memset(&g_state, 0, sizeof(g_state));
    g_state.refreshInterval = 1.0f;
    g_state.lastError[0] = '\0';

    LlzMediaConfig cfg = {0};
    if (LlzMediaInit(&cfg)) {
        g_state.mediaInitDone = true;
        FetchState();
    } else {
        snprintf(g_state.lastError, sizeof(g_state.lastError), "Redis connect failed");
    }
}

static void PluginShutdown(void)
{
    if (g_state.mediaInitDone) {
        LlzMediaShutdown();
    }
    memset(&g_state, 0, sizeof(g_state));
}

static bool PluginWantsClose(void)
{
    return g_state.wantsClose;
}

static void DrawStatusIndicator(float x, float y, bool connected)
{
    Color color = connected ? RS_SUCCESS_COLOR : RS_ERROR_COLOR;
    DrawCircle((int)x, (int)y, 8.0f, color);
    // Glow effect
    DrawCircle((int)x, (int)y, 12.0f, ColorAlpha(color, 0.3f));
}

static void DrawLabelValue(const char *label, const char *value, float x, float y, float maxWidth)
{
    DrawText(label, (int)x, (int)y, 16, RS_TEXT_MUTED);
    // Truncate value if too long
    char truncated[64];
    int valueWidth = MeasureText(value, 22);
    if (valueWidth > (int)maxWidth - 20) {
        strncpy(truncated, value, sizeof(truncated) - 4);
        truncated[sizeof(truncated) - 4] = '\0';
        strcat(truncated, "...");
        DrawText(truncated, (int)x, (int)y + 20, 22, RS_TEXT_PRIMARY);
    } else {
        DrawText(value, (int)x, (int)y + 20, 22, RS_TEXT_PRIMARY);
    }
}

static void DrawProgress(float pct, Rectangle bounds, Color fg)
{
    Color bg = ColorAlpha(RS_TEXT_MUTED, 0.3f);
    DrawRectangleRounded(bounds, 0.5f, 8, bg);
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (pct > 0.01f) {
        Rectangle fill = {bounds.x, bounds.y, bounds.width * pct, bounds.height};
        DrawRectangleRounded(fill, 0.5f, 8, fg);
    }
}

// Get reconnect button rectangle
static Rectangle GetReconnectButtonRect(void)
{
    return (Rectangle){
        RS_SPACING_MD,
        g_screenHeight - 48.0f,
        140.0f,
        36.0f
    };
}

static void PluginUpdate(const LlzInputState *input, float deltaTime)
{
    if (input && input->backReleased) {
        g_state.wantsClose = true;
    }

    g_state.refreshTimer += deltaTime;
    if (g_state.refreshTimer >= g_state.refreshInterval) {
        g_state.refreshTimer = 0.0f;
        if (g_state.mediaInitDone) {
            FetchState();
        }
    }

    // Update reconnect feedback timer
    if (g_state.reconnectFeedbackTimer > 0.0f) {
        g_state.reconnectFeedbackTimer -= deltaTime;
        if (g_state.reconnectFeedbackTimer < 0.0f) {
            g_state.reconnectFeedbackTimer = 0.0f;
        }
    }

    if (input && input->selectPressed && g_state.mediaValid) {
        LlzMediaSendCommand(LLZ_PLAYBACK_TOGGLE, 0);
    }

    // Check for reconnect button tap
    Rectangle reconnectBtn = GetReconnectButtonRect();
    g_state.reconnectButtonHover = false;

    if (input && input->tap) {
        if (CheckCollisionPointRec(input->tapPosition, reconnectBtn)) {
            // Attempt BLE reconnect
            bool success = LlzMediaRequestBLEReconnect();
            g_state.reconnectSuccess = success;
            g_state.reconnectFeedbackTimer = 2.0f;  // Show feedback for 2 seconds
            printf("[REDIS_STATUS] BLE reconnect request %s\n", success ? "sent" : "failed");
        }
    }

    // Track hover state for touch/mouse
    if (input && (input->mousePressed || input->hold)) {
        Vector2 pos = input->mousePressed ? input->mousePos : input->holdPosition;
        g_state.reconnectButtonHover = CheckCollisionPointRec(pos, reconnectBtn);
    }
}

static void DrawHeader(void)
{
    // Header bar
    float headerHeight = 56.0f;
    DrawRectangle(0, 0, g_screenWidth, (int)headerHeight, RS_PANEL_COLOR);

    // Title
    DrawText("Redis Status", (int)RS_SPACING_MD, 16, 28, RS_TEXT_PRIMARY);

    // Connection indicators on right side
    float indicatorX = g_screenWidth - RS_SPACING_MD - 120;

    // Redis connection
    bool redisOk = g_state.mediaInitDone;
    DrawText("Redis", (int)indicatorX, 12, 14, RS_TEXT_MUTED);
    DrawStatusIndicator(indicatorX + 70, 20, redisOk);

    // BLE connection
    bool bleOk = g_state.connValid && g_state.conn.connected;
    DrawText("BLE", (int)indicatorX, 32, 14, RS_TEXT_MUTED);
    DrawStatusIndicator(indicatorX + 70, 40, bleOk);
}

static void DrawConnectionCard(Rectangle bounds)
{
    DrawRectangleRounded(bounds, 0.1f, 8, RS_PANEL_COLOR);

    float pad = RS_SPACING_SM;
    float y = bounds.y + pad;

    // Section title
    DrawText("Connection", (int)(bounds.x + pad), (int)y, 20, RS_TEXT_SECONDARY);
    y += 32;

    if (!g_state.mediaInitDone) {
        DrawText("Redis not connected", (int)(bounds.x + pad), (int)y, 18, RS_ERROR_COLOR);
        y += 28;
        if (g_state.lastError[0]) {
            DrawText(g_state.lastError, (int)(bounds.x + pad), (int)y, 14, RS_TEXT_MUTED);
        }
        return;
    }

    if (!g_state.connValid) {
        DrawText("Waiting for BLE data...", (int)(bounds.x + pad), (int)y, 18, RS_WARNING_COLOR);
        return;
    }

    // Status row
    const char *status = g_state.conn.connected ? "Connected" : "Disconnected";
    Color statusColor = g_state.conn.connected ? RS_SUCCESS_COLOR : RS_ERROR_COLOR;
    DrawText("Status", (int)(bounds.x + pad), (int)y, 14, RS_TEXT_MUTED);
    DrawText(status, (int)(bounds.x + pad + 80), (int)y, 14, statusColor);
    y += 24;

    // Device name
    if (g_state.conn.deviceName[0]) {
        DrawLabelValue("Device", g_state.conn.deviceName, bounds.x + pad, y, bounds.width - pad * 2);
    }
}

static void DrawMediaCard(Rectangle bounds)
{
    DrawRectangleRounded(bounds, 0.1f, 8, RS_PANEL_COLOR);

    float pad = RS_SPACING_SM;
    float y = bounds.y + pad;
    float contentWidth = bounds.width - pad * 2;

    // Section title with play state
    DrawText("Now Playing", (int)(bounds.x + pad), (int)y, 20, RS_TEXT_SECONDARY);

    if (g_state.mediaValid) {
        const char *stateText = g_state.media.isPlaying ? "PLAYING" : "PAUSED";
        Color stateColor = g_state.media.isPlaying ? RS_SUCCESS_COLOR : RS_WARNING_COLOR;
        int stateWidth = MeasureText(stateText, 14);
        DrawText(stateText, (int)(bounds.x + bounds.width - pad - stateWidth), (int)y + 4, 14, stateColor);
    }
    y += 36;

    if (!g_state.mediaValid) {
        DrawText("No media playing", (int)(bounds.x + pad), (int)y, 18, RS_TEXT_MUTED);
        return;
    }

    // Track info - larger text for main content
    DrawText(g_state.media.track, (int)(bounds.x + pad), (int)y, 26, RS_TEXT_PRIMARY);
    y += 34;

    DrawText(g_state.media.artist, (int)(bounds.x + pad), (int)y, 20, RS_TEXT_SECONDARY);
    y += 28;

    if (g_state.media.album[0]) {
        DrawText(g_state.media.album, (int)(bounds.x + pad), (int)y, 16, RS_TEXT_MUTED);
        y += 24;
    }

    // Progress section at bottom of card
    y = bounds.y + bounds.height - 60;

    // Time display
    char elapsed[16], duration[16];
    snprintf(elapsed, sizeof(elapsed), "%d:%02d",
             g_state.media.positionSeconds / 60, g_state.media.positionSeconds % 60);
    snprintf(duration, sizeof(duration), "%d:%02d",
             g_state.media.durationSeconds / 60, g_state.media.durationSeconds % 60);

    DrawText(elapsed, (int)(bounds.x + pad), (int)y, 18, RS_TEXT_PRIMARY);
    int durWidth = MeasureText(duration, 18);
    DrawText(duration, (int)(bounds.x + bounds.width - pad - durWidth), (int)y, 18, RS_TEXT_MUTED);
    y += 26;

    // Progress bar
    Rectangle progressRect = {bounds.x + pad, y, contentWidth, 10.0f};
    Color progressColor = g_state.media.isPlaying ? RS_ACCENT_COLOR : RS_TEXT_SECONDARY;
    DrawProgress(LlzMediaGetProgressPercent(&g_state.media), progressRect, progressColor);
}

static void DrawReconnectButton(void)
{
    Rectangle btn = GetReconnectButtonRect();
    bool bleDisconnected = !g_state.connValid || !g_state.conn.connected;

    // Button colors based on state
    Color bgColor = RS_PANEL_COLOR;
    Color borderColor = RS_ACCENT_COLOR;
    Color textColor = RS_ACCENT_COLOR;

    if (g_state.reconnectFeedbackTimer > 0.0f) {
        // Show feedback
        if (g_state.reconnectSuccess) {
            bgColor = ColorAlpha(RS_SUCCESS_COLOR, 0.2f);
            borderColor = RS_SUCCESS_COLOR;
            textColor = RS_SUCCESS_COLOR;
        } else {
            bgColor = ColorAlpha(RS_ERROR_COLOR, 0.2f);
            borderColor = RS_ERROR_COLOR;
            textColor = RS_ERROR_COLOR;
        }
    } else if (g_state.reconnectButtonHover) {
        bgColor = ColorAlpha(RS_ACCENT_COLOR, 0.15f);
    } else if (bleDisconnected) {
        // Highlight when BLE is disconnected
        borderColor = RS_WARNING_COLOR;
        textColor = RS_WARNING_COLOR;
    }

    // Draw button background
    DrawRectangleRounded(btn, 0.3f, 8, bgColor);
    DrawRectangleRoundedLines(btn, 0.3f, 8, borderColor);

    // Button text
    const char *btnText;
    if (g_state.reconnectFeedbackTimer > 0.0f) {
        btnText = g_state.reconnectSuccess ? "Sent!" : "Failed";
    } else {
        btnText = "Reconnect BLE";
    }

    int textWidth = MeasureText(btnText, 16);
    float textX = btn.x + (btn.width - textWidth) / 2;
    float textY = btn.y + (btn.height - 16) / 2;
    DrawText(btnText, (int)textX, (int)textY, 16, textColor);
}

static void DrawHelpFooter(void)
{
    float footerY = g_screenHeight - 40;

    // Draw reconnect button on the left
    DrawReconnectButton();

    // Hint text (shifted right to make room for button)
    float hintX = RS_SPACING_MD + 160;
    DrawText("BACK Exit", (int)hintX, (int)footerY + 10, 16, RS_TEXT_MUTED);
    DrawText("SELECT Play/Pause", (int)(hintX + 100), (int)footerY + 10, 16, RS_TEXT_MUTED);

    // Refresh indicator
    char refresh[32];
    snprintf(refresh, sizeof(refresh), "Refresh: %.1fs", g_state.refreshInterval - g_state.refreshTimer);
    int refreshWidth = MeasureText(refresh, 14);
    DrawText(refresh, g_screenWidth - RS_SPACING_MD - refreshWidth, (int)footerY + 12, 14, RS_TEXT_MUTED);
}

static void PluginDraw(void)
{
    ClearBackground(RS_BG_COLOR);

    // Draw header with status indicators
    DrawHeader();

    // Content area below header
    float headerHeight = 56.0f;
    float footerHeight = 44.0f;
    float contentY = headerHeight + RS_SPACING_SM;
    float contentHeight = g_screenHeight - headerHeight - footerHeight - RS_SPACING_SM * 2;

    // Two-column layout: Connection (left, narrower), Media (right, wider)
    float leftWidth = 240.0f;
    float gap = RS_SPACING_SM;
    float rightWidth = g_screenWidth - RS_SPACING_MD * 2 - leftWidth - gap;

    Rectangle connectionCard = {
        RS_SPACING_MD,
        contentY,
        leftWidth,
        contentHeight
    };
    DrawConnectionCard(connectionCard);

    Rectangle mediaCard = {
        RS_SPACING_MD + leftWidth + gap,
        contentY,
        rightWidth,
        contentHeight
    };
    DrawMediaCard(mediaCard);

    // Footer with controls hint
    DrawHelpFooter();
}

static LlzPluginAPI g_plugin = {
    .name = "Redis Status",
    .description = "Displays Redis/MediaDash state",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose
};

const LlzPluginAPI *LlzGetPlugin(void)
{
    return &g_plugin;
}
