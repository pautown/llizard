#ifndef NP_PLUGIN_OVERLAY_MEDIA_CHANNELS_H
#define NP_PLUGIN_OVERLAY_MEDIA_CHANNELS_H

#include "raylib.h"
#include <stdbool.h>
#include "llz_sdk_input.h"
#include "../screens/np_screen_now_playing.h"  // For NpAlbumArtUIColors

// Media channels overlay structure
typedef struct NpMediaChannelsOverlay {
    bool visible;
    bool active;             // true while overlay is visible OR animating out
    bool channelSelected;    // true if user selected a channel
    bool refreshTriggered;   // true if user triggered refresh
    int selectedIndex;       // 0 = Refresh, 1+ = channels
    float animAlpha;
    bool channelsLoading;
    float loadingAnim;
    float requestTime;
    char selectedChannel[128]; // Name of selected channel
} NpMediaChannelsOverlay;

// Initialize the media channels overlay
void NpMediaChannelsOverlayInit(NpMediaChannelsOverlay *overlay);

// Update media channels overlay state
void NpMediaChannelsOverlayUpdate(NpMediaChannelsOverlay *overlay, const LlzInputState *input, float deltaTime);

// Draw the media channels overlay (uiColors can be NULL for theme defaults)
void NpMediaChannelsOverlayDraw(const NpMediaChannelsOverlay *overlay, const NpAlbumArtUIColors *uiColors);

// Show the media channels overlay
void NpMediaChannelsOverlayShow(NpMediaChannelsOverlay *overlay);

// Hide the media channels overlay
void NpMediaChannelsOverlayHide(NpMediaChannelsOverlay *overlay);

// Check if overlay is active (visible or animating)
bool NpMediaChannelsOverlayIsActive(const NpMediaChannelsOverlay *overlay);

// Get the selected channel name (returns NULL if none selected)
const char* NpMediaChannelsOverlayGetSelectedChannel(const NpMediaChannelsOverlay *overlay);

// Check if a channel was selected (vs cancelled)
bool NpMediaChannelsOverlayWasChannelSelected(const NpMediaChannelsOverlay *overlay);

// Check if refresh was triggered
bool NpMediaChannelsOverlayWasRefreshTriggered(const NpMediaChannelsOverlay *overlay);

// Shutdown and free resources
void NpMediaChannelsOverlayShutdown(NpMediaChannelsOverlay *overlay);

#endif
