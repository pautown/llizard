#ifndef NP_PLUGIN_OVERLAY_COLORPICKER_H
#define NP_PLUGIN_OVERLAY_COLORPICKER_H

#include "raylib.h"
#include <stdbool.h>
#include "llz_sdk_input.h"
#include "../screens/np_screen_now_playing.h"  // For NpAlbumArtUIColors

// Predefined color options for background
typedef struct {
    const char *name;
    Color bgColor;
} NpColorOption;

// Color picker overlay structure
typedef struct NpColorPickerOverlay {
    bool visible;
    bool active;             // true while overlay is visible OR animating out
    bool colorSelected;      // true if user selected a color, false if cancelled
    int selectedIndex;
    float animAlpha;
    int numColors;
    NpColorOption *colors;
} NpColorPickerOverlay;

// Initialize the color picker overlay
void NpColorPickerOverlayInit(NpColorPickerOverlay *overlay);

// Update color picker overlay state
void NpColorPickerOverlayUpdate(NpColorPickerOverlay *overlay, const LlzInputState *input, float deltaTime);

// Draw the color picker overlay (uiColors can be NULL for theme defaults)
void NpColorPickerOverlayDraw(const NpColorPickerOverlay *overlay, const NpAlbumArtUIColors *uiColors);

// Show the color picker
void NpColorPickerOverlayShow(NpColorPickerOverlay *overlay);

// Hide the color picker
void NpColorPickerOverlayHide(NpColorPickerOverlay *overlay);

// Check if overlay is active (visible or animating)
bool NpColorPickerOverlayIsActive(const NpColorPickerOverlay *overlay);

// Get the currently selected color (returns NULL if none selected or overlay not visible)
const Color* NpColorPickerOverlayGetSelectedColor(const NpColorPickerOverlay *overlay);

// Check if a color was selected (vs cancelled)
bool NpColorPickerOverlayWasColorSelected(const NpColorPickerOverlay *overlay);

// Shutdown and free resources
void NpColorPickerOverlayShutdown(NpColorPickerOverlay *overlay);

#endif
