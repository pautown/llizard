#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Screen dimensions
static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;

// Channel data
static LlzMediaChannels g_channels = {0};
static bool g_channelsLoading = false;
static bool g_channelsLoaded = false;
static float g_requestTime = 0.0f;
static char g_controlledChannel[LLZ_MEDIA_CHANNEL_NAME_MAX] = {0};

// UI state
static int g_selectedIndex = 0;
static float g_scrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;
static float g_animTime = 0.0f;

// Refresh button is at index 0 when channels are loaded
#define REFRESH_ITEM_INDEX 0
static bool HasRefreshItem(void) { return g_channelsLoaded && g_channels.count > 0; }
static int GetChannelIndex(int uiIndex) { return HasRefreshItem() ? uiIndex - 1 : uiIndex; }
static int GetUIIndex(int channelIndex) { return HasRefreshItem() ? channelIndex + 1 : channelIndex; }
static int GetItemCount(void) { return g_channels.count + (HasRefreshItem() ? 1 : 0); }

// Animation
static float g_selectionAnim[LLZ_MEDIA_CHANNEL_MAX] = {0};
static float g_loadingAnim = 0.0f;

// Colors
static const Color COLOR_BG_DARK = {10, 10, 16, 255};
static const Color COLOR_BG_GRADIENT_START = {16, 16, 24, 255};
static const Color COLOR_BG_GRADIENT_END = {24, 20, 32, 255};

static const Color COLOR_CARD = {28, 28, 40, 220};
static const Color COLOR_CARD_SELECTED = {38, 38, 54, 240};
static const Color COLOR_CARD_ACTIVE = {30, 50, 40, 240};
static const Color COLOR_CARD_BORDER = {55, 55, 75, 120};
static const Color COLOR_CARD_BORDER_SELECTED = {80, 80, 110, 180};

static const Color COLOR_ACCENT = {30, 215, 96, 255};
static const Color COLOR_ACCENT_SOFT = {30, 215, 96, 80};
static const Color COLOR_ACCENT_GLOW = {30, 215, 96, 40};

static const Color COLOR_TEXT_PRIMARY = {255, 255, 255, 255};
static const Color COLOR_TEXT_SECONDARY = {180, 180, 190, 255};
static const Color COLOR_TEXT_TERTIARY = {110, 110, 125, 255};

// Layout
#define HEADER_HEIGHT 80
#define FOOTER_HEIGHT 55
#define CARD_MARGIN_X 28
#define CARD_HEIGHT 70
#define CARD_SPACING 10
#define CARD_ROUNDNESS 0.10f

#define CONTENT_TOP (HEADER_HEIGHT + 8)
#define CONTENT_HEIGHT (g_screenHeight - HEADER_HEIGHT - FOOTER_HEIGHT - 16)

// ============================================================================
// Utility Functions
// ============================================================================
static float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

// ============================================================================
// Scroll Management
// ============================================================================
static float CalculateTargetScroll(int selected) {
    float itemTotalHeight = CARD_HEIGHT + CARD_SPACING;
    int itemCount = GetItemCount();
    if (itemCount < 1) itemCount = 1;
    float totalListHeight = itemCount * itemTotalHeight;
    float maxScroll = totalListHeight - CONTENT_HEIGHT;
    if (maxScroll < 0) maxScroll = 0;

    float selectedTop = selected * itemTotalHeight;
    float selectedBottom = selectedTop + CARD_HEIGHT;

    float visibleTop = g_targetScrollOffset;
    float visibleBottom = g_targetScrollOffset + CONTENT_HEIGHT;

    float topMargin = CARD_HEIGHT * 0.3f;
    float bottomMargin = CARD_HEIGHT * 0.5f;

    float newTarget = g_targetScrollOffset;

    if (selectedTop < visibleTop + topMargin) {
        newTarget = selectedTop - topMargin;
    } else if (selectedBottom > visibleBottom - bottomMargin) {
        newTarget = selectedBottom - CONTENT_HEIGHT + bottomMargin;
    }

    if (newTarget < 0) newTarget = 0;
    if (newTarget > maxScroll) newTarget = maxScroll;

    return newTarget;
}

static void UpdateScroll(float deltaTime) {
    float diff = g_targetScrollOffset - g_scrollOffset;
    float speed = 14.0f;
    g_scrollOffset += diff * speed * deltaTime;
    if (fabsf(diff) < 0.5f) {
        g_scrollOffset = g_targetScrollOffset;
    }
}

// ============================================================================
// Drawing Functions
// ============================================================================
static void DrawGradientBackground(void) {
    ClearBackground(COLOR_BG_DARK);
    DrawRectangleGradientV(0, 0, g_screenWidth, g_screenHeight,
                           COLOR_BG_GRADIENT_START, COLOR_BG_GRADIENT_END);

    // Subtle animated accent glow
    float glowPulse = 0.4f + 0.3f * sinf(g_animTime * 0.6f);
    Color glowColor = COLOR_ACCENT_GLOW;
    glowColor.a = (unsigned char)(25 * glowPulse);
    DrawCircleGradient(g_screenWidth - 80, 80, 250, glowColor, BLANK);
}

static void DrawHeader(void) {
    float headerY = 22;

    LlzDrawText("Media Channels", CARD_MARGIN_X, (int)headerY, LLZ_FONT_SIZE_TITLE, COLOR_TEXT_PRIMARY);

    const char *subtitle;
    if (g_channelsLoading) {
        subtitle = "Loading available channels...";
    } else if (g_channels.count == 0) {
        subtitle = "No media apps active";
    } else {
        subtitle = "Select which app to control";
    }
    LlzDrawText(subtitle, CARD_MARGIN_X, (int)headerY + 38, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);
}

static void DrawChannelCard(int index, const char *name, float y, bool selected, bool isControlled, float selectionAnim) {
    float cardX = CARD_MARGIN_X;
    float cardWidth = g_screenWidth - CARD_MARGIN_X * 2;

    // Card color based on state
    Color cardColor;
    if (isControlled) {
        cardColor = ColorLerp(COLOR_CARD_ACTIVE, COLOR_CARD_SELECTED, selected ? EaseOutCubic(selectionAnim) : 0);
    } else {
        cardColor = ColorLerp(COLOR_CARD, COLOR_CARD_SELECTED, EaseOutCubic(selectionAnim));
    }

    // Subtle lift when selected
    float liftOffset = selected ? -2.0f * selectionAnim : 0.0f;
    float cardY = y + liftOffset;

    Rectangle cardRect = {cardX, cardY, cardWidth, CARD_HEIGHT};

    // Shadow
    if (selected) {
        Color shadowColor = {0, 0, 0, (unsigned char)(40 * selectionAnim)};
        DrawRectangleRounded((Rectangle){cardX + 2, cardY + 4, cardWidth, CARD_HEIGHT},
                             CARD_ROUNDNESS, 12, shadowColor);
    }

    // Card background
    DrawRectangleRounded(cardRect, CARD_ROUNDNESS, 12, cardColor);

    // Selection indicator bar (left edge)
    if (selectionAnim > 0.01f) {
        Color indicatorColor = COLOR_ACCENT;
        indicatorColor.a = (unsigned char)(255 * selectionAnim);
        float barHeight = CARD_HEIGHT * (0.4f + 0.6f * selectionAnim);
        float barY = cardY + (CARD_HEIGHT - barHeight) / 2;
        DrawRectangleRounded((Rectangle){cardX, barY, 4, barHeight}, 0.5f, 4, indicatorColor);
    }

    // Border
    Color borderColor = selected ? COLOR_CARD_BORDER_SELECTED : COLOR_CARD_BORDER;
    if (isControlled) {
        borderColor = COLOR_ACCENT;
        borderColor.a = 120;
    }
    DrawRectangleRoundedLinesEx(cardRect, CARD_ROUNDNESS, 12, 1.0f, borderColor);

    // Channel name
    float textX = cardX + 22;
    float textY = cardY + (CARD_HEIGHT - 24) / 2;
    LlzDrawText(name, (int)textX, (int)textY, LLZ_FONT_SIZE_LARGE - 2, COLOR_TEXT_PRIMARY);

    // Active indicator
    if (isControlled) {
        const char *activeText = "ACTIVE";
        int activeWidth = LlzMeasureText(activeText, LLZ_FONT_SIZE_SMALL);
        float badgeX = cardX + cardWidth - activeWidth - 32;
        float badgeY = cardY + (CARD_HEIGHT - 22) / 2;

        Rectangle badgeRect = {badgeX - 8, badgeY - 2, (float)activeWidth + 16, 26};
        DrawRectangleRounded(badgeRect, 0.4f, 8, COLOR_ACCENT_SOFT);
        LlzDrawText(activeText, (int)badgeX, (int)badgeY + 2, LLZ_FONT_SIZE_SMALL, COLOR_ACCENT);
    }
}

static void DrawRefreshCard(float y, bool selected, float selectionAnim) {
    float cardX = CARD_MARGIN_X;
    float cardWidth = g_screenWidth - CARD_MARGIN_X * 2;

    // Card color - use a slightly different tint for the refresh button
    Color cardColor = ColorLerp(COLOR_CARD, COLOR_CARD_SELECTED, EaseOutCubic(selectionAnim));

    // Subtle lift when selected
    float liftOffset = selected ? -2.0f * selectionAnim : 0.0f;
    float cardY = y + liftOffset;

    Rectangle cardRect = {cardX, cardY, cardWidth, CARD_HEIGHT};

    // Shadow
    if (selected) {
        Color shadowColor = {0, 0, 0, (unsigned char)(40 * selectionAnim)};
        DrawRectangleRounded((Rectangle){cardX + 2, cardY + 4, cardWidth, CARD_HEIGHT},
                             CARD_ROUNDNESS, 12, shadowColor);
    }

    // Card background
    DrawRectangleRounded(cardRect, CARD_ROUNDNESS, 12, cardColor);

    // Selection indicator bar (left edge)
    if (selectionAnim > 0.01f) {
        Color indicatorColor = COLOR_ACCENT;
        indicatorColor.a = (unsigned char)(255 * selectionAnim);
        float barHeight = CARD_HEIGHT * (0.4f + 0.6f * selectionAnim);
        float barY = cardY + (CARD_HEIGHT - barHeight) / 2;
        DrawRectangleRounded((Rectangle){cardX, barY, 4, barHeight}, 0.5f, 4, indicatorColor);
    }

    // Border
    Color borderColor = selected ? COLOR_CARD_BORDER_SELECTED : COLOR_CARD_BORDER;
    DrawRectangleRoundedLinesEx(cardRect, CARD_ROUNDNESS, 12, 1.0f, borderColor);

    // Refresh icon (circular arrows) - simplified as text for now
    float textX = cardX + 22;
    float textY = cardY + (CARD_HEIGHT - 24) / 2;

    // Draw refresh symbol and text
    const char *refreshText = g_channelsLoading ? "Refreshing..." : "Refresh Channels";
    Color textColor = g_channelsLoading ? COLOR_ACCENT : COLOR_TEXT_PRIMARY;
    LlzDrawText(refreshText, (int)textX, (int)textY, LLZ_FONT_SIZE_LARGE - 2, textColor);

    // Loading indicator when refreshing
    if (g_channelsLoading) {
        float dotX = cardX + cardWidth - 50;
        float dotY = cardY + CARD_HEIGHT / 2;
        float pulse = 0.5f + 0.5f * sinf(g_loadingAnim * 4.0f);
        Color dotColor = COLOR_ACCENT;
        dotColor.a = (unsigned char)(180 + 75 * pulse);
        DrawCircle((int)dotX, (int)dotY, 4 + 2 * pulse, dotColor);
    }
}

static void DrawLoadingState(void) {
    float centerY = g_screenHeight / 2;

    // Animated loading indicator
    float pulse = 0.5f + 0.5f * sinf(g_loadingAnim * 4.0f);
    Color dotColor = COLOR_ACCENT;
    dotColor.a = (unsigned char)(180 + 75 * pulse);

    for (int i = 0; i < 3; i++) {
        float phase = g_loadingAnim * 4.0f + i * 0.5f;
        float scale = 0.6f + 0.4f * sinf(phase);
        float dotX = g_screenWidth / 2 - 30 + i * 30;
        DrawCircle((int)dotX, (int)centerY, 6 * scale, dotColor);
    }

    LlzDrawTextCentered("Loading channels...", g_screenWidth / 2, (int)centerY + 40,
                        LLZ_FONT_SIZE_NORMAL, COLOR_TEXT_SECONDARY);
}

static void DrawEmptyState(void) {
    float centerY = g_screenHeight / 2;

    LlzDrawTextCentered("No Media Apps Active", g_screenWidth / 2, (int)centerY - 20,
                        LLZ_FONT_SIZE_LARGE, COLOR_TEXT_PRIMARY);
    LlzDrawTextCentered("Play music in Spotify, YouTube Music, etc.", g_screenWidth / 2,
                        (int)centerY + 20, LLZ_FONT_SIZE_NORMAL, COLOR_TEXT_TERTIARY);
    LlzDrawTextCentered("Press select to refresh", g_screenWidth / 2,
                        (int)centerY + 60, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);
}

static void DrawFooter(void) {
    float footerY = g_screenHeight - FOOTER_HEIGHT + 10;

    // Separator
    DrawRectangle(CARD_MARGIN_X, (int)footerY - 12, g_screenWidth - CARD_MARGIN_X * 2, 1,
                  (Color){55, 55, 75, 100});

    // Navigation hints
    const char *hint = "Scroll: navigate | Select: choose | Back: exit";
    LlzDrawText(hint, CARD_MARGIN_X, (int)footerY, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);

    // Channel count
    if (g_channels.count > 0) {
        char countText[32];
        snprintf(countText, sizeof(countText), "%d channel%s", g_channels.count,
                 g_channels.count == 1 ? "" : "s");
        int countWidth = LlzMeasureText(countText, LLZ_FONT_SIZE_SMALL);
        LlzDrawText(countText, g_screenWidth - countWidth - CARD_MARGIN_X, (int)footerY,
                    LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);
    }
}

// ============================================================================
// Plugin Callbacks
// ============================================================================
static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;
    g_selectedIndex = 0;
    g_scrollOffset = 0.0f;
    g_targetScrollOffset = 0.0f;
    g_animTime = 0.0f;
    g_loadingAnim = 0.0f;

    // Reset state
    memset(&g_channels, 0, sizeof(g_channels));
    memset(g_controlledChannel, 0, sizeof(g_controlledChannel));
    g_channelsLoading = false;
    g_channelsLoaded = false;

    // Reset animations
    for (int i = 0; i < LLZ_MEDIA_CHANNEL_MAX; i++) {
        g_selectionAnim[i] = (i == 0) ? 1.0f : 0.0f;
    }

    // Initialize media SDK
    LlzMediaInit(NULL);

    // Try to get existing channels first
    if (LlzMediaGetChannels(&g_channels)) {
        g_channelsLoaded = true;
        printf("Media Channels: Loaded %d cached channels\n", g_channels.count);

        // Get currently controlled channel
        LlzMediaGetControlledChannel(g_controlledChannel, sizeof(g_controlledChannel));
        if (g_controlledChannel[0]) {
            printf("Media Channels: Currently controlling: %s\n", g_controlledChannel);
            // Find and select the controlled channel (account for refresh item at index 0)
            for (int i = 0; i < g_channels.count; i++) {
                if (strcmp(g_channels.channels[i], g_controlledChannel) == 0) {
                    g_selectedIndex = GetUIIndex(i);
                    break;
                }
            }
        } else {
            // Default to first channel (after refresh item)
            g_selectedIndex = HasRefreshItem() ? 1 : 0;
        }
    } else {
        // Request fresh channels
        g_channelsLoading = true;
        g_requestTime = 0.0f;
        LlzMediaRequestChannels();
        printf("Media Channels: Requesting channels...\n");
    }

    printf("Media Channels plugin initialized\n");
}

static void PluginUpdate(const LlzInputState *hostInput, float deltaTime) {
    LlzInputState emptyInput = {0};
    const LlzInputState *input = hostInput ? hostInput : &emptyInput;

    // Update animations
    g_animTime += deltaTime;
    g_loadingAnim += deltaTime;

    // Selection animations
    int itemCount = GetItemCount();
    if (itemCount < 1) itemCount = 1;
    for (int i = 0; i < itemCount && i < LLZ_MEDIA_CHANNEL_MAX; i++) {
        float target = (i == g_selectedIndex) ? 1.0f : 0.0f;
        g_selectionAnim[i] = Lerp(g_selectionAnim[i], target, deltaTime * 14.0f);
    }

    // Scroll animation
    UpdateScroll(deltaTime);

    // Poll for channels if loading
    if (g_channelsLoading) {
        g_requestTime += deltaTime;

        if (LlzMediaGetChannels(&g_channels)) {
            g_channelsLoading = false;
            g_channelsLoaded = true;
            printf("Media Channels: Received %d channels\n", g_channels.count);

            // Get currently controlled channel
            LlzMediaGetControlledChannel(g_controlledChannel, sizeof(g_controlledChannel));
        } else if (g_requestTime > 10.0f) {
            // Timeout
            g_channelsLoading = false;
            printf("Media Channels: Request timed out\n");
        }
    }

    // Navigation (reuse itemCount from above)
    itemCount = GetItemCount();
    if (itemCount > 0) {
        // Scroll wheel navigates
        if (input->scrollDelta != 0) {
            int delta = (input->scrollDelta > 0) ? 1 : -1;
            g_selectedIndex += delta;
            if (g_selectedIndex < 0) g_selectedIndex = 0;
            if (g_selectedIndex >= itemCount) g_selectedIndex = itemCount - 1;
            g_targetScrollOffset = CalculateTargetScroll(g_selectedIndex);
        }

        // Up/down buttons
        if (input->downPressed || IsKeyPressed(KEY_DOWN)) {
            g_selectedIndex = (g_selectedIndex + 1) % itemCount;
            g_targetScrollOffset = CalculateTargetScroll(g_selectedIndex);
        }
        if (input->upPressed || IsKeyPressed(KEY_UP)) {
            g_selectedIndex = (g_selectedIndex - 1 + itemCount) % itemCount;
            g_targetScrollOffset = CalculateTargetScroll(g_selectedIndex);
        }

        // Select item
        if (input->selectPressed || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (HasRefreshItem() && g_selectedIndex == REFRESH_ITEM_INDEX) {
                // Refresh button selected
                if (!g_channelsLoading) {
                    g_channelsLoading = true;
                    g_requestTime = 0.0f;
                    LlzMediaRequestChannels();
                    printf("Media Channels: Refreshing...\n");
                }
            } else {
                // Channel selected
                int channelIndex = GetChannelIndex(g_selectedIndex);
                if (channelIndex >= 0 && channelIndex < g_channels.count) {
                    const char *selectedChannel = g_channels.channels[channelIndex];
                    printf("Media Channels: Selecting channel: %s\n", selectedChannel);

                    // Send select command
                    LlzMediaSelectChannel(selectedChannel);

                    // Update local state
                    strncpy(g_controlledChannel, selectedChannel, sizeof(g_controlledChannel) - 1);
                }
            }
        }

        // Tap to select
        if (input->tap || input->mouseJustPressed) {
            Vector2 tapPos = input->tap ? input->tapPosition : input->mousePos;
            for (int i = 0; i < itemCount; i++) {
                float cardY = CONTENT_TOP + i * (CARD_HEIGHT + CARD_SPACING) - g_scrollOffset;
                Rectangle bounds = {CARD_MARGIN_X, cardY, g_screenWidth - CARD_MARGIN_X * 2, CARD_HEIGHT};
                if (CheckCollisionPointRec(tapPos, bounds)) {
                    if (g_selectedIndex == i) {
                        // Double tap - activate
                        if (HasRefreshItem() && i == REFRESH_ITEM_INDEX) {
                            // Refresh
                            if (!g_channelsLoading) {
                                g_channelsLoading = true;
                                g_requestTime = 0.0f;
                                LlzMediaRequestChannels();
                                printf("Media Channels: Refreshing...\n");
                            }
                        } else {
                            // Select channel
                            int channelIndex = GetChannelIndex(i);
                            if (channelIndex >= 0 && channelIndex < g_channels.count) {
                                const char *selectedChannel = g_channels.channels[channelIndex];
                                printf("Media Channels: Selecting channel: %s\n", selectedChannel);
                                LlzMediaSelectChannel(selectedChannel);
                                strncpy(g_controlledChannel, selectedChannel, sizeof(g_controlledChannel) - 1);
                            }
                        }
                    } else {
                        g_selectedIndex = i;
                        g_targetScrollOffset = CalculateTargetScroll(g_selectedIndex);
                    }
                    break;
                }
            }
        }
    }

    // Refresh on select when empty or loading failed
    if (g_channels.count == 0 && !g_channelsLoading) {
        if (input->selectPressed || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            g_channelsLoading = true;
            g_requestTime = 0.0f;
            LlzMediaRequestChannels();
            printf("Media Channels: Refreshing...\n");
        }
    }

    // Back exits
    if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
        g_wantsClose = true;
    }
}

static void PluginDraw(void) {
    DrawGradientBackground();
    DrawHeader();

    if (g_channelsLoading && g_channels.count == 0) {
        DrawLoadingState();
    } else if (g_channels.count == 0) {
        DrawEmptyState();
    } else {
        // Clip content area
        BeginScissorMode(0, CONTENT_TOP, g_screenWidth, (int)CONTENT_HEIGHT);

        int uiIndex = 0;

        // Draw refresh button first if we have channels
        if (HasRefreshItem()) {
            float cardY = CONTENT_TOP + uiIndex * (CARD_HEIGHT + CARD_SPACING) - g_scrollOffset;
            if (cardY >= CONTENT_TOP - CARD_HEIGHT && cardY <= g_screenHeight) {
                bool selected = (g_selectedIndex == REFRESH_ITEM_INDEX);
                DrawRefreshCard(cardY, selected, g_selectionAnim[REFRESH_ITEM_INDEX]);
            }
            uiIndex++;
        }

        // Draw channel cards
        for (int i = 0; i < g_channels.count; i++) {
            float cardY = CONTENT_TOP + uiIndex * (CARD_HEIGHT + CARD_SPACING) - g_scrollOffset;

            // Skip if outside visible area
            if (cardY < CONTENT_TOP - CARD_HEIGHT || cardY > g_screenHeight) {
                uiIndex++;
                continue;
            }

            bool selected = (uiIndex == g_selectedIndex);
            bool isControlled = (g_controlledChannel[0] != '\0' &&
                                strcmp(g_channels.channels[i], g_controlledChannel) == 0);

            DrawChannelCard(i, g_channels.channels[i], cardY, selected, isControlled,
                           g_selectionAnim[uiIndex]);
            uiIndex++;
        }

        EndScissorMode();
    }

    DrawFooter();
}

static void PluginShutdown(void) {
    g_wantsClose = false;
    printf("Media Channels plugin shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

static LlzPluginAPI g_api = {
    .name = "Media Channels",
    .description = "Select which app to control",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .category = LLZ_CATEGORY_MEDIA
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
