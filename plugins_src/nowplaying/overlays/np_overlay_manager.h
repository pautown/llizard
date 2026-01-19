#ifndef NP_PLUGIN_OVERLAY_MANAGER_H
#define NP_PLUGIN_OVERLAY_MANAGER_H

#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include "../core/np_effects.h"
#include "../screens/np_screen_now_playing.h"  // For NpAlbumArtUIColors

typedef enum {
    NP_OVERLAY_NONE,
    NP_OVERLAY_TEMPLATE,
    NP_OVERLAY_CLOCK,
    NP_OVERLAY_LYRICS,
    NP_OVERLAY_COUNT
} NpOverlayType;

typedef struct {
    NpOverlayType currentOverlay;
    NpOverlayType pendingOverlay;
    NpEffect fadeEffect;
    bool isTransitioning;
} NpOverlayManager;

void NpOverlayManagerInit(NpOverlayManager *mgr);
void NpOverlayManagerUpdate(NpOverlayManager *mgr, float deltaTime);
void NpOverlayManagerDraw(NpOverlayManager *mgr, Rectangle viewport, const NpAlbumArtUIColors *uiColors);
void NpOverlayManagerShow(NpOverlayManager *mgr, NpOverlayType type);
void NpOverlayManagerHide(NpOverlayManager *mgr);
bool NpOverlayManagerIsVisible(const NpOverlayManager *mgr);
NpOverlayType NpOverlayManagerGetCurrent(const NpOverlayManager *mgr);
float NpOverlayManagerGetAlpha(const NpOverlayManager *mgr);

// Forward declaration for clock overlay access
typedef struct NpClockOverlay NpClockOverlay;
NpClockOverlay* NpOverlayManagerGetClock(NpOverlayManager *mgr);

// Forward declaration for lyrics overlay access
typedef struct NpLyricsOverlay NpLyricsOverlay;
NpLyricsOverlay* NpOverlayManagerGetLyrics(NpOverlayManager *mgr);

// Update lyrics overlay with playback position (must be called from plugin update)
void NpOverlayManagerUpdateLyrics(NpOverlayManager *mgr, float deltaTime, int64_t positionMs);

#endif
