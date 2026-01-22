#include "nowplaying/screens/np_screen_now_playing.h"
#include "nowplaying/core/np_theme.h"
#include "nowplaying/widgets/np_widget_button.h"
#include "nowplaying/widgets/np_widget_progress.h"
#include "nowplaying/widgets/np_widget_panel.h"
#include "nowplaying/widgets/np_widget_label.h"
#include "nowplaying/widgets/np_widget_album_art.h"
#include "llz_sdk_image.h"
#include <stdio.h>
#include <math.h>

// Internal state
static bool s_isScrubbing = false;
static bool s_justFinishedScrubbing = false;  // Prevents play/pause on scrub release
static float s_scrubStartX = 0.0f;
static float s_scrubStartY = 0.0f;
static float s_scrubStartPosition = 0.0f;
static float s_scrubTrackWidth = 0.0f;  // Store progress bar width for delta calculation
static float s_scrubPulseTimer = 0.0f;
static const float SCRUB_PULSE_DURATION = 0.20f;
static const float TOP_MARGIN = 20.0f;
static const float NP_BAREBONES_PANEL_HEIGHT = 150.0f;
#define NP_CONTROL_COUNT 5

// Layout spacing constants
#define NP_SPACING_XS      8.0f    // Tight spacing
#define NP_SPACING_SM     16.0f    // Small spacing
#define NP_SPACING_MD     24.0f    // Medium spacing
#define NP_SPACING_LG     32.0f    // Large spacing
#define NP_SPACING_XL     48.0f    // Extra large

// Typography line height multipliers
#define NP_LINE_HEIGHT_TIGHT   1.2f
#define NP_LINE_HEIGHT_NORMAL  1.4f
#define NP_LINE_HEIGHT_LOOSE   1.6f

// Control button sizing
#define NP_BUTTON_HEIGHT      56.0f
#define NP_BUTTON_SPACING     16.0f

typedef struct {
    int contentX;
    int contentWidth;
    int leftColumnWidth;
    int rightColumnX;
    int rightColumnWidth;
    int bodyY;
    float artSize;
    bool showAlbumArt;
    bool showUpNext;
    NpDisplayMode mode;
} NpLayoutMetrics;

static bool LayoutHasNormalArt(const NpLayoutMetrics *layout)
{
    return layout->mode == NP_DISPLAY_NORMAL && layout->showAlbumArt && layout->artSize > 1.0f;
}

static bool LayoutIsBarebones(const NpLayoutMetrics *layout)
{
    return layout->mode == NP_DISPLAY_BAREBONES;
}

// Utility to format time
static void FormatTime(float seconds, char *out, size_t size) {
    if (seconds < 0) seconds = 0;
    int total = (int)roundf(seconds);
    int mins = total / 60;
    int secs = total % 60;
    snprintf(out, size, "%02d:%02d", mins, secs);
}

static void ComputeLayout(const NpNowPlayingScreen *screen, NpLayoutMetrics *layout)
{
    layout->mode = screen->displayMode;
    layout->contentWidth = (int)screen->viewport.width - 40;
    if (layout->contentWidth < 240) layout->contentWidth = (int)screen->viewport.width - 20;
    layout->contentX = (int)(screen->viewport.x + (screen->viewport.width - layout->contentWidth) * 0.5f);
    layout->bodyY = (int)screen->viewport.y + (int)TOP_MARGIN;
    layout->showUpNext = false;  // Advanced mode removed
    layout->showAlbumArt = (screen->displayMode != NP_DISPLAY_BAREBONES && screen->displayMode != NP_DISPLAY_ALBUM_ART);
    layout->leftColumnWidth = layout->contentWidth;

    switch (screen->displayMode) {
        case NP_DISPLAY_NORMAL:
            layout->showUpNext = false;
            layout->leftColumnWidth = (int)(screen->viewport.width * 0.74f);
            if (layout->leftColumnWidth > (int)screen->viewport.width - 80) layout->leftColumnWidth = (int)screen->viewport.width - 80;
            if (layout->leftColumnWidth < 320) layout->leftColumnWidth = 320;
            layout->contentWidth = layout->leftColumnWidth;
            layout->contentX = (int)(screen->viewport.x + (screen->viewport.width - layout->contentWidth) * 0.5f);
            layout->bodyY = (int)screen->viewport.y + (int)NP_SPACING_LG;
            break;
        case NP_DISPLAY_BAREBONES:
            layout->showUpNext = false;
            layout->showAlbumArt = false;
            layout->leftColumnWidth = (int)(screen->viewport.width * 0.6f);
            if (layout->leftColumnWidth > (int)screen->viewport.width - 120) layout->leftColumnWidth = (int)screen->viewport.width - 120;
            if (layout->leftColumnWidth < 280) layout->leftColumnWidth = 280;
            layout->contentWidth = layout->leftColumnWidth;
            layout->contentX = (int)(screen->viewport.x + (screen->viewport.width - layout->contentWidth) * 0.5f);
            layout->bodyY = (int)screen->viewport.y + (int)(NP_SPACING_XL * 2);
            break;
        case NP_DISPLAY_ALBUM_ART:
            layout->showUpNext = false;
            layout->showAlbumArt = false;
            layout->leftColumnWidth = layout->contentWidth;
            break;
    }

    layout->rightColumnX = layout->showUpNext ? layout->contentX + layout->leftColumnWidth + (int)NP_SPACING_LG : 0;
    layout->rightColumnWidth = layout->showUpNext ? (layout->contentWidth - layout->leftColumnWidth - (int)NP_SPACING_LG) : 0;
    if (layout->rightColumnWidth < 120 && layout->showUpNext) layout->rightColumnWidth = layout->contentWidth - layout->leftColumnWidth;

    const float maxArt = (screen->displayMode == NP_DISPLAY_NORMAL) ? 250.0f : 260.0f;
    layout->artSize = layout->showAlbumArt ? fminf((float)layout->leftColumnWidth - 32.0f, maxArt) : 0.0f;
    if (layout->artSize < 0) layout->artSize = 0;
}

static int ComputeInfoTopY(const NpLayoutMetrics *layout)
{
    int infoY = layout->bodyY;
    if (layout->showAlbumArt && layout->artSize > 0.0f) infoY += (int)layout->artSize + (int)NP_SPACING_MD;
    return infoY;
}

static void ComputeInfoArea(const NpLayoutMetrics *layout, int *outX, int *outWidth, int *outY)
{
    int x = layout->contentX;
    int width = layout->leftColumnWidth;
    int y = ComputeInfoTopY(layout);

    bool normalWithArt = LayoutHasNormalArt(layout);
    if (normalWithArt) {
        int offset = (int)layout->artSize + (int)NP_SPACING_MD;
        x = layout->contentX + offset;
        width = layout->leftColumnWidth - offset;
        if (width < 220) width = 220;
        if (width > layout->leftColumnWidth) width = layout->leftColumnWidth;
        y = layout->bodyY;
    }

    if (outX) *outX = x;
    if (outWidth) *outWidth = width;
    if (outY) *outY = y;
}

static Rectangle ComputeProgressTrackRect(const NpNowPlayingScreen *screen, const NpLayoutMetrics *layout,
                                          int infoX, int infoWidth, int infoY)
{
    if (LayoutHasNormalArt(layout)) {
        float margin = NP_SPACING_MD;
        float width = screen->viewport.width - margin * 2.0f;
        if (width > screen->viewport.width - 32.0f) width = screen->viewport.width - 32.0f;
        if (width < 200.0f) width = 200.0f;
        float x = screen->viewport.x + (screen->viewport.width - width) * 0.5f;
        // Position trackbar near bottom of screen
        float y = screen->viewport.y + screen->viewport.height - 56.0f;
        return (Rectangle){x, y, width, 10.0f};
    }

    if (LayoutIsBarebones(layout)) {
        float y = screen->viewport.y + screen->viewport.height - NP_BAREBONES_PANEL_HEIGHT + NP_SPACING_SM;
        return (Rectangle){screen->viewport.x, y, screen->viewport.width, 12.0f};
    }

    return (Rectangle){(float)infoX, (float)infoY + NP_SPACING_MD, (float)infoWidth, 10.0f};
}

static Rectangle BuildControlRects(const NpNowPlayingScreen *screen, const NpLayoutMetrics *layout,
                                   Rectangle rects[NP_CONTROL_COUNT])
{
    bool compact = LayoutIsBarebones(layout);
    int infoY = ComputeInfoTopY(layout);
    infoY += (int)(NpThemeGetLineHeight(NP_TYPO_TITLE) * NP_LINE_HEIGHT_NORMAL);
    infoY += (int)(NpThemeGetLineHeight(NP_TYPO_BODY) * NP_LINE_HEIGHT_TIGHT);

    Rectangle bounds;
    if (compact) {
        float panelY = screen->viewport.y + screen->viewport.height - NP_BAREBONES_PANEL_HEIGHT;
        bounds = (Rectangle){screen->viewport.x, panelY, screen->viewport.width, NP_BAREBONES_PANEL_HEIGHT};
    } else {
        int buttonsOffset = (int)NP_SPACING_LG;
        bounds = (Rectangle){
            (float)layout->contentX,
            (float)(infoY + buttonsOffset),
            (float)layout->leftColumnWidth,
            64.0f
        };
    }

    if (rects) {
        const float widths[NP_CONTROL_COUNT] = {52, 56, 72, 56, 52};
        float spacing = NP_BUTTON_SPACING;
        float total = 0.0f;
        for (int i = 0; i < NP_CONTROL_COUNT; i++) total += widths[i];
        total += spacing * (NP_CONTROL_COUNT - 1);

        float startX;
        // Always center buttons now that advanced mode is removed
        startX = bounds.x + fmaxf(NP_SPACING_SM, (bounds.width - total) * 0.5f);

        float buttonHeight = compact ? 72.0f : (bounds.height - NP_SPACING_SM);
        float buttonY = compact ? (bounds.y + bounds.height - buttonHeight - NP_SPACING_SM)
                                : (bounds.y + (bounds.height - buttonHeight) * 0.5f);

        float x = startX;
        for (int i = 0; i < NP_CONTROL_COUNT; i++) {
            rects[i] = (Rectangle){x, buttonY, widths[i], buttonHeight};
            x += widths[i] + spacing;
        }
    }

    return bounds;
}

static void DrawAlbumArtOnly(const NpNowPlayingScreen *screen, Texture2D *albumArtTexture, Texture2D *albumArtBlurred, const NpAlbumArtTransition *transition, const NpAlbumArtUIColors *uiColors)
{
    // Draw blurred background with crossfade support
    bool hasPrevBlurred = transition && transition->prevBlurred && transition->prevBlurred->id != 0 && transition->prevAlpha > 0.01f;
    bool hasCurrentBlurred = albumArtBlurred && albumArtBlurred->id != 0;
    float currentAlpha = transition ? transition->currentAlpha : 1.0f;

    // Draw previous blurred (fading out) first
    if (hasPrevBlurred) {
        Color tint = ColorAlpha(WHITE, transition->prevAlpha);
        LlzDrawTextureCover(*transition->prevBlurred, screen->viewport, tint);
    }

    // Draw current blurred (fading in) on top
    if (hasCurrentBlurred && currentAlpha > 0.01f) {
        Color tint = ColorAlpha(WHITE, currentAlpha);
        LlzDrawTextureCover(*albumArtBlurred, screen->viewport, tint);
    }

    // Fallback background if nothing to show
    if (!hasPrevBlurred && (!hasCurrentBlurred || currentAlpha <= 0.01f)) {
        ClearBackground(NpThemeGetColor(NP_COLOR_BG_DARK));
    }

    // Calculate centered album art size (maintain aspect ratio, fit within screen with padding)
    float padding = 40.0f;
    float maxWidth = screen->viewport.width - padding * 2;
    float maxHeight = screen->viewport.height - padding * 2 - 120.0f;  // Leave room for text overlay

    float artSize = fminf(maxWidth, maxHeight);
    if (albumArtTexture && albumArtTexture->id != 0) {
        // Use actual texture aspect ratio
        float texRatio = (float)albumArtTexture->width / (float)albumArtTexture->height;
        if (texRatio > 1.0f) {
            artSize = fminf(maxWidth, maxHeight / texRatio);
        } else {
            artSize = fminf(maxWidth * texRatio, maxHeight);
        }
    }

    // Center the album art
    Rectangle artBounds = {
        screen->viewport.x + (screen->viewport.width - artSize) * 0.5f,
        screen->viewport.y + padding,
        artSize,
        artSize
    };

    // Draw previous album art (fading out)
    bool hasPrevTexture = transition && transition->prevTexture && transition->prevTexture->id != 0 && transition->prevAlpha > 0.01f;
    if (hasPrevTexture) {
        Color tint = ColorAlpha(WHITE, transition->prevAlpha);
        LlzDrawTextureContain(*transition->prevTexture, artBounds, tint);
    }

    // Draw current album art (fading in)
    if (albumArtTexture && albumArtTexture->id != 0 && currentAlpha > 0.01f) {
        Color tint = ColorAlpha(WHITE, currentAlpha);
        LlzDrawTextureContain(*albumArtTexture, artBounds, tint);
    } else if (!hasPrevTexture) {
        // Draw placeholder only if nothing else is visible
        NpAlbumArt albumArt;
        NpAlbumArtInit(&albumArt, artBounds);
        NpAlbumArtDraw(&albumArt);
    }

    // Draw gradient overlay for text at bottom (moved up to make room for seekbar)
    Rectangle overlay = {
        screen->viewport.x,
        screen->viewport.y + screen->viewport.height - 150,
        screen->viewport.width,
        150
    };

    // Use accent color for overlay gradient if available
    Color overlayBase = BLACK;
    if (uiColors && uiColors->hasColors) {
        // Create a dark version of the accent color for the overlay
        Color accent = uiColors->accent;
        overlayBase = (Color){
            (unsigned char)(accent.r * 0.15f),
            (unsigned char)(accent.g * 0.15f),
            (unsigned char)(accent.b * 0.15f),
            255
        };
    }
    Color top = ColorAlpha(overlayBase, 0.0f);
    Color bottom = ColorAlpha(overlayBase, 0.9f);
    DrawRectangleGradientV((int)overlay.x, (int)overlay.y, (int)overlay.width, (int)overlay.height, top, bottom);

    // Draw track info at bottom (moved up to make room for seekbar)
    float textX = overlay.x + NP_SPACING_MD;
    float textY = overlay.y + NP_SPACING_MD;

    // Draw media channel badge if available (e.g., "Spotify", "YouTube Music")
    if (screen->playback.mediaChannel && screen->playback.mediaChannel[0] != '\0') {
        Color channelColor = NpThemeGetColor(NP_COLOR_TEXT_SECONDARY);
        channelColor.a = 180;  // Slightly transparent
        NpThemeDrawTextColored(NP_TYPO_DETAIL, screen->playback.mediaChannel, (Vector2){textX, textY}, channelColor);
        textY += NpThemeGetLineHeight(NP_TYPO_DETAIL) * NP_LINE_HEIGHT_TIGHT;
    }

    NpThemeDrawText(NP_TYPO_TITLE, screen->playback.trackTitle, (Vector2){textX, textY});
    textY += NpThemeGetLineHeight(NP_TYPO_TITLE) * NP_LINE_HEIGHT_TIGHT;
    NpThemeDrawTextColored(NP_TYPO_BODY, screen->playback.trackArtist, (Vector2){textX, textY},
                           NpThemeGetColor(NP_COLOR_TEXT_SECONDARY));
    textY += NpThemeGetLineHeight(NP_TYPO_BODY) * NP_LINE_HEIGHT_TIGHT;
    NpThemeDrawTextColored(NP_TYPO_BODY, screen->playback.trackAlbum, (Vector2){textX, textY},
                           NpThemeGetColor(NP_COLOR_TEXT_SECONDARY));

    // Draw small seekbar touching the very bottom of the screen
    float seekbarHeight = 4.0f;
    float seekbarY = screen->viewport.y + screen->viewport.height - seekbarHeight;
    Rectangle seekbarBg = {
        screen->viewport.x,
        seekbarY,
        screen->viewport.width,
        seekbarHeight
    };

    float progress = screen->playback.trackDuration > 0 ?
                     screen->playback.trackPosition / screen->playback.trackDuration : 0.0f;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    Rectangle seekbarFill = seekbarBg;
    seekbarFill.width = seekbarBg.width * progress;

    // Use contextual colors if available
    Color bgColor, fillColor;
    if (uiColors && uiColors->hasColors) {
        bgColor = ColorAlpha(uiColors->trackBackground, 0.5f);
        fillColor = uiColors->accent;
    } else {
        bgColor = ColorAlpha(NpThemeGetColor(NP_COLOR_PANEL), 0.5f);
        fillColor = NpThemeGetColor(NP_COLOR_ACCENT);
    }

    DrawRectangleRec(seekbarBg, bgColor);
    if (seekbarFill.width > 0) {
        DrawRectangleRec(seekbarFill, fillColor);
    }
}

void NpNowPlayingInit(NpNowPlayingScreen *screen, Rectangle viewport) {
    screen->viewport = viewport;
    screen->initialized = true;

    // Default playback state
    screen->playback.isPlaying = false;
    screen->playback.volume = 50;
    screen->playback.trackPosition = 0.0f;
    screen->playback.trackDuration = 204.0f;
    screen->playback.trackTitle = "Texas";
    screen->playback.trackArtist = "Sharleen Spiteri";
    screen->playback.shuffleEnabled = false;
    screen->playback.repeatEnabled = false;

    // Clear actions
    screen->actions = (NpPlaybackActions){0};
    screen->displayMode = NP_DISPLAY_NORMAL;
}

void NpNowPlayingUpdate(NpNowPlayingScreen *screen, const LlzInputState *input, float deltaTime) {
    // Swipe cooldown tracking
    static bool s_recentSwipe = false;
    static float s_swipeCooldown = 0.0f;

    // Clear actions
    screen->actions = (NpPlaybackActions){0};

    // Update scrub pulse timer
    if (s_scrubPulseTimer > 0.0f) {
        s_scrubPulseTimer -= deltaTime;
        if (s_scrubPulseTimer < 0.0f) s_scrubPulseTimer = 0.0f;
    }

    // Update swipe cooldown timer
    if (s_swipeCooldown > 0.0f) {
        s_swipeCooldown -= deltaTime;
        if (s_swipeCooldown <= 0.0f) {
            s_swipeCooldown = 0.0f;
            s_recentSwipe = false;
        }
    }

    // Detect swipe gestures (blocked while scrubbing to prevent accidental track skip)
    if (input->swipeLeft && !s_isScrubbing) {
        screen->actions.swipeNextTriggered = true;
        s_recentSwipe = true;
        s_swipeCooldown = 0.4f;  // 400ms cooldown
    }
    if (input->swipeRight && !s_isScrubbing) {
        screen->actions.swipePreviousTriggered = true;
        s_recentSwipe = true;
        s_swipeCooldown = 0.4f;  // 400ms cooldown
    }

    // Handle back button (on release)
    if (input->backReleased) {
        screen->actions.backPressed = true;
    }

    // Handle volume scroll
    if (input->scrollDelta != 0) {
        screen->actions.volumeDelta = (int)(input->scrollDelta * 5.0f);
    }

    if (screen->displayMode == NP_DISPLAY_ALBUM_ART) {
        s_isScrubbing = false;
        s_justFinishedScrubbing = false;
        return;
    }

    NpLayoutMetrics layout;
    ComputeLayout(screen, &layout);

    int infoX, infoWidth, infoY;
    ComputeInfoArea(&layout, &infoX, &infoWidth, &infoY);

    int progressBaseY = infoY;
    progressBaseY += (int)(NpThemeGetLineHeight(NP_TYPO_TITLE) * NP_LINE_HEIGHT_NORMAL);
    progressBaseY += (int)(NpThemeGetLineHeight(NP_TYPO_BODY) * NP_LINE_HEIGHT_TIGHT);

    Rectangle progressTrack = ComputeProgressTrackRect(screen, &layout, infoX, infoWidth, progressBaseY);
    Rectangle scrubHitArea = {progressTrack.x, progressTrack.y - NP_SPACING_SM, progressTrack.width, 40.0f};
    Rectangle controlRects[NP_CONTROL_COUNT];
    BuildControlRects(screen, &layout, controlRects);

    // Handle scrubbing
    if (input->mouseJustPressed && CheckCollisionPointRec(input->mousePos, scrubHitArea)) {
        s_isScrubbing = true;
        s_scrubStartX = input->mousePos.x;
        s_scrubStartY = input->mousePos.y;
        s_scrubStartPosition = screen->playback.trackPosition;
        s_scrubTrackWidth = progressTrack.width;
        s_scrubPulseTimer = SCRUB_PULSE_DURATION;
    }

    if (input->mouseJustReleased) {
        if (s_isScrubbing) {
            s_justFinishedScrubbing = true;  // Block play/pause on this release
        }
        s_isScrubbing = false;
    }

    if (s_isScrubbing && input->mousePressed) {
        // Calculate vertical distance from start point for variable-speed scrubbing
        float verticalDistance = fabsf(input->mousePos.y - s_scrubStartY);

        // Calculate speed multiplier based on vertical distance (INVERTED - closer = faster, farther = slower)
        // Close to seekbar (0-50px): Fast control (1.0x - 2.0x speed)
        // Medium distance (50-100px): Normal speed (0.5x - 1.0x)
        // Far from seekbar (100px+): Fine control (0.1x - 0.5x speed) - slower for precision
        float speedMultiplier;
        if (verticalDistance < 50.0f) {
            // Fast control: 2.0x to 1.0x (closer = faster)
            speedMultiplier = 2.0f - (verticalDistance / 50.0f) * 1.0f;
        } else if (verticalDistance < 100.0f) {
            // Normal speed: 1.0x to 0.5x
            speedMultiplier = 1.0f - ((verticalDistance - 50.0f) / 50.0f) * 0.5f;
        } else {
            // Fine control: 0.5x to 0.1x (further = slower for precision)
            float excessDistance = fminf(verticalDistance - 100.0f, 100.0f); // Cap at 100px excess
            speedMultiplier = 0.5f - (excessDistance / 100.0f) * 0.4f;
        }

        if (speedMultiplier > 1.0f) speedMultiplier = 1.0f;
        if (speedMultiplier < 0.1f) speedMultiplier = 0.1f;

        // Calculate position based on X DELTA from start position (not absolute X)
        // This ensures Y movement only affects speed multiplier, not track position
        float xDelta = input->mousePos.x - s_scrubStartX;

        // Convert X delta to time delta (pixels to seconds)
        // Use stored track width to maintain consistent sensitivity
        float timeDelta = (xDelta / s_scrubTrackWidth) * screen->playback.trackDuration;

        // Apply speed multiplier to the time delta
        float adjustedDelta = timeDelta * speedMultiplier;
        float newPosition = s_scrubStartPosition + adjustedDelta;

        // Clamp to valid range
        if (newPosition < 0.0f) newPosition = 0.0f;
        if (newPosition > screen->playback.trackDuration) newPosition = screen->playback.trackDuration;

        screen->actions.isScrubbing = true;
        screen->actions.scrubPosition = newPosition;
        screen->actions.scrubSpeedMultiplier = speedMultiplier;  // Store for visual feedback
    }

    bool pointerReleased = input->mouseJustReleased || input->tap;
    Vector2 pointer = input->mousePos;
    if (input->tap) pointer = input->tapPosition;
    bool triggeredControl = false;
    if (!s_isScrubbing && !s_justFinishedScrubbing && pointerReleased) {
        for (int i = 0; i < NP_CONTROL_COUNT; i++) {
            if (CheckCollisionPointRec(pointer, controlRects[i])) {
                switch (i) {
                    case 0: screen->actions.shufflePressed = true; break;
                    case 1: screen->actions.previousPressed = true; break;
                    case 2: screen->actions.playPausePressed = true; break;
                    case 3: screen->actions.nextPressed = true; break;
                    case 4: screen->actions.repeatPressed = true; break;
                }
                triggeredControl = true;
                break;
            }
        }
    }

    // Fallback tap-to-play/pause (context-aware: don't trigger if recently swiped or just finished scrubbing)
    if (!s_isScrubbing && !s_justFinishedScrubbing && !s_recentSwipe && pointerReleased && !triggeredControl) {
        screen->actions.playPausePressed = true;
    }

    // Reset the just-finished-scrubbing flag at the end of update
    s_justFinishedScrubbing = false;
}

// Store UI colors for use in drawing functions
static const NpAlbumArtUIColors *s_currentUIColors = NULL;

void NpNowPlayingDraw(const NpNowPlayingScreen *screen, const LlzInputState *input, bool useCustomBackground, Texture2D *albumArtTexture, Texture2D *albumArtBlurred, const NpAlbumArtTransition *transition, const NpAlbumArtUIColors *uiColors) {
    s_currentUIColors = uiColors;

    if (screen->displayMode == NP_DISPLAY_ALBUM_ART) {
        DrawAlbumArtOnly(screen, albumArtTexture, albumArtBlurred, transition, uiColors);
        return;
    }

    NpLayoutMetrics layout;
    ComputeLayout(screen, &layout);

    // Draw background
    if (!useCustomBackground) {
        Color overlay = NpThemeGetColor(NP_COLOR_BG_DARK);
        overlay.a = 255;
        DrawRectangleRec(screen->viewport, overlay);
    }

    // Draw scrub pulse animation if active
    if (s_scrubPulseTimer > 0.0f) {
        float pulseAlpha = (s_scrubPulseTimer / SCRUB_PULSE_DURATION);
        Color pulseColor = NpThemeGetColor(NP_COLOR_TEXT_PRIMARY);
        pulseColor.a = (unsigned char)(0.12f * pulseAlpha * 255.0f);
        DrawRectangleRec(screen->viewport, pulseColor);
    }

    int contentX = layout.contentX;
    int bodyY = layout.bodyY;
    int leftColumnWidth = layout.leftColumnWidth;
    bool normalWithArt = LayoutHasNormalArt(&layout);
    bool compact = LayoutIsBarebones(&layout);

    // Album art
    if (layout.showAlbumArt && layout.artSize > 0.0f) {
        NpAlbumArt albumArt;
        float artX = (float)contentX;
        float artY = (float)bodyY;
        if (normalWithArt) {
            // Shift album art to the left edge and vertically center it
            artX = screen->viewport.x + NP_SPACING_MD;
            float availableHeight = screen->viewport.height - 120.0f;  // Leave room for trackbar area
            artY = screen->viewport.y + (availableHeight - layout.artSize) * 0.5f;
            if (artY < screen->viewport.y + NP_SPACING_SM) artY = screen->viewport.y + NP_SPACING_SM;
        }
        NpAlbumArtInit(&albumArt, (Rectangle){artX, artY, layout.artSize, layout.artSize});
        NpAlbumArtSetTexture(&albumArt, albumArtTexture);
        NpAlbumArtDraw(&albumArt);
    }

    // Track info
    int infoX, infoWidth, infoY;
    ComputeInfoArea(&layout, &infoX, &infoWidth, &infoY);

    if (normalWithArt && layout.artSize > 0.0f) {
        // Position text to the right of the left-aligned album art
        infoX = (int)(screen->viewport.x + NP_SPACING_MD + layout.artSize + NP_SPACING_MD);
        infoWidth = (int)(screen->viewport.width - infoX - NP_SPACING_MD);
        // Vertically center the text block alongside the album art
        float titleHeight = NpThemeGetLineHeight(NP_TYPO_TITLE);
        float bodyHeight = NpThemeGetLineHeight(NP_TYPO_BODY);
        float blockHeight = titleHeight * NP_LINE_HEIGHT_NORMAL + bodyHeight * NP_LINE_HEIGHT_TIGHT * 2;
        float availableHeight = screen->viewport.height - 120.0f;
        float artCenterY = screen->viewport.y + (availableHeight - layout.artSize) * 0.5f + layout.artSize * 0.5f;
        infoY = (int)(artCenterY - blockHeight * 0.5f);
        if (infoY < (int)(screen->viewport.y + NP_SPACING_SM)) infoY = (int)(screen->viewport.y + NP_SPACING_SM);
    }

    // Draw media channel badge if available (e.g., "Spotify", "YouTube Music")
    if (screen->playback.mediaChannel && screen->playback.mediaChannel[0] != '\0') {
        Color channelColor = NpThemeGetColor(NP_COLOR_TEXT_SECONDARY);
        channelColor.a = 180;  // Slightly transparent
        NpThemeDrawTextColored(NP_TYPO_DETAIL, screen->playback.mediaChannel, (Vector2){infoX, infoY}, channelColor);
        infoY += (int)(NpThemeGetLineHeight(NP_TYPO_DETAIL) * NP_LINE_HEIGHT_TIGHT);
    }

    // Draw track title - use standard fonts (consistent across all modes)
    if (compact) {
        // Barebones: 1.5x bigger text (use TITLE instead of BODY)
        NpThemeDrawText(NP_TYPO_TITLE, screen->playback.trackTitle, (Vector2){infoX, infoY});
        infoY += (int)(NpThemeGetLineHeight(NP_TYPO_TITLE) * 1.2f);
    } else {
        // Normal and normalWithArt: use standard TITLE typography
        NpThemeDrawText(NP_TYPO_TITLE, screen->playback.trackTitle, (Vector2){infoX, infoY});
        infoY += (int)(NpThemeGetLineHeight(NP_TYPO_TITLE) * NP_LINE_HEIGHT_NORMAL);
    }

    if (compact) {
        // Barebones: 1.5x bigger text for artist and album
        NpThemeDrawTextColored(NP_TYPO_TITLE, screen->playback.trackArtist, (Vector2){infoX, infoY},
                               NpThemeGetColor(NP_COLOR_TEXT_SECONDARY));
        infoY += (int)(NpThemeGetLineHeight(NP_TYPO_TITLE) * 1.1f);
        NpThemeDrawTextColored(NP_TYPO_TITLE, screen->playback.trackAlbum, (Vector2){infoX, infoY},
                               NpThemeGetColor(NP_COLOR_TEXT_SECONDARY));
        infoY += (int)(NpThemeGetLineHeight(NP_TYPO_TITLE) * 1.1f);
    } else {
        NpThemeDrawTextColored(NP_TYPO_BODY, screen->playback.trackArtist, (Vector2){infoX, infoY},
                               NpThemeGetColor(NP_COLOR_TEXT_SECONDARY));
        infoY += (int)(NpThemeGetLineHeight(NP_TYPO_BODY) * NP_LINE_HEIGHT_TIGHT);
        NpThemeDrawTextColored(NP_TYPO_BODY, screen->playback.trackAlbum, (Vector2){infoX, infoY},
                               NpThemeGetColor(NP_COLOR_TEXT_SECONDARY));
        infoY += (int)(NpThemeGetLineHeight(NP_TYPO_BODY) * NP_LINE_HEIGHT_TIGHT);
    }

    // Time labels and progress bar
    char elapsedText[16], remainingText[16], tempText[16];
    FormatTime(screen->playback.trackPosition, elapsedText, sizeof(elapsedText));
    FormatTime(screen->playback.trackDuration - screen->playback.trackPosition, tempText, sizeof(tempText));
    snprintf(remainingText, sizeof(remainingText), "-%s", tempText);

    Rectangle progressTrack = ComputeProgressTrackRect(screen, &layout, infoX, infoWidth, infoY);
    float progress = screen->playback.trackDuration > 0 ?
                     screen->playback.trackPosition / screen->playback.trackDuration : 0.0f;
    NpProgressBar progressBar;
    NpProgressInit(&progressBar, progressTrack);
    NpProgressSetValue(&progressBar, progress);
    progressBar.showThumb = s_isScrubbing;

    // Use album art colors if available
    if (s_currentUIColors && s_currentUIColors->hasColors) {
        NpProgressDrawWithColors(&progressBar, &s_currentUIColors->trackBackground, &s_currentUIColors->accent, &s_currentUIColors->complementary);
    } else {
        NpProgressDraw(&progressBar);
    }

    if (normalWithArt) {
        float labelY = progressTrack.y - (NpThemeGetLineHeight(NP_TYPO_BODY) + NP_SPACING_SM);
        NpThemeDrawText(NP_TYPO_BODY, elapsedText, (Vector2){progressTrack.x, labelY});
        float remWidth = NpThemeMeasureTextWidth(NP_TYPO_BODY, remainingText);
        NpThemeDrawText(NP_TYPO_BODY, remainingText,
                        (Vector2){progressTrack.x + progressTrack.width - remWidth, labelY});
    } else if (compact) {
        // Barebones: time labels below the progress bar
        float labelY = progressTrack.y + progressTrack.height + NP_SPACING_SM;
        NpThemeDrawText(NP_TYPO_BODY, elapsedText, (Vector2){progressTrack.x + NP_SPACING_SM, labelY});
        float remWidth = NpThemeMeasureTextWidth(NP_TYPO_BODY, remainingText);
        NpThemeDrawText(NP_TYPO_BODY, remainingText,
                        (Vector2){progressTrack.x + progressTrack.width - remWidth - NP_SPACING_SM, labelY});
    } else {
        NpThemeDrawText(NP_TYPO_DETAIL, elapsedText, (Vector2){infoX, infoY});
        float remWidth = NpThemeMeasureTextWidth(NP_TYPO_DETAIL, remainingText);
        NpThemeDrawText(NP_TYPO_DETAIL, remainingText, (Vector2){infoX + infoWidth - remWidth, infoY});
    }

    if (normalWithArt) {
        const char *stateText = screen->playback.isPlaying ? "Playing" : "Paused";
        Color stateColor = screen->playback.isPlaying ? NpThemeGetColor(NP_COLOR_TEXT_PRIMARY)
                                                     : NpThemeGetColor(NP_COLOR_TEXT_SECONDARY);
        float textWidth = NpThemeMeasureTextWidth(NP_TYPO_BODY, stateText);
        float textX = progressTrack.x + (progressTrack.width - textWidth) * 0.5f;
        float textY = progressTrack.y + progressTrack.height + NP_SPACING_XS;
        NpThemeDrawTextColored(NP_TYPO_BODY, stateText, (Vector2){textX, textY}, stateColor);
    }

    // Draw scrubbing visual feedback
    if (s_isScrubbing && input && screen->actions.isScrubbing) {
        float speedMultiplier = screen->actions.scrubSpeedMultiplier;
        Vector2 mousePos = input->mousePos;

        char speedText[32];
        if (speedMultiplier < 0.5f) {
            snprintf(speedText, sizeof(speedText), "FINE %.1fx", speedMultiplier);
        } else if (speedMultiplier > 0.9f) {
            snprintf(speedText, sizeof(speedText), "1.0x");
        } else {
            snprintf(speedText, sizeof(speedText), "%.1fx", speedMultiplier);
        }

        float speedTextWidth = NpThemeMeasureTextWidth(NP_TYPO_BODY, speedText);
        float speedHeight = NpThemeGetLineHeight(NP_TYPO_BODY);
        float indicatorX = mousePos.x - speedTextWidth * 0.5f;
        float indicatorY = mousePos.y - 58;

        // Use contextual colors if available
        Color textAccent = (s_currentUIColors && s_currentUIColors->hasColors) ?
                           s_currentUIColors->accent : NpThemeGetColor(NP_COLOR_ACCENT);
        Color lineAccent = textAccent;
        lineAccent.a = 100;

        Rectangle indicatorBg = {indicatorX - 14, indicatorY - 10, speedTextWidth + 28, speedHeight + 16};
        DrawRectangleRounded(indicatorBg, 0.35f, 10, (Color){0, 0, 0, 200});
        NpThemeDrawTextColored(NP_TYPO_BODY, speedText, (Vector2){indicatorX, indicatorY}, textAccent);

        DrawLineEx((Vector2){mousePos.x, mousePos.y},
                   (Vector2){mousePos.x, progressTrack.y + progressTrack.height / 2},
                   2.0f, lineAccent);
    }

    // Playback control buttons - skip for normalWithArt mode (cleaner look)
    if (!normalWithArt) {
        Rectangle controlRects[NP_CONTROL_COUNT];
        Rectangle controlsBounds = BuildControlRects(screen, &layout, controlRects);
        Color controlsBg = NpThemeGetColor(NP_COLOR_PANEL_HOVER);
        if (compact) {
            DrawRectangleRec(controlsBounds, controlsBg);
        } else {
            DrawRectangleRounded(controlsBounds, 0.2f, 12, controlsBg);
        }

        const char *btnLabels[] = {"⇄", "<<", screen->playback.isPlaying ? "||" : ">", ">>", "↻"};
        bool btnActive[] = {screen->playback.shuffleEnabled, false, screen->playback.isPlaying, false, screen->playback.repeatEnabled};

        for (int i = 0; i < NP_CONTROL_COUNT; i++) {
            Rectangle btnRect = controlRects[i];
            Color bgColor, textColor;

            // Play/pause button (index 2) uses album art colors when available
            if (i == 2 && s_currentUIColors && s_currentUIColors->hasColors) {
                if (btnActive[i]) {
                    // Playing: use accent color background with contrasting text
                    bgColor = s_currentUIColors->accent;
                    // Use trackBackground color for text (it's designed to contrast with accent)
                    textColor = s_currentUIColors->trackBackground;
                } else {
                    // Paused: use trackBackground as bg with accent text
                    bgColor = s_currentUIColors->trackBackground;
                    textColor = s_currentUIColors->accent;
                }
            } else {
                // Default theme colors for other buttons
                bgColor = btnActive[i] ? NpThemeGetColor(NP_COLOR_ACCENT) : NpThemeGetColor(NP_COLOR_PANEL);
                textColor = btnActive[i] ? NpThemeGetColor(NP_COLOR_BG_DARK) : NpThemeGetColor(NP_COLOR_TEXT_PRIMARY);
            }

            DrawRectangleRounded(btnRect, 0.4f, 10, bgColor);
            NpLabelDrawCenteredInRect(NP_TYPO_TITLE, btnLabels[i], btnRect, &textColor);
        }
    }

    if (!layout.showUpNext) return;

    // Right column: Queue
    int rightColumnX = layout.rightColumnX;
    int queueY = bodyY;
    NpThemeDrawText(NP_TYPO_TITLE, "Up next", (Vector2){rightColumnX, queueY});
    queueY += (int)(NpThemeGetLineHeight(NP_TYPO_TITLE) * NP_LINE_HEIGHT_NORMAL);

    const char *queueArtists[] = {"Chromatics", "M83", "Cut Copy", "Yeah Yeah Yeahs"};
    const char *queueTitles[] = {"Cherry", "Midnight City", "Hearts on Fire", "Maps"};
    const char *queueDurations[] = {"4:36", "4:47", "4:23", "3:30"};

    const float queueItemHeight = 64.0f;
    const float queueItemSpacing = 72.0f;  // 8px gap between items

    for (int i = 0; i < 4; i++) {
        Rectangle itemBounds = {(float)rightColumnX, (float)queueY, (float)layout.rightColumnWidth, queueItemHeight};
        Color rowColor = NpThemeGetColor(NP_COLOR_PANEL_HOVER);
        if (i > 0) rowColor.a = 150;
        DrawRectangleRounded(itemBounds, 0.2f, 10, rowColor);
        DrawRectangleRoundedLines(itemBounds, 0.2f, 10, NpThemeGetColor(NP_COLOR_PANEL));

        // Vertically center the two lines of text within the item
        float artistY = queueY + (int)NP_SPACING_SM;
        float titleY = queueY + queueItemHeight - NP_SPACING_SM - NpThemeGetLineHeight(NP_TYPO_DETAIL);
        float durationY = queueY + (queueItemHeight - NpThemeGetLineHeight(NP_TYPO_DETAIL)) * 0.5f;

        NpThemeDrawText(NP_TYPO_BODY, queueArtists[i], (Vector2){rightColumnX + (int)NP_SPACING_SM, artistY});
        NpThemeDrawTextColored(NP_TYPO_DETAIL, queueTitles[i], (Vector2){rightColumnX + (int)NP_SPACING_SM, titleY},
                               NpThemeGetColor(NP_COLOR_TEXT_SECONDARY));

        float durWidth = NpThemeMeasureTextWidth(NP_TYPO_DETAIL, queueDurations[i]);
        NpThemeDrawTextColored(NP_TYPO_DETAIL, queueDurations[i],
                               (Vector2){rightColumnX + layout.rightColumnWidth - durWidth - (int)NP_SPACING_SM, durationY},
                               NpThemeGetColor(NP_COLOR_TEXT_SECONDARY));

        queueY += (int)queueItemSpacing;
    }

    // Volume HUD
    int volumeHudY = queueY + (int)NP_SPACING_XS;
    int volumeHudHeight = (int)screen->viewport.height - volumeHudY - (int)NP_SPACING_SM;
    if (volumeHudHeight < 80) volumeHudHeight = 80;

    Rectangle volumeHud = {(float)rightColumnX, (float)volumeHudY, (float)layout.rightColumnWidth, (float)volumeHudHeight};
    DrawRectangleRounded(volumeHud, 0.2f, 12, NpThemeGetColor(NP_COLOR_PANEL_HOVER));

    char volumeLabel[32];
    snprintf(volumeLabel, sizeof(volumeLabel), "Volume %d%%", screen->playback.volume);
    NpThemeDrawText(NP_TYPO_BODY, volumeLabel, (Vector2){rightColumnX + (int)NP_SPACING_SM, volumeHudY + (int)NP_SPACING_SM});

    char volumeValue[8];
    snprintf(volumeValue, sizeof(volumeValue), "%d", screen->playback.volume);
    float volWidth = NpThemeMeasureTextWidth(NP_TYPO_TITLE, volumeValue);
    NpThemeDrawText(NP_TYPO_TITLE, volumeValue, (Vector2){rightColumnX + layout.rightColumnWidth - volWidth - (int)NP_SPACING_SM, volumeHudY + (int)NP_SPACING_SM});

    // Volume bar
    int volumeBarY = volumeHudY + volumeHudHeight - (int)NP_SPACING_LG;
    NpProgressBar volumeBar;
    NpProgressInit(&volumeBar, (Rectangle){(float)(rightColumnX + (int)NP_SPACING_SM), (float)volumeBarY, (float)(layout.rightColumnWidth - (int)(NP_SPACING_SM * 2)), 10.0f});
    NpProgressSetValue(&volumeBar, (float)screen->playback.volume / 100.0f);
    NpProgressDraw(&volumeBar);
}

NpPlaybackActions *NpNowPlayingGetActions(NpNowPlayingScreen *screen) {
    return &screen->actions;
}

void NpNowPlayingSetPlayback(NpNowPlayingScreen *screen, const NpPlaybackState *playback) {
    if (playback) {
        screen->playback = *playback;
    }
}

void NpNowPlayingSetDisplayMode(NpNowPlayingScreen *screen, NpDisplayMode mode)
{
    if (!screen) return;
    if (mode < 0 || mode >= NP_DISPLAY_MODE_COUNT) mode = NP_DISPLAY_NORMAL;
    screen->displayMode = mode;
    s_isScrubbing = false;
    s_justFinishedScrubbing = false;
}
