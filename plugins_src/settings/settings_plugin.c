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

// UI state
static int g_selectedItem = 0;
#define MENU_ITEM_COUNT 3

// Restart confirmation state
static bool g_restartConfirmActive = false;
static float g_restartSwipeProgress = 0.0f;
static float g_restartPulseAnim = 0.0f;
static int g_restartSwipeStartY = 0;
static bool g_restartSwipeTracking = false;
#define RESTART_SWIPE_THRESHOLD 80

// Pending changes
static int g_pendingBrightness = 80;
static bool g_lyricsEnabled = false;
static bool g_hasChanges = false;
static bool g_isAutoBrightness = false;

// Animation state
static float g_animTime = 0.0f;
static float g_selectionAnim[MENU_ITEM_COUNT] = {0};
static float g_toggleAnim = 0.0f;
static float g_sliderPulse = 0.0f;

// ============================================================================
// Color Palette - Modern Dark Theme
// ============================================================================
static const Color COLOR_BG_DARK = {12, 12, 18, 255};
static const Color COLOR_BG_GRADIENT_START = {18, 18, 28, 255};
static const Color COLOR_BG_GRADIENT_END = {28, 24, 38, 255};

static const Color COLOR_CARD = {32, 32, 44, 200};
static const Color COLOR_CARD_SELECTED = {42, 42, 58, 220};
static const Color COLOR_CARD_BORDER = {60, 60, 80, 100};

static const Color COLOR_ACCENT = {30, 215, 96, 255};      // Spotify green
static const Color COLOR_ACCENT_SOFT = {30, 215, 96, 60};
static const Color COLOR_ACCENT_GLOW = {30, 215, 96, 30};

static const Color COLOR_TEXT_PRIMARY = {255, 255, 255, 255};
static const Color COLOR_TEXT_SECONDARY = {170, 170, 180, 255};
static const Color COLOR_TEXT_TERTIARY = {120, 120, 135, 255};

static const Color COLOR_SLIDER_BG = {50, 50, 65, 255};
static const Color COLOR_SLIDER_FILL = {30, 215, 96, 255};

static const Color COLOR_TOGGLE_BG_OFF = {60, 60, 75, 255};
static const Color COLOR_TOGGLE_BG_ON = {30, 215, 96, 255};
static const Color COLOR_TOGGLE_KNOB = {255, 255, 255, 255};

static const Color COLOR_DANGER = {220, 60, 60, 255};
static const Color COLOR_DANGER_SOFT = {220, 60, 60, 60};
static const Color COLOR_DANGER_GLOW = {220, 60, 60, 30};

// ============================================================================
// Layout Constants
// ============================================================================
#define HEADER_HEIGHT 90
#define CARD_MARGIN_X 32
#define CARD_HEIGHT 110
#define CARD_SPACING 16
#define CARD_ROUNDNESS 0.12f
#define CARD_SEGMENTS 12

#define SLIDER_HEIGHT 8
#define SLIDER_THUMB_RADIUS 12
#define SLIDER_TRACK_ROUNDNESS 0.5f

#define TOGGLE_WIDTH 56
#define TOGGLE_HEIGHT 32
#define TOGGLE_KNOB_SIZE 26
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

static float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

// Note: Using raylib's ColorLerp() function for color interpolation

// ============================================================================
// Drawing Functions
// ============================================================================
static void DrawGradientBackground(void) {
    // Dark base
    ClearBackground(COLOR_BG_DARK);

    // Subtle gradient overlay
    DrawRectangleGradientV(0, 0, g_screenWidth, g_screenHeight,
                           COLOR_BG_GRADIENT_START, COLOR_BG_GRADIENT_END);

    // Subtle animated accent glow in corner
    float glowPulse = 0.5f + 0.5f * sinf(g_animTime * 0.8f);
    Color glowColor = COLOR_ACCENT_GLOW;
    glowColor.a = (unsigned char)(30 * glowPulse);

    DrawCircleGradient(g_screenWidth - 100, 100, 300, glowColor, BLANK);
}

static void DrawHeader(void) {
    float headerY = 28;

    // Title with SDK font
    LlzDrawText("Settings", CARD_MARGIN_X, (int)headerY, LLZ_FONT_SIZE_TITLE, COLOR_TEXT_PRIMARY);

    // Subtitle
    const char *subtitle = g_hasChanges ? "Changes saved automatically" : "Adjust your preferences";
    LlzDrawText(subtitle, CARD_MARGIN_X, (int)headerY + 44, LLZ_FONT_SIZE_NORMAL, COLOR_TEXT_TERTIARY);
}

static void DrawModernSlider(float x, float y, float width, int value, int maxValue, bool selected, bool isAuto) {
    float progress = (float)value / (float)maxValue;
    float fillWidth = width * progress;

    // Track background
    Rectangle trackBg = {x, y, width, SLIDER_HEIGHT};
    DrawRectangleRounded(trackBg, SLIDER_TRACK_ROUNDNESS, 8, COLOR_SLIDER_BG);

    // Fill gradient
    if (fillWidth > 4) {
        Rectangle trackFill = {x, y, fillWidth, SLIDER_HEIGHT};
        DrawRectangleRounded(trackFill, SLIDER_TRACK_ROUNDNESS, 8, COLOR_SLIDER_FILL);

        // Subtle shine on fill
        Rectangle shine = {x, y, fillWidth, SLIDER_HEIGHT / 2};
        Color shineColor = {255, 255, 255, 30};
        DrawRectangleRounded(shine, SLIDER_TRACK_ROUNDNESS, 8, shineColor);
    }

    // Thumb with glow effect when selected
    float thumbX = x + fillWidth;
    float thumbY = y + SLIDER_HEIGHT / 2;

    if (selected && !isAuto) {
        // Outer glow
        float pulseScale = 1.0f + 0.15f * sinf(g_sliderPulse * 4.0f);
        DrawCircle((int)thumbX, (int)thumbY, SLIDER_THUMB_RADIUS * pulseScale + 4, COLOR_ACCENT_SOFT);

        // Inner glow
        DrawCircle((int)thumbX, (int)thumbY, SLIDER_THUMB_RADIUS + 2, COLOR_ACCENT);
    }

    // Thumb
    DrawCircle((int)thumbX, (int)thumbY, SLIDER_THUMB_RADIUS, COLOR_TOGGLE_KNOB);

    // Inner dot
    DrawCircle((int)thumbX, (int)thumbY, 4, COLOR_ACCENT);
}

static void DrawModernToggle(float x, float y, bool enabled, bool selected, float animProgress) {
    // Animated toggle position
    float knobProgress = EaseOutCubic(animProgress);
    float knobX = Lerp(x + TOGGLE_KNOB_SIZE/2 + 3, x + TOGGLE_WIDTH - TOGGLE_KNOB_SIZE/2 - 3, knobProgress);

    // Background color transition
    Color bgColor = ColorLerp(COLOR_TOGGLE_BG_OFF, COLOR_TOGGLE_BG_ON, knobProgress);

    // Selection glow
    if (selected) {
        float pulse = 0.5f + 0.5f * sinf(g_animTime * 4.0f);
        Color glowColor = COLOR_ACCENT_SOFT;
        glowColor.a = (unsigned char)(60 * pulse);
        DrawRectangleRounded((Rectangle){x - 4, y - 4, TOGGLE_WIDTH + 8, TOGGLE_HEIGHT + 8},
                             TOGGLE_ROUNDNESS, 12, glowColor);
    }

    // Toggle background
    Rectangle toggleBg = {x, y, TOGGLE_WIDTH, TOGGLE_HEIGHT};
    DrawRectangleRounded(toggleBg, TOGGLE_ROUNDNESS, 12, bgColor);

    // Knob shadow
    DrawCircle((int)knobX, (int)(y + TOGGLE_HEIGHT/2 + 2), TOGGLE_KNOB_SIZE/2 - 1, (Color){0, 0, 0, 40});

    // Knob
    DrawCircle((int)knobX, (int)(y + TOGGLE_HEIGHT/2), TOGGLE_KNOB_SIZE/2, COLOR_TOGGLE_KNOB);
}

static void DrawSettingCard(int index, const char *title, const char *description,
                            float y, bool selected, float selectionAnim) {
    float cardX = CARD_MARGIN_X;
    float cardWidth = g_screenWidth - CARD_MARGIN_X * 2;

    // Card background with selection animation
    Color cardColor = ColorLerp(COLOR_CARD, COLOR_CARD_SELECTED, EaseOutCubic(selectionAnim));

    // Subtle lift effect when selected
    float liftOffset = selected ? -2.0f : 0.0f;
    float cardY = y + liftOffset;

    Rectangle cardRect = {cardX, cardY, cardWidth, CARD_HEIGHT};

    // Card shadow when selected
    if (selected) {
        Color shadowColor = {0, 0, 0, 40};
        DrawRectangleRounded((Rectangle){cardX + 2, cardY + 4, cardWidth, CARD_HEIGHT},
                             CARD_ROUNDNESS, CARD_SEGMENTS, shadowColor);
    }

    // Card background
    DrawRectangleRounded(cardRect, CARD_ROUNDNESS, CARD_SEGMENTS, cardColor);

    // Selection indicator bar
    if (selectionAnim > 0.01f) {
        Color indicatorColor = COLOR_ACCENT;
        indicatorColor.a = (unsigned char)(255 * selectionAnim);
        DrawRectangleRounded((Rectangle){cardX, cardY, 4, CARD_HEIGHT},
                             0.5f, 4, indicatorColor);
    }

    // Subtle border
    DrawRectangleRoundedLinesEx(cardRect, CARD_ROUNDNESS, CARD_SEGMENTS, 1.0f, COLOR_CARD_BORDER);

    // Title
    float textX = cardX + 24;
    float textY = cardY + 20;
    LlzDrawText(title, (int)textX, (int)textY, LLZ_FONT_SIZE_LARGE - 4, COLOR_TEXT_PRIMARY);

    // Description
    LlzDrawText(description, (int)textX, (int)textY + 32, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_SECONDARY);

    // Control widget area
    float controlY = cardY + CARD_HEIGHT/2 - 4;
    float controlEndX = cardX + cardWidth - 24;

    if (index == 0) {
        // Brightness control
        if (g_isAutoBrightness) {
            // Auto mode indicator
            Color modeColor = selected ? COLOR_ACCENT : COLOR_TEXT_SECONDARY;
            const char *autoText = "AUTO";
            int autoWidth = LlzMeasureText(autoText, LLZ_FONT_SIZE_NORMAL);

            // Auto badge
            Rectangle badgeRect = {controlEndX - autoWidth - 20, controlY - 4, (float)autoWidth + 16, 28};
            DrawRectangleRounded(badgeRect, 0.4f, 8, COLOR_ACCENT_SOFT);
            LlzDrawText(autoText, (int)(controlEndX - autoWidth - 12), (int)controlY, LLZ_FONT_SIZE_NORMAL, modeColor);

            // Show lux reading if available
            int lux = LlzConfigReadAmbientLight();
            if (lux >= 0) {
                char luxText[32];
                snprintf(luxText, sizeof(luxText), "%d lux", lux);
                int luxWidth = LlzMeasureText(luxText, LLZ_FONT_SIZE_SMALL);
                LlzDrawText(luxText, (int)(badgeRect.x - luxWidth - 16), (int)controlY + 4,
                           LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);
            }
        } else {
            // Manual brightness slider
            float sliderWidth = 200;
            float sliderX = controlEndX - sliderWidth - 60;
            DrawModernSlider(sliderX, controlY, sliderWidth, g_pendingBrightness, 100, selected, false);

            // Value display
            char valueText[16];
            snprintf(valueText, sizeof(valueText), "%d%%", g_pendingBrightness);
            LlzDrawText(valueText, (int)(controlEndX - 50), (int)controlY - 6, LLZ_FONT_SIZE_NORMAL, COLOR_TEXT_PRIMARY);
        }
    } else if (index == 1) {
        // Lyrics toggle
        float toggleX = controlEndX - TOGGLE_WIDTH;
        float toggleY = controlY - TOGGLE_HEIGHT/2 + 4;
        DrawModernToggle(toggleX, toggleY, g_lyricsEnabled, selected, g_toggleAnim);

        // Status label
        const char *status = g_lyricsEnabled ? "On" : "Off";
        Color statusColor = g_lyricsEnabled ? COLOR_ACCENT : COLOR_TEXT_TERTIARY;
        int statusWidth = LlzMeasureText(status, LLZ_FONT_SIZE_NORMAL);
        LlzDrawText(status, (int)(toggleX - statusWidth - 16), (int)controlY, LLZ_FONT_SIZE_NORMAL, statusColor);
    } else if (index == 2) {
        // Restart device indicator
        const char *restartText = g_restartConfirmActive ? "Swipe Up" : "Tap to restart";
        Color restartColor = selected ? COLOR_DANGER : COLOR_TEXT_SECONDARY;
        int restartWidth = LlzMeasureText(restartText, LLZ_FONT_SIZE_NORMAL);
        LlzDrawText(restartText, (int)(controlEndX - restartWidth), (int)controlY, LLZ_FONT_SIZE_NORMAL, restartColor);
    }
}

static void DrawFooter(void) {
    float footerY = g_screenHeight - 50;

    // Separator line
    DrawRectangle(CARD_MARGIN_X, (int)footerY - 16, g_screenWidth - CARD_MARGIN_X * 2, 1,
                  (Color){60, 60, 80, 100});

    // Back instruction
    LlzDrawText("Press Back to exit", CARD_MARGIN_X, (int)footerY, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);

    // Config path
    const char *configPath =
#ifdef PLATFORM_DRM
        "/var/llizard/config.ini";
#else
        "./llizard_config.ini";
#endif
    int pathWidth = LlzMeasureText(configPath, 14);
    LlzDrawText(configPath, g_screenWidth - pathWidth - CARD_MARGIN_X, (int)footerY, 14,
               (Color){80, 80, 95, 255});
}

static void DrawRestartConfirmation(void) {
    if (!g_restartConfirmActive) return;

    // Dim overlay
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 180});

    // Center panel
    float panelWidth = 400;
    float panelHeight = 280;
    float panelX = (g_screenWidth - panelWidth) / 2;
    float panelY = (g_screenHeight - panelHeight) / 2;

    Rectangle panelRect = {panelX, panelY, panelWidth, panelHeight};
    DrawRectangleRounded(panelRect, 0.08f, 12, (Color){32, 32, 44, 250});
    DrawRectangleRoundedLinesEx(panelRect, 0.08f, 12, 2.0f, COLOR_DANGER_SOFT);

    // Warning icon (triangle with !)
    float iconCenterX = g_screenWidth / 2;
    float iconY = panelY + 40;
    float pulseScale = 1.0f + 0.1f * sinf(g_restartPulseAnim * 3.0f);

    // Draw warning triangle
    Vector2 p1 = {iconCenterX, iconY};
    Vector2 p2 = {iconCenterX - 25 * pulseScale, iconY + 45 * pulseScale};
    Vector2 p3 = {iconCenterX + 25 * pulseScale, iconY + 45 * pulseScale};
    DrawTriangle(p1, p2, p3, COLOR_DANGER);

    // Exclamation mark
    LlzDrawText("!", (int)iconCenterX - 5, (int)iconY + 14, LLZ_FONT_SIZE_LARGE, COLOR_TEXT_PRIMARY);

    // Title
    const char *title = "Restart Device?";
    int titleWidth = LlzMeasureText(title, LLZ_FONT_SIZE_TITLE);
    LlzDrawText(title, (g_screenWidth - titleWidth) / 2, (int)panelY + 100, LLZ_FONT_SIZE_TITLE, COLOR_TEXT_PRIMARY);

    // Instructions
    const char *instr = "Swipe up to confirm";
    int instrWidth = LlzMeasureText(instr, LLZ_FONT_SIZE_NORMAL);
    LlzDrawText(instr, (g_screenWidth - instrWidth) / 2, (int)panelY + 145, LLZ_FONT_SIZE_NORMAL, COLOR_TEXT_SECONDARY);

    // Progress bar
    float barWidth = panelWidth - 60;
    float barX = panelX + 30;
    float barY = panelY + 190;
    float barHeight = 12;

    // Background
    DrawRectangleRounded((Rectangle){barX, barY, barWidth, barHeight}, 0.5f, 8, (Color){50, 50, 65, 255});

    // Progress fill
    if (g_restartSwipeProgress > 0) {
        float fillWidth = barWidth * g_restartSwipeProgress;
        Color fillColor = g_restartSwipeProgress >= 1.0f ? COLOR_DANGER : COLOR_ACCENT;
        DrawRectangleRounded((Rectangle){barX, barY, fillWidth, barHeight}, 0.5f, 8, fillColor);
    }

    // Cancel hint
    const char *cancelHint = "Tap outside or press Back to cancel";
    int cancelWidth = LlzMeasureText(cancelHint, LLZ_FONT_SIZE_SMALL);
    LlzDrawText(cancelHint, (g_screenWidth - cancelWidth) / 2, (int)panelY + 230, LLZ_FONT_SIZE_SMALL, COLOR_TEXT_TERTIARY);

    // Swipe up arrow indicator
    float arrowY = panelY + panelHeight + 30;
    float arrowPulse = sinf(g_restartPulseAnim * 4.0f);
    Color arrowColor = COLOR_TEXT_SECONDARY;
    arrowColor.a = (unsigned char)(150 + 50 * arrowPulse);

    // Draw up arrows
    for (int i = 0; i < 3; i++) {
        float offset = i * 15 + arrowPulse * 5;
        int arrowCenterX = g_screenWidth / 2;
        DrawTriangle(
            (Vector2){arrowCenterX, arrowY + offset - 10},
            (Vector2){arrowCenterX - 15, arrowY + offset + 5},
            (Vector2){arrowCenterX + 15, arrowY + offset + 5},
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
    g_animTime = 0.0f;
    g_sliderPulse = 0.0f;

    // Reset restart confirmation state
    g_restartConfirmActive = false;
    g_restartSwipeProgress = 0.0f;
    g_restartPulseAnim = 0.0f;
    g_restartSwipeTracking = false;

    // Reset selection animations
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        g_selectionAnim[i] = (i == 0) ? 1.0f : 0.0f;
    }

    // Initialize media system for lyrics API access
    LlzMediaInit(NULL);

    // Load current config values
    const LlzConfig *config = LlzConfigGet();
    g_isAutoBrightness = (config->brightness == LLZ_BRIGHTNESS_AUTO);
    g_pendingBrightness = g_isAutoBrightness ? 80 : config->brightness;
    g_lyricsEnabled = LlzLyricsIsEnabled();
    g_toggleAnim = g_lyricsEnabled ? 1.0f : 0.0f;
    g_hasChanges = false;

    printf("Settings plugin initialized\n");
    printf("Current config: brightness=%s%d, lyrics=%s\n",
           g_isAutoBrightness ? "AUTO (last=" : "",
           g_pendingBrightness,
           g_lyricsEnabled ? "ON" : "OFF");
}

static Rectangle GetMenuItemBounds(int index) {
    float cardY = HEADER_HEIGHT + index * (CARD_HEIGHT + CARD_SPACING);
    return (Rectangle){CARD_MARGIN_X, cardY, g_screenWidth - CARD_MARGIN_X * 2, CARD_HEIGHT};
}

static void PluginUpdate(const LlzInputState *hostInput, float deltaTime) {
    LlzInputState emptyInput = {0};
    const LlzInputState *input = hostInput ? hostInput : &emptyInput;

    // Update animations
    g_animTime += deltaTime;
    g_sliderPulse += deltaTime;

    // Update selection animations (smooth transitions)
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        float target = (i == g_selectedItem) ? 1.0f : 0.0f;
        g_selectionAnim[i] = Lerp(g_selectionAnim[i], target, deltaTime * 12.0f);
    }

    // Update toggle animation
    float toggleTarget = g_lyricsEnabled ? 1.0f : 0.0f;
    g_toggleAnim = Lerp(g_toggleAnim, toggleTarget, deltaTime * 10.0f);

    // Navigation
    if (input->downPressed || IsKeyPressed(KEY_DOWN)) {
        g_selectedItem = (g_selectedItem + 1) % MENU_ITEM_COUNT;
    }
    if (input->upPressed || IsKeyPressed(KEY_UP)) {
        g_selectedItem = (g_selectedItem - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
    }

    // Tap to select
    if (input->tap || input->mouseJustPressed) {
        Vector2 tapPos = input->tap ? input->tapPosition : input->mousePos;
        for (int i = 0; i < MENU_ITEM_COUNT; i++) {
            Rectangle bounds = GetMenuItemBounds(i);
            if (CheckCollisionPointRec(tapPos, bounds)) {
                if (g_selectedItem != i) {
                    g_selectedItem = i;
                } else {
                    // Tap on already selected item
                    if (i == 1) {
                        // Toggle lyrics
                        g_lyricsEnabled = !g_lyricsEnabled;
                        g_hasChanges = true;
                        LlzLyricsSetEnabled(g_lyricsEnabled);
                    } else if (i == 2) {
                        // Enter restart confirmation
                        g_restartConfirmActive = true;
                        g_restartSwipeProgress = 0.0f;
                        g_restartPulseAnim = 0.0f;
                    }
                }
                break;
            }
        }
    }

    // Value adjustment
    bool leftPressed = IsKeyPressed(KEY_LEFT) || input->swipeRight;
    bool rightPressed = IsKeyPressed(KEY_RIGHT) || input->swipeLeft;
    float scrollDelta = input->scrollDelta;

    if (g_selectedItem == 0) {
        // Brightness adjustment
        int delta = 0;
        if (leftPressed) delta = -5;
        if (rightPressed) delta = 5;
        if (scrollDelta != 0) delta = (int)(scrollDelta * 5);

        if (delta != 0) {
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
    } else if (g_selectedItem == 1) {
        // Lyrics toggle
        if (leftPressed || rightPressed || scrollDelta != 0 ||
            IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || input->selectPressed) {
            g_lyricsEnabled = !g_lyricsEnabled;
            g_hasChanges = true;
            LlzLyricsSetEnabled(g_lyricsEnabled);
        }
    } else if (g_selectedItem == 2) {
        // Restart device - enter confirmation on select
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || input->selectPressed) {
            g_restartConfirmActive = true;
            g_restartSwipeProgress = 0.0f;
            g_restartPulseAnim = 0.0f;
        }
    }

    // Handle restart confirmation mode
    if (g_restartConfirmActive) {
        g_restartPulseAnim += deltaTime;

        // Track swipe up gesture via drag
        if (input->mousePressed || input->dragActive) {
            if (!g_restartSwipeTracking) {
                g_restartSwipeTracking = true;
                g_restartSwipeStartY = (int)input->mousePos.y;
            } else {
                // Calculate swipe progress (swipe up = negative delta)
                int currentY = (int)input->mousePos.y;
                int swipeDelta = g_restartSwipeStartY - currentY;
                if (swipeDelta > 0) {
                    g_restartSwipeProgress = (float)swipeDelta / RESTART_SWIPE_THRESHOLD;
                    if (g_restartSwipeProgress > 1.0f) g_restartSwipeProgress = 1.0f;
                } else {
                    g_restartSwipeProgress = 0.0f;
                }
            }
        } else if (g_restartSwipeTracking) {
            // Touch released - check if confirmed
            if (g_restartSwipeProgress >= 1.0f) {
                // Confirmed! Execute restart
                printf("Restarting device...\n");
#ifdef PLATFORM_DRM
                (void)system("reboot");
#else
                printf("Desktop mode: Would restart here\n");
                g_restartConfirmActive = false;
#endif
            } else {
                // Reset
                g_restartSwipeProgress = 0.0f;
            }
            g_restartSwipeTracking = false;
        }

        // Handle swipeUp gesture from input state
        if (input->swipeUp) {
            printf("Restarting device...\n");
#ifdef PLATFORM_DRM
            (void)system("reboot");
#else
            printf("Desktop mode: Would restart here\n");
            g_restartConfirmActive = false;
#endif
        }

        // Cancel on back or tap outside
        if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
            g_restartConfirmActive = false;
            g_restartSwipeProgress = 0.0f;
            g_restartSwipeTracking = false;
        }

        // Cancel on tap outside panel
        if ((input->tap || input->mouseJustPressed) && !g_restartSwipeTracking) {
            Vector2 tapPos = input->tap ? input->tapPosition : input->mousePos;
            float panelWidth = 400;
            float panelHeight = 280;
            float panelX = (g_screenWidth - panelWidth) / 2;
            float panelY = (g_screenHeight - panelHeight) / 2;
            Rectangle panelRect = {panelX, panelY, panelWidth, panelHeight};
            if (!CheckCollisionPointRec(tapPos, panelRect)) {
                g_restartConfirmActive = false;
                g_restartSwipeProgress = 0.0f;
            }
        }

        return; // Don't process other input while in confirmation mode
    }

    // Back button
    if (input->backReleased || IsKeyReleased(KEY_ESCAPE)) {
        g_wantsClose = true;
    }
}

static void PluginDraw(void) {
    DrawGradientBackground();
    DrawHeader();

    // Setting cards
    float startY = HEADER_HEIGHT;

    DrawSettingCard(0, "Brightness",
                    g_isAutoBrightness ? "Adjusts automatically based on ambient light" : "Manual brightness control",
                    startY, g_selectedItem == 0, g_selectionAnim[0]);

    DrawSettingCard(1, "Lyrics",
                    "Show synchronized lyrics during playback",
                    startY + CARD_HEIGHT + CARD_SPACING, g_selectedItem == 1, g_selectionAnim[1]);

    DrawSettingCard(2, "Restart Device",
                    "Reboot the CarThing (swipe up to confirm)",
                    startY + (CARD_HEIGHT + CARD_SPACING) * 2, g_selectedItem == 2, g_selectionAnim[2]);

    DrawFooter();

    // Draw restart confirmation overlay if active
    DrawRestartConfirmation();
}

static void PluginShutdown(void) {
    if (g_hasChanges) {
        LlzConfigSave();
        if (g_isAutoBrightness) {
            printf("Settings saved: brightness=AUTO, lyrics=%s\n",
                   g_lyricsEnabled ? "ON" : "OFF");
        } else {
            printf("Settings saved: brightness=%d, lyrics=%s\n",
                   g_pendingBrightness, g_lyricsEnabled ? "ON" : "OFF");
        }
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
