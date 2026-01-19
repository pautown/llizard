#ifndef NP_OVERLAY_LYRICS_H
#define NP_OVERLAY_LYRICS_H

#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include "../screens/np_screen_now_playing.h"  // For NpAlbumArtUIColors

typedef struct NpLyricsOverlay {
    Rectangle bounds;
    int currentLineIndex;      // Current playing line
    char lyricsHash[64];       // Hash of currently loaded lyrics
    bool hasLyrics;            // Whether lyrics are available
    bool isSynced;             // Whether lyrics have timestamps
    float scrollOffset;        // For smooth scrolling animation
    float targetScrollOffset;  // Target scroll position
} NpLyricsOverlay;

// Initialize the lyrics overlay
void NpLyricsOverlayInit(NpLyricsOverlay *overlay);

// Shutdown and cleanup the lyrics overlay
void NpLyricsOverlayShutdown(NpLyricsOverlay *overlay);

// Update lyrics overlay state
// deltaTime: time since last update in seconds
// positionMs: current playback position in milliseconds
void NpLyricsOverlayUpdate(NpLyricsOverlay *overlay, float deltaTime, int64_t positionMs);

// Draw the lyrics overlay
// alpha: overall opacity (0.0 - 1.0)
// uiColors: optional album art derived colors (can be NULL for theme defaults)
void NpLyricsOverlayDraw(const NpLyricsOverlay *overlay, float alpha, const NpAlbumArtUIColors *uiColors);

// Set the bounds for the lyrics display area
void NpLyricsOverlaySetBounds(NpLyricsOverlay *overlay, Rectangle bounds);

// Manually scroll lyrics by a number of lines (for unsynced lyrics or manual navigation)
// lineDelta: positive = scroll down (later), negative = scroll up (earlier)
void NpLyricsOverlayScrollLines(NpLyricsOverlay *overlay, int lineDelta);

// Jump directly to a specific line index (instant, no animation)
void NpLyricsOverlayJumpToLine(NpLyricsOverlay *overlay, int lineIndex);

// Get current line index
int NpLyricsOverlayGetCurrentLine(const NpLyricsOverlay *overlay);

// Get total number of lyrics lines
int NpLyricsOverlayGetLineCount(const NpLyricsOverlay *overlay);

// Check if lyrics are currently available (alias for NpLyricsOverlayHasContent)
bool NpLyricsOverlayHasLyrics(const NpLyricsOverlay *overlay);

// Check if lyrics have content available
bool NpLyricsOverlayHasContent(const NpLyricsOverlay *overlay);

// Check if lyrics are synced (have timestamps)
bool NpLyricsOverlayIsSynced(const NpLyricsOverlay *overlay);

// Force refresh lyrics from Redis (e.g., after track change)
void NpLyricsOverlayRefresh(NpLyricsOverlay *overlay);

#endif // NP_OVERLAY_LYRICS_H
