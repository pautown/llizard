#ifndef NP_PLUGIN_OVERLAY_CLOCK_H
#define NP_PLUGIN_OVERLAY_CLOCK_H

#include "raylib.h"
#include <stdbool.h>
#include "../screens/np_screen_now_playing.h"  // For NpAlbumArtUIColors

// Clock style enum
typedef enum {
    NP_CLOCK_STYLE_DIGITAL = 0,   // Simple digital clock with glow
    NP_CLOCK_STYLE_FULLSCREEN,    // Minimal full-bleed typography clock
    NP_CLOCK_STYLE_ANALOG,        // Classic analog clock with hands
    NP_CLOCK_STYLE_COUNT
} NpClockStyle;

// Clock overlay structure
typedef struct NpClockOverlay {
    Rectangle bounds;
    NpClockStyle currentStyle;
    float volumeOverlayAlpha;
    float volumeOverlayTimeout;
    int lastVolume;
} NpClockOverlay;

// Initialize the clock overlay
void NpClockOverlayInit(NpClockOverlay *overlay);

// Update clock overlay state (delta time in seconds)
void NpClockOverlayUpdate(NpClockOverlay *overlay, float deltaTime);

// Draw the clock overlay (uiColors can be NULL for theme defaults)
void NpClockOverlayDraw(const NpClockOverlay *overlay, float alpha, const NpAlbumArtUIColors *uiColors);

// Cycle to the next clock style
void NpClockOverlayCycleStyle(NpClockOverlay *overlay);

// Show volume overlay (call when volume changes)
void NpClockOverlayShowVolume(NpClockOverlay *overlay, int volume);

#endif
