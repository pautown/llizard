/**
 * millionaire_draw.h - Visual Rendering for "Who Wants to Be a Millionaire" Game
 *
 * Complete drawing functions for the Millionaire game mode including:
 * - Animated background with spotlight and particle effects
 * - Prize ladder with glow effects
 * - Question panel with hexagonal answer buttons
 * - Lifeline icons
 * - Title screen with animated logo
 * - Win/lose animations with confetti
 *
 * For llizardgui-host (raylib, 800x480 display)
 */

#ifndef MILLIONAIRE_DRAW_H
#define MILLIONAIRE_DRAW_H

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Color Definitions
// ============================================================================

#define MILLIONAIRE_BLUE       (Color){0, 20, 80, 255}
#define MILLIONAIRE_BLUE_DARK  (Color){8, 12, 35, 255}
#define MILLIONAIRE_BLUE_MID   (Color){20, 40, 100, 255}
#define MILLIONAIRE_BLUE_LIGHT (Color){60, 100, 180, 255}
#define MILLIONAIRE_GOLD       (Color){255, 215, 0, 255}
#define MILLIONAIRE_GOLD_DIM   (Color){180, 150, 50, 255}
#define MILLIONAIRE_PURPLE     (Color){100, 0, 150, 255}
#define MILLIONAIRE_PURPLE_GLOW (Color){150, 80, 200, 255}
#define MILLIONAIRE_ORANGE     (Color){255, 180, 100, 255}
#define MILLIONAIRE_GREEN      (Color){50, 200, 100, 255}
#define MILLIONAIRE_RED        (Color){220, 50, 50, 255}
#define MILLIONAIRE_WHITE      (Color){240, 245, 255, 255}
#define MILLIONAIRE_GRAY       (Color){120, 130, 150, 255}

// ============================================================================
// Constants
// ============================================================================

#define MILLIONAIRE_SCREEN_WIDTH  800
#define MILLIONAIRE_SCREEN_HEIGHT 480

#define MAX_PARTICLES 100
#define MAX_CONFETTI  150

// Prize ladder values
static const char *PRIZE_LEVELS[15] = {
    "$100", "$200", "$300", "$500", "$1,000",
    "$2,000", "$4,000", "$8,000", "$16,000", "$32,000",
    "$64,000", "$125,000", "$250,000", "$500,000", "$1,000,000"
};

// Safe haven levels (indices 4 and 9)
static inline bool IsSafeHavenLevel(int level) {
    return level == 4 || level == 9;
}

// ============================================================================
// Particle System
// ============================================================================

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float size;
    float alpha;
    float lifetime;
    float maxLifetime;
    Color color;
    bool active;
} Particle;

typedef struct {
    Particle particles[MAX_PARTICLES];
    int count;
    float spawnTimer;
} ParticleSystem;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float rotation;
    float rotationSpeed;
    float size;
    Color color;
    bool active;
} Confetti;

typedef struct {
    Confetti pieces[MAX_CONFETTI];
    int count;
    float timer;
    bool active;
} ConfettiSystem;

// ============================================================================
// Lifeline State
// ============================================================================

typedef struct {
    bool fiftyFiftyUsed;
    bool phoneUsed;
    bool audienceUsed;
    int eliminatedOptions[2];  // For 50:50, which options are eliminated
} LifelineState;

// ============================================================================
// Answer State for Animation
// ============================================================================

typedef enum {
    ANSWER_STATE_NORMAL,
    ANSWER_STATE_SELECTED,
    ANSWER_STATE_LOCKED,
    ANSWER_STATE_CORRECT,
    ANSWER_STATE_WRONG,
    ANSWER_STATE_ELIMINATED  // For 50:50
} AnswerState;

// ============================================================================
// Global State (should be managed externally in real implementation)
// ============================================================================

static ParticleSystem g_backgroundParticles = {0};
static ConfettiSystem g_confetti = {0};
static float g_animTime = 0.0f;
static float g_pulseTime = 0.0f;

// ============================================================================
// Helper Functions
// ============================================================================

static inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline Color LerpColor(Color a, Color b, float t) {
    return (Color){
        (unsigned char)Lerp((float)a.r, (float)b.r, t),
        (unsigned char)Lerp((float)a.g, (float)b.g, t),
        (unsigned char)Lerp((float)a.b, (float)b.b, t),
        (unsigned char)Lerp((float)a.a, (float)b.a, t)
    };
}

static inline float EaseOutQuad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

static inline float EaseInOutSine(float t) {
    return -(cosf(PI * t) - 1.0f) / 2.0f;
}

// ============================================================================
// Particle System Functions
// ============================================================================

static void InitParticleSystem(ParticleSystem *ps) {
    memset(ps, 0, sizeof(ParticleSystem));
}

static void SpawnParticle(ParticleSystem *ps) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!ps->particles[i].active) {
            Particle *p = &ps->particles[i];
            p->active = true;
            p->position = (Vector2){
                GetRandomValue(0, MILLIONAIRE_SCREEN_WIDTH),
                GetRandomValue(0, MILLIONAIRE_SCREEN_HEIGHT)
            };
            p->velocity = (Vector2){
                GetRandomValue(-20, 20) / 100.0f,
                GetRandomValue(-50, -20) / 100.0f
            };
            p->size = GetRandomValue(1, 4);
            p->alpha = GetRandomValue(20, 60) / 100.0f;
            p->maxLifetime = GetRandomValue(200, 500) / 100.0f;
            p->lifetime = p->maxLifetime;

            // Random sparkle color: gold, blue, or white
            int colorChoice = GetRandomValue(0, 2);
            if (colorChoice == 0) p->color = MILLIONAIRE_GOLD;
            else if (colorChoice == 1) p->color = MILLIONAIRE_BLUE_LIGHT;
            else p->color = MILLIONAIRE_WHITE;

            ps->count++;
            break;
        }
    }
}

static void UpdateParticleSystem(ParticleSystem *ps, float deltaTime) {
    // Spawn new particles periodically
    ps->spawnTimer += deltaTime;
    if (ps->spawnTimer > 0.1f) {
        ps->spawnTimer = 0.0f;
        if (ps->count < MAX_PARTICLES / 2) {
            SpawnParticle(ps);
        }
    }

    // Update existing particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &ps->particles[i];
        if (p->active) {
            p->position.x += p->velocity.x;
            p->position.y += p->velocity.y;
            p->lifetime -= deltaTime;

            // Fade out as lifetime decreases
            p->alpha = (p->lifetime / p->maxLifetime) * 0.6f;

            // Deactivate if expired or off-screen
            if (p->lifetime <= 0 || p->position.y < -10) {
                p->active = false;
                ps->count--;
            }
        }
    }
}

static void DrawParticleSystem(ParticleSystem *ps) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &ps->particles[i];
        if (p->active) {
            Color c = p->color;
            c.a = (unsigned char)(p->alpha * 255);

            // Draw as a soft glow
            DrawCircleGradient(
                (int)p->position.x,
                (int)p->position.y,
                p->size * 2,
                c,
                ColorAlpha(c, 0)
            );
            DrawCircle((int)p->position.x, (int)p->position.y, p->size * 0.5f, c);
        }
    }
}

// ============================================================================
// Confetti System Functions
// ============================================================================

static void InitConfettiSystem(ConfettiSystem *cs) {
    memset(cs, 0, sizeof(ConfettiSystem));
}

static void StartConfetti(ConfettiSystem *cs) {
    cs->active = true;
    cs->timer = 0.0f;
    cs->count = 0;

    // Spawn initial burst
    for (int i = 0; i < MAX_CONFETTI; i++) {
        Confetti *c = &cs->pieces[i];
        c->active = true;
        c->position = (Vector2){
            GetRandomValue(100, MILLIONAIRE_SCREEN_WIDTH - 100),
            GetRandomValue(-50, 50)
        };
        c->velocity = (Vector2){
            GetRandomValue(-200, 200) / 100.0f,
            GetRandomValue(100, 300) / 100.0f
        };
        c->rotation = GetRandomValue(0, 360);
        c->rotationSpeed = GetRandomValue(-500, 500) / 100.0f;
        c->size = GetRandomValue(4, 10);

        // Random festive colors
        int colorChoice = GetRandomValue(0, 4);
        switch (colorChoice) {
            case 0: c->color = MILLIONAIRE_GOLD; break;
            case 1: c->color = MILLIONAIRE_PURPLE_GLOW; break;
            case 2: c->color = MILLIONAIRE_GREEN; break;
            case 3: c->color = MILLIONAIRE_BLUE_LIGHT; break;
            default: c->color = MILLIONAIRE_WHITE; break;
        }

        cs->count++;
    }
}

static void UpdateConfettiSystem(ConfettiSystem *cs, float deltaTime) {
    if (!cs->active) return;

    cs->timer += deltaTime;

    for (int i = 0; i < MAX_CONFETTI; i++) {
        Confetti *c = &cs->pieces[i];
        if (c->active) {
            c->position.x += c->velocity.x;
            c->position.y += c->velocity.y;
            c->velocity.y += 2.0f * deltaTime;  // Gravity
            c->velocity.x *= 0.99f;  // Air resistance
            c->rotation += c->rotationSpeed * deltaTime;

            // Deactivate if off-screen
            if (c->position.y > MILLIONAIRE_SCREEN_HEIGHT + 20) {
                c->active = false;
                cs->count--;
            }
        }
    }

    // Stop after 5 seconds
    if (cs->timer > 5.0f && cs->count == 0) {
        cs->active = false;
    }
}

static void DrawConfettiSystem(ConfettiSystem *cs) {
    if (!cs->active) return;

    for (int i = 0; i < MAX_CONFETTI; i++) {
        Confetti *c = &cs->pieces[i];
        if (c->active) {
            // Draw rotated rectangle
            Rectangle rect = {c->position.x, c->position.y, c->size, c->size * 0.6f};
            DrawRectanglePro(
                rect,
                (Vector2){c->size / 2, c->size * 0.3f},
                c->rotation,
                c->color
            );
        }
    }
}

// ============================================================================
// 1. DrawMillionaireBackground - Dark blue gradient with spotlight and particles
// ============================================================================

/**
 * DrawMillionaireBackground
 *
 * Draws an animated dark blue gradient background with:
 * - Vertical gradient from dark navy to deep blue
 * - Multiple spotlight effects that subtly pulse
 * - Floating sparkle particles
 * - Subtle radial glow effects
 */
static void DrawMillionaireBackground(float deltaTime) {
    g_animTime += deltaTime;
    g_pulseTime += deltaTime * 2.0f;

    // Main gradient: dark navy to deep blue
    Color topColor = MILLIONAIRE_BLUE_DARK;
    Color bottomColor = MILLIONAIRE_BLUE_MID;
    DrawRectangleGradientV(0, 0, MILLIONAIRE_SCREEN_WIDTH, MILLIONAIRE_SCREEN_HEIGHT, topColor, bottomColor);

    // Primary center spotlight (pulsing)
    float pulseIntensity = 0.04f + sinf(g_pulseTime) * 0.01f;
    for (int i = 0; i < 4; i++) {
        float alpha = pulseIntensity - i * 0.01f;
        if (alpha < 0) alpha = 0;
        Color glow = ColorAlpha(MILLIONAIRE_BLUE_LIGHT, alpha);
        DrawCircleGradient(
            MILLIONAIRE_SCREEN_WIDTH / 2,
            MILLIONAIRE_SCREEN_HEIGHT / 2 - 50,
            400 - i * 80,
            glow,
            ColorAlpha(glow, 0.0f)
        );
    }

    // Secondary spotlight from top (for drama)
    for (int i = 0; i < 3; i++) {
        float alpha = 0.03f - i * 0.01f;
        Color glow = ColorAlpha(MILLIONAIRE_PURPLE_GLOW, alpha);
        DrawCircleGradient(
            MILLIONAIRE_SCREEN_WIDTH / 2,
            -100,
            300 - i * 60,
            glow,
            ColorAlpha(glow, 0.0f)
        );
    }

    // Gold accent spotlights in corners (subtle)
    float cornerPulse = 0.02f + sinf(g_pulseTime * 0.7f + 1.0f) * 0.01f;
    Color goldGlow = ColorAlpha(MILLIONAIRE_GOLD, cornerPulse);
    DrawCircleGradient(0, MILLIONAIRE_SCREEN_HEIGHT, 200, goldGlow, ColorAlpha(goldGlow, 0.0f));
    DrawCircleGradient(MILLIONAIRE_SCREEN_WIDTH, MILLIONAIRE_SCREEN_HEIGHT, 200, goldGlow, ColorAlpha(goldGlow, 0.0f));

    // Update and draw floating particles
    UpdateParticleSystem(&g_backgroundParticles, deltaTime);
    DrawParticleSystem(&g_backgroundParticles);

    // Vignette effect (darker corners)
    for (int i = 0; i < 4; i++) {
        float alpha = 0.15f - i * 0.04f;
        Color vignette = ColorAlpha(BLACK, alpha);
        DrawCircleGradient(0, 0, 300 - i * 50, vignette, ColorAlpha(vignette, 0.0f));
        DrawCircleGradient(MILLIONAIRE_SCREEN_WIDTH, 0, 300 - i * 50, vignette, ColorAlpha(vignette, 0.0f));
    }
}

// ============================================================================
// 2. DrawPrizeLadder - Left side prize levels with effects
// ============================================================================

/**
 * DrawPrizeLadder
 *
 * Draws the prize ladder on the left side of the screen:
 * - 15 prize levels from $100 to $1,000,000
 * - Current level highlighted with golden glow
 * - Safe havens ($1,000 and $32,000) in orange/gold
 * - Passed levels with green checkmark style
 * - Upcoming levels in dim blue
 *
 * @param currentLevel  Current prize level (0-14)
 * @param font          Font to use for text
 */
static void DrawPrizeLadder(int currentLevel, Font font) {
    float ladderX = 15;
    float ladderY = 50;
    float itemHeight = 26;
    float ladderWidth = 130;

    // Background panel for ladder
    Rectangle ladderBg = {ladderX - 8, ladderY - 10, ladderWidth + 16, itemHeight * 15 + 20};
    DrawRectangleRounded(ladderBg, 0.08f, 8, ColorAlpha(MILLIONAIRE_BLUE_DARK, 0.7f));
    DrawRectangleRoundedLines(ladderBg, 0.08f, 8, ColorAlpha(MILLIONAIRE_BLUE_LIGHT, 0.3f));

    // Draw each level (from top: $1,000,000 to bottom: $100)
    for (int i = 14; i >= 0; i--) {
        float itemY = ladderY + (14 - i) * itemHeight;
        bool isCurrent = (i == currentLevel);
        bool isPassed = (i < currentLevel);
        bool isSafe = IsSafeHavenLevel(i);

        // Determine styling based on state
        Color textColor;
        Color bgColor = ColorAlpha(BLACK, 0);
        float fontSize = 15;

        if (isCurrent) {
            // Current level: gold with glow effect
            textColor = MILLIONAIRE_GOLD;
            fontSize = 17;

            // Animated glow box
            float glowPulse = 0.3f + sinf(g_pulseTime * 3) * 0.1f;
            Rectangle highlight = {ladderX - 4, itemY - 2, ladderWidth + 8, itemHeight - 2};
            DrawRectangleRounded(highlight, 0.3f, 4, ColorAlpha(MILLIONAIRE_GOLD, glowPulse));
            DrawRectangleRounded(highlight, 0.3f, 4, ColorAlpha(MILLIONAIRE_GOLD_DIM, 0.2f));

            // Arrow indicator
            DrawTriangle(
                (Vector2){ladderX - 10, itemY + itemHeight / 2 - 5},
                (Vector2){ladderX - 10, itemY + itemHeight / 2 + 5},
                (Vector2){ladderX - 4, itemY + itemHeight / 2},
                MILLIONAIRE_GOLD
            );
        } else if (isPassed) {
            // Passed level: green checkmark style
            textColor = MILLIONAIRE_GREEN;

            // Small checkmark or dot
            DrawCircle(ladderX + ladderWidth + 5, itemY + itemHeight / 2, 4, MILLIONAIRE_GREEN);
        } else if (isSafe) {
            // Safe haven: orange/gold
            textColor = MILLIONAIRE_ORANGE;

            // Subtle safe indicator
            Rectangle safeBox = {ladderX - 2, itemY, ladderWidth + 4, itemHeight - 4};
            DrawRectangleRoundedLines(safeBox, 0.2f, 4, ColorAlpha(MILLIONAIRE_ORANGE, 0.4f));
        } else {
            // Upcoming level: dim
            textColor = ColorAlpha(MILLIONAIRE_GRAY, 0.6f);
        }

        // Draw level number and amount
        char levelText[32];
        snprintf(levelText, sizeof(levelText), "%2d. %s", i + 1, PRIZE_LEVELS[i]);
        DrawTextEx(font, levelText, (Vector2){ladderX, itemY}, fontSize, 1, textColor);
    }

    // Title above ladder
    DrawTextEx(font, "PRIZE LADDER", (Vector2){ladderX, ladderY - 25}, 12, 1, MILLIONAIRE_GOLD_DIM);
}

// ============================================================================
// 3. DrawQuestionPanel - Center/right question and answer area
// ============================================================================

/**
 * Helper: Draw hexagonal-style answer button
 */
static void DrawHexagonalButton(Rectangle bounds, const char *letter, const char *text,
                                 AnswerState state, Font font, float animTime) {
    // Determine colors based on state
    Color bgColor, borderColor, letterColor, textColor;
    float borderWidth = 2.0f;

    switch (state) {
        case ANSWER_STATE_SELECTED:
            bgColor = ColorAlpha(MILLIONAIRE_GOLD, 0.25f);
            borderColor = MILLIONAIRE_GOLD;
            letterColor = MILLIONAIRE_GOLD;
            textColor = MILLIONAIRE_WHITE;
            borderWidth = 3.0f;
            break;

        case ANSWER_STATE_LOCKED:
            // Pulsing orange/gold for "final answer"
            {
                float pulse = 0.5f + sinf(animTime * 8) * 0.3f;
                bgColor = ColorAlpha(MILLIONAIRE_ORANGE, pulse * 0.4f);
                borderColor = LerpColor(MILLIONAIRE_ORANGE, MILLIONAIRE_GOLD, pulse);
                letterColor = MILLIONAIRE_GOLD;
                textColor = MILLIONAIRE_WHITE;
                borderWidth = 4.0f;
            }
            break;

        case ANSWER_STATE_CORRECT:
            bgColor = ColorAlpha(MILLIONAIRE_GREEN, 0.4f);
            borderColor = MILLIONAIRE_GREEN;
            letterColor = MILLIONAIRE_GREEN;
            textColor = MILLIONAIRE_WHITE;
            borderWidth = 3.0f;
            break;

        case ANSWER_STATE_WRONG:
            bgColor = ColorAlpha(MILLIONAIRE_RED, 0.4f);
            borderColor = MILLIONAIRE_RED;
            letterColor = MILLIONAIRE_RED;
            textColor = MILLIONAIRE_WHITE;
            borderWidth = 3.0f;
            break;

        case ANSWER_STATE_ELIMINATED:
            bgColor = ColorAlpha(MILLIONAIRE_BLUE_DARK, 0.5f);
            borderColor = ColorAlpha(MILLIONAIRE_GRAY, 0.3f);
            letterColor = ColorAlpha(MILLIONAIRE_GRAY, 0.4f);
            textColor = ColorAlpha(MILLIONAIRE_GRAY, 0.4f);
            break;

        default:  // NORMAL
            bgColor = ColorAlpha(MILLIONAIRE_BLUE_DARK, 0.8f);
            borderColor = ColorAlpha(MILLIONAIRE_BLUE_LIGHT, 0.5f);
            letterColor = MILLIONAIRE_GOLD_DIM;
            textColor = ColorAlpha(MILLIONAIRE_WHITE, 0.8f);
            break;
    }

    // Draw hexagonal-ish shape (rounded rectangle with pointed ends simulation)
    float cornerRadius = 0.25f;

    // Main button body
    DrawRectangleRounded(bounds, cornerRadius, 8, bgColor);

    // Border with glow for selected/locked
    if (state == ANSWER_STATE_SELECTED || state == ANSWER_STATE_LOCKED) {
        Rectangle glowBounds = {bounds.x - 2, bounds.y - 2, bounds.width + 4, bounds.height + 4};
        DrawRectangleRoundedLines(glowBounds, cornerRadius, 8, ColorAlpha(borderColor, 0.3f));
    }
    DrawRectangleRoundedLines(bounds, cornerRadius, 8, borderColor);

    // Hexagonal accents (small triangles at ends)
    float triSize = 8;
    float centerY = bounds.y + bounds.height / 2;

    // Left triangle
    DrawTriangle(
        (Vector2){bounds.x + 2, centerY - triSize},
        (Vector2){bounds.x + 2, centerY + triSize},
        (Vector2){bounds.x - 4, centerY},
        bgColor
    );
    DrawLineEx(
        (Vector2){bounds.x + 2, centerY - triSize},
        (Vector2){bounds.x - 4, centerY},
        borderWidth * 0.5f, borderColor
    );
    DrawLineEx(
        (Vector2){bounds.x - 4, centerY},
        (Vector2){bounds.x + 2, centerY + triSize},
        borderWidth * 0.5f, borderColor
    );

    // Right triangle
    DrawTriangle(
        (Vector2){bounds.x + bounds.width - 2, centerY - triSize},
        (Vector2){bounds.x + bounds.width + 4, centerY},
        (Vector2){bounds.x + bounds.width - 2, centerY + triSize},
        bgColor
    );
    DrawLineEx(
        (Vector2){bounds.x + bounds.width - 2, centerY - triSize},
        (Vector2){bounds.x + bounds.width + 4, centerY},
        borderWidth * 0.5f, borderColor
    );
    DrawLineEx(
        (Vector2){bounds.x + bounds.width + 4, centerY},
        (Vector2){bounds.x + bounds.width - 2, centerY + triSize},
        borderWidth * 0.5f, borderColor
    );

    // Letter (A, B, C, D)
    DrawTextEx(font, letter, (Vector2){bounds.x + 12, bounds.y + (bounds.height - 22) / 2}, 22, 1, letterColor);

    // Answer text
    float textX = bounds.x + 45;
    float maxTextWidth = bounds.width - 55;

    // Simple text truncation if needed
    Vector2 textSize = MeasureTextEx(font, text, 18, 1);
    if (textSize.x <= maxTextWidth) {
        DrawTextEx(font, text, (Vector2){textX, bounds.y + (bounds.height - 18) / 2}, 18, 1, textColor);
    } else {
        // Truncate with ellipsis
        char truncated[128];
        strncpy(truncated, text, sizeof(truncated) - 4);
        truncated[sizeof(truncated) - 4] = '\0';
        while (strlen(truncated) > 3) {
            Vector2 size = MeasureTextEx(font, truncated, 18, 1);
            if (size.x + 20 <= maxTextWidth) break;
            truncated[strlen(truncated) - 1] = '\0';
        }
        strcat(truncated, "...");
        DrawTextEx(font, truncated, (Vector2){textX, bounds.y + (bounds.height - 18) / 2}, 18, 1, textColor);
    }

    // Selection bar on left for selected state
    if (state == ANSWER_STATE_SELECTED || state == ANSWER_STATE_LOCKED) {
        Rectangle selectBar = {bounds.x + 2, bounds.y + 6, 3, bounds.height - 12};
        DrawRectangleRounded(selectBar, 0.5f, 4, MILLIONAIRE_GOLD);
    }
}

/**
 * DrawQuestionPanel
 *
 * Draws the main question and answer panel:
 * - Question text in styled box at top
 * - 2x2 answer grid (A/B top, C/D bottom)
 * - Each answer in hexagonal-style button
 * - Handles selection, lock, correct/wrong states
 *
 * @param question       Question text
 * @param answers        Array of 4 answer strings
 * @param answerStates   Array of 4 AnswerState values
 * @param selectedIdx    Currently selected answer (0-3)
 * @param font           Font to use
 */
static void DrawQuestionPanel(const char *question, const char *answers[4],
                               AnswerState answerStates[4], int selectedIdx, Font font) {
    float panelX = 155;  // After prize ladder
    float panelWidth = MILLIONAIRE_SCREEN_WIDTH - panelX - 10;

    // Question box
    Rectangle questionBox = {panelX, 45, panelWidth, 95};
    DrawRectangleRounded(questionBox, 0.08f, 8, ColorAlpha(MILLIONAIRE_BLUE_DARK, 0.85f));
    DrawRectangleRoundedLines(questionBox, 0.08f, 8, ColorAlpha(MILLIONAIRE_BLUE_LIGHT, 0.6f));

    // Decorative lines at top and bottom of question box
    DrawLineEx(
        (Vector2){questionBox.x + 20, questionBox.y + 2},
        (Vector2){questionBox.x + questionBox.width - 20, questionBox.y + 2},
        2, ColorAlpha(MILLIONAIRE_GOLD, 0.3f)
    );
    DrawLineEx(
        (Vector2){questionBox.x + 20, questionBox.y + questionBox.height - 2},
        (Vector2){questionBox.x + questionBox.width - 20, questionBox.y + questionBox.height - 2},
        2, ColorAlpha(MILLIONAIRE_GOLD, 0.3f)
    );

    // Question text (with word wrap approximation)
    float questionX = questionBox.x + 15;
    float questionY = questionBox.y + 12;
    float maxQuestionWidth = questionBox.width - 30;

    // Simple word wrap
    char line[256];
    const char *p = question;
    float lineY = questionY;
    float lineHeight = 26;

    while (*p != '\0' && lineY < questionBox.y + questionBox.height - 20) {
        int lineLen = 0;
        line[0] = '\0';

        while (*p != '\0' && *p != '\n') {
            const char *wordStart = p;
            while (*p != '\0' && *p != ' ' && *p != '\n') p++;
            int wordLen = p - wordStart;

            char testLine[256];
            if (lineLen > 0) {
                snprintf(testLine, sizeof(testLine), "%s %.*s", line, wordLen, wordStart);
            } else {
                snprintf(testLine, sizeof(testLine), "%.*s", wordLen, wordStart);
            }

            Vector2 testSize = MeasureTextEx(font, testLine, 22, 1);
            if (testSize.x > maxQuestionWidth && lineLen > 0) {
                DrawTextEx(font, line, (Vector2){questionX, lineY}, 22, 1, MILLIONAIRE_WHITE);
                lineY += lineHeight;
                snprintf(line, sizeof(line), "%.*s", wordLen, wordStart);
                lineLen = wordLen;
            } else {
                strcpy(line, testLine);
                lineLen = strlen(line);
            }

            while (*p == ' ') p++;
        }

        if (lineLen > 0) {
            DrawTextEx(font, line, (Vector2){questionX, lineY}, 22, 1, MILLIONAIRE_WHITE);
            lineY += lineHeight;
        }

        while (*p == '\n') p++;
    }

    // Answer buttons (2x2 grid)
    float answerStartY = 155;
    float buttonWidth = (panelWidth - 20) / 2;
    float buttonHeight = 55;
    float buttonSpacingX = 15;
    float buttonSpacingY = 12;

    const char *letters[4] = {"A:", "B:", "C:", "D:"};

    for (int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;

        Rectangle bounds = {
            panelX + col * (buttonWidth + buttonSpacingX),
            answerStartY + row * (buttonHeight + buttonSpacingY),
            buttonWidth,
            buttonHeight
        };

        DrawHexagonalButton(bounds, letters[i], answers[i], answerStates[i], font, g_animTime);
    }

    // "FINAL ANSWER" effect when locked
    for (int i = 0; i < 4; i++) {
        if (answerStates[i] == ANSWER_STATE_LOCKED) {
            float pulse = 0.5f + sinf(g_animTime * 6) * 0.5f;
            Color finalColor = ColorAlpha(MILLIONAIRE_GOLD, pulse * 0.8f);

            const char *finalText = "FINAL ANSWER";
            Vector2 finalSize = MeasureTextEx(font, finalText, 16, 1);
            float finalX = (MILLIONAIRE_SCREEN_WIDTH - finalSize.x) / 2;
            DrawTextEx(font, finalText, (Vector2){finalX, answerStartY + 2 * buttonHeight + 25}, 16, 1, finalColor);
        }
    }
}

// ============================================================================
// 4. DrawLifelinePanel - Three lifeline icons
// ============================================================================

/**
 * DrawLifelinePanel
 *
 * Draws the three lifeline icons:
 * - 50:50 (eliminates two wrong answers)
 * - Phone a Friend (phone icon)
 * - Ask the Audience (bar chart icon)
 *
 * @param lifelines  Lifeline state (used/available)
 * @param selectedLifeline  Currently selected lifeline (-1 for none)
 * @param font       Font to use
 */
static void DrawLifelinePanel(LifelineState lifelines, int selectedLifeline, Font font) {
    float panelX = 155;
    float panelY = MILLIONAIRE_SCREEN_HEIGHT - 65;
    float iconSize = 50;
    float iconSpacing = 20;

    // Background panel
    float panelWidth = 3 * iconSize + 2 * iconSpacing + 20;
    Rectangle panelBg = {panelX, panelY - 5, panelWidth, iconSize + 15};
    DrawRectangleRounded(panelBg, 0.15f, 8, ColorAlpha(MILLIONAIRE_BLUE_DARK, 0.6f));
    DrawRectangleRoundedLines(panelBg, 0.15f, 8, ColorAlpha(MILLIONAIRE_BLUE_LIGHT, 0.3f));

    // Label
    DrawTextEx(font, "LIFELINES", (Vector2){panelX + 5, panelY - 20}, 11, 1, MILLIONAIRE_GOLD_DIM);

    // 50:50 Lifeline
    {
        float x = panelX + 10;
        float y = panelY;
        bool used = lifelines.fiftyFiftyUsed;
        bool selected = (selectedLifeline == 0);

        Rectangle iconBounds = {x, y, iconSize, iconSize};
        Color bgColor = used ? ColorAlpha(MILLIONAIRE_GRAY, 0.2f) : ColorAlpha(MILLIONAIRE_PURPLE, 0.4f);
        Color borderColor = used ? MILLIONAIRE_GRAY : (selected ? MILLIONAIRE_GOLD : MILLIONAIRE_PURPLE_GLOW);
        Color textColor = used ? ColorAlpha(MILLIONAIRE_GRAY, 0.5f) : MILLIONAIRE_WHITE;

        if (selected && !used) {
            float pulse = 0.3f + sinf(g_pulseTime * 4) * 0.2f;
            bgColor = ColorAlpha(MILLIONAIRE_PURPLE, pulse + 0.3f);
        }

        DrawRectangleRounded(iconBounds, 0.2f, 6, bgColor);
        DrawRectangleRoundedLines(iconBounds, 0.2f, 6, borderColor);

        // "50:50" text
        const char *fiftyText = "50:50";
        Vector2 textSize = MeasureTextEx(font, fiftyText, 16, 1);
        DrawTextEx(font, fiftyText,
            (Vector2){x + (iconSize - textSize.x) / 2, y + (iconSize - 16) / 2},
            16, 1, textColor);

        // X mark if used
        if (used) {
            DrawLineEx((Vector2){x + 8, y + 8}, (Vector2){x + iconSize - 8, y + iconSize - 8}, 3, MILLIONAIRE_RED);
            DrawLineEx((Vector2){x + iconSize - 8, y + 8}, (Vector2){x + 8, y + iconSize - 8}, 3, MILLIONAIRE_RED);
        }
    }

    // Phone a Friend Lifeline
    {
        float x = panelX + 10 + iconSize + iconSpacing;
        float y = panelY;
        bool used = lifelines.phoneUsed;
        bool selected = (selectedLifeline == 1);

        Rectangle iconBounds = {x, y, iconSize, iconSize};
        Color bgColor = used ? ColorAlpha(MILLIONAIRE_GRAY, 0.2f) : ColorAlpha(MILLIONAIRE_BLUE_MID, 0.4f);
        Color borderColor = used ? MILLIONAIRE_GRAY : (selected ? MILLIONAIRE_GOLD : MILLIONAIRE_BLUE_LIGHT);
        Color iconColor = used ? ColorAlpha(MILLIONAIRE_GRAY, 0.5f) : MILLIONAIRE_WHITE;

        if (selected && !used) {
            float pulse = 0.3f + sinf(g_pulseTime * 4) * 0.2f;
            bgColor = ColorAlpha(MILLIONAIRE_BLUE_MID, pulse + 0.3f);
        }

        DrawRectangleRounded(iconBounds, 0.2f, 6, bgColor);
        DrawRectangleRoundedLines(iconBounds, 0.2f, 6, borderColor);

        // Phone icon (simplified)
        float cx = x + iconSize / 2;
        float cy = y + iconSize / 2;

        // Phone body
        Rectangle phoneBody = {cx - 8, cy - 14, 16, 28};
        DrawRectangleRounded(phoneBody, 0.3f, 4, iconColor);

        // Screen
        Rectangle phoneScreen = {cx - 6, cy - 10, 12, 16};
        DrawRectangleRounded(phoneScreen, 0.2f, 3, bgColor);

        // Home button
        DrawCircle(cx, cy + 10, 2, iconColor);

        if (used) {
            DrawLineEx((Vector2){x + 8, y + 8}, (Vector2){x + iconSize - 8, y + iconSize - 8}, 3, MILLIONAIRE_RED);
            DrawLineEx((Vector2){x + iconSize - 8, y + 8}, (Vector2){x + 8, y + iconSize - 8}, 3, MILLIONAIRE_RED);
        }
    }

    // Ask the Audience Lifeline
    {
        float x = panelX + 10 + 2 * (iconSize + iconSpacing);
        float y = panelY;
        bool used = lifelines.audienceUsed;
        bool selected = (selectedLifeline == 2);

        Rectangle iconBounds = {x, y, iconSize, iconSize};
        Color bgColor = used ? ColorAlpha(MILLIONAIRE_GRAY, 0.2f) : ColorAlpha(MILLIONAIRE_GREEN, 0.3f);
        Color borderColor = used ? MILLIONAIRE_GRAY : (selected ? MILLIONAIRE_GOLD : MILLIONAIRE_GREEN);
        Color barColor = used ? ColorAlpha(MILLIONAIRE_GRAY, 0.5f) : MILLIONAIRE_WHITE;

        if (selected && !used) {
            float pulse = 0.3f + sinf(g_pulseTime * 4) * 0.2f;
            bgColor = ColorAlpha(MILLIONAIRE_GREEN, pulse + 0.2f);
        }

        DrawRectangleRounded(iconBounds, 0.2f, 6, bgColor);
        DrawRectangleRoundedLines(iconBounds, 0.2f, 6, borderColor);

        // Bar chart icon
        float barWidth = 8;
        float barSpacing = 3;
        float baseY = y + iconSize - 10;
        float barHeights[4] = {15, 28, 20, 12};  // Varying bar heights

        for (int i = 0; i < 4; i++) {
            float bx = x + 8 + i * (barWidth + barSpacing);
            float by = baseY - barHeights[i];
            DrawRectangle(bx, by, barWidth, barHeights[i], barColor);
        }

        if (used) {
            DrawLineEx((Vector2){x + 8, y + 8}, (Vector2){x + iconSize - 8, y + iconSize - 8}, 3, MILLIONAIRE_RED);
            DrawLineEx((Vector2){x + iconSize - 8, y + 8}, (Vector2){x + 8, y + iconSize - 8}, 3, MILLIONAIRE_RED);
        }
    }
}

// ============================================================================
// 5. DrawTitleScreen - Animated logo and start prompt
// ============================================================================

/**
 * DrawTitleScreen
 *
 * Draws the title screen with:
 * - Animated "Who Wants to Be a Millionaire?" logo
 * - Floating particles
 * - Pulsing "Press SELECT to play" prompt
 *
 * @param deltaTime  Frame time for animation
 * @param font       Font to use
 */
static void DrawTitleScreen(float deltaTime, Font font) {
    // Draw animated background
    DrawMillionaireBackground(deltaTime);

    float centerX = MILLIONAIRE_SCREEN_WIDTH / 2;
    float centerY = MILLIONAIRE_SCREEN_HEIGHT / 2;

    // Main logo area glow
    for (int i = 0; i < 5; i++) {
        float pulse = 0.08f + sinf(g_animTime * 2 + i * 0.5f) * 0.04f;
        Color glow = ColorAlpha(MILLIONAIRE_GOLD, pulse - i * 0.015f);
        DrawCircleGradient(centerX, centerY - 60, 250 - i * 30, glow, ColorAlpha(glow, 0.0f));
    }

    // "WHO WANTS TO BE A" text
    const char *line1 = "WHO WANTS TO BE A";
    Vector2 size1 = MeasureTextEx(font, line1, 28, 2);
    float y1 = centerY - 100;

    // Shadow
    DrawTextEx(font, line1, (Vector2){centerX - size1.x / 2 + 2, y1 + 2}, 28, 2, ColorAlpha(BLACK, 0.5f));
    DrawTextEx(font, line1, (Vector2){centerX - size1.x / 2, y1}, 28, 2, MILLIONAIRE_WHITE);

    // "MILLIONAIRE" main text (animated glow)
    const char *line2 = "MILLIONAIRE";
    Vector2 size2 = MeasureTextEx(font, line2, 64, 3);
    float y2 = centerY - 50;

    // Glow layers
    for (int i = 3; i >= 0; i--) {
        float pulse = sinf(g_animTime * 3 + i) * 0.3f + 0.7f;
        Color glowColor = ColorAlpha(MILLIONAIRE_GOLD, 0.15f * pulse);
        DrawTextEx(font, line2, (Vector2){centerX - size2.x / 2 - i, y2 - i}, 64, 3, glowColor);
        DrawTextEx(font, line2, (Vector2){centerX - size2.x / 2 + i, y2 - i}, 64, 3, glowColor);
    }

    // Main text shadow
    DrawTextEx(font, line2, (Vector2){centerX - size2.x / 2 + 3, y2 + 3}, 64, 3, ColorAlpha(BLACK, 0.6f));

    // Main text with golden gradient simulation (using two colors)
    DrawTextEx(font, line2, (Vector2){centerX - size2.x / 2, y2}, 64, 3, MILLIONAIRE_GOLD);

    // Question mark decoration
    const char *qmark = "?";
    float qmarkPulse = 1.0f + sinf(g_animTime * 4) * 0.1f;
    Vector2 qmarkSize = MeasureTextEx(font, qmark, 80 * qmarkPulse, 2);
    DrawTextEx(font, qmark, (Vector2){centerX + size2.x / 2 + 10, y2 - 10}, 80 * qmarkPulse, 2,
               ColorAlpha(MILLIONAIRE_GOLD, 0.8f));

    // Decorative line
    float lineWidth = 400;
    float lineY = centerY + 40;
    float linePulse = 0.6f + sinf(g_animTime * 2) * 0.2f;
    DrawLineEx(
        (Vector2){centerX - lineWidth / 2, lineY},
        (Vector2){centerX + lineWidth / 2, lineY},
        3, ColorAlpha(MILLIONAIRE_GOLD, linePulse)
    );

    // Diamond shapes on line
    float diamondSize = 8;
    DrawPoly((Vector2){centerX - lineWidth / 2 - 5, lineY}, 4, diamondSize, 45, MILLIONAIRE_GOLD);
    DrawPoly((Vector2){centerX + lineWidth / 2 + 5, lineY}, 4, diamondSize, 45, MILLIONAIRE_GOLD);
    DrawPoly((Vector2){centerX, lineY}, 4, diamondSize * 1.2f, 45, MILLIONAIRE_GOLD);

    // "Press SELECT to play" prompt
    const char *startText = "Press SELECT to play";
    Vector2 startSize = MeasureTextEx(font, startText, 24, 1);
    float startY = centerY + 100;
    float startPulse = 0.5f + sinf(g_animTime * 4) * 0.5f;

    // Background box for prompt
    Rectangle startBox = {centerX - startSize.x / 2 - 20, startY - 8, startSize.x + 40, 40};
    DrawRectangleRounded(startBox, 0.3f, 8, ColorAlpha(MILLIONAIRE_BLUE_DARK, 0.7f));
    DrawRectangleRoundedLines(startBox, 0.3f, 8, ColorAlpha(MILLIONAIRE_GOLD, startPulse * 0.8f));

    DrawTextEx(font, startText, (Vector2){centerX - startSize.x / 2, startY}, 24, 1,
               ColorAlpha(MILLIONAIRE_WHITE, startPulse * 0.8f + 0.2f));

    // Version/credits at bottom
    const char *creditsText = "A llizardgui plugin";
    Vector2 creditsSize = MeasureTextEx(font, creditsText, 14, 1);
    DrawTextEx(font, creditsText,
               (Vector2){centerX - creditsSize.x / 2, MILLIONAIRE_SCREEN_HEIGHT - 35},
               14, 1, ColorAlpha(MILLIONAIRE_GRAY, 0.5f));
}

// ============================================================================
// 6. DrawWinAnimation - Confetti and gold effects
// ============================================================================

/**
 * DrawWinAnimation
 *
 * Draws the winning celebration screen with:
 * - Confetti rain
 * - Golden sparkles and glow effects
 * - "MILLIONAIRE!" text with animation
 * - Prize amount display
 *
 * @param deltaTime    Frame time for animation
 * @param font         Font to use
 * @param prizeWon     Prize amount string (e.g., "$1,000,000")
 * @param isMillionaire True if won the million
 */
static void DrawWinAnimation(float deltaTime, Font font, const char *prizeWon, bool isMillionaire) {
    // Draw background
    DrawMillionaireBackground(deltaTime);

    // Update and draw confetti
    UpdateConfettiSystem(&g_confetti, deltaTime);
    if (!g_confetti.active && isMillionaire) {
        StartConfetti(&g_confetti);
    }

    float centerX = MILLIONAIRE_SCREEN_WIDTH / 2;
    float centerY = MILLIONAIRE_SCREEN_HEIGHT / 2 - 40;

    // Golden radial burst effect
    for (int i = 0; i < 12; i++) {
        float angle = (i * 30 + g_animTime * 20) * DEG2RAD;
        float length = 150 + sinf(g_animTime * 3 + i) * 30;
        float alpha = 0.2f + sinf(g_animTime * 5 + i * 0.5f) * 0.1f;

        Vector2 start = {centerX, centerY};
        Vector2 end = {
            centerX + cosf(angle) * length,
            centerY + sinf(angle) * length
        };
        DrawLineEx(start, end, 3, ColorAlpha(MILLIONAIRE_GOLD, alpha));
    }

    // Central glow
    for (int i = 0; i < 6; i++) {
        float pulse = 0.15f + sinf(g_animTime * 3 + i) * 0.08f;
        Color glow = ColorAlpha(MILLIONAIRE_GOLD, pulse - i * 0.025f);
        DrawCircleGradient(centerX, centerY, 200 - i * 25, glow, ColorAlpha(glow, 0.0f));
    }

    // Floating sparkles around the text
    for (int i = 0; i < 30; i++) {
        float sparkleAngle = g_animTime * 1.5f + i * (360.0f / 30);
        float sparkleRadius = 120 + sinf(g_animTime * 2 + i * 0.3f) * 40;
        float sparkleX = centerX + cosf(sparkleAngle * DEG2RAD) * sparkleRadius;
        float sparkleY = centerY + sinf(sparkleAngle * DEG2RAD) * sparkleRadius * 0.6f;
        float sparkleSize = 2 + sinf(g_animTime * 8 + i) * 1.5f;
        float sparkleAlpha = 0.6f + sinf(g_animTime * 6 + i * 0.5f) * 0.4f;

        DrawCircle(sparkleX, sparkleY, sparkleSize, ColorAlpha(MILLIONAIRE_GOLD, sparkleAlpha));
    }

    // Title text
    const char *titleText = isMillionaire ? "MILLIONAIRE!" : "CONGRATULATIONS!";
    float titleSize = isMillionaire ? 56 : 42;
    Vector2 titleMeasure = MeasureTextEx(font, titleText, titleSize, 3);
    float titleY = centerY - 60;

    // Text glow effect
    for (int i = 4; i >= 0; i--) {
        float glowAlpha = 0.1f * (5 - i) / 5.0f;
        Color glowColor = ColorAlpha(MILLIONAIRE_GOLD, glowAlpha);
        DrawTextEx(font, titleText,
                   (Vector2){centerX - titleMeasure.x / 2 + i, titleY - i},
                   titleSize, 3, glowColor);
        DrawTextEx(font, titleText,
                   (Vector2){centerX - titleMeasure.x / 2 - i, titleY - i},
                   titleSize, 3, glowColor);
    }

    // Shadow
    DrawTextEx(font, titleText,
               (Vector2){centerX - titleMeasure.x / 2 + 3, titleY + 3},
               titleSize, 3, ColorAlpha(BLACK, 0.5f));

    // Main title
    DrawTextEx(font, titleText,
               (Vector2){centerX - titleMeasure.x / 2, titleY},
               titleSize, 3, MILLIONAIRE_GOLD);

    // Prize won display
    char prizeText[64];
    snprintf(prizeText, sizeof(prizeText), "You've won %s!", prizeWon);
    Vector2 prizeMeasure = MeasureTextEx(font, prizeText, 32, 1);
    float prizeY = centerY + 20;

    // Prize box
    Rectangle prizeBox = {centerX - prizeMeasure.x / 2 - 30, prizeY - 10, prizeMeasure.x + 60, 50};
    DrawRectangleRounded(prizeBox, 0.2f, 8, ColorAlpha(MILLIONAIRE_BLUE_DARK, 0.8f));

    float boxPulse = 0.6f + sinf(g_animTime * 4) * 0.4f;
    DrawRectangleRoundedLines(prizeBox, 0.2f, 8, ColorAlpha(MILLIONAIRE_GOLD, boxPulse));

    DrawTextEx(font, prizeText,
               (Vector2){centerX - prizeMeasure.x / 2, prizeY},
               32, 1, MILLIONAIRE_GREEN);

    // Draw confetti on top
    DrawConfettiSystem(&g_confetti);

    // Continue prompt at bottom
    const char *continueText = "Press SELECT to continue";
    Vector2 contMeasure = MeasureTextEx(font, continueText, 18, 1);
    float contPulse = 0.5f + sinf(g_animTime * 3) * 0.5f;
    DrawTextEx(font, continueText,
               (Vector2){centerX - contMeasure.x / 2, MILLIONAIRE_SCREEN_HEIGHT - 45},
               18, 1, ColorAlpha(MILLIONAIRE_WHITE, contPulse * 0.6f + 0.4f));
}

// ============================================================================
// 7. DrawLoseScreen - Game over display
// ============================================================================

/**
 * DrawLoseScreen
 *
 * Draws the losing/game over screen:
 * - Red-tinted background
 * - "GAME OVER" text
 * - Shows correct answer
 * - Shows walk-away prize
 *
 * @param deltaTime      Frame time
 * @param font           Font to use
 * @param correctAnswer  The correct answer text
 * @param correctLetter  Letter of correct answer (A, B, C, or D)
 * @param walkAwayPrize  Prize won before losing
 */
static void DrawLoseScreen(float deltaTime, Font font, const char *correctAnswer,
                           char correctLetter, const char *walkAwayPrize) {
    g_animTime += deltaTime;

    // Darker, slightly red-tinted background
    Color topColor = {20, 10, 15, 255};
    Color bottomColor = {35, 20, 25, 255};
    DrawRectangleGradientV(0, 0, MILLIONAIRE_SCREEN_WIDTH, MILLIONAIRE_SCREEN_HEIGHT, topColor, bottomColor);

    // Dim spotlight
    for (int i = 0; i < 3; i++) {
        float alpha = 0.03f - i * 0.01f;
        Color glow = ColorAlpha(MILLIONAIRE_RED, alpha);
        DrawCircleGradient(MILLIONAIRE_SCREEN_WIDTH / 2, MILLIONAIRE_SCREEN_HEIGHT / 2, 300 - i * 60, glow, ColorAlpha(glow, 0.0f));
    }

    float centerX = MILLIONAIRE_SCREEN_WIDTH / 2;

    // "GAME OVER" text
    const char *gameOverText = "GAME OVER";
    Vector2 goSize = MeasureTextEx(font, gameOverText, 52, 3);
    float goY = 60;

    // Subtle pulsing
    float pulse = 0.7f + sinf(g_animTime * 2) * 0.3f;

    DrawTextEx(font, gameOverText, (Vector2){centerX - goSize.x / 2 + 2, goY + 2}, 52, 3, ColorAlpha(BLACK, 0.5f));
    DrawTextEx(font, gameOverText, (Vector2){centerX - goSize.x / 2, goY}, 52, 3, ColorAlpha(MILLIONAIRE_RED, pulse));

    // "The correct answer was:"
    const char *correctLabel = "The correct answer was:";
    Vector2 labelSize = MeasureTextEx(font, correctLabel, 20, 1);
    DrawTextEx(font, correctLabel, (Vector2){centerX - labelSize.x / 2, 140}, 20, 1, MILLIONAIRE_GRAY);

    // Correct answer box
    char answerText[256];
    snprintf(answerText, sizeof(answerText), "%c: %s", correctLetter, correctAnswer);
    Vector2 answerSize = MeasureTextEx(font, answerText, 24, 1);

    Rectangle answerBox = {centerX - answerSize.x / 2 - 30, 170, answerSize.x + 60, 50};
    DrawRectangleRounded(answerBox, 0.15f, 8, ColorAlpha(MILLIONAIRE_GREEN, 0.25f));
    DrawRectangleRoundedLines(answerBox, 0.15f, 8, MILLIONAIRE_GREEN);
    DrawTextEx(font, answerText, (Vector2){centerX - answerSize.x / 2, 182}, 24, 1, MILLIONAIRE_GREEN);

    // Walk away prize
    char prizeText[64];
    snprintf(prizeText, sizeof(prizeText), "You walk away with: %s", walkAwayPrize);
    Vector2 prizeSize = MeasureTextEx(font, prizeText, 28, 1);

    Rectangle prizeBox = {centerX - prizeSize.x / 2 - 30, 260, prizeSize.x + 60, 50};
    DrawRectangleRounded(prizeBox, 0.15f, 8, ColorAlpha(MILLIONAIRE_GOLD, 0.2f));
    DrawRectangleRoundedLines(prizeBox, 0.15f, 8, MILLIONAIRE_GOLD_DIM);
    DrawTextEx(font, prizeText, (Vector2){centerX - prizeSize.x / 2, 272}, 28, 1, MILLIONAIRE_GOLD);

    // Continue prompt
    const char *continueText = "Press SELECT to continue";
    Vector2 contSize = MeasureTextEx(font, continueText, 18, 1);
    float contPulse = 0.5f + sinf(g_animTime * 3) * 0.5f;
    DrawTextEx(font, continueText, (Vector2){centerX - contSize.x / 2, MILLIONAIRE_SCREEN_HEIGHT - 45},
               18, 1, ColorAlpha(MILLIONAIRE_WHITE, contPulse * 0.5f + 0.3f));
}

// ============================================================================
// Initialization Function
// ============================================================================

/**
 * InitMillionaireGraphics
 *
 * Initialize all graphics systems (particles, confetti, etc.)
 * Call once at plugin startup.
 */
static void InitMillionaireGraphics(void) {
    InitParticleSystem(&g_backgroundParticles);
    InitConfettiSystem(&g_confetti);
    g_animTime = 0.0f;
    g_pulseTime = 0.0f;
}

/**
 * UpdateMillionaireGraphics
 *
 * Update animation timers. Call once per frame.
 */
static void UpdateMillionaireGraphics(float deltaTime) {
    g_animTime += deltaTime;
    g_pulseTime += deltaTime;
}

#endif // MILLIONAIRE_DRAW_H
