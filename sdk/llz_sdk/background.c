/**
 * Animated Background System Implementation
 *
 * Provides reusable animated backgrounds for the host and plugins.
 */

#include "llz_sdk_background.h"
#include "llz_sdk_image.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Style info for each background type
static const char* kStyleNames[LLZ_BG_STYLE_COUNT] = {
    "Pulse Glow",
    "Aurora Sweep",
    "Radial Echo",
    "Neon Strands",
    "Grid Spark",
    "Blurred Album",
    "Constellation",
    "Liquid Gradient",
    "Bokeh Lights"
};

// Internal state
typedef struct {
    bool initialized;
    bool enabled;
    bool inTransition;
    int screenWidth;
    int screenHeight;

    LlzBackgroundStyle currentStyle;
    LlzBackgroundStyle targetStyle;
    float transition;           // 0.0 to 1.0 during transitions
    float time;                 // Master animation timer

    float flashStrength;        // Flash effect on transition
    float indicatorTimer;       // How long to show indicator
    float indicatorAlpha;       // Current indicator opacity
    float indicatorFlashPhase;  // Indicator border pulse

    LlzBackgroundPalette palette;
    float styleSeedA;           // Random seed for variations
    float styleSeedB;           // Random seed for variations

    bool hasCustomColors;
    Color customPrimary;
    Color customAccent;

    float energy;               // For responsive effects (0.0-1.0)

    // Blur texture state (for BG_STYLE_BLUR)
    Texture2D blurTexture;
    Texture2D blurPrevTexture;
    float blurCurrentAlpha;
    float blurPrevAlpha;
} BackgroundState;

static BackgroundState g_bg = {0};

// Utility functions
static float Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static Color PaletteColor(int index, float alpha)
{
    return ColorAlpha(g_bg.palette.colors[index % 6], Clamp01(alpha));
}

// Generate the 6-color palette from primary and accent colors
static void GeneratePalette(void)
{
    Color primary, accent;

    if (g_bg.hasCustomColors) {
        primary = g_bg.customPrimary;
        accent = g_bg.customAccent;
    } else {
        // Default colors if none set
        primary = (Color){180, 180, 200, 255};
        accent = (Color){138, 106, 210, 255};
    }

    Vector3 primaryHsv = ColorToHSV(primary);
    Vector3 accentHsv = ColorToHSV(accent);

    // Palette[0]: Primary color
    g_bg.palette.colors[0] = primary;

    // Palette[1]: Accent color (vibrant)
    g_bg.palette.colors[1] = accent;

    // Palette[2]: Triadic color (120° hue rotation from accent)
    g_bg.palette.colors[2] = ColorFromHSV(fmodf(accentHsv.x + 120.0f, 360.0f),
                                          Clamp01(accentHsv.y * 0.8f + 0.1f),
                                          Clamp01(accentHsv.z * 1.05f));

    // Palette[3]: Complementary-adjacent (200° rotation for variety)
    g_bg.palette.colors[3] = ColorFromHSV(fmodf(primaryHsv.x + 200.0f, 360.0f),
                                          Clamp01(primaryHsv.y * 0.6f + 0.2f),
                                          Clamp01(primaryHsv.z * 0.85f));

    // Palette[4]: Analogous color (30° rotation from accent, high saturation)
    g_bg.palette.colors[4] = ColorFromHSV(fmodf(accentHsv.x + 30.0f, 360.0f),
                                          Clamp01(accentHsv.y * 0.9f + 0.1f),
                                          Clamp01(0.8f + 0.2f * accentHsv.z));

    // Palette[5]: Dark background - derived from primary for cohesion
    if (g_bg.hasCustomColors) {
        g_bg.palette.colors[5] = ColorFromHSV(primaryHsv.x,
                                              Clamp01(primaryHsv.y * 0.3f),
                                              Clamp01(primaryHsv.z * 0.15f));
    } else {
        g_bg.palette.colors[5] = (Color){18, 18, 22, 255};
    }

    g_bg.styleSeedA = (float)GetRandomValue(25, 90) / 100.0f;
    g_bg.styleSeedB = (float)GetRandomValue(0, 1000) / 1000.0f;
}

// === Background Drawing Functions ===

static void DrawPulse(float alpha)
{
    if (alpha <= 0.01f) return;
    Rectangle screen = {0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight};

    float pulse = 0.5f + 0.5f * sinf(g_bg.time * 0.4f);
    float pulse2 = 0.5f + 0.5f * sinf(g_bg.time * 0.25f + 1.0f);

    Color base = PaletteColor(5, alpha);
    DrawRectangleRec(screen, base);

    Vector2 center = {(float)g_bg.screenWidth * 0.5f,
                      (float)g_bg.screenHeight * (0.45f + 0.05f * sinf(g_bg.time * 0.2f))};
    float radius = 380.0f + 60.0f * pulse;
    Color tint = PaletteColor(0, alpha * (0.12f + 0.08f * pulse));
    DrawCircleGradient((int)center.x, (int)center.y, radius, tint, ColorAlpha(tint, 0.0f));

    Color highlight = PaletteColor(1, alpha * (0.08f + 0.04f * pulse2));
    float ox = 80.0f * sinf(g_bg.time * 0.15f);
    float oy = 50.0f * cosf(g_bg.time * 0.12f);
    DrawCircleGradient((int)(center.x + ox), (int)(center.y + oy),
                       200.0f + 30.0f * pulse2, highlight, ColorAlpha(highlight, 0.0f));
}

static void DrawAurora(float alpha)
{
    if (alpha <= 0.01f) return;
    Rectangle screen = {0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight};

    float shift = g_bg.time * 0.08f;
    float blend1 = 0.5f + 0.5f * sinf(shift);
    float blend2 = 0.5f + 0.5f * sinf(shift + 2.0f);

    Color c1 = PaletteColor(5, alpha);
    Color c2 = PaletteColor(2, alpha * (0.15f + 0.1f * blend1));
    Color c3 = PaletteColor(3, alpha * (0.12f + 0.08f * blend2));
    Color c4 = PaletteColor(1, alpha * 0.1f);
    DrawRectangleGradientEx(screen, c1, c2, c3, c4);

    float drift = fmodf(g_bg.time * 8.0f, (float)g_bg.screenHeight * 1.5f);
    for (int i = 0; i < 2; ++i) {
        float bandY = drift + (float)i * (float)g_bg.screenHeight * 0.6f - (float)g_bg.screenHeight * 0.3f;
        float bandHeight = (float)g_bg.screenHeight * 0.25f;
        float bandAlpha = 0.06f + 0.03f * sinf(g_bg.time * 0.3f + i);
        Color bandColor = PaletteColor(i + 1, alpha * bandAlpha);
        DrawRectangleGradientV(0, (int)bandY, g_bg.screenWidth, (int)bandHeight,
                               ColorAlpha(bandColor, 0.0f), bandColor);
        DrawRectangleGradientV(0, (int)(bandY + bandHeight), g_bg.screenWidth, (int)bandHeight,
                               bandColor, ColorAlpha(bandColor, 0.0f));
    }
}

static void DrawRadial(float alpha)
{
    if (alpha <= 0.01f) return;
    Rectangle screen = {0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight};
    DrawRectangleRec(screen, PaletteColor(5, alpha));

    Vector2 center = {(float)g_bg.screenWidth * 0.5f, (float)g_bg.screenHeight * 0.5f};

    for (int i = 0; i < 4; ++i) {
        float phase = fmodf(g_bg.time * 0.12f + i * 0.25f, 1.0f);
        float radius = 60.0f + phase * 500.0f;
        float ringAlpha = (1.0f - phase) * 0.08f;
        Color ring = PaletteColor(i % 3, alpha * ringAlpha);
        DrawRing(center, radius, radius + 3.0f, 0.0f, 360.0f, 64, ring);
    }

    float pulse = 0.5f + 0.5f * sinf(g_bg.time * 0.3f);
    Color glow = PaletteColor(0, alpha * (0.06f + 0.03f * pulse));
    DrawCircleGradient((int)center.x, (int)center.y, 180.0f, glow, ColorAlpha(glow, 0.0f));
}

static void DrawWave(float alpha)
{
    if (alpha <= 0.01f) return;
    DrawRectangleRec((Rectangle){0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight},
                     PaletteColor(5, alpha));

    for (int i = 0; i < 3; ++i) {
        float baseAmplitude = 15.0f + i * 8.0f;
        float amplitude = baseAmplitude * (0.3f + 0.7f * g_bg.energy);
        float speed = 0.25f + 0.1f * i;
        float phaseOffset = g_bg.time * speed + i * 1.5f;
        float baseY = (float)g_bg.screenHeight * (0.3f + 0.2f * i);

        float colorAlpha = 0.04f + 0.06f * g_bg.energy - 0.01f * i;
        if (colorAlpha < 0.02f) colorAlpha = 0.02f;
        Color waveColor = PaletteColor(i + 1, alpha * colorAlpha);

        Vector2 prev = {0.0f, baseY + sinf(phaseOffset) * amplitude};
        for (int x = 4; x <= g_bg.screenWidth; x += 4) {
            float y = baseY + sinf((float)x / 120.0f + phaseOffset) * amplitude;
            Vector2 curr = {(float)x, y};
            DrawLineEx(prev, curr, 2.0f, waveColor);
            prev = curr;
        }
    }
}

static void DrawBgGrid(float alpha)
{
    if (alpha <= 0.01f) return;
    Rectangle screen = {0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight};
    DrawRectangleRec(screen, PaletteColor(5, alpha));

    float spacing = 80.0f;
    float scroll = fmodf(g_bg.time * 6.0f, spacing);
    Color lineColor = PaletteColor(2, alpha * 0.08f);

    for (float x = -spacing; x < g_bg.screenWidth + spacing; x += spacing) {
        DrawLineEx((Vector2){x + scroll, 0}, (Vector2){x + scroll, (float)g_bg.screenHeight}, 1.0f, lineColor);
    }
    for (float y = -spacing; y < g_bg.screenHeight + spacing; y += spacing) {
        DrawLineEx((Vector2){0, y + scroll}, (Vector2){(float)g_bg.screenWidth, y + scroll}, 1.0f, lineColor);
    }

    for (int i = 0; i < 3; ++i) {
        float pulse = 0.5f + 0.5f * sinf(g_bg.time * 0.4f + i * 2.0f);
        float glowX = fmodf(g_bg.styleSeedA * g_bg.screenWidth + i * 200.0f + scroll, g_bg.screenWidth);
        float glowY = fmodf(g_bg.styleSeedB * g_bg.screenHeight + i * 150.0f + scroll * 0.7f, g_bg.screenHeight);
        Color glowColor = PaletteColor(i, alpha * 0.04f * pulse);
        DrawCircleGradient((int)glowX, (int)glowY, 60.0f, glowColor, ColorAlpha(glowColor, 0.0f));
    }
}

static void DrawBlur(float alpha)
{
    if (alpha <= 0.01f) return;
    Rectangle screen = {0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight};

    bool hasPrev = g_bg.blurPrevTexture.id != 0 && g_bg.blurPrevAlpha > 0.01f;
    bool hasCurrent = g_bg.blurTexture.id != 0 && g_bg.blurCurrentAlpha > 0.01f;

    if (hasPrev) {
        float prevA = alpha * g_bg.blurPrevAlpha;
        Color tint = ColorAlpha(WHITE, prevA);
        LlzDrawTextureCover(g_bg.blurPrevTexture, screen, tint);
    }

    if (hasCurrent) {
        float currA = alpha * g_bg.blurCurrentAlpha;
        Color tint = ColorAlpha(WHITE, currA);
        LlzDrawTextureCover(g_bg.blurTexture, screen, tint);
    }

    if (!hasPrev && !hasCurrent) {
        DrawRectangleRec(screen, PaletteColor(5, alpha));
    }
}

static void DrawConstellation(float alpha)
{
    if (alpha <= 0.01f) return;
    Rectangle screen = {0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight};
    DrawRectangleRec(screen, PaletteColor(5, alpha));

    #define CONSTELLATION_POINTS 12
    Vector2 points[CONSTELLATION_POINTS];
    float time = g_bg.time;

    for (int i = 0; i < CONSTELLATION_POINTS; i++) {
        float seed = (float)i * 0.7f + g_bg.styleSeedA * 3.0f;
        float xBase = (float)g_bg.screenWidth * (0.1f + 0.8f * ((float)(i % 4) / 3.0f));
        float yBase = (float)g_bg.screenHeight * (0.15f + 0.7f * ((float)(i / 4) / 2.0f));

        float xOff = 40.0f * sinf(time * 0.15f + seed);
        float yOff = 30.0f * cosf(time * 0.12f + seed * 1.3f);

        points[i] = (Vector2){xBase + xOff, yBase + yOff};
    }

    float connectionDist = 180.0f;
    for (int i = 0; i < CONSTELLATION_POINTS; i++) {
        for (int j = i + 1; j < CONSTELLATION_POINTS; j++) {
            float dx = points[j].x - points[i].x;
            float dy = points[j].y - points[i].y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < connectionDist) {
                float lineFade = 1.0f - (dist / connectionDist);
                float pulse = 0.5f + 0.5f * sinf(time * 0.3f + (float)(i + j) * 0.5f);
                Color lineColor = PaletteColor((i + j) % 3, alpha * 0.06f * lineFade * pulse);
                DrawLineEx(points[i], points[j], 1.5f, lineColor);
            }
        }
    }

    for (int i = 0; i < CONSTELLATION_POINTS; i++) {
        float pulse = 0.6f + 0.4f * sinf(time * 0.4f + (float)i * 0.8f);
        float radius = 3.0f + 2.0f * pulse;
        Color starColor = PaletteColor(i % 4, alpha * (0.15f + 0.1f * pulse));

        DrawCircleV(points[i], radius, starColor);

        Color glowColor = PaletteColor(i % 4, alpha * 0.04f * pulse);
        DrawCircleGradient((int)points[i].x, (int)points[i].y, 25.0f + 10.0f * pulse,
                           glowColor, ColorAlpha(glowColor, 0.0f));
    }

    #undef CONSTELLATION_POINTS
}

static void DrawLiquid(float alpha)
{
    if (alpha <= 0.01f) return;
    Rectangle screen = {0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight};
    DrawRectangleRec(screen, PaletteColor(5, alpha));

    float time = g_bg.time;

    #define LIQUID_BLOBS 5
    struct {
        float xPhase, yPhase, xSpeed, ySpeed;
        float radiusBase, radiusMod;
        int colorIdx;
    } blobs[LIQUID_BLOBS] = {
        {0.0f, 0.5f, 0.08f, 0.06f, 300.0f, 50.0f, 0},
        {1.5f, 2.0f, 0.10f, 0.07f, 250.0f, 40.0f, 1},
        {3.0f, 1.0f, 0.07f, 0.09f, 280.0f, 60.0f, 2},
        {4.5f, 3.5f, 0.09f, 0.05f, 220.0f, 35.0f, 3},
        {2.5f, 4.0f, 0.06f, 0.08f, 260.0f, 45.0f, 4}
    };

    for (int i = 0; i < LIQUID_BLOBS; i++) {
        float xNorm = 0.5f + 0.45f * sinf(time * blobs[i].xSpeed + blobs[i].xPhase);
        float yNorm = 0.5f + 0.45f * sinf(time * blobs[i].ySpeed + blobs[i].yPhase);
        float x = xNorm * (float)g_bg.screenWidth;
        float y = yNorm * (float)g_bg.screenHeight;

        float radiusPulse = sinf(time * 0.15f + blobs[i].xPhase * 0.5f);
        float radius = blobs[i].radiusBase + blobs[i].radiusMod * radiusPulse;

        Color blobColor = PaletteColor(blobs[i].colorIdx, alpha * 0.08f);
        DrawCircleGradient((int)x, (int)y, radius, blobColor, ColorAlpha(blobColor, 0.0f));

        Color innerColor = PaletteColor((blobs[i].colorIdx + 1) % 5, alpha * 0.05f);
        DrawCircleGradient((int)x, (int)y, radius * 0.4f, innerColor, ColorAlpha(innerColor, 0.0f));
    }

    #undef LIQUID_BLOBS
}

static void DrawBokeh(float alpha)
{
    if (alpha <= 0.01f) return;
    Rectangle screen = {0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight};
    DrawRectangleRec(screen, PaletteColor(5, alpha));

    float time = g_bg.time;

    #define BOKEH_COUNT 15
    for (int i = 0; i < BOKEH_COUNT; i++) {
        float seed = (float)i * 1.7f + g_bg.styleSeedA * 5.0f + g_bg.styleSeedB * 3.0f;

        float speedMult = 0.5f + (float)(i % 3) * 0.3f;
        float xSpeed = 0.03f * speedMult;
        float ySpeed = 0.02f * speedMult;

        float xBase = fmodf(seed * 0.37f, 1.0f);
        float yBase = fmodf(seed * 0.53f, 1.0f);
        float x = (float)g_bg.screenWidth * (xBase + 0.3f * sinf(time * xSpeed + seed));
        float y = (float)g_bg.screenHeight * (yBase + 0.25f * sinf(time * ySpeed + seed * 1.4f));

        x = fmodf(x + (float)g_bg.screenWidth, (float)g_bg.screenWidth);
        y = fmodf(y + (float)g_bg.screenHeight, (float)g_bg.screenHeight);

        float baseRadius = 30.0f + (float)(i % 5) * 15.0f;
        float pulse = 0.85f + 0.15f * sinf(time * 0.25f + seed);
        float radius = baseRadius * pulse;

        float depthAlpha = 0.04f + 0.03f * (float)(i % 4) / 3.0f;
        Color bokehColor = PaletteColor(i % 5, alpha * depthAlpha);

        DrawCircleGradient((int)x, (int)y, radius * 1.3f,
                           ColorAlpha(bokehColor, 0.0f), ColorAlpha(bokehColor, 0.0f));

        Color centerColor = PaletteColor(i % 5, alpha * depthAlpha * 1.2f);
        DrawCircleGradient((int)x, (int)y, radius, centerColor, bokehColor);

        Color highlightColor = PaletteColor((i + 1) % 5, alpha * depthAlpha * 0.3f);
        float hlX = x - radius * 0.25f;
        float hlY = y - radius * 0.25f;
        DrawCircleGradient((int)hlX, (int)hlY, radius * 0.2f,
                           highlightColor, ColorAlpha(highlightColor, 0.0f));
    }

    #undef BOKEH_COUNT
}

static void DrawStyle(LlzBackgroundStyle style, float alpha)
{
    switch (style) {
        case LLZ_BG_STYLE_PULSE: DrawPulse(alpha); break;
        case LLZ_BG_STYLE_AURORA: DrawAurora(alpha); break;
        case LLZ_BG_STYLE_RADIAL: DrawRadial(alpha); break;
        case LLZ_BG_STYLE_WAVE: DrawWave(alpha); break;
        case LLZ_BG_STYLE_GRID: DrawBgGrid(alpha); break;
        case LLZ_BG_STYLE_BLUR: DrawBlur(alpha); break;
        case LLZ_BG_STYLE_CONSTELLATION: DrawConstellation(alpha); break;
        case LLZ_BG_STYLE_LIQUID: DrawLiquid(alpha); break;
        case LLZ_BG_STYLE_BOKEH: DrawBokeh(alpha); break;
        default: break;
    }
}

// === Public API Implementation ===

void LlzBackgroundInit(int screenWidth, int screenHeight)
{
    memset(&g_bg, 0, sizeof(g_bg));
    g_bg.screenWidth = screenWidth;
    g_bg.screenHeight = screenHeight;
    g_bg.currentStyle = LLZ_BG_STYLE_PULSE;
    g_bg.targetStyle = LLZ_BG_STYLE_PULSE;
    g_bg.energy = 1.0f;
    g_bg.initialized = true;

    GeneratePalette();

    printf("[SDK] Background system initialized (%dx%d)\n", screenWidth, screenHeight);
}

void LlzBackgroundShutdown(void)
{
    memset(&g_bg, 0, sizeof(g_bg));
    printf("[SDK] Background system shutdown\n");
}

void LlzBackgroundUpdate(float deltaTime)
{
    if (!g_bg.initialized) return;

    g_bg.time += deltaTime;

    // Handle transitions
    if (g_bg.inTransition) {
        const float transitionTime = 0.65f;
        g_bg.transition += deltaTime / transitionTime;
        if (g_bg.transition >= 1.0f) {
            g_bg.currentStyle = g_bg.targetStyle;
            g_bg.transition = 1.0f;
            g_bg.inTransition = false;
        }
    }

    // Update indicator fade
    if (g_bg.indicatorTimer > 0.0f) {
        g_bg.indicatorTimer -= deltaTime;
        if (g_bg.indicatorTimer < 0.0f) g_bg.indicatorTimer = 0.0f;
        float normalized = g_bg.indicatorTimer / 1.3f;
        g_bg.indicatorAlpha = Clamp01(normalized);
        g_bg.indicatorFlashPhase += deltaTime * 12.0f;
    } else {
        g_bg.indicatorAlpha = 0.0f;
    }

    // Update flash effect
    if (g_bg.flashStrength > 0.0f) {
        g_bg.flashStrength -= deltaTime * 2.6f;
        if (g_bg.flashStrength < 0.0f) g_bg.flashStrength = 0.0f;
    }
}

void LlzBackgroundDraw(void)
{
    if (!g_bg.initialized || !g_bg.enabled) return;

    if (g_bg.inTransition) {
        DrawStyle(g_bg.currentStyle, Clamp01(1.0f - g_bg.transition));
        DrawStyle(g_bg.targetStyle, Clamp01(g_bg.transition));
    } else {
        DrawStyle(g_bg.currentStyle, 1.0f);
    }

    // Flash overlay on transition
    if (g_bg.flashStrength > 0.01f) {
        Color flash = ColorAlpha(g_bg.palette.colors[1], 0.1f * g_bg.flashStrength);
        DrawRectangleRec((Rectangle){0, 0, (float)g_bg.screenWidth, (float)g_bg.screenHeight}, flash);
    }
}

void LlzBackgroundDrawIndicator(void)
{
    if (!g_bg.initialized || g_bg.indicatorAlpha <= 0.01f) return;

    float alpha = g_bg.indicatorAlpha;
    float width = 320.0f;
    float height = 64.0f;
    Rectangle panel = {
        (float)g_bg.screenWidth * 0.5f - width * 0.5f,
        28.0f,
        width,
        height
    };

    Color accentColor = g_bg.palette.colors[1];

    float flash = 0.5f + 0.5f * sinf(g_bg.indicatorFlashPhase * 2.0f);
    Color panelColor = ColorAlpha(g_bg.palette.colors[5], 0.85f * alpha);
    Color borderColor = ColorAlpha(accentColor, alpha * (0.6f + 0.4f * flash));

    DrawRectangleRounded(panel, 0.4f, 16, panelColor);
    DrawRectangleRoundedLines(panel, 0.4f, 16, borderColor);

    Color titleColor = ColorAlpha(accentColor, alpha);
    Color detailColor = ColorAlpha(g_bg.palette.colors[0], alpha * 0.7f);

    const char *styleName = kStyleNames[g_bg.targetStyle];
    DrawText(styleName, (int)(panel.x + 20), (int)(panel.y + 12), 24, titleColor);

    char detail[32];
    snprintf(detail, sizeof(detail), "Style %d/%d", g_bg.targetStyle + 1, LLZ_BG_STYLE_COUNT);
    DrawText(detail, (int)(panel.x + 20), (int)(panel.y + 40), 16, detailColor);
}

void LlzBackgroundCycleNext(void)
{
    if (!g_bg.initialized) return;

    // Commit any in-progress transition
    if (g_bg.inTransition) {
        g_bg.currentStyle = g_bg.targetStyle;
        g_bg.transition = 1.0f;
        g_bg.inTransition = false;
    }

    GeneratePalette();

    g_bg.targetStyle = (g_bg.currentStyle + 1) % LLZ_BG_STYLE_COUNT;
    g_bg.transition = 0.0f;
    g_bg.enabled = true;
    g_bg.inTransition = true;
    g_bg.time = 0.0f;

    // Show indicator
    g_bg.indicatorTimer = 1.3f;
    g_bg.indicatorAlpha = 1.0f;
    g_bg.indicatorFlashPhase = 0.0f;
    g_bg.flashStrength = 1.0f;

    printf("[SDK] Background cycling to style %d: %s\n",
           g_bg.targetStyle, kStyleNames[g_bg.targetStyle]);
}

void LlzBackgroundSetStyle(LlzBackgroundStyle style, bool animate)
{
    if (!g_bg.initialized || style >= LLZ_BG_STYLE_COUNT) return;

    if (animate) {
        if (g_bg.inTransition) {
            g_bg.currentStyle = g_bg.targetStyle;
        }
        g_bg.targetStyle = style;
        g_bg.transition = 0.0f;
        g_bg.enabled = true;
        g_bg.inTransition = true;
        g_bg.indicatorTimer = 1.3f;
        g_bg.indicatorAlpha = 1.0f;
        g_bg.indicatorFlashPhase = 0.0f;
        g_bg.flashStrength = 1.0f;
    } else {
        g_bg.currentStyle = style;
        g_bg.targetStyle = style;
        g_bg.enabled = true;
        g_bg.inTransition = false;
    }

    GeneratePalette();
}

LlzBackgroundStyle LlzBackgroundGetStyle(void)
{
    return g_bg.currentStyle;
}

bool LlzBackgroundIsEnabled(void)
{
    return g_bg.initialized && g_bg.enabled;
}

void LlzBackgroundSetEnabled(bool enabled)
{
    g_bg.enabled = enabled;
}

void LlzBackgroundSetColors(Color primary, Color accent)
{
    g_bg.hasCustomColors = true;
    g_bg.customPrimary = primary;
    g_bg.customAccent = accent;
    GeneratePalette();
}

void LlzBackgroundClearColors(void)
{
    g_bg.hasCustomColors = false;
    GeneratePalette();
}

void LlzBackgroundSetBlurTexture(Texture2D texture, Texture2D prevTexture,
                                  float currentAlpha, float prevAlpha)
{
    g_bg.blurTexture = texture;
    g_bg.blurPrevTexture = prevTexture;
    g_bg.blurCurrentAlpha = currentAlpha;
    g_bg.blurPrevAlpha = prevAlpha;
}

void LlzBackgroundSetEnergy(float energy)
{
    g_bg.energy = Clamp01(energy);
}

const char* LlzBackgroundGetStyleName(LlzBackgroundStyle style)
{
    if (style >= LLZ_BG_STYLE_COUNT) return "Unknown";
    return kStyleNames[style];
}

int LlzBackgroundGetStyleCount(void)
{
    return LLZ_BG_STYLE_COUNT;
}

const LlzBackgroundPalette* LlzBackgroundGetPalette(void)
{
    return &g_bg.palette;
}
