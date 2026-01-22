#include "nowplaying/overlays/np_overlay_media_channels.h"
#include "nowplaying/core/np_theme.h"
#include "llz_sdk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Layout constants
#define PANEL_WIDTH 400
#define PANEL_HEIGHT 340
#define ITEM_HEIGHT 56
#define ITEM_SPACING 8
#define ITEM_MARGIN 16
#define VISIBLE_ITEMS 4

// Local state
static LlzMediaChannels g_channels = {0};
static char g_controlledChannel[LLZ_MEDIA_CHANNEL_NAME_MAX] = {0};

void NpMediaChannelsOverlayInit(NpMediaChannelsOverlay *overlay) {
    memset(overlay, 0, sizeof(*overlay));
    overlay->visible = false;
    overlay->active = false;
    overlay->channelSelected = false;
    overlay->refreshTriggered = false;
    overlay->selectedIndex = 0;
    overlay->animAlpha = 0.0f;
    overlay->channelsLoading = false;
    overlay->loadingAnim = 0.0f;
    overlay->requestTime = 0.0f;
    overlay->selectedChannel[0] = '\0';
}

static int GetItemCount(void) {
    // +1 for refresh button at index 0
    return g_channels.count + 1;
}

void NpMediaChannelsOverlayUpdate(NpMediaChannelsOverlay *overlay, const LlzInputState *input, float deltaTime) {
    if (!overlay) return;

    // Animate alpha
    float targetAlpha = overlay->visible ? 1.0f : 0.0f;
    overlay->animAlpha += (targetAlpha - overlay->animAlpha) * 10.0f * deltaTime;
    if (fabsf(overlay->animAlpha - targetAlpha) < 0.01f) {
        overlay->animAlpha = targetAlpha;
    }

    // Update active state - active while visible or still fading out
    overlay->active = overlay->visible || overlay->animAlpha > 0.01f;

    // Update loading animation
    if (overlay->channelsLoading) {
        overlay->loadingAnim += deltaTime;
        overlay->requestTime += deltaTime;

        // Poll for channels
        if (LlzMediaGetChannels(&g_channels)) {
            overlay->channelsLoading = false;
            printf("[MEDIA_CHANNELS_OVERLAY] Received %d channels\n", g_channels.count);
            LlzMediaGetControlledChannel(g_controlledChannel, sizeof(g_controlledChannel));
        } else if (overlay->requestTime > 10.0f) {
            // Timeout
            overlay->channelsLoading = false;
            printf("[MEDIA_CHANNELS_OVERLAY] Request timed out\n");
        }
    }

    // Don't process input if not visible
    if (!overlay->visible) return;

    int itemCount = GetItemCount();

    // Navigate with scroll (flipped direction) or buttons
    int delta = 0;
    if (input->scrollDelta != 0.0f) {
        delta = (input->scrollDelta > 0) ? 1 : -1;
    }
    if (input->upPressed || input->button1Pressed) delta = -1;
    if (input->downPressed || input->button2Pressed) delta = 1;

    if (delta != 0 && itemCount > 0) {
        overlay->selectedIndex += delta;
        if (overlay->selectedIndex < 0) overlay->selectedIndex = itemCount - 1;
        if (overlay->selectedIndex >= itemCount) overlay->selectedIndex = 0;
    }

    // Close on back button quick click only (not on release after hold)
    // This prevents closing when user releases after long-pressing to open
    if (input->backClick) {
        overlay->channelSelected = false;
        overlay->refreshTriggered = false;
        overlay->visible = false;
    }

    // Select on enter/select or tap
    if (input->selectPressed || input->tap) {
        if (overlay->selectedIndex == 0) {
            // Refresh selected
            if (!overlay->channelsLoading) {
                overlay->channelsLoading = true;
                overlay->requestTime = 0.0f;
                LlzMediaRequestChannels();
                overlay->refreshTriggered = true;
                printf("[MEDIA_CHANNELS_OVERLAY] Refreshing channels...\n");
            }
        } else {
            // Channel selected
            int channelIndex = overlay->selectedIndex - 1;
            if (channelIndex >= 0 && channelIndex < g_channels.count) {
                strncpy(overlay->selectedChannel, g_channels.channels[channelIndex],
                        sizeof(overlay->selectedChannel) - 1);
                overlay->channelSelected = true;
                overlay->visible = false;
                printf("[MEDIA_CHANNELS_OVERLAY] Selected: %s\n", overlay->selectedChannel);
            }
        }
    }
}

void NpMediaChannelsOverlayDraw(const NpMediaChannelsOverlay *overlay, const NpAlbumArtUIColors *uiColors) {
    if (!overlay || overlay->animAlpha <= 0.01f) return;

    int screenWidth = 800;
    int screenHeight = 480;

    // Determine colors
    Color accentColor;
    if (uiColors && uiColors->hasColors) {
        accentColor = uiColors->accent;
    } else {
        accentColor = NpThemeGetColor(NP_COLOR_ACCENT);
    }

    // Draw dimmed backdrop
    Color dimOverlay = {0, 0, 0, (unsigned char)(180 * overlay->animAlpha)};
    DrawRectangle(0, 0, screenWidth, screenHeight, dimOverlay);

    // Calculate panel position (centered)
    float panelX = (screenWidth - PANEL_WIDTH) / 2.0f;
    float panelY = (screenHeight - PANEL_HEIGHT) / 2.0f;
    Rectangle panelRect = {panelX, panelY, PANEL_WIDTH, PANEL_HEIGHT};

    // Draw panel background
    Color panelBg = {20, 20, 28, (unsigned char)(250 * overlay->animAlpha)};
    DrawRectangleRounded(panelRect, 0.06f, 12, panelBg);

    // Draw panel border with accent
    Color borderColor = ColorAlpha(accentColor, 0.5f * overlay->animAlpha);
    DrawRectangleRoundedLinesEx(panelRect, 0.06f, 12, 2.0f, borderColor);

    Font font = NpThemeGetFont();

    // Draw title
    const char *title = "Media Channels";
    float titleSize = 28.0f;
    Vector2 titleMeasure = MeasureTextEx(font, title, titleSize, 2.0f);
    float titleX = panelX + (PANEL_WIDTH - titleMeasure.x) / 2.0f;
    float titleY = panelY + 16.0f;
    Color titleColor = {255, 255, 255, (unsigned char)(255 * overlay->animAlpha)};
    DrawTextEx(font, title, (Vector2){titleX, titleY}, titleSize, 2.0f, titleColor);

    // Content area
    float contentY = panelY + 60.0f;
    float contentHeight = PANEL_HEIGHT - 100.0f;

    // Clip content area
    BeginScissorMode((int)panelX + ITEM_MARGIN, (int)contentY,
                     PANEL_WIDTH - ITEM_MARGIN * 2, (int)contentHeight);

    int itemCount = GetItemCount();

    // Calculate scroll offset to keep selected item visible
    float totalItemsHeight = itemCount * (ITEM_HEIGHT + ITEM_SPACING);
    float scrollOffset = 0;
    if (totalItemsHeight > contentHeight && overlay->selectedIndex > 0) {
        float selectedTop = overlay->selectedIndex * (ITEM_HEIGHT + ITEM_SPACING);
        float selectedBottom = selectedTop + ITEM_HEIGHT;
        float maxScroll = totalItemsHeight - contentHeight;

        // Center the selected item if possible
        scrollOffset = selectedTop - (contentHeight - ITEM_HEIGHT) / 2.0f;
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    }

    // Draw items
    for (int i = 0; i < itemCount; i++) {
        float itemY = contentY + i * (ITEM_HEIGHT + ITEM_SPACING) - scrollOffset;

        // Skip if outside visible area
        if (itemY + ITEM_HEIGHT < contentY || itemY > contentY + contentHeight) continue;

        bool isSelected = (i == overlay->selectedIndex);
        bool isControlled = false;
        const char *itemName;

        if (i == 0) {
            // Refresh item
            itemName = overlay->channelsLoading ? "Refreshing..." : "Refresh Channels";
        } else {
            // Channel item
            int channelIndex = i - 1;
            itemName = g_channels.channels[channelIndex];
            isControlled = (g_controlledChannel[0] != '\0' &&
                          strcmp(itemName, g_controlledChannel) == 0);
        }

        Rectangle itemRect = {
            panelX + ITEM_MARGIN,
            itemY,
            PANEL_WIDTH - ITEM_MARGIN * 2,
            ITEM_HEIGHT
        };

        // Item background
        Color itemBg;
        if (isSelected) {
            itemBg = (Color){40, 40, 55, (unsigned char)(240 * overlay->animAlpha)};
        } else if (isControlled) {
            itemBg = (Color){30, 50, 40, (unsigned char)(220 * overlay->animAlpha)};
        } else {
            itemBg = (Color){28, 28, 38, (unsigned char)(200 * overlay->animAlpha)};
        }
        DrawRectangleRounded(itemRect, 0.15f, 8, itemBg);

        // Selection indicator
        if (isSelected) {
            Color indicatorColor = ColorAlpha(accentColor, overlay->animAlpha);
            DrawRectangleRounded(
                (Rectangle){itemRect.x, itemRect.y + 8, 4, ITEM_HEIGHT - 16},
                0.5f, 4, indicatorColor
            );
        }

        // Item border
        if (isControlled) {
            Color ctrlBorder = ColorAlpha(accentColor, 0.6f * overlay->animAlpha);
            DrawRectangleRoundedLinesEx(itemRect, 0.15f, 8, 1.0f, ctrlBorder);
        }

        // Item text
        float textSize = 22.0f;
        float textX = itemRect.x + 20.0f;
        float textY = itemRect.y + (ITEM_HEIGHT - textSize) / 2.0f;
        Color textColor = {255, 255, 255, (unsigned char)((isSelected ? 255 : 200) * overlay->animAlpha)};

        if (i == 0 && overlay->channelsLoading) {
            // Loading indicator
            textColor = ColorAlpha(accentColor, overlay->animAlpha);
        }
        DrawTextEx(font, itemName, (Vector2){textX, textY}, textSize, 1.5f, textColor);

        // Loading spinner for refresh item
        if (i == 0 && overlay->channelsLoading) {
            float dotX = itemRect.x + itemRect.width - 30;
            float dotY = itemRect.y + ITEM_HEIGHT / 2;
            float pulse = 0.5f + 0.5f * sinf(overlay->loadingAnim * 4.0f);
            Color dotColor = ColorAlpha(accentColor, overlay->animAlpha);
            DrawCircle((int)dotX, (int)dotY, 4 + 2 * pulse, dotColor);
        }

        // Active badge for controlled channel
        if (isControlled) {
            const char *badge = "ACTIVE";
            float badgeSize = 14.0f;
            Vector2 badgeMeasure = MeasureTextEx(font, badge, badgeSize, 1.2f);
            float badgeX = itemRect.x + itemRect.width - badgeMeasure.x - 16;
            float badgeY = itemRect.y + (ITEM_HEIGHT - badgeSize) / 2.0f;
            Color badgeColor = ColorAlpha(accentColor, overlay->animAlpha);
            DrawTextEx(font, badge, (Vector2){badgeX, badgeY}, badgeSize, 1.2f, badgeColor);
        }
    }

    EndScissorMode();

    // Draw hint at bottom
    const char *hint = "Scroll: navigate  |  Select: choose  |  Back: cancel";
    float hintSize = 16.0f;
    Vector2 hintMeasure = MeasureTextEx(font, hint, hintSize, 1.2f);
    float hintX = panelX + (PANEL_WIDTH - hintMeasure.x) / 2.0f;
    float hintY = panelY + PANEL_HEIGHT - 30.0f;
    Color hintColor = {160, 160, 170, (unsigned char)(180 * overlay->animAlpha)};
    DrawTextEx(font, hint, (Vector2){hintX, hintY}, hintSize, 1.2f, hintColor);
}

void NpMediaChannelsOverlayShow(NpMediaChannelsOverlay *overlay) {
    if (!overlay) return;
    overlay->visible = true;
    overlay->active = true;
    overlay->channelSelected = false;
    overlay->refreshTriggered = false;
    overlay->selectedIndex = 0;

    // Load current channels
    if (LlzMediaGetChannels(&g_channels)) {
        printf("[MEDIA_CHANNELS_OVERLAY] Loaded %d cached channels\n", g_channels.count);
        LlzMediaGetControlledChannel(g_controlledChannel, sizeof(g_controlledChannel));

        // Select the currently controlled channel
        if (g_controlledChannel[0] != '\0') {
            for (int i = 0; i < g_channels.count; i++) {
                if (strcmp(g_channels.channels[i], g_controlledChannel) == 0) {
                    overlay->selectedIndex = i + 1; // +1 for refresh item
                    break;
                }
            }
        }
    } else {
        // Request channels
        overlay->channelsLoading = true;
        overlay->requestTime = 0.0f;
        LlzMediaRequestChannels();
        printf("[MEDIA_CHANNELS_OVERLAY] Requesting channels...\n");
    }
}

void NpMediaChannelsOverlayHide(NpMediaChannelsOverlay *overlay) {
    if (!overlay) return;
    overlay->visible = false;
    // active remains true until animation completes
}

bool NpMediaChannelsOverlayIsActive(const NpMediaChannelsOverlay *overlay) {
    if (!overlay) return false;
    return overlay->active;
}

const char* NpMediaChannelsOverlayGetSelectedChannel(const NpMediaChannelsOverlay *overlay) {
    if (!overlay || !overlay->channelSelected || overlay->selectedChannel[0] == '\0') {
        return NULL;
    }
    return overlay->selectedChannel;
}

bool NpMediaChannelsOverlayWasChannelSelected(const NpMediaChannelsOverlay *overlay) {
    return overlay && overlay->channelSelected;
}

bool NpMediaChannelsOverlayWasRefreshTriggered(const NpMediaChannelsOverlay *overlay) {
    return overlay && overlay->refreshTriggered;
}

void NpMediaChannelsOverlayShutdown(NpMediaChannelsOverlay *overlay) {
    if (!overlay) return;
    memset(overlay, 0, sizeof(*overlay));
}
