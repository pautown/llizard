#include "np_overlay_lyrics.h"
#include "../core/np_theme.h"
#include "../screens/np_screen_now_playing.h"
#include "llz_sdk_media.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

// Configuration constants
#define LYRICS_VISIBLE_LINES_BEFORE 3
#define LYRICS_VISIBLE_LINES_AFTER 3
#define LYRICS_LINE_HEIGHT 48.0f
#define LYRICS_FONT_SIZE 28.0f
#define LYRICS_CURRENT_FONT_SIZE 34.0f
#define LYRICS_SCROLL_SPEED 8.0f
#define LYRICS_FADE_DISTANCE 2.0f
#define LYRICS_CENTER_Y_OFFSET 0.0f

// Internal cached lyrics data
static LlzLyricsData s_cachedLyrics = {0};
static bool s_lyricsLoaded = false;

// Clamp a value between min and max
static float Clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Calculate fade alpha based on distance from current line
static float CalculateLineFade(int lineIndex, int currentLine, int totalLines) {
    if (totalLines <= 0) return 0.0f;

    int distance = abs(lineIndex - currentLine);

    if (distance == 0) {
        return 1.0f;
    } else if (distance <= LYRICS_FADE_DISTANCE) {
        // Linear fade based on distance
        return 1.0f - (distance / (LYRICS_FADE_DISTANCE + 1.0f)) * 0.6f;
    } else {
        // Further lines get more faded
        float baseFade = 0.4f - (distance - LYRICS_FADE_DISTANCE) * 0.1f;
        return Clamp(baseFade, 0.15f, 0.4f);
    }
}

// Draw a single lyrics line with proper styling
static void DrawLyricsLine(const char *text, float x, float y, float fontSize,
                          Color color, bool isCurrent, float alpha) {
    Font font = NpThemeGetFont();
    float spacing = 1.5f;

    // Apply alpha to color
    Color finalColor = color;
    finalColor.a = (unsigned char)(finalColor.a * alpha);

    // Measure text for centering
    Vector2 measure = MeasureTextEx(font, text, fontSize, spacing);
    Vector2 pos = { x - measure.x / 2.0f, y };

    if (isCurrent) {
        // Draw subtle glow for current line
        Color glowColor = finalColor;
        glowColor.a = (unsigned char)(glowColor.a * 0.3f);
        for (int i = 1; i <= 2; i++) {
            Vector2 glowPos = { pos.x, pos.y + i };
            DrawTextEx(font, text, glowPos, fontSize, spacing, glowColor);
        }
    }

    DrawTextEx(font, text, pos, fontSize, spacing, finalColor);
}

// Draw "no lyrics" message
static void DrawNoLyricsMessage(Rectangle bounds, float alpha) {
    Font font = NpThemeGetFont();
    Color textColor = NpThemeGetColorAlpha(NP_COLOR_TEXT_SECONDARY, alpha * 0.7f);

    const char *message = "No lyrics available";
    float fontSize = 24.0f;
    float spacing = 1.5f;

    Vector2 measure = MeasureTextEx(font, message, fontSize, spacing);
    Vector2 pos = {
        bounds.x + (bounds.width - measure.x) / 2.0f,
        bounds.y + (bounds.height - measure.y) / 2.0f
    };

    DrawTextEx(font, message, pos, fontSize, spacing, textColor);
}

// Draw "unsynced lyrics" indicator
static void DrawUnsyncedIndicator(Rectangle bounds, float alpha) {
    Font font = NpThemeGetFont();
    Color textColor = NpThemeGetColorAlpha(NP_COLOR_TEXT_SECONDARY, alpha * 0.5f);

    const char *indicator = "Lyrics not synced - scroll to navigate";
    float fontSize = 14.0f;
    float spacing = 1.0f;

    Vector2 measure = MeasureTextEx(font, indicator, fontSize, spacing);
    Vector2 pos = {
        bounds.x + (bounds.width - measure.x) / 2.0f,
        bounds.y + bounds.height - 40.0f
    };

    DrawTextEx(font, indicator, pos, fontSize, spacing, textColor);
}

void NpLyricsOverlayInit(NpLyricsOverlay *overlay) {
    if (!overlay) return;

    printf("[LYRICS] NpLyricsOverlayInit: Initializing lyrics overlay\n");

    overlay->bounds = (Rectangle){0, 0, 800, 480};
    overlay->currentLineIndex = 0;
    overlay->lyricsHash[0] = '\0';
    overlay->hasLyrics = false;
    overlay->isSynced = false;
    overlay->scrollOffset = 0.0f;
    overlay->targetScrollOffset = 0.0f;

    // Clear cached lyrics
    if (s_lyricsLoaded && s_cachedLyrics.lines) {
        printf("[LYRICS] NpLyricsOverlayInit: Freeing existing cached lyrics\n");
        LlzLyricsFree(&s_cachedLyrics);
    }
    memset(&s_cachedLyrics, 0, sizeof(s_cachedLyrics));
    s_lyricsLoaded = false;
    printf("[LYRICS] NpLyricsOverlayInit: Initialization complete\n");
}

void NpLyricsOverlayShutdown(NpLyricsOverlay *overlay) {
    if (!overlay) return;

    printf("[LYRICS] NpLyricsOverlayShutdown: Shutting down lyrics overlay\n");

    // Free cached lyrics
    if (s_lyricsLoaded && s_cachedLyrics.lines) {
        printf("[LYRICS] NpLyricsOverlayShutdown: Freeing cached lyrics (lineCount=%d)\n", s_cachedLyrics.lineCount);
        LlzLyricsFree(&s_cachedLyrics);
    }
    memset(&s_cachedLyrics, 0, sizeof(s_cachedLyrics));
    s_lyricsLoaded = false;

    overlay->hasLyrics = false;
    overlay->lyricsHash[0] = '\0';
    printf("[LYRICS] NpLyricsOverlayShutdown: Shutdown complete\n");
}

void NpLyricsOverlayUpdate(NpLyricsOverlay *overlay, float deltaTime, int64_t positionMs) {
    if (!overlay) return;

    // Check if lyrics hash has changed (new track)
    char currentHash[64] = {0};
    if (LlzLyricsGetHash(currentHash, sizeof(currentHash))) {
        if (strcmp(currentHash, overlay->lyricsHash) != 0) {
            printf("[LYRICS] Update: Hash changed from '%s' to '%s' - reloading lyrics\n",
                   overlay->lyricsHash, currentHash);

            // Hash changed - need to reload lyrics
            if (s_lyricsLoaded && s_cachedLyrics.lines) {
                printf("[LYRICS] Update: Freeing old lyrics (lineCount=%d)\n", s_cachedLyrics.lineCount);
                LlzLyricsFree(&s_cachedLyrics);
            }
            memset(&s_cachedLyrics, 0, sizeof(s_cachedLyrics));
            s_lyricsLoaded = false;

            // Try to load new lyrics
            printf("[LYRICS] Update: Attempting to load new lyrics from Redis\n");
            if (LlzLyricsGet(&s_cachedLyrics)) {
                s_lyricsLoaded = true;
                overlay->hasLyrics = (s_cachedLyrics.lineCount > 0);
                overlay->isSynced = s_cachedLyrics.synced;
                strncpy(overlay->lyricsHash, currentHash, sizeof(overlay->lyricsHash) - 1);
                overlay->lyricsHash[sizeof(overlay->lyricsHash) - 1] = '\0';

                printf("[LYRICS] Update: Lyrics loaded successfully - lineCount=%d, synced=%d, hash='%s'\n",
                       s_cachedLyrics.lineCount, overlay->isSynced, overlay->lyricsHash);

                // Reset scroll position
                overlay->currentLineIndex = 0;
                overlay->scrollOffset = 0.0f;
                overlay->targetScrollOffset = 0.0f;
            } else {
                overlay->hasLyrics = false;
                overlay->isSynced = false;
                strncpy(overlay->lyricsHash, currentHash, sizeof(overlay->lyricsHash) - 1);
                overlay->lyricsHash[sizeof(overlay->lyricsHash) - 1] = '\0';
                printf("[LYRICS] Update: No lyrics available for hash '%s'\n", currentHash);
            }
        }
    } else if (!s_lyricsLoaded) {
        // No hash available but we haven't tried loading yet
        printf("[LYRICS] Update: No hash in Redis, attempting initial lyrics load\n");
        if (LlzLyricsGet(&s_cachedLyrics)) {
            s_lyricsLoaded = true;
            overlay->hasLyrics = (s_cachedLyrics.lineCount > 0);
            overlay->isSynced = s_cachedLyrics.synced;
            if (s_cachedLyrics.hash[0]) {
                strncpy(overlay->lyricsHash, s_cachedLyrics.hash, sizeof(overlay->lyricsHash) - 1);
                overlay->lyricsHash[sizeof(overlay->lyricsHash) - 1] = '\0';
            }
            printf("[LYRICS] Update: Initial lyrics loaded - lineCount=%d, synced=%d, hash='%s'\n",
                   s_cachedLyrics.lineCount, overlay->isSynced, overlay->lyricsHash);
        }
    }

    // Update current line based on playback position (for synced lyrics)
    if (overlay->hasLyrics && overlay->isSynced && s_lyricsLoaded) {
        int newLineIndex = LlzLyricsFindCurrentLine(positionMs, &s_cachedLyrics);
        if (newLineIndex >= 0 && newLineIndex != overlay->currentLineIndex) {
            printf("[LYRICS] Sync: Line changed from %d to %d at position %lldms\n",
                   overlay->currentLineIndex, newLineIndex, (long long)positionMs);
            overlay->currentLineIndex = newLineIndex;
            // Set target scroll offset to center the current line
            overlay->targetScrollOffset = (float)newLineIndex * LYRICS_LINE_HEIGHT;
        }
    }

    // Smooth scroll animation
    float scrollDiff = overlay->targetScrollOffset - overlay->scrollOffset;
    if (fabsf(scrollDiff) > 0.5f) {
        overlay->scrollOffset += scrollDiff * LYRICS_SCROLL_SPEED * deltaTime;
    } else {
        overlay->scrollOffset = overlay->targetScrollOffset;
    }
}

void NpLyricsOverlayDraw(const NpLyricsOverlay *overlay, float alpha,
                         const NpAlbumArtUIColors *uiColors) {
    if (!overlay) return;

    // If no lyrics available, show message
    if (!overlay->hasLyrics || !s_lyricsLoaded || s_cachedLyrics.lineCount == 0) {
        // Only log once per state change to avoid spam
        static bool s_lastHadLyrics = true;
        if (s_lastHadLyrics) {
            printf("[LYRICS] Draw: No lyrics to display (hasLyrics=%d, loaded=%d, lineCount=%d)\n",
                   overlay->hasLyrics, s_lyricsLoaded, s_cachedLyrics.lineCount);
            s_lastHadLyrics = false;
        }
        DrawNoLyricsMessage(overlay->bounds, alpha);
        return;
    }

    // Reset state tracker when we have lyrics
    static bool s_lastHadLyrics = true;
    if (!s_lastHadLyrics) {
        printf("[LYRICS] Draw: Rendering lyrics (lineCount=%d, currentLine=%d, synced=%d)\n",
               s_cachedLyrics.lineCount, overlay->currentLineIndex, overlay->isSynced);
        s_lastHadLyrics = true;
    }

    // Get colors
    Color textPrimary = NpThemeGetColorAlpha(NP_COLOR_TEXT_PRIMARY, alpha);
    Color textSecondary = NpThemeGetColorAlpha(NP_COLOR_TEXT_SECONDARY, alpha);
    Color accentColor;
    if (uiColors && uiColors->hasColors) {
        accentColor = ColorAlpha(uiColors->accent, alpha);
    } else {
        accentColor = NpThemeGetColorAlpha(NP_COLOR_ACCENT, alpha);
    }

    // Calculate center position
    float centerX = overlay->bounds.x + overlay->bounds.width / 2.0f;
    float centerY = overlay->bounds.y + overlay->bounds.height / 2.0f + LYRICS_CENTER_Y_OFFSET;

    // Calculate the Y offset for scrolling
    // Current line should be at centerY
    float baseY = centerY - overlay->scrollOffset;

    // Draw visible lyrics lines
    int lineCount = s_cachedLyrics.lineCount;
    int currentLine = overlay->currentLineIndex;

    // Calculate visible range
    int startLine = currentLine - LYRICS_VISIBLE_LINES_BEFORE - 2;
    int endLine = currentLine + LYRICS_VISIBLE_LINES_AFTER + 2;

    if (startLine < 0) startLine = 0;
    if (endLine >= lineCount) endLine = lineCount - 1;

    for (int i = startLine; i <= endLine; i++) {
        float lineY = baseY + (float)i * LYRICS_LINE_HEIGHT;

        // Skip lines that are outside the visible bounds
        if (lineY < overlay->bounds.y - LYRICS_LINE_HEIGHT ||
            lineY > overlay->bounds.y + overlay->bounds.height + LYRICS_LINE_HEIGHT) {
            continue;
        }

        // Calculate fade based on distance from current line
        float lineFade = CalculateLineFade(i, currentLine, lineCount);

        // Additional fade for lines near edges of viewport
        float edgeFadeTop = Clamp((lineY - overlay->bounds.y) / 60.0f, 0.0f, 1.0f);
        float edgeFadeBottom = Clamp((overlay->bounds.y + overlay->bounds.height - lineY) / 60.0f, 0.0f, 1.0f);
        float edgeFade = fminf(edgeFadeTop, edgeFadeBottom);

        float finalFade = lineFade * edgeFade;

        bool isCurrent = (i == currentLine);
        float fontSize = isCurrent ? LYRICS_CURRENT_FONT_SIZE : LYRICS_FONT_SIZE;
        Color lineColor = isCurrent ? accentColor : textPrimary;

        // Draw the lyrics line
        const char *lineText = s_cachedLyrics.lines[i].text;
        if (lineText && lineText[0]) {
            DrawLyricsLine(lineText, centerX, lineY, fontSize, lineColor, isCurrent, finalFade);
        }
    }

    // Draw unsynced indicator if lyrics are not synced
    if (!overlay->isSynced) {
        DrawUnsyncedIndicator(overlay->bounds, alpha);
    }

    // Draw subtle gradient overlays at top and bottom for fade effect
    // Top gradient
    for (int i = 0; i < 50; i++) {
        float gradientAlpha = 1.0f - (float)i / 50.0f;
        Color bgColor = NpThemeGetColor(NP_COLOR_BG_DARK);
        bgColor.a = (unsigned char)(bgColor.a * gradientAlpha * 0.7f * alpha);
        DrawRectangle(
            (int)overlay->bounds.x,
            (int)(overlay->bounds.y + i),
            (int)overlay->bounds.width,
            1,
            bgColor
        );
    }

    // Bottom gradient
    for (int i = 0; i < 50; i++) {
        float gradientAlpha = (float)i / 50.0f;
        Color bgColor = NpThemeGetColor(NP_COLOR_BG_DARK);
        bgColor.a = (unsigned char)(bgColor.a * gradientAlpha * 0.7f * alpha);
        DrawRectangle(
            (int)overlay->bounds.x,
            (int)(overlay->bounds.y + overlay->bounds.height - 50 + i),
            (int)overlay->bounds.width,
            1,
            bgColor
        );
    }
}

void NpLyricsOverlaySetBounds(NpLyricsOverlay *overlay, Rectangle bounds) {
    if (!overlay) return;
    overlay->bounds = bounds;
}

void NpLyricsOverlayScrollLines(NpLyricsOverlay *overlay, int lineDelta) {
    if (!overlay || !overlay->hasLyrics || !s_lyricsLoaded) return;

    int newLine = overlay->currentLineIndex + lineDelta;

    // Clamp to valid range
    if (newLine < 0) newLine = 0;
    if (newLine >= s_cachedLyrics.lineCount) newLine = s_cachedLyrics.lineCount - 1;

    printf("[LYRICS] Scroll: Manual scroll by %d lines (%d -> %d)\n",
           lineDelta, overlay->currentLineIndex, newLine);

    overlay->currentLineIndex = newLine;
    overlay->targetScrollOffset = (float)newLine * LYRICS_LINE_HEIGHT;
}

void NpLyricsOverlayJumpToLine(NpLyricsOverlay *overlay, int lineIndex) {
    if (!overlay || !overlay->hasLyrics || !s_lyricsLoaded) return;

    // Clamp to valid range
    if (lineIndex < 0) lineIndex = 0;
    if (lineIndex >= s_cachedLyrics.lineCount) lineIndex = s_cachedLyrics.lineCount - 1;

    printf("[LYRICS] Jump: Jumping to line %d (instant scroll)\n", lineIndex);

    overlay->currentLineIndex = lineIndex;
    overlay->targetScrollOffset = (float)lineIndex * LYRICS_LINE_HEIGHT;
    // Instant jump - no smooth scroll
    overlay->scrollOffset = overlay->targetScrollOffset;
}

int NpLyricsOverlayGetCurrentLine(const NpLyricsOverlay *overlay) {
    if (!overlay) return -1;
    return overlay->currentLineIndex;
}

int NpLyricsOverlayGetLineCount(const NpLyricsOverlay *overlay) {
    if (!overlay || !s_lyricsLoaded) return 0;
    return s_cachedLyrics.lineCount;
}

bool NpLyricsOverlayHasLyrics(const NpLyricsOverlay *overlay) {
    if (!overlay) return false;
    return overlay->hasLyrics;
}

bool NpLyricsOverlayIsSynced(const NpLyricsOverlay *overlay) {
    if (!overlay) return false;
    return overlay->isSynced;
}

void NpLyricsOverlayRefresh(NpLyricsOverlay *overlay) {
    if (!overlay) return;

    printf("[LYRICS] Refresh: Force reloading lyrics (current hash='%s')\n", overlay->lyricsHash);

    // Force reload by clearing the hash
    overlay->lyricsHash[0] = '\0';

    // Free cached lyrics
    if (s_lyricsLoaded && s_cachedLyrics.lines) {
        printf("[LYRICS] Refresh: Freeing cached lyrics (lineCount=%d)\n", s_cachedLyrics.lineCount);
        LlzLyricsFree(&s_cachedLyrics);
    }
    memset(&s_cachedLyrics, 0, sizeof(s_cachedLyrics));
    s_lyricsLoaded = false;
    overlay->hasLyrics = false;
    printf("[LYRICS] Refresh: Lyrics cache cleared, will reload on next update\n");
}

bool NpLyricsOverlayHasContent(const NpLyricsOverlay *overlay) {
    return NpLyricsOverlayHasLyrics(overlay);
}
