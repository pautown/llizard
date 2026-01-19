#ifndef NP_PLUGIN_SCREEN_NOW_PLAYING_H
#define NP_PLUGIN_SCREEN_NOW_PLAYING_H

#include "raylib.h"
#include <stdbool.h>
#include "llz_sdk_input.h"

// Playback state for now playing screen
typedef struct {
    bool isPlaying;
    int volume;             // 0-100
    float trackPosition;    // seconds
    float trackDuration;    // seconds
    const char *trackTitle;
    const char *trackArtist;
    const char *trackAlbum;
    bool shuffleEnabled;
    bool repeatEnabled;
} NpPlaybackState;

// Actions triggered by user input
typedef struct {
    bool playPausePressed;
    bool previousPressed;
    bool nextPressed;
    bool shufflePressed;
    bool repeatPressed;
    bool backPressed;
    int volumeDelta;        // change in volume
    bool isScrubbing;
    float scrubPosition;    // new position if scrubbing
    float scrubSpeedMultiplier;  // speed multiplier for visual feedback
    bool swipeNextTriggered;      // swipe left triggered next track
    bool swipePreviousTriggered;  // swipe right triggered previous track
} NpPlaybackActions;

typedef enum {
    NP_DISPLAY_NORMAL = 0,
    NP_DISPLAY_BAREBONES,
    NP_DISPLAY_ALBUM_ART,
    NP_DISPLAY_MODE_COUNT
} NpDisplayMode;

// Album art transition state for crossfade effects
typedef struct {
    Texture2D *prevTexture;     // Previous album art (fading out)
    Texture2D *prevBlurred;     // Previous blurred (fading out)
    float currentAlpha;         // Alpha for current textures (0-1)
    float prevAlpha;            // Alpha for previous textures (0-1)
} NpAlbumArtTransition;

// Album art extracted colors for UI theming
typedef struct {
    Color accent;               // Vibrant color from album art
    Color complementary;        // Complementary color for contrast
    Color trackBackground;      // Contrasting background for progress bars
    bool hasColors;             // Whether colors are available
} NpAlbumArtUIColors;

// Screen state
typedef struct {
    Rectangle viewport;
    NpPlaybackState playback;
    NpPlaybackActions actions;
    bool initialized;
    NpDisplayMode displayMode;
} NpNowPlayingScreen;

void NpNowPlayingInit(NpNowPlayingScreen *screen, Rectangle viewport);
void NpNowPlayingUpdate(NpNowPlayingScreen *screen, const LlzInputState *input, float deltaTime);
void NpNowPlayingDraw(const NpNowPlayingScreen *screen, const LlzInputState *input, bool useCustomBackground, Texture2D *albumArtTexture, Texture2D *albumArtBlurred, const NpAlbumArtTransition *transition, const NpAlbumArtUIColors *uiColors);
NpPlaybackActions *NpNowPlayingGetActions(NpNowPlayingScreen *screen);
void NpNowPlayingSetPlayback(NpNowPlayingScreen *screen, const NpPlaybackState *playback);
void NpNowPlayingSetDisplayMode(NpNowPlayingScreen *screen, NpDisplayMode mode);

#endif
