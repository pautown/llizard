#include "nowplaying/overlays/np_overlay_actions.h"
#include "nowplaying/core/np_theme.h"
#include "llz_sdk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Layout constants
#define PANEL_WIDTH 320
#define PANEL_HEIGHT 180
#define ITEM_HEIGHT 52
#define ITEM_SPACING 8
#define ITEM_MARGIN 16

// Menu items
typedef struct {
    const char *label;
    NpActionType action;
} ActionMenuItem;

static const ActionMenuItem g_menuItems[] = {
    {"View Lyrics", NP_ACTION_VIEW_LYRICS},
    {"View Queue", NP_ACTION_VIEW_QUEUE}
};
static const int g_menuItemCount = sizeof(g_menuItems) / sizeof(g_menuItems[0]);

void NpActionsOverlayInit(NpActionsOverlay *overlay) {
    memset(overlay, 0, sizeof(*overlay));
    overlay->visible = false;
    overlay->active = false;
    overlay->selectedAction = NP_ACTION_NONE;
    overlay->selectedIndex = 0;
    overlay->animAlpha = 0.0f;
}

void NpActionsOverlayUpdate(NpActionsOverlay *overlay, const LlzInputState *input, float deltaTime) {
    if (!overlay) return;

    // Animate alpha
    float targetAlpha = overlay->visible ? 1.0f : 0.0f;
    overlay->animAlpha += (targetAlpha - overlay->animAlpha) * 12.0f * deltaTime;
    if (fabsf(overlay->animAlpha - targetAlpha) < 0.01f) {
        overlay->animAlpha = targetAlpha;
    }

    // Update active state
    overlay->active = overlay->visible || overlay->animAlpha > 0.01f;

    // Don't process input if not visible
    if (!overlay->visible) return;

    // Navigate with scroll or buttons
    int delta = 0;
    if (input->scrollDelta != 0.0f) {
        delta = (input->scrollDelta > 0) ? 1 : -1;
    }
    if (input->upPressed || input->button1Pressed) delta = -1;
    if (input->downPressed || input->button2Pressed) delta = 1;

    if (delta != 0 && g_menuItemCount > 0) {
        overlay->selectedIndex += delta;
        if (overlay->selectedIndex < 0) overlay->selectedIndex = g_menuItemCount - 1;
        if (overlay->selectedIndex >= g_menuItemCount) overlay->selectedIndex = 0;
    }

    // Close on back button
    if (input->backClick || input->backPressed) {
        overlay->selectedAction = NP_ACTION_NONE;
        overlay->visible = false;
    }

    // Select on enter/select or tap
    if (input->selectPressed || input->tap) {
        if (overlay->selectedIndex >= 0 && overlay->selectedIndex < g_menuItemCount) {
            overlay->selectedAction = g_menuItems[overlay->selectedIndex].action;
            overlay->visible = false;
            printf("[ACTIONS_OVERLAY] Selected: %s\n", g_menuItems[overlay->selectedIndex].label);
        }
    }
}

void NpActionsOverlayDraw(const NpActionsOverlay *overlay, const NpAlbumArtUIColors *uiColors) {
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
    Color dimOverlay = {0, 0, 0, (unsigned char)(160 * overlay->animAlpha)};
    DrawRectangle(0, 0, screenWidth, screenHeight, dimOverlay);

    // Calculate panel position (centered)
    float panelX = (screenWidth - PANEL_WIDTH) / 2.0f;
    float panelY = (screenHeight - PANEL_HEIGHT) / 2.0f;
    Rectangle panelRect = {panelX, panelY, PANEL_WIDTH, PANEL_HEIGHT};

    // Draw panel background with blur effect simulation
    Color panelBg = {18, 18, 24, (unsigned char)(245 * overlay->animAlpha)};
    DrawRectangleRounded(panelRect, 0.1f, 12, panelBg);

    // Subtle border
    Color borderColor = ColorAlpha(accentColor, 0.3f * overlay->animAlpha);
    DrawRectangleRoundedLinesEx(panelRect, 0.1f, 12, 1.5f, borderColor);

    Font font = NpThemeGetFont();

    // Draw title
    const char *title = "Actions";
    float titleSize = 24.0f;
    Vector2 titleMeasure = MeasureTextEx(font, title, titleSize, 1.5f);
    float titleX = panelX + (PANEL_WIDTH - titleMeasure.x) / 2.0f;
    float titleY = panelY + 14.0f;
    Color titleColor = {255, 255, 255, (unsigned char)(255 * overlay->animAlpha)};
    DrawTextEx(font, title, (Vector2){titleX, titleY}, titleSize, 1.5f, titleColor);

    // Content area
    float contentY = panelY + 50.0f;

    // Draw menu items
    for (int i = 0; i < g_menuItemCount; i++) {
        float itemY = contentY + i * (ITEM_HEIGHT + ITEM_SPACING);
        bool isSelected = (i == overlay->selectedIndex);

        Rectangle itemRect = {
            panelX + ITEM_MARGIN,
            itemY,
            PANEL_WIDTH - ITEM_MARGIN * 2,
            ITEM_HEIGHT
        };

        // Item background
        Color itemBg;
        if (isSelected) {
            itemBg = (Color){45, 45, 60, (unsigned char)(240 * overlay->animAlpha)};
        } else {
            itemBg = (Color){30, 30, 40, (unsigned char)(180 * overlay->animAlpha)};
        }
        DrawRectangleRounded(itemRect, 0.2f, 8, itemBg);

        // Selection indicator
        if (isSelected) {
            Color indicatorColor = ColorAlpha(accentColor, overlay->animAlpha);
            DrawRectangleRounded(
                (Rectangle){itemRect.x, itemRect.y + 6, 3, ITEM_HEIGHT - 12},
                0.5f, 4, indicatorColor
            );
        }

        // Item text
        float textSize = 20.0f;
        Vector2 textMeasure = MeasureTextEx(font, g_menuItems[i].label, textSize, 1.2f);
        float textX = itemRect.x + (itemRect.width - textMeasure.x) / 2.0f;
        float textY = itemRect.y + (ITEM_HEIGHT - textSize) / 2.0f;
        Color textColor = {255, 255, 255, (unsigned char)((isSelected ? 255 : 200) * overlay->animAlpha)};
        DrawTextEx(font, g_menuItems[i].label, (Vector2){textX, textY}, textSize, 1.2f, textColor);
    }
}

void NpActionsOverlayShow(NpActionsOverlay *overlay) {
    if (!overlay) return;
    overlay->visible = true;
    overlay->active = true;
    overlay->selectedAction = NP_ACTION_NONE;
    overlay->selectedIndex = 0;  // Default to first item (Lyrics)
    printf("[ACTIONS_OVERLAY] Showing actions menu\n");
}

void NpActionsOverlayHide(NpActionsOverlay *overlay) {
    if (!overlay) return;
    overlay->visible = false;
}

bool NpActionsOverlayIsActive(const NpActionsOverlay *overlay) {
    if (!overlay) return false;
    return overlay->active;
}

NpActionType NpActionsOverlayGetSelectedAction(const NpActionsOverlay *overlay) {
    if (!overlay) return NP_ACTION_NONE;
    return overlay->selectedAction;
}

void NpActionsOverlayShutdown(NpActionsOverlay *overlay) {
    if (!overlay) return;
    memset(overlay, 0, sizeof(*overlay));
}
