#include "nowplaying/overlays/np_overlay_manager.h"
#include "nowplaying/overlays/np_overlay_template.h"
#include "nowplaying/overlays/np_overlay_clock.h"
#include "nowplaying/overlays/np_overlay_lyrics.h"
#include "nowplaying/core/np_theme.h"

// Static overlay instances
static NpTemplateOverlay g_templateOverlay;
static NpClockOverlay g_clockOverlay;
static NpLyricsOverlay g_lyricsOverlay;

void NpOverlayManagerInit(NpOverlayManager *mgr) {
    mgr->currentOverlay = NP_OVERLAY_NONE;
    mgr->pendingOverlay = NP_OVERLAY_NONE;
    mgr->isTransitioning = false;
    NpEffectInit(&mgr->fadeEffect);

    // Initialize overlays
    NpTemplateOverlayInit(&g_templateOverlay);
    NpClockOverlayInit(&g_clockOverlay);
    NpLyricsOverlayInit(&g_lyricsOverlay);
}

void NpOverlayManagerUpdate(NpOverlayManager *mgr, float deltaTime) {
    if (mgr->isTransitioning) {
        NpEffectUpdate(&mgr->fadeEffect, deltaTime);

        if (NpEffectIsFinished(&mgr->fadeEffect)) {
            if (mgr->fadeEffect.type == NP_EFFECT_FADE_OUT) {
                // Fade out complete, switch to pending overlay
                mgr->currentOverlay = mgr->pendingOverlay;
                mgr->pendingOverlay = NP_OVERLAY_NONE;

                if (mgr->currentOverlay != NP_OVERLAY_NONE) {
                    // Start fade in
                    NpEffectStart(&mgr->fadeEffect, NP_EFFECT_FADE_IN, 0.25f);
                } else {
                    mgr->isTransitioning = false;
                }
            } else {
                // Fade in complete
                mgr->isTransitioning = false;
            }
        }
    }

    // Update clock overlay if active
    if (mgr->currentOverlay == NP_OVERLAY_CLOCK) {
        NpClockOverlayUpdate(&g_clockOverlay, deltaTime);
    }
}

void NpOverlayManagerDraw(NpOverlayManager *mgr, Rectangle viewport, const NpAlbumArtUIColors *uiColors) {
    if (mgr->currentOverlay == NP_OVERLAY_NONE && !mgr->isTransitioning) {
        return;
    }

    float alpha = NpOverlayManagerGetAlpha(mgr);
    if (alpha <= 0.0f) return;

    // Draw semi-transparent background
    Color bg = NpThemeGetColor(NP_COLOR_BG_DARK);
    bg.a = (unsigned char)(alpha * 255.0f);
    DrawRectangleRec(viewport, bg);

    // Draw current overlay
    switch (mgr->currentOverlay) {
        case NP_OVERLAY_TEMPLATE:
            g_templateOverlay.bounds = viewport;
            NpTemplateOverlayDraw(&g_templateOverlay, alpha);
            break;
        case NP_OVERLAY_CLOCK:
            g_clockOverlay.bounds = viewport;
            NpClockOverlayDraw(&g_clockOverlay, alpha, uiColors);
            break;
        case NP_OVERLAY_LYRICS:
            g_lyricsOverlay.bounds = viewport;
            NpLyricsOverlayDraw(&g_lyricsOverlay, alpha, uiColors);
            break;
        default:
            break;
    }
}

void NpOverlayManagerShow(NpOverlayManager *mgr, NpOverlayType type) {
    if (type == mgr->currentOverlay && !mgr->isTransitioning) {
        return;  // Already showing this overlay
    }

    if (mgr->currentOverlay == NP_OVERLAY_NONE) {
        // No current overlay, just fade in
        mgr->currentOverlay = type;
        mgr->isTransitioning = true;
        NpEffectStart(&mgr->fadeEffect, NP_EFFECT_FADE_IN, 0.25f);
    } else {
        // Fade out current, then show new
        mgr->pendingOverlay = type;
        mgr->isTransitioning = true;
        NpEffectStart(&mgr->fadeEffect, NP_EFFECT_FADE_OUT, 0.2f);
    }
}

void NpOverlayManagerHide(NpOverlayManager *mgr) {
    if (mgr->currentOverlay == NP_OVERLAY_NONE) return;

    mgr->pendingOverlay = NP_OVERLAY_NONE;
    mgr->isTransitioning = true;
    NpEffectStart(&mgr->fadeEffect, NP_EFFECT_FADE_OUT, 0.2f);
}

bool NpOverlayManagerIsVisible(const NpOverlayManager *mgr) {
    return mgr->currentOverlay != NP_OVERLAY_NONE || mgr->isTransitioning;
}

NpOverlayType NpOverlayManagerGetCurrent(const NpOverlayManager *mgr) {
    return mgr->currentOverlay;
}

float NpOverlayManagerGetAlpha(const NpOverlayManager *mgr) {
    if (!mgr->isTransitioning) {
        return (mgr->currentOverlay != NP_OVERLAY_NONE) ? 1.0f : 0.0f;
    }
    return NpEffectGetAlpha((NpEffect*)&mgr->fadeEffect);
}

NpClockOverlay* NpOverlayManagerGetClock(NpOverlayManager *mgr) {
    (void)mgr;  // Unused, we use static instance
    return &g_clockOverlay;
}

NpLyricsOverlay* NpOverlayManagerGetLyrics(NpOverlayManager *mgr) {
    (void)mgr;  // Unused, we use static instance
    return &g_lyricsOverlay;
}

void NpOverlayManagerUpdateLyrics(NpOverlayManager *mgr, float deltaTime, int64_t positionMs) {
    // Update lyrics overlay if it's the current overlay
    if (mgr->currentOverlay == NP_OVERLAY_LYRICS) {
        NpLyricsOverlayUpdate(&g_lyricsOverlay, deltaTime, positionMs);
    }
}
