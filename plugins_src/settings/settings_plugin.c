#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

// Screen dimensions
static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;

// UI state - Two modes: NAVIGATE (scroll moves selection) and EDIT (scroll adjusts value)
typedef enum {
    MODE_NAVIGATE,
    MODE_EDIT
} SettingsMode;

static SettingsMode g_mode = MODE_NAVIGATE;
static int g_selectedItem = 0;
#define MENU_ITEM_COUNT 3

// Smooth scroll state (like main host)
static float g_scrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;

// Restart confirmation state
static bool g_restartConfirmActive = false;
static float g_restartSwipeProgress = 0.0f;
static float g_restartPulseAnim = 0.0f;
static int g_restartSwipeStartY = 0;
static bool g_restartSwipeTracking = false;
static float g_restartGlowPhase = 0.0f;
static float g_restartParticles[12][3]; // x offset, y offset, phase
#define RESTART_SWIPE_THRESHOLD 100

// Pending changes
static int g_pendingBrightness = 80;
static bool g_lyricsEnabled = false;
static bool g_hasChanges = false;
static bool g_isAutoBrightness = false;

// Animation state
static float g_animTime = 0.0f;
static float g_selectionAnim[MENU_ITEM_COUNT] = {0};
static float g_editModeAnim = 0.0f;
static float g_toggleAnim = 0.0f;
static float g_sliderPulse = 0.0f;
static float g_modeTransitionAnim = 0.0f;

// ============================================================================
// Color Palette - Modern Dark Theme
// ============================================================================
static const Color COLOR_BG_DARK = {10, 10, 16, 255};
static const Color COLOR_BG_GRADIENT_START = {16, 16, 24, 255};
static const Color COLOR_BG_GRADIENT_END = {24, 20, 32, 255};

static const Color COLOR_CARD = {28, 28, 40, 220};
static const Color COLOR_CARD_SELECTED = {38, 38, 54, 240};
static const Color COLOR_CARD_EDITING = {45, 42, 62, 250};
static const Color COLOR_CARD_BORDER = {55, 55, 75, 120};
static const Color COLOR_CARD_BORDER_SELECTED = {80, 80, 110, 180};

static const Color COLOR_ACCENT = {30, 215, 96, 255};      // Spotify green
static const Color COLOR_ACCENT_SOFT = {30, 215, 96, 80};
static const Color COLOR_ACCENT_GLOW = {30, 215, 96, 40};
static const Color COLOR_ACCENT_BRIGHT = {60, 235, 120, 255};

static const Color COLOR_TEXT_PRIMARY = {255, 255, 255, 255};
static const Color COLOR_TEXT_SECONDARY = {180, 180, 190, 255};
static const Color COLOR_TEXT_TERTIARY = {110, 110, 125, 255};
static const Color COLOR_TEXT_HINT = {90, 90, 105, 255};

static const Color COLOR_SLIDER_BG = {45, 45, 60, 255};
static const Color COLOR_SLIDER_FILL = {30, 215, 96, 255};

static const Color COLOR_TOGGLE_BG_OFF = {55, 55, 70, 255};
static const Color COLOR_TOGGLE_BG_ON = {30, 215, 96, 255};
static const Color COLOR_TOGGLE_KNOB = {255, 255, 255, 255};

static const Color COLOR_DANGER = {235, 70, 70, 255};
static const Color COLOR_DANGER_SOFT = {235, 70, 70, 80};
static const Color COLOR_DANGER_GLOW = {255, 80, 80, 50};

// ============================================================================
// Layout Constants - Improved proportions
// ============================================================================
#define HEADER_HEIGHT 80
#define FOOTER_HEIGHT 55
#define CARD_MARGIN_X 28
#define CARD_HEIGHT 100
#define CARD_SPACING 14
#define CARD_ROUNDNESS 0.10f
#define CARD_SEGMENTS 16

#define CONTENT_TOP (HEADER_HEIGHT + 8)
#define CONTENT_HEIGHT (g_screenHeight - HEADER_HEIGHT - FOOTER_HEIGHT - 16)

#define SLIDER_HEIGHT 10
#define SLIDER_THUMB_RADIUS 14
#define SLIDER_TRACK_ROUNDNESS 0.5f

#define TOGGLE_WIDTH 52
#define TOGGLE_HEIGHT 30
#define TOGGLE_KNOB_SIZE 24
#define TOGGLE_ROUNDNESS 0.5f

// ============================================================================
// Utility Functions
// ============================================================================
static float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

static float EaseOutBack(float t) {
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

static float EaseInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

// ============================================================================
// Scroll Management (like main host)
// ============================================================================
static float CalculateTargetScroll(int selected) {
    float itemTotalHeight = CARD_HEIGHT + CARD_SPACING;
    float totalListHeight = MENU_ITEM_COUNT * itemTotalHeight;
    float maxScroll = totalListHeight - CONTENT_HEIGHT;
    if (maxScroll < 0) maxScroll = 0;

    float selectedTop = selected * itemTotalHeight;
    float selectedBottom = selectedTop + CARD_HEIGHT;

    float visibleTop = g_targetScrollOffset;
    float visibleBottom = g_targetScrollOffset + CONTENT_HEIGHT;

    float topMargin = CARD_HEIGHT * 0.3f;
    float bottomMargin = CARD_HEIGHT * 0.5f;

    float newTarget = g_targetScrollOffset;

    if (selectedTop < visibleTop + topMargin) {
        newTarget = selectedTop - topMargin;
    } else if (selectedBottom > visibleBottom - bottomMargin) {
        newTarget = selectedBottom - CONTENT_HEIGHT + bottomMargin;
    }

    if (newTarget < 0) newTarget = 0;
    if (newTarget > maxScroll) newTarget = maxScroll;

    return newTarget;
}

static void UpdateScroll(float deltaTime) {
    float diff = g_targetScrollOffset - g_scrollOffset;
    float speed = 14.0f;
    g_scrollOffset += diff * speed * deltaTime;
    if (fabsf(diff) < 0.5f) {
        g_scrollOffset = g_targetScrollOffset;
    }
}

// ============================================================================
// Drawing Functions
// ============================================================================
static void DrawGradientBackground(void) {
    ClearBackground(COLOR_BG_DARK);
    DrawRectangleGradientV(0, 0, g_screenWidth, g_screenHeight,
                           COLOR_BG_GRADIENT_START, COLOR_BG_GRADIENT_END);

    // Subtle animated accent glow
    float glowPulse = 0.4f + 0.3f * sinf(g_animTime * 0.6f);
    Color glowColor = COLOR_ACCENT_GLOW;
    glowColor.a = (unsigned char)(25 * glowPulse);
    DrawCircleGradient(g_screenWidth - 80, 80, 250, glowColor, BLANK);

    // Secondary glow
    float glow2 = 0.3f + 0.2f * sinf(g_animTime * 0.4f + 1.0f);
    glowColor.a = (unsigned char)(15 * glow2);
    DrawCircleGradient(100, g_screenHeight - 100, 200, glowColor, BLANK);
}

static void DrawHeader(void) {
    float headerY = 22;

    // Title
    LlzDrawText("Settings", CARD_MARGIN_X, (int)headerY, LLZ_FONT_SIZE_TITLE, COLOR_TEXT_PRIMARY);

    // Mode indicator / subtitle
    const char *subtitle;
    Color subtitleColor;
    if (g_mode == MODE_EDIT) {
        subtitle = "Adjusting value - press select to confirm";
        subtitleColor = COLOR_ACCENT;
    } else if (g_hasChanges) {
        subtitle = "Changes saved automatically";
        subtitleColor = COLOR_ACCENT;
    } else {
        subtitle = "Scroll to navigate, press select to adjust";
        subtitleColor = COLOR_TEXT_TERTIARY;
    }
    LlzDrawText(subtitle, CARD_MARGIN_X, (int)headerY + 38, LLZ_FONT_SIZE_SMALL, subtitleColor);
}

static void DrawModernSlider(float x, float y, float width, int value, int maxValue, bool selected, bool editing, bool isAuto) {
    float progress = (float)value / (float)maxValue;
    float fillWidth = width * progress;

    // Track background with subtle inner shadow
    Rectangle trackBg = {x, y, width, SLIDER_HEIGHT};
    DrawRectangleRounded(trackBg, SLIDER_TRACK_ROUNDNESS, 10, COLOR_SLIDER_BG);

    // Fill
    if (fillWidth > 4) {
        Rectangle trackFill = {x, y, fillWidth, SLIDER_HEIGHT};
        Color fillColor = editing ? COLOR_ACCENT_BRIGHT : COLOR_SLIDER_FILL;
        DrawRectangleRounded(trackFill, SLIDER_TRACK_ROUNDNESS, 10, fillColor);

        // Shine effect
        Rectangle shine = {x + 2, y + 1, fillWidth - 4, SLIDER_HEIGHT / 2 - 1};
        if (shine.width > 0) {
            DrawRectangleRounded(shine, SLIDER_TRACK_ROUNDNESS, 8, (Color){255, 255, 255, 35});
        }
    }

    // Thumb
    float thumbX = x + fillWidth;
    float thumbY = y + SLIDER_HEIGHT / 2;

    if (editing && !isAuto) {
        // Pulsing glow when editing
        float pulseScale = 1.0f + 0.2f * sinf(g_sliderPulse * 5.0f);
        DrawCircle((int)thumbX, (int)thumbY, SLIDER_THUMB_RADIUS * pulseScale + 6, COLOR_ACCENT_SOFT);
        DrawCircle((int)thumbX, (int)thumbY, SLIDER_THUMB_RADIUS + 3, COLOR_ACCENT);
    } else if (selected) {
        DrawCircle((int)thumbX, (int)thumbY, SLIDER_THUMB_RADIUS + 2, COLOR_ACCENT_SOFT);
    }

    // Main thumb
    DrawCircle((int)thumbX, (int)thumbY, SLIDER_THUMB_RADIUS, COLOR_TOGGLE_KNOB);
    DrawCircle((int)thumbX, (int)thumbY, 5, COLOR_ACCENT);
}

static void DrawModernToggle(float x, float y, bool enabled, bool selected, bool editing, float animProgress) {
    float knobProgress = EaseOutCubic(animProgress);
    float knobX = Lerp(x + TOGGLE_KNOB_SIZE/2 + 3, x + TOGGLE_WIDTH - TOGGLE_KNOB_SIZE/2 - 3, knobProgress);

    Color bgColor = ColorLerp(COLOR_TOGGLE_BG_OFF, COLOR_TOGGLE_BG_ON, knobProgress);

    // Glow when editing or selected
    if (editing) {
        float pulse = 0.6f + 0.4f * sinf(g_animTime * 5.0f);
        Color glowColor = COLOR_ACCENT;
        glowColor.a = (unsigned char)(80 * pulse);
        DrawRectangleRounded((Rectangle){x - 5, y - 5, TOGGLE_WIDTH + 10, TOGGLE_HEIGHT + 10},
                             TOGGLE_ROUNDNESS, 12, glowColor);
    } else if (selected) {
        DrawRectangleRounded((Rectangle){x - 3, y - 3, TOGGLE_WIDTH + 6, TOGGLE_HEIGHT + 6},
                             TOGGLE_ROUNDNESS, 12, COLOR_ACCENT_SOFT);
    }

    // Background
    Rectangle toggleBg = {x, y, TOGGLE_WIDTH, TOGGLE_HEIGHT};
    DrawRectangleRounded(toggleBg, TOGGLE_ROUNDNESS, 12, bgColor);

    // Knob shadow
    DrawCircle((int)knobX, (int)(y + TOGGLE_HEIGHT/2 + 2), TOGGLE_KNOB_SIZE/2 - 1, (Color){0, 0, 0, 50});

    // Knob
    DrawCircle((int)knobX, (int)(y + TOGGLE_HEIGHT/2), TOGGLE_KNOB_SIZE/2, COLOR_TOGGLE_KNOB);
}

static void DrawSettingCard(int index, const char *title, const char *description,
                            float y, bool selected, bool editing, float selectionAnim) {
    float cardX = CARD_MARGIN_X;
    float cardWidth = g_screenWidth - CARD_MARGIN_X * 2;

    // Card color based on state
    Color cardColor;
    if (editing) {
        cardColor = ColorLerp(COLOR_CARD_SELECTED, COLOR_CARD_EDITING, g_editModeAnim);
    } else {
        cardColor = ColorLerp(COLOR_CARD, COLOR_CARD_SELECTED, EaseOutCubic(selectionAnim));
    }

    // Subtle lift when selected
    float liftOffset = selected ? -3.0f * selectionAnim : 0.0f;
    float cardY = y + liftOffset;

    Rectangle cardRect = {cardX, cardY, cardWidth, CARD_HEIGHT};

    // Shadow
    if (selected) {
        Color shadowColor = {0, 0, 0, (unsigned char)(50 * selectionAnim)};
        DrawRectangleRounded((Rectangle){cardX + 3, cardY + 5, cardWidth, CARD_HEIGHT},
                             CARD_ROUNDNESS, CARD_SEGMENTS, shadowColor);
    }

    // Card background
    DrawRectangleRounded(cardRect, CARD_ROUNDNESS, CARD_SEGMENTS, cardColor);

    // Selection indicator bar (left edge)
    if (selectionAnim > 0.01f) {
        Color indicatorColor = editing ? COLOR_ACCENT_BRIGHT : COLOR_ACCENT;
        indicatorColor.a = (unsigned char)(255 * selectionAnim);
        float barHeight = CARD_HEIGHT * (0.4f + 0.6f * selectionAnim);
        float barY = cardY + (CARD_HEIGHT - barHeight) / 2;
        DrawRectangleRounded((Rectangle){cardX, barY, 4, barHeight}, 0.5f, 4, indicatorColor);
    }

    // Border
    Color borderColor = selected ? COLOR_CARD_BORDER_SELECTED : COLOR_CARD_BORDER;
    if (editing) {
        borderColor = COLOR_ACCENT;
        borderColor.a = 150;
    }
    DrawRectangleRoundedLinesEx(cardRect, CARD_ROUNDNESS, CARD_SEGMENTS, 1.0f, borderColor);

    // Text content
    float textX = cardX + 22;
    float textY = cardY + 18;
    LlzDrawText(title, (int)textX, (int)textY, LLZ_FONT_SIZE_LARGE - 2, COLOR_TEXT_PRIMARY);
    LlzDrawText(description, (int)textX, (int)textY + 30, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_SECONDARY);

    // Control widget
    float controlY = cardY + CARD_HEIGHT/2;
    float controlEndX = cardX + cardWidth - 22;

    if (index == 0) {
        // Brightness
        if (g_isAutoBrightness) {
            Color modeColor = selected ? COLOR_ACCENT : COLOR_TEXT_SECONDARY;
            const char *autoText = "AUTO";
            int autoWidth = LlzMeasureText(autoText, LLZ_FONT_SIZE_NORMAL);

            Rectangle badgeRect = {controlEndX - autoWidth - 18, controlY - 12, (float)autoWidth + 14, 26};
            DrawRectangleRounded(badgeRect, 0.4f, 8, COLOR_ACCENT_SOFT);
            LlzDrawText(autoText, (int)(controlEndX - autoWidth - 11), (int)controlY - 7, LLZ_FONT_SIZE_NORMAL, modeColor);

            int lux = LlzConfigReadAmbientLight();
            if (lux >= 0) {
                char luxText[32];
                snprintf(luxText, sizeof(luxText), "%d lux", lux);
                int luxWidth = LlzMeasureText(luxText, LLZ_FONT_SIZE_SMALL);
                LlzDrawText(luxText, (int)(badgeRect.x - luxWidth - 14), (int)controlY - 4,
                           LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);
            }
        } else {
            float sliderWidth = 180;
            float sliderX = controlEndX - sliderWidth - 55;
            DrawModernSlider(sliderX, controlY - SLIDER_HEIGHT/2, sliderWidth, g_pendingBrightness, 100, selected, editing, false);

            char valueText[16];
            snprintf(valueText, sizeof(valueText), "%d%%", g_pendingBrightness);
            Color valueColor = editing ? COLOR_ACCENT_BRIGHT : COLOR_TEXT_PRIMARY;
            LlzDrawText(valueText, (int)(controlEndX - 45), (int)controlY - 10, LLZ_FONT_SIZE_NORMAL, valueColor);
        }

        // Hint for edit mode
        if (selected && !editing && !g_isAutoBrightness) {
            LlzDrawText("select to adjust", (int)textX, (int)textY + 52, 12, COLOR_TEXT_HINT);
        }
    } else if (index == 1) {
        // Lyrics toggle
        float toggleX = controlEndX - TOGGLE_WIDTH;
        float toggleY = controlY - TOGGLE_HEIGHT/2;
        DrawModernToggle(toggleX, toggleY, g_lyricsEnabled, selected, editing, g_toggleAnim);

        const char *status = g_lyricsEnabled ? "On" : "Off";
        Color statusColor = g_lyricsEnabled ? COLOR_ACCENT : COLOR_TEXT_TERTIARY;
        int statusWidth = LlzMeasureText(status, LLZ_FONT_SIZE_NORMAL);
        LlzDrawText(status, (int)(toggleX - statusWidth - 14), (int)controlY - 7, LLZ_FONT_SIZE_NORMAL, statusColor);

        if (selected && !editing) {
            LlzDrawText("select to toggle", (int)textX, (int)textY + 52, 12, COLOR_TEXT_HINT);
        }
    } else if (index == 2) {
        // Restart
        const char *restartText = "Tap or select";
        Color restartColor = selected ? COLOR_DANGER : COLOR_TEXT_SECONDARY;
        int restartWidth = LlzMeasureText(restartText, LLZ_FONT_SIZE_NORMAL);
        LlzDrawText(restartText, (int)(controlEndX - restartWidth), (int)controlY - 7, LLZ_FONT_SIZE_NORMAL, restartColor);

        if (selected) {
            LlzDrawText("opens confirmation", (int)textX, (int)textY + 52, 12, COLOR_TEXT_HINT);
        }
    }
}

static void DrawFooter(void) {
    float footerY = g_screenHeight - FOOTER_HEIGHT + 10;

    // Separator
    DrawRectangle(CARD_MARGIN_X, (int)footerY - 12, g_screenWidth - CARD_MARGIN_X * 2, 1,
                  (Color){55, 55, 75, 100});

    // Navigation hints
    const char *hint;
    if (g_mode == MODE_EDIT) {
        hint = "Scroll: adjust | Select: confirm | Back: cancel";
    } else {
        hint = "Scroll: navigate | Select: edit | Back: exit";
    }
    LlzDrawText(hint, CARD_MARGIN_X, (int)footerY, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);

    // Config path
    const char *configPath =
#ifdef PLATFORM_DRM
        "/var/llizard/config.ini";
#else
        "./llizard_config.ini";
#endif
    int pathWidth = LlzMeasureText(configPath, 12);
    LlzDrawText(configPath, g_screenWidth - pathWidth - CARD_MARGIN_X, (int)footerY + 3, 12,
               (Color){70, 70, 85, 255});
}

static void DrawRestartConfirmation(void) {
    if (!g_restartConfirmActive) return;

    // Animated dim overlay
    float dimAlpha = 180 + 20 * sinf(g_restartPulseAnim * 2.0f);
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, (unsigned char)dimAlpha});

    // Floating particles around the panel
    for (int i = 0; i < 12; i++) {
        float px = g_screenWidth/2 + g_restartParticles[i][0] + 30 * sinf(g_restartPulseAnim * 0.8f + g_restartParticles[i][2]);
        float py = g_screenHeight/2 + g_restartParticles[i][1] + 20 * cosf(g_restartPulseAnim * 0.6f + g_restartParticles[i][2]);
        float particleAlpha = 0.3f + 0.3f * sinf(g_restartPulseAnim * 2.0f + g_restartParticles[i][2]);
        Color particleColor = COLOR_DANGER;
        particleColor.a = (unsigned char)(particleAlpha * 100);
        DrawCircle((int)px, (int)py, 3 + 2 * sinf(g_restartPulseAnim * 3.0f + i), particleColor);
    }

    // Panel with breathing glow
    float panelWidth = 380;
    float panelHeight = 260;
    float panelX = (g_screenWidth - panelWidth) / 2;
    float panelY = (g_screenHeight - panelHeight) / 2;

    // Outer glow
    float glowPulse = 0.5f + 0.3f * sinf(g_restartGlowPhase * 2.5f);
    for (int i = 3; i >= 1; i--) {
        Color glowColor = COLOR_DANGER_GLOW;
        glowColor.a = (unsigned char)(30 * glowPulse / i);
        DrawRectangleRounded((Rectangle){panelX - i*4, panelY - i*4, panelWidth + i*8, panelHeight + i*8},
                             0.08f, 12, glowColor);
    }

    // Panel background
    Rectangle panelRect = {panelX, panelY, panelWidth, panelHeight};
    DrawRectangleRounded(panelRect, 0.08f, 12, (Color){28, 28, 40, 250});
    DrawRectangleRoundedLinesEx(panelRect, 0.08f, 12, 2.0f, COLOR_DANGER_SOFT);

    // Warning icon - animated
    float iconCenterX = g_screenWidth / 2;
    float iconY = panelY + 35;
    float breathScale = 1.0f + 0.08f * sinf(g_restartPulseAnim * 3.0f);

    // Triangle with gradient effect
    float triSize = 28 * breathScale;
    Vector2 p1 = {iconCenterX, iconY};
    Vector2 p2 = {iconCenterX - triSize, iconY + triSize * 1.7f};
    Vector2 p3 = {iconCenterX + triSize, iconY + triSize * 1.7f};
    DrawTriangle(p1, p2, p3, COLOR_DANGER);

    // Inner highlight
    float innerSize = triSize * 0.6f;
    float innerY = iconY + triSize * 0.4f;
    DrawTriangle(
        (Vector2){iconCenterX, innerY},
        (Vector2){iconCenterX - innerSize, innerY + innerSize * 1.5f},
        (Vector2){iconCenterX + innerSize, innerY + innerSize * 1.5f},
        (Color){255, 100, 100, 80}
    );

    // Exclamation
    LlzDrawTextCentered("!", (int)iconCenterX, (int)iconY + 18, LLZ_FONT_SIZE_LARGE + 4, COLOR_TEXT_PRIMARY);

    // Title
    const char *title = "Restart Device?";
    LlzDrawTextCentered(title, g_screenWidth / 2, (int)panelY + 95, LLZ_FONT_SIZE_TITLE, COLOR_TEXT_PRIMARY);

    // Instructions
    const char *instr = "Swipe up to confirm";
    LlzDrawTextCentered(instr, g_screenWidth / 2, (int)panelY + 135, LLZ_FONT_SIZE_NORMAL, COLOR_TEXT_SECONDARY);

    // Progress bar with smooth fill
    float barWidth = panelWidth - 50;
    float barX = panelX + 25;
    float barY = panelY + 175;
    float barHeight = 14;

    // Bar background
    DrawRectangleRounded((Rectangle){barX, barY, barWidth, barHeight}, 0.5f, 10, (Color){45, 45, 60, 255});

    // Progress fill with glow
    if (g_restartSwipeProgress > 0) {
        float fillWidth = barWidth * EaseOutCubic(g_restartSwipeProgress);
        Color fillColor = g_restartSwipeProgress >= 1.0f ? COLOR_DANGER : COLOR_ACCENT;

        // Glow under fill
        if (fillWidth > 8) {
            Color glowFill = fillColor;
            glowFill.a = 60;
            DrawRectangleRounded((Rectangle){barX - 2, barY - 2, fillWidth + 4, barHeight + 4}, 0.5f, 10, glowFill);
        }

        DrawRectangleRounded((Rectangle){barX, barY, fillWidth, barHeight}, 0.5f, 10, fillColor);

        // Shine on fill
        if (fillWidth > 6) {
            DrawRectangleRounded((Rectangle){barX + 2, barY + 2, fillWidth - 4, barHeight/2 - 2}, 0.5f, 8, (Color){255, 255, 255, 40});
        }
    }

    // Cancel hint
    const char *cancelHint = "Tap outside or press Back to cancel";
    LlzDrawTextCentered(cancelHint, g_screenWidth / 2, (int)panelY + 215, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);

    // Animated swipe arrows
    float arrowY = panelY + panelHeight + 25;
    for (int i = 0; i < 3; i++) {
        float phase = g_restartPulseAnim * 4.0f - i * 0.5f;
        float offset = fmodf(phase, 3.14159f * 2.0f);
        float alpha = 0.3f + 0.4f * (1.0f - offset / (3.14159f * 2.0f));
        float yOffset = -20 * (offset / (3.14159f * 2.0f));

        Color arrowColor = COLOR_TEXT_SECONDARY;
        arrowColor.a = (unsigned char)(alpha * 200);

        int arrowCenterX = g_screenWidth / 2;
        DrawTriangle(
            (Vector2){arrowCenterX, arrowY + yOffset - 8},
            (Vector2){arrowCenterX - 12, arrowY + yOffset + 6},
            (Vector2){arrowCenterX + 12, arrowY + yOffset + 6},
            arrowColor
        );
    }
}

// ============================================================================
// Plugin Callbacks
// ============================================================================
static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;
    g_selectedItem = 0;
    g_mode = MODE_NAVIGATE;
    g_animTime = 0.0f;
    g_sliderPulse = 0.0f;
    g_scrollOffset = 0.0f;
    g_targetScrollOffset = 0.0f;
    g_editModeAnim = 0.0f;
    g_modeTransitionAnim = 0.0f;

    // Reset restart state
    g_restartConfirmActive = false;
    g_restartSwipeProgress = 0.0f;
    g_restartPulseAnim = 0.0f;
    g_restartSwipeTracking = false;
    g_restartGlowPhase = 0.0f;

    // Initialize particles
    for (int i = 0; i < 12; i++) {
        g_restartParticles[i][0] = (float)((i % 4) - 1.5f) * 120;
        g_restartParticles[i][1] = (float)((i / 4) - 1) * 100;
        g_restartParticles[i][2] = (float)i * 0.5f;
    }

    // Reset animations
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        g_selectionAnim[i] = (i == 0) ? 1.0f : 0.0f;
    }

    // Initialize media for lyrics
    LlzMediaInit(NULL);

    // Load config
    const LlzConfig *config = LlzConfigGet();
    g_isAutoBrightness = (config->brightness == LLZ_BRIGHTNESS_AUTO);
    g_pendingBrightness = g_isAutoBrightness ? 80 : config->brightness;
    g_lyricsEnabled = LlzLyricsIsEnabled();
    g_toggleAnim = g_lyricsEnabled ? 1.0f : 0.0f;
    g_hasChanges = false;

    printf("Settings plugin initialized (brightness=%s%d, lyrics=%s)\n",
           g_isAutoBrightness ? "AUTO/" : "", g_pendingBrightness,
           g_lyricsEnabled ? "ON" : "OFF");
}

static Rectangle GetMenuItemBounds(int index) {
    float cardY = CONTENT_TOP + index * (CARD_HEIGHT + CARD_SPACING) - g_scrollOffset;
    return (Rectangle){CARD_MARGIN_X, cardY, g_screenWidth - CARD_MARGIN_X * 2, CARD_HEIGHT};
}

static void PluginUpdate(const LlzInputState *hostInput, float deltaTime) {
    LlzInputState emptyInput = {0};
    const LlzInputState *input = hostInput ? hostInput : &emptyInput;

    // Update animations
    g_animTime += deltaTime;
    g_sliderPulse += deltaTime;

    // Selection animations
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        float target = (i == g_selectedItem) ? 1.0f : 0.0f;
        g_selectionAnim[i] = Lerp(g_selectionAnim[i], target, deltaTime * 14.0f);
    }

    // Edit mode animation
    float editTarget = (g_mode == MODE_EDIT) ? 1.0f : 0.0f;
    g_editModeAnim = Lerp(g_editModeAnim, editTarget, deltaTime * 12.0f);

    // Toggle animation
    float toggleTarget = g_lyricsEnabled ? 1.0f : 0.0f;
    g_toggleAnim = Lerp(g_toggleAnim, toggleTarget, deltaTime * 10.0f);

    // Scroll animation
    UpdateScroll(deltaTime);

    // Handle restart confirmation mode
    if (g_restartConfirmActive) {
        g_restartPulseAnim += deltaTime;
        g_restartGlowPhase += deltaTime;

        // Swipe tracking
        if (input->mousePressed || input->dragActive) {
            if (!g_restartSwipeTracking) {
                g_restartSwipeTracking = true;
                g_restartSwipeStartY = (int)input->mousePos.y;
            } else {
                int swipeDelta = g_restartSwipeStartY - (int)input->mousePos.y;
                if (swipeDelta > 0) {
                    g_restartSwipeProgress = (float)swipeDelta / RESTART_SWIPE_THRESHOLD;
                    if (g_restartSwipeProgress > 1.0f) g_restartSwipeProgress = 1.0f;
                } else {
                    g_restartSwipeProgress *= 0.9f; // Smooth decay
                }
            }
        } else if (g_restartSwipeTracking) {
            if (g_restartSwipeProgress >= 1.0f) {
                printf("Restarting device...\n");
#ifdef PLATFORM_DRM
                (void)system("reboot");
#else
                printf("Desktop: Would restart here\n");
                g_restartConfirmActive = false;
#endif
            } else {
                g_restartSwipeProgress = 0.0f;
            }
            g_restartSwipeTracking = false;
        }

        // Direct swipe gesture
        if (input->swipeUp) {
            printf("Restarting device...\n");
#ifdef PLATFORM_DRM
            (void)system("reboot");
#else
            printf("Desktop: Would restart here\n");
            g_restartConfirmActive = false;
#endif
        }

        // Cancel
        if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
            g_restartConfirmActive = false;
            g_restartSwipeProgress = 0.0f;
            g_restartSwipeTracking = false;
        }

        // Tap outside
        if ((input->tap || input->mouseJustPressed) && !g_restartSwipeTracking) {
            Vector2 tapPos = input->tap ? input->tapPosition : input->mousePos;
            float panelWidth = 380;
            float panelHeight = 260;
            float panelX = (g_screenWidth - panelWidth) / 2;
            float panelY = (g_screenHeight - panelHeight) / 2;
            Rectangle panelRect = {panelX, panelY, panelWidth, panelHeight};
            if (!CheckCollisionPointRec(tapPos, panelRect)) {
                g_restartConfirmActive = false;
                g_restartSwipeProgress = 0.0f;
            }
        }

        return;
    }

    // === NAVIGATION MODE ===
    if (g_mode == MODE_NAVIGATE) {
        // Scroll wheel navigates selection
        if (input->scrollDelta != 0) {
            int delta = (input->scrollDelta > 0) ? 1 : -1;
            g_selectedItem += delta;
            if (g_selectedItem < 0) g_selectedItem = 0;
            if (g_selectedItem >= MENU_ITEM_COUNT) g_selectedItem = MENU_ITEM_COUNT - 1;
            g_targetScrollOffset = CalculateTargetScroll(g_selectedItem);
        }

        // Up/down buttons
        if (input->downPressed || IsKeyPressed(KEY_DOWN)) {
            g_selectedItem = (g_selectedItem + 1) % MENU_ITEM_COUNT;
            g_targetScrollOffset = CalculateTargetScroll(g_selectedItem);
        }
        if (input->upPressed || IsKeyPressed(KEY_UP)) {
            g_selectedItem = (g_selectedItem - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
            g_targetScrollOffset = CalculateTargetScroll(g_selectedItem);
        }

        // Select button enters edit mode (or activates for restart)
        if (input->selectPressed || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (g_selectedItem == 2) {
                // Restart - open confirmation
                g_restartConfirmActive = true;
                g_restartSwipeProgress = 0.0f;
                g_restartPulseAnim = 0.0f;
            } else if (g_selectedItem == 1) {
                // Lyrics toggle - just toggle immediately
                g_lyricsEnabled = !g_lyricsEnabled;
                g_hasChanges = true;
                LlzLyricsSetEnabled(g_lyricsEnabled);
            } else {
                // Brightness - enter edit mode
                g_mode = MODE_EDIT;
            }
        }

        // Tap to select or activate
        if (input->tap || input->mouseJustPressed) {
            Vector2 tapPos = input->tap ? input->tapPosition : input->mousePos;
            for (int i = 0; i < MENU_ITEM_COUNT; i++) {
                Rectangle bounds = GetMenuItemBounds(i);
                if (CheckCollisionPointRec(tapPos, bounds)) {
                    if (g_selectedItem != i) {
                        g_selectedItem = i;
                        g_targetScrollOffset = CalculateTargetScroll(g_selectedItem);
                    } else {
                        // Already selected - activate
                        if (i == 1) {
                            g_lyricsEnabled = !g_lyricsEnabled;
                            g_hasChanges = true;
                            LlzLyricsSetEnabled(g_lyricsEnabled);
                        } else if (i == 2) {
                            g_restartConfirmActive = true;
                            g_restartSwipeProgress = 0.0f;
                            g_restartPulseAnim = 0.0f;
                        } else {
                            g_mode = MODE_EDIT;
                        }
                    }
                    break;
                }
            }
        }

        // Back exits plugin
        if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
            g_wantsClose = true;
        }
    }
    // === EDIT MODE ===
    else if (g_mode == MODE_EDIT) {
        // Scroll wheel adjusts value
        if (input->scrollDelta != 0) {
            if (g_selectedItem == 0) {
                // Brightness
                int delta = (int)(input->scrollDelta * 5);
                if (g_isAutoBrightness) {
                    g_isAutoBrightness = false;
                    g_pendingBrightness = (delta < 0) ? 100 : 5;
                } else {
                    g_pendingBrightness += delta;
                    if (g_pendingBrightness < 5) {
                        g_isAutoBrightness = true;
                        g_pendingBrightness = 5;
                    }
                    if (g_pendingBrightness > 100) g_pendingBrightness = 100;
                }
                g_hasChanges = true;
                if (g_isAutoBrightness) {
                    LlzConfigSetAutoBrightness();
                } else {
                    LlzConfigSetBrightness(g_pendingBrightness);
                }
            }
        }

        // Left/right arrows adjust value
        bool leftPressed = IsKeyPressed(KEY_LEFT) || input->swipeRight;
        bool rightPressed = IsKeyPressed(KEY_RIGHT) || input->swipeLeft;

        if (g_selectedItem == 0 && (leftPressed || rightPressed)) {
            int delta = leftPressed ? -5 : 5;
            if (g_isAutoBrightness) {
                g_isAutoBrightness = false;
                g_pendingBrightness = (delta < 0) ? 100 : 5;
            } else {
                g_pendingBrightness += delta;
                if (g_pendingBrightness < 5) {
                    g_isAutoBrightness = true;
                    g_pendingBrightness = 5;
                }
                if (g_pendingBrightness > 100) g_pendingBrightness = 100;
            }
            g_hasChanges = true;
            if (g_isAutoBrightness) {
                LlzConfigSetAutoBrightness();
            } else {
                LlzConfigSetBrightness(g_pendingBrightness);
            }
        }

        // Select or back exits edit mode
        if (input->selectPressed || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
            input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
            g_mode = MODE_NAVIGATE;
        }

        // Tap outside current card exits edit mode
        if (input->tap || input->mouseJustPressed) {
            Vector2 tapPos = input->tap ? input->tapPosition : input->mousePos;
            Rectangle bounds = GetMenuItemBounds(g_selectedItem);
            if (!CheckCollisionPointRec(tapPos, bounds)) {
                g_mode = MODE_NAVIGATE;
            }
        }
    }
}

static void PluginDraw(void) {
    DrawGradientBackground();
    DrawHeader();

    // Clip content area
    BeginScissorMode(0, CONTENT_TOP, g_screenWidth, (int)CONTENT_HEIGHT);

    // Draw setting cards
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        float cardY = CONTENT_TOP + i * (CARD_HEIGHT + CARD_SPACING) - g_scrollOffset;

        // Skip if outside visible area
        if (cardY < CONTENT_TOP - CARD_HEIGHT || cardY > g_screenHeight) continue;

        bool selected = (i == g_selectedItem);
        bool editing = selected && (g_mode == MODE_EDIT);

        const char *titles[] = {"Brightness", "Lyrics", "Restart Device"};
        const char *descriptions[] = {
            g_isAutoBrightness ? "Auto-adjusts based on ambient light" : "Manual brightness control",
            "Show synchronized lyrics during playback",
            "Reboot the CarThing"
        };

        DrawSettingCard(i, titles[i], descriptions[i], cardY, selected, editing, g_selectionAnim[i]);
    }

    EndScissorMode();

    DrawFooter();
    DrawRestartConfirmation();
}

static void PluginShutdown(void) {
    if (g_hasChanges) {
        LlzConfigSave();
        printf("Settings saved: brightness=%s%d, lyrics=%s\n",
               g_isAutoBrightness ? "AUTO/" : "", g_pendingBrightness,
               g_lyricsEnabled ? "ON" : "OFF");
    }
    g_wantsClose = false;
    g_restartConfirmActive = false;
    printf("Settings plugin shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

static LlzPluginAPI g_api = {
    .name = "Settings",
    .description = "Brightness, lyrics, restart device",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
