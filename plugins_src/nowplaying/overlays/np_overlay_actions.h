#ifndef NP_PLUGIN_OVERLAY_ACTIONS_H
#define NP_PLUGIN_OVERLAY_ACTIONS_H

#include "raylib.h"
#include <stdbool.h>
#include "llz_sdk_input.h"
#include "../screens/np_screen_now_playing.h"  // For NpAlbumArtUIColors

// Action types that can be selected
typedef enum {
    NP_ACTION_NONE = 0,
    NP_ACTION_VIEW_LYRICS,
    NP_ACTION_VIEW_QUEUE
} NpActionType;

// Actions overlay structure
typedef struct NpActionsOverlay {
    bool visible;
    bool active;             // true while overlay is visible OR animating out
    NpActionType selectedAction;  // The action that was selected
    int selectedIndex;       // Currently highlighted index
    float animAlpha;
} NpActionsOverlay;

// Initialize the actions overlay
void NpActionsOverlayInit(NpActionsOverlay *overlay);

// Update actions overlay state
void NpActionsOverlayUpdate(NpActionsOverlay *overlay, const LlzInputState *input, float deltaTime);

// Draw the actions overlay (uiColors can be NULL for theme defaults)
void NpActionsOverlayDraw(const NpActionsOverlay *overlay, const NpAlbumArtUIColors *uiColors);

// Show the actions overlay
void NpActionsOverlayShow(NpActionsOverlay *overlay);

// Hide the actions overlay
void NpActionsOverlayHide(NpActionsOverlay *overlay);

// Check if overlay is active (visible or animating)
bool NpActionsOverlayIsActive(const NpActionsOverlay *overlay);

// Get the selected action (returns NP_ACTION_NONE if cancelled)
NpActionType NpActionsOverlayGetSelectedAction(const NpActionsOverlay *overlay);

// Shutdown and free resources
void NpActionsOverlayShutdown(NpActionsOverlay *overlay);

#endif
