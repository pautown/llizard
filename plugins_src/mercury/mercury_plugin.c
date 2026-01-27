#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

// Mercury color palette
#define MC_BG             (Color){20, 22, 30, 255}
#define MC_SILVER          (Color){192, 197, 206, 255}
#define MC_SILVER_BRIGHT   (Color){220, 225, 235, 255}
#define MC_SILVER_DIM      (Color){100, 108, 120, 255}
#define MC_CONNECTED       (Color){72, 199, 142, 255}
#define MC_SCANNING        (Color){140, 160, 200, 255}
#define MC_DISCONNECTED    (Color){180, 120, 100, 255}
#define MC_PANEL           (Color){30, 34, 48, 255}

typedef struct {
    LlzConnectionStatus conn;
    bool connValid;
    bool mediaInitDone;
    bool wantsClose;
    float refreshTimer;
    float pulseTimer;
    // Connect button
    bool connectHover;
    float connectFeedbackTimer;
    bool connectSuccess;
    // Restart button
    bool restartHover;
    float restartFeedbackTimer;
    bool restartSuccess;
} MercuryState;

static MercuryState g_state;
static int g_screenWidth = 800;
static int g_screenHeight = 480;

static void FetchState(void)
{
    g_state.connValid = LlzMediaGetConnection(&g_state.conn);
}

static void PluginInit(int width, int height)
{
    g_screenWidth = width;
    g_screenHeight = height;
    memset(&g_state, 0, sizeof(g_state));

    LlzMediaConfig cfg = {0};
    if (LlzMediaInit(&cfg)) {
        g_state.mediaInitDone = true;
        FetchState();
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

// Button rectangles
static Rectangle GetConnectButtonRect(void)
{
    float btnW = 180.0f;
    float btnH = 44.0f;
    float gap = 20.0f;
    float totalW = btnW * 2 + gap;
    float startX = (g_screenWidth - totalW) / 2.0f;
    return (Rectangle){startX, g_screenHeight - 70.0f, btnW, btnH};
}

static Rectangle GetRestartButtonRect(void)
{
    float btnW = 180.0f;
    float btnH = 44.0f;
    float gap = 20.0f;
    float totalW = btnW * 2 + gap;
    float startX = (g_screenWidth - totalW) / 2.0f;
    return (Rectangle){startX + btnW + gap, g_screenHeight - 70.0f, btnW, btnH};
}

static void PluginUpdate(const LlzInputState *input, float deltaTime)
{
    if (input && input->backReleased) {
        g_state.wantsClose = true;
    }

    g_state.pulseTimer += deltaTime;

    g_state.refreshTimer += deltaTime;
    if (g_state.refreshTimer >= 1.0f) {
        g_state.refreshTimer = 0.0f;
        if (g_state.mediaInitDone) {
            FetchState();
        }
    }

    // Update feedback timers
    if (g_state.connectFeedbackTimer > 0.0f) {
        g_state.connectFeedbackTimer -= deltaTime;
        if (g_state.connectFeedbackTimer < 0.0f) g_state.connectFeedbackTimer = 0.0f;
    }
    if (g_state.restartFeedbackTimer > 0.0f) {
        g_state.restartFeedbackTimer -= deltaTime;
        if (g_state.restartFeedbackTimer < 0.0f) g_state.restartFeedbackTimer = 0.0f;
    }

    // Button interactions
    Rectangle connectBtn = GetConnectButtonRect();
    Rectangle restartBtn = GetRestartButtonRect();
    g_state.connectHover = false;
    g_state.restartHover = false;

    if (input && input->tap) {
        if (CheckCollisionPointRec(input->tapPosition, connectBtn)) {
            bool success = LlzMediaRequestBLEReconnect();
            printf("[MERCURY] BLE reconnect request %s\n", success ? "sent" : "failed");
            g_state.connectSuccess = success;
            g_state.connectFeedbackTimer = 2.0f;
        } else if (CheckCollisionPointRec(input->tapPosition, restartBtn)) {
            printf("[MERCURY] Restarting BLE service...\n");
            bool success = LlzMediaRestartBLEService();
            printf("[MERCURY] BLE service restart %s\n", success ? "initiated" : "failed");
            g_state.restartSuccess = success;
            g_state.restartFeedbackTimer = 2.0f;
        }
    }

    if (input && (input->mousePressed || input->hold)) {
        Vector2 pos = input->mousePressed ? input->mousePos : input->holdPosition;
        g_state.connectHover = CheckCollisionPointRec(pos, connectBtn);
        g_state.restartHover = CheckCollisionPointRec(pos, restartBtn);
    }
}

static void DrawOrb(float cx, float cy, float radius, bool connected, bool serviceRunning)
{
    float pulse = sinf(g_state.pulseTimer * 2.0f) * 0.5f + 0.5f;

    Color orbColor;
    if (connected) {
        orbColor = MC_CONNECTED;
    } else if (serviceRunning) {
        orbColor = MC_SCANNING;
    } else {
        orbColor = MC_DISCONNECTED;
    }

    // Outer glow - pulses when not connected
    float glowAlpha = connected ? 0.15f : (0.1f + pulse * 0.2f);
    float glowRadius = connected ? radius * 1.6f : radius * (1.5f + pulse * 0.3f);
    DrawCircle((int)cx, (int)cy, glowRadius, ColorAlpha(orbColor, glowAlpha));

    // Mid glow
    float midAlpha = connected ? 0.25f : (0.15f + pulse * 0.15f);
    DrawCircle((int)cx, (int)cy, radius * 1.25f, ColorAlpha(orbColor, midAlpha));

    // Core orb
    DrawCircle((int)cx, (int)cy, radius, orbColor);

    // Inner highlight (mercury reflection)
    float highlightOffset = radius * 0.25f;
    DrawCircle((int)(cx - highlightOffset), (int)(cy - highlightOffset),
               radius * 0.35f, ColorAlpha(MC_SILVER_BRIGHT, 0.4f));
}

static void DrawMercuryButton(Rectangle btn, const char *text, const char *feedbackOk,
                               const char *feedbackFail, Color accentColor,
                               bool hover, float feedbackTimer, bool feedbackSuccess)
{
    Color bgColor = MC_PANEL;
    Color borderColor = accentColor;
    Color textColor = accentColor;

    if (feedbackTimer > 0.0f) {
        if (feedbackSuccess) {
            bgColor = ColorAlpha(MC_CONNECTED, 0.2f);
            borderColor = MC_CONNECTED;
            textColor = MC_CONNECTED;
        } else {
            bgColor = ColorAlpha(MC_DISCONNECTED, 0.2f);
            borderColor = MC_DISCONNECTED;
            textColor = MC_DISCONNECTED;
        }
    } else if (hover) {
        bgColor = ColorAlpha(accentColor, 0.12f);
    }

    DrawRectangleRounded(btn, 0.35f, 8, bgColor);
    DrawRectangleRoundedLines(btn, 0.35f, 8, borderColor);

    const char *label;
    if (feedbackTimer > 0.0f) {
        label = feedbackSuccess ? feedbackOk : feedbackFail;
    } else {
        label = text;
    }

    int textWidth = LlzMeasureText(label, 18);
    float textX = btn.x + (btn.width - textWidth) / 2.0f;
    float textY = btn.y + (btn.height - 18) / 2.0f;
    LlzDrawText(label, (int)textX, (int)textY, 18, textColor);
}

static void PluginDraw(void)
{
    ClearBackground(MC_BG);

    bool connected = g_state.connValid && g_state.conn.connected;
    bool serviceRunning = LlzMediaIsBLEServiceRunning();

    // Center orb
    float orbCX = g_screenWidth / 2.0f;
    float orbCY = g_screenHeight / 2.0f - 60.0f;
    float orbRadius = 55.0f;

    DrawOrb(orbCX, orbCY, orbRadius, connected, serviceRunning);

    // Status text below orb
    float textY = orbCY + orbRadius + 30.0f;
    const char *statusText;
    Color statusColor;

    if (!g_state.mediaInitDone) {
        statusText = "Redis Unavailable";
        statusColor = MC_DISCONNECTED;
    } else if (connected) {
        statusText = "Connected";
        statusColor = MC_CONNECTED;
    } else if (serviceRunning) {
        statusText = "Scanning...";
        statusColor = MC_SCANNING;
    } else {
        statusText = "Disconnected";
        statusColor = MC_DISCONNECTED;
    }

    LlzDrawTextCentered(statusText, (int)orbCX, (int)textY, 28, statusColor);

    // Device name or service info
    textY += 36.0f;
    if (connected && g_state.conn.deviceName[0]) {
        LlzDrawTextCentered(g_state.conn.deviceName, (int)orbCX, (int)textY, 20, MC_SILVER);
    } else if (!serviceRunning) {
        LlzDrawTextCentered("BLE service not running", (int)orbCX, (int)textY, 18, MC_SILVER_DIM);
    } else if (!g_state.mediaInitDone) {
        LlzDrawTextCentered("Cannot reach Redis", (int)orbCX, (int)textY, 18, MC_SILVER_DIM);
    }

    // Buttons
    Rectangle connectBtn = GetConnectButtonRect();
    Rectangle restartBtn = GetRestartButtonRect();

    DrawMercuryButton(connectBtn, "Connect", "Sent!", "Failed",
                      MC_SILVER_BRIGHT, g_state.connectHover,
                      g_state.connectFeedbackTimer, g_state.connectSuccess);

    DrawMercuryButton(restartBtn, "Restart Service", "Restarting...", "Failed",
                      MC_SILVER_DIM, g_state.restartHover,
                      g_state.restartFeedbackTimer, g_state.restartSuccess);

    // Title at top
    LlzDrawTextCentered("Mercury", g_screenWidth / 2, 24, 22, MC_SILVER_DIM);
}

static LlzPluginAPI g_plugin = {
    .name = "Mercury",
    .description = "BLE connection status and control",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .category = LLZ_CATEGORY_UTILITIES
};

const LlzPluginAPI *LlzGetPlugin(void)
{
    return &g_plugin;
}
