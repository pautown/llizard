/**
 * bejeweled_plugin.c - Flashy Bejeweled match-3 game plugin
 *
 * A polished, visually stunning match-3 puzzle game with:
 * - Beautiful gem rendering with gradients and shine effects
 * - Particle explosions on matches
 * - Screen shake effects for big combos
 * - Score popups that float up
 * - Smooth animations for swapping, falling, and spawning
 */

#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "llz_notification.h"
#include "raylib.h"
#include "bejeweled_logic.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * VISUAL CONSTANTS
 * ============================================================================ */

/* Colors - dark elegant theme */
#define COLOR_BG            (Color){15, 15, 25, 255}
#define COLOR_BOARD_BG      (Color){25, 28, 40, 255}
#define COLOR_CELL_EMPTY    (Color){35, 40, 55, 255}
#define COLOR_TEXT          (Color){240, 240, 250, 255}
#define COLOR_TEXT_MUTED    (Color){140, 145, 165, 255}
#define COLOR_HIGHLIGHT     (Color){255, 215, 0, 255}
#define COLOR_GLOW          (Color){255, 255, 255, 100}

/* Gem colors with gradients */
static const Color GEM_COLORS_BASE[] = {
    {0, 0, 0, 0},           /* GEM_EMPTY */
    {220, 50, 50, 255},     /* GEM_RED */
    {255, 140, 0, 255},     /* GEM_ORANGE */
    {255, 220, 0, 255},     /* GEM_YELLOW */
    {50, 200, 80, 255},     /* GEM_GREEN */
    {60, 120, 230, 255},    /* GEM_BLUE */
    {150, 80, 200, 255},    /* GEM_PURPLE */
    {230, 230, 250, 255}    /* GEM_WHITE */
};

static const Color GEM_COLORS_LIGHT[] = {
    {0, 0, 0, 0},
    {255, 120, 120, 255},
    {255, 190, 80, 255},
    {255, 255, 120, 255},
    {120, 255, 150, 255},
    {140, 180, 255, 255},
    {200, 150, 255, 255},
    {255, 255, 255, 255}
};

static const Color GEM_COLORS_DARK[] = {
    {0, 0, 0, 0},
    {150, 20, 20, 255},
    {180, 80, 0, 255},
    {180, 150, 0, 255},
    {20, 120, 40, 255},
    {30, 70, 160, 255},
    {90, 40, 140, 255},
    {180, 180, 200, 255}
};

/* Animation timing */
#define ANIM_SWAP_SPEED     12.0f
#define ANIM_FALL_SPEED     15.0f
#define ANIM_REMOVE_SPEED   8.0f
#define ANIM_SPAWN_SPEED    6.0f

/* Particle system - optimized for performance */
#define MAX_PARTICLES       96
#define PARTICLE_LIFE       0.6f

/* Score popups */
#define MAX_SCORE_POPUPS    16
#define POPUP_LIFE          1.2f

/* Screen shake */
#define SHAKE_DECAY         8.0f

/* ============================================================================
 * PARTICLE SYSTEM
 * ============================================================================ */

typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float maxLife;
    float size;
    float rotation;
    float rotSpeed;
} Particle;

typedef struct {
    float x;
    float y;
    int score;
    float life;
    float maxLife;
    Color color;
} ScorePopup;

/* ============================================================================
 * PLUGIN STATE
 * ============================================================================ */

static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;
static Font g_font;

/* Board rendering */
static float g_boardX = 0;
static float g_boardY = 0;
static float g_cellSize = 0;
static float g_boardSize = 0;

/* Animation state */
static float g_animTimer = 0;
static float g_stateTimer = 0;
static float g_shimmerTime = 0;

/* Screen shake */
static float g_shakeIntensity = 0;
static Vector2 g_shakeOffset = {0, 0};

/* Particles */
static Particle g_particles[MAX_PARTICLES];
static int g_particleCount = 0;

/* Score popups */
static ScorePopup g_popups[MAX_SCORE_POPUPS];
static int g_popupCount = 0;

/* Input tracking */
static int g_cursorX = 0;
static int g_cursorY = 0;
static bool g_touchActive = false;
static Vector2 g_touchStart = {0, 0};

/* Hint system */
static float g_hintTimer = 0;
static int g_hintX1 = -1, g_hintY1 = -1;
static int g_hintX2 = -1, g_hintY2 = -1;
static bool g_showHint = false;

/* Media notification */
static bool g_mediaInitialized = false;
static LlzSubscriptionId g_trackSubId = 0;

/* Lightning effect state */
static bool g_lightningActive = false;
static float g_lightningTimer = 0;
static float g_lightningDuration = 0.6f;
static bool g_lightningHorizontal = false;
static int g_lightningIndex = 0;
static int g_lightningCenterX = 0;
static int g_lightningCenterY = 0;

/* Lightning bolt segments for jagged effect */
#define LIGHTNING_SEGMENTS 12
static Vector2 g_lightningPoints[LIGHTNING_SEGMENTS + 1];

/* ============================================================================
 * ENHANCED VISUAL SYSTEM - Background, Combos, Effects
 * ============================================================================ */

/* Background star field - reduced for performance */
#define MAX_BG_STARS 12
typedef struct {
    Vector2 pos;
    float depth;
    float brightness;
    float twinklePhase;
    float size;
} BackgroundStar;
static BackgroundStar g_bgStars[MAX_BG_STARS];

/* Background animation state */
static float g_bgPulseIntensity = 0.0f;
static float g_bgGridOffset = 0.0f;
static float g_cascadeFlashTimer = 0.0f;
static Color g_cascadeFlashColor = {0, 0, 0, 0};

/* Combo announcement system */
#define MAX_COMBO_ANNOUNCEMENTS 3
typedef struct {
    char text[32];
    float x, y;
    float life;
    float maxLife;
    float scale;
    float rotation;
    Color color;
    bool active;
} ComboAnnouncement;
static ComboAnnouncement g_comboAnnouncements[MAX_COMBO_ANNOUNCEMENTS];
static int g_lastCascadeLevel = 0;

/* Animated score display */
static int g_displayScore = 0;
static float g_scoreAnimTimer = 0.0f;
static float g_scorePulse = 0.0f;
static int g_previousLevel = 1;

/* Level up celebration */
static bool g_levelUpActive = false;
static float g_levelUpTimer = 0.0f;
static int g_levelUpLevel = 0;

/* Screen flash effect */
static float g_screenFlashTimer = 0.0f;
static Color g_screenFlashColor = {255, 255, 255, 0};

/* Combo tier definitions */
static const char* COMBO_TEXTS[] = {
    "",           /* 0 - unused */
    "",           /* 1 - no combo */
    "COMBO!",     /* 2 */
    "EXCELLENT!", /* 3 */
    "FANTASTIC!", /* 4 */
    "INCREDIBLE!",/* 5 */
    "LEGENDARY!", /* 6 */
    "GODLIKE!"    /* 7+ */
};

static const Color COMBO_COLORS[] = {
    {255, 255, 255, 255},  /* 0 */
    {255, 255, 255, 255},  /* 1 */
    {255, 215, 0, 255},    /* 2 - Gold */
    {50, 255, 50, 255},    /* 3 - Green */
    {0, 200, 255, 255},    /* 4 - Cyan */
    {255, 100, 255, 255},  /* 5 - Magenta */
    {255, 50, 50, 255},    /* 6 - Red */
    {255, 255, 255, 255}   /* 7+ - White (rainbow) */
};

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

static float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static float EaseOutBounce(float t) {
    float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.0f / d1) return n1 * t * t;
    if (t < 2.0f / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
    if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
    t -= 2.625f / d1; return n1 * t * t + 0.984375f;
}

static float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float EaseOutBack(float t) {
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

static float EaseOutElastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    float p = 0.3f;
    return powf(2.0f, -10.0f * t) * sinf((t - p / 4.0f) * (2.0f * PI) / p) + 1.0f;
}

static Color LerpColor(Color a, Color b, float t) {
    return (Color){
        (unsigned char)Lerp(a.r, b.r, t),
        (unsigned char)Lerp(a.g, b.g, t),
        (unsigned char)Lerp(a.b, b.b, t),
        (unsigned char)Lerp(a.a, b.a, t)
    };
}

/* Convert string to uppercase in-place */
static void ToUpperCase(char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = str[i] - 'a' + 'A';
        }
    }
}

/* ============================================================================
 * LAYOUT CALCULATION
 * ============================================================================ */

static void CalculateLayout(void) {
    float margin = 16.0f;
    float headerHeight = 60.0f;  /* Reduced header for smaller overlays */
    float availableHeight = g_screenHeight - headerHeight - margin * 2;
    float availableWidth = g_screenHeight - margin * 2; /* Keep board square-ish */

    g_boardSize = (availableHeight < availableWidth) ? availableHeight : availableWidth;
    g_cellSize = g_boardSize / BOARD_WIDTH;

    /* Center board both horizontally and vertically */
    g_boardX = (g_screenWidth - g_boardSize) / 2.0f;
    g_boardY = headerHeight + (availableHeight - g_boardSize) / 2.0f + margin;
}

static void GridToScreen(int gx, int gy, float *sx, float *sy) {
    *sx = g_boardX + gx * g_cellSize + g_cellSize / 2.0f;
    *sy = g_boardY + gy * g_cellSize + g_cellSize / 2.0f;
}

static bool ScreenToGrid(float sx, float sy, int *gx, int *gy) {
    float localX = sx - g_boardX;
    float localY = sy - g_boardY;

    if (localX < 0 || localY < 0 || localX >= g_boardSize || localY >= g_boardSize) {
        return false;
    }

    *gx = (int)(localX / g_cellSize);
    *gy = (int)(localY / g_cellSize);

    return (*gx >= 0 && *gx < BOARD_WIDTH && *gy >= 0 && *gy < BOARD_HEIGHT);
}

/* ============================================================================
 * PARTICLE SYSTEM
 * ============================================================================ */

static void SpawnParticles(float x, float y, Color color, int count) {
    for (int i = 0; i < count && g_particleCount < MAX_PARTICLES; i++) {
        Particle *p = &g_particles[g_particleCount++];
        p->pos = (Vector2){x, y};
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float speed = GetRandomValue(100, 300);
        p->vel = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
        p->color = color;
        p->life = PARTICLE_LIFE + GetRandomValue(-20, 20) / 100.0f;
        p->maxLife = p->life;
        p->size = GetRandomValue(4, 12);
        p->rotation = GetRandomValue(0, 360);
        p->rotSpeed = GetRandomValue(-500, 500);
    }
}

static void SpawnMatchParticles(int x, int y, int gemType) {
    float sx, sy;
    GridToScreen(x, y, &sx, &sy);

    Color baseColor = GEM_COLORS_BASE[gemType];

    /* Spawn simple burst - reduced for performance */
    SpawnParticles(sx, sy, baseColor, 4);
    SpawnParticles(sx, sy, WHITE, 2);
}

static void UpdateParticles(float deltaTime) {
    for (int i = g_particleCount - 1; i >= 0; i--) {
        Particle *p = &g_particles[i];
        p->life -= deltaTime;

        if (p->life <= 0) {
            g_particles[i] = g_particles[--g_particleCount];
            continue;
        }

        /* Apply physics */
        p->vel.y += 400.0f * deltaTime; /* Gravity */
        p->pos.x += p->vel.x * deltaTime;
        p->pos.y += p->vel.y * deltaTime;
        p->rotation += p->rotSpeed * deltaTime;

        /* Fade and shrink */
        float lifeRatio = p->life / p->maxLife;
        p->size *= (0.98f + lifeRatio * 0.02f);
    }
}

static void DrawParticles(void) {
    for (int i = 0; i < g_particleCount; i++) {
        Particle *p = &g_particles[i];
        float lifeRatio = p->life / p->maxLife;

        Color color = p->color;
        color.a = (unsigned char)(color.a * lifeRatio);

        /* Simple circle drawing - much faster than polys */
        Vector2 pos = {p->pos.x + g_shakeOffset.x, p->pos.y + g_shakeOffset.y};
        float size = p->size * lifeRatio;
        DrawCircleV(pos, size, color);
    }
}

/* ============================================================================
 * SCORE POPUPS
 * ============================================================================ */

static void SpawnScorePopup(float x, float y, int score, Color color) {
    if (g_popupCount >= MAX_SCORE_POPUPS) return;

    ScorePopup *popup = &g_popups[g_popupCount++];
    popup->x = x;
    popup->y = y;
    popup->score = score;
    popup->life = POPUP_LIFE;
    popup->maxLife = POPUP_LIFE;
    popup->color = color;
}

static void UpdatePopups(float deltaTime) {
    for (int i = g_popupCount - 1; i >= 0; i--) {
        ScorePopup *p = &g_popups[i];
        p->life -= deltaTime;

        if (p->life <= 0) {
            g_popups[i] = g_popups[--g_popupCount];
            continue;
        }

        /* Float upward */
        p->y -= 60.0f * deltaTime;
    }
}

static void DrawPopups(void) {
    for (int i = 0; i < g_popupCount; i++) {
        ScorePopup *p = &g_popups[i];
        float lifeRatio = p->life / p->maxLife;

        char text[32];
        snprintf(text, sizeof(text), "+%d", p->score);

        int fontSize = 24 + (int)(8.0f * lifeRatio);
        float scale = 0.5f + 0.5f * EaseOutBack(1.0f - lifeRatio + 0.5f);
        if (scale > 1.0f) scale = 1.0f;

        Color color = p->color;
        color.a = (unsigned char)(255 * lifeRatio);

        Vector2 textSize = MeasureTextEx(g_font, text, fontSize, 1);
        float x = p->x - textSize.x / 2.0f + g_shakeOffset.x;
        float y = p->y - textSize.y / 2.0f + g_shakeOffset.y;

        /* Shadow */
        DrawTextEx(g_font, text, (Vector2){x + 2, y + 2}, fontSize, 1, (Color){0, 0, 0, color.a / 2});
        /* Main text */
        DrawTextEx(g_font, text, (Vector2){x, y}, fontSize, 1, color);
    }
}

/* ============================================================================
 * SCREEN SHAKE
 * ============================================================================ */

static void TriggerShake(float intensity) {
    if (intensity > g_shakeIntensity) {
        g_shakeIntensity = intensity;
    }
}

static void UpdateShake(float deltaTime) {
    if (g_shakeIntensity > 0.1f) {
        g_shakeOffset.x = (GetRandomValue(-100, 100) / 100.0f) * g_shakeIntensity;
        g_shakeOffset.y = (GetRandomValue(-100, 100) / 100.0f) * g_shakeIntensity;
        g_shakeIntensity -= g_shakeIntensity * SHAKE_DECAY * deltaTime;
    } else {
        g_shakeIntensity = 0;
        g_shakeOffset.x = 0;
        g_shakeOffset.y = 0;
    }
}

/* ============================================================================
 * LIGHTNING EFFECT
 * ============================================================================ */

/* Generate jagged lightning bolt points */
static void GenerateLightningBolt(float startX, float startY, float endX, float endY) {
    g_lightningPoints[0] = (Vector2){startX, startY};
    g_lightningPoints[LIGHTNING_SEGMENTS] = (Vector2){endX, endY};

    float dx = (endX - startX) / LIGHTNING_SEGMENTS;
    float dy = (endY - startY) / LIGHTNING_SEGMENTS;

    /* Calculate perpendicular direction for jagged offsets */
    float length = sqrtf(dx * dx + dy * dy);
    float perpX = -dy / length;
    float perpY = dx / length;

    /* Generate intermediate points with random offsets */
    for (int i = 1; i < LIGHTNING_SEGMENTS; i++) {
        float baseX = startX + dx * i;
        float baseY = startY + dy * i;

        /* Offset perpendicular to the line, more in the middle */
        float midFactor = 1.0f - fabsf((float)i / LIGHTNING_SEGMENTS - 0.5f) * 2.0f;
        float maxOffset = g_cellSize * 0.4f * midFactor;
        float offset = (GetRandomValue(-100, 100) / 100.0f) * maxOffset;

        g_lightningPoints[i] = (Vector2){
            baseX + perpX * offset,
            baseY + perpY * offset
        };
    }
}

/* Start a lightning strike effect */
static void TriggerLightning(bool isHorizontal, int index, int centerX, int centerY) {
    g_lightningActive = true;
    g_lightningTimer = g_lightningDuration;
    g_lightningHorizontal = isHorizontal;
    g_lightningIndex = index;
    g_lightningCenterX = centerX;
    g_lightningCenterY = centerY;

    /* Calculate start and end points */
    float cx, cy;
    GridToScreen(centerX, centerY, &cx, &cy);

    float startX, startY, endX, endY;
    if (isHorizontal) {
        /* Horizontal strike across the row */
        startX = g_boardX - 20;
        startY = g_boardY + index * g_cellSize + g_cellSize / 2.0f;
        endX = g_boardX + g_boardSize + 20;
        endY = startY;
    } else {
        /* Vertical strike down the column */
        startX = g_boardX + index * g_cellSize + g_cellSize / 2.0f;
        startY = g_boardY - 20;
        endX = startX;
        endY = g_boardY + g_boardSize + 20;
    }

    GenerateLightningBolt(startX, startY, endX, endY);

    /* Big screen shake for lightning */
    TriggerShake(20.0f);
}

/* Update lightning effect */
static void UpdateLightning(float deltaTime) {
    if (!g_lightningActive) return;

    g_lightningTimer -= deltaTime;

    /* Regenerate bolt shape periodically for flickering effect */
    if (GetRandomValue(0, 100) < 30) {
        float startX, startY, endX, endY;
        if (g_lightningHorizontal) {
            startX = g_boardX - 20;
            startY = g_boardY + g_lightningIndex * g_cellSize + g_cellSize / 2.0f;
            endX = g_boardX + g_boardSize + 20;
            endY = startY;
        } else {
            startX = g_boardX + g_lightningIndex * g_cellSize + g_cellSize / 2.0f;
            startY = g_boardY - 20;
            endX = startX;
            endY = g_boardY + g_boardSize + 20;
        }
        GenerateLightningBolt(startX, startY, endX, endY);
    }

    if (g_lightningTimer <= 0) {
        g_lightningActive = false;
    }
}

/* Draw the lightning effect */
static void DrawLightning(void) {
    if (!g_lightningActive) return;

    float intensity = g_lightningTimer / g_lightningDuration;
    float flicker = (sinf(g_animTimer * 60.0f) + 1.0f) * 0.5f;

    /* Draw glow behind lightning */
    Color glowColor = (Color){200, 220, 255, (unsigned char)(100 * intensity * flicker)};

    if (g_lightningHorizontal) {
        float y = g_boardY + g_lightningIndex * g_cellSize + g_cellSize / 2.0f + g_shakeOffset.y;
        DrawRectangle((int)(g_boardX + g_shakeOffset.x - 10), (int)(y - g_cellSize * 0.6f),
                     (int)(g_boardSize + 20), (int)(g_cellSize * 1.2f), glowColor);
    } else {
        float x = g_boardX + g_lightningIndex * g_cellSize + g_cellSize / 2.0f + g_shakeOffset.x;
        DrawRectangle((int)(x - g_cellSize * 0.6f), (int)(g_boardY + g_shakeOffset.y - 10),
                     (int)(g_cellSize * 1.2f), (int)(g_boardSize + 20), glowColor);
    }

    /* Draw multiple layers of lightning bolt */
    for (int layer = 0; layer < 3; layer++) {
        float thickness = (3 - layer) * 4.0f;
        unsigned char alpha = (unsigned char)((layer == 0 ? 255 : (layer == 1 ? 180 : 100)) * intensity * flicker);

        Color boltColor;
        if (layer == 0) {
            boltColor = (Color){255, 255, 255, alpha};  /* White core */
        } else if (layer == 1) {
            boltColor = (Color){180, 200, 255, alpha};  /* Blue-white middle */
        } else {
            boltColor = (Color){100, 150, 255, alpha};  /* Blue outer glow */
        }

        /* Draw the jagged bolt */
        for (int i = 0; i < LIGHTNING_SEGMENTS; i++) {
            Vector2 p1 = {g_lightningPoints[i].x + g_shakeOffset.x,
                         g_lightningPoints[i].y + g_shakeOffset.y};
            Vector2 p2 = {g_lightningPoints[i + 1].x + g_shakeOffset.x,
                         g_lightningPoints[i + 1].y + g_shakeOffset.y};
            DrawLineEx(p1, p2, thickness, boltColor);
        }

        /* Draw branching bolts for outer layer */
        if (layer == 2 && GetRandomValue(0, 100) < 50) {
            int branchPoint = GetRandomValue(2, LIGHTNING_SEGMENTS - 2);
            Vector2 branchStart = {g_lightningPoints[branchPoint].x + g_shakeOffset.x,
                                  g_lightningPoints[branchPoint].y + g_shakeOffset.y};
            float branchAngle = GetRandomValue(-60, 60) * DEG2RAD;
            float branchLen = g_cellSize * (0.5f + GetRandomValue(0, 100) / 200.0f);
            Vector2 branchEnd = {
                branchStart.x + cosf(branchAngle + (g_lightningHorizontal ? 0 : PI/2)) * branchLen,
                branchStart.y + sinf(branchAngle + (g_lightningHorizontal ? 0 : PI/2)) * branchLen
            };
            DrawLineEx(branchStart, branchEnd, 2.0f, boltColor);
        }
    }

    /* Spawn particles along the lightning path */
    if (GetRandomValue(0, 100) < 40) {
        int idx = GetRandomValue(0, LIGHTNING_SEGMENTS);
        SpawnParticles(g_lightningPoints[idx].x, g_lightningPoints[idx].y,
                      (Color){200, 220, 255, 255}, 2);
    }
}

/* ============================================================================
 * GEM RENDERING - Faceted Jewel Style
 * ============================================================================ */

/* Draw a faceted jewel with cut gem appearance */
static void DrawGem(int gemType, float cx, float cy, float size, float scale, float alpha) {
    if (gemType == GEM_EMPTY || gemType < 0 || gemType > GEM_TYPE_COUNT) return;

    Color baseColor = GEM_COLORS_BASE[gemType];
    Color lightColor = GEM_COLORS_LIGHT[gemType];
    Color darkColor = GEM_COLORS_DARK[gemType];

    baseColor.a = (unsigned char)(baseColor.a * alpha);
    lightColor.a = (unsigned char)(lightColor.a * alpha);
    darkColor.a = (unsigned char)(darkColor.a * alpha);

    float gemSize = size * 0.40f * scale;
    cx += g_shakeOffset.x;
    cy += g_shakeOffset.y;

    /* Outer glow effect */
    Color glowColor = baseColor;
    glowColor.a = (unsigned char)(50 * alpha);
    DrawPoly((Vector2){cx, cy}, 8, gemSize * 1.25f, 22.5f, glowColor);

    /* Shadow offset for 3D depth */
    DrawPoly((Vector2){cx + 2, cy + 2}, 8, gemSize, 22.5f, darkColor);

    /* Main gem body - octagonal cut */
    DrawPoly((Vector2){cx, cy}, 8, gemSize, 22.5f, baseColor);

    /* Define facet vertices for a brilliant cut appearance */
    float innerRadius = gemSize * 0.6f;
    float tableRadius = gemSize * 0.35f;

    /* Draw bottom facets (pavilion) - darker */
    for (int i = 0; i < 8; i++) {
        float angle1 = (i * 45.0f + 22.5f) * DEG2RAD;
        float angle2 = ((i + 1) * 45.0f + 22.5f) * DEG2RAD;

        Vector2 outer1 = {cx + cosf(angle1) * gemSize, cy + sinf(angle1) * gemSize};
        Vector2 outer2 = {cx + cosf(angle2) * gemSize, cy + sinf(angle2) * gemSize};
        Vector2 center = {cx, cy + gemSize * 0.15f}; /* Culet slightly below center */

        /* Bottom facets - gradient from dark to base */
        Color facetColor = (i < 4) ? darkColor : LerpColor(darkColor, baseColor, 0.3f);
        DrawTriangle(outer1, center, outer2, facetColor);
    }

    /* Draw crown facets (top facets) - lighter, creates the sparkle */
    for (int i = 0; i < 8; i++) {
        float angle1 = (i * 45.0f + 22.5f) * DEG2RAD;
        float angle2 = ((i + 1) * 45.0f + 22.5f) * DEG2RAD;
        float midAngle = ((i + 0.5f) * 45.0f + 22.5f) * DEG2RAD;

        Vector2 outer1 = {cx + cosf(angle1) * gemSize, cy + sinf(angle1) * gemSize};
        Vector2 outer2 = {cx + cosf(angle2) * gemSize, cy + sinf(angle2) * gemSize};
        Vector2 inner1 = {cx + cosf(angle1) * innerRadius, cy + sinf(angle1) * innerRadius};
        Vector2 inner2 = {cx + cosf(angle2) * innerRadius, cy + sinf(angle2) * innerRadius};
        Vector2 midOuter = {cx + cosf(midAngle) * gemSize * 0.95f, cy + sinf(midAngle) * gemSize * 0.95f};

        /* Star facets - alternate light/medium */
        Color starColor = (i % 2 == 0) ? LerpColor(baseColor, lightColor, 0.6f) : LerpColor(baseColor, lightColor, 0.3f);
        /* Upper-left facets are brighter (light source) */
        if (i >= 5 || i <= 1) {
            starColor = LerpColor(starColor, lightColor, 0.4f);
        }

        DrawTriangle(outer1, inner1, midOuter, starColor);
        DrawTriangle(midOuter, inner2, outer2, starColor);

        /* Bezel facets connecting to table */
        Color bezelColor = LerpColor(baseColor, lightColor, (i >= 5 || i <= 1) ? 0.5f : 0.2f);
        DrawTriangle(inner1, (Vector2){cx + cosf(angle1) * tableRadius, cy + sinf(angle1) * tableRadius}, inner2, bezelColor);
    }

    /* Table facet (flat top of the gem) */
    Color tableColor = LerpColor(baseColor, lightColor, 0.7f);
    tableColor.a = (unsigned char)(tableColor.a * alpha);
    DrawPoly((Vector2){cx - gemSize * 0.05f, cy - gemSize * 0.05f}, 8, tableRadius, 22.5f, tableColor);

    /* Bright highlight on table (light reflection) */
    Color highlightColor = WHITE;
    highlightColor.a = (unsigned char)(200 * alpha);
    DrawPoly((Vector2){cx - gemSize * 0.12f, cy - gemSize * 0.12f}, 6, tableRadius * 0.4f, 0, highlightColor);

    /* Small sparkle point */
    highlightColor.a = (unsigned char)(255 * alpha);
    DrawCircle((int)(cx - gemSize * 0.2f), (int)(cy - gemSize * 0.2f), gemSize * 0.08f, highlightColor);

    /* Animated shimmer effect - light that dances across facets */
    float shimmerPhase = fmodf(g_shimmerTime * 0.8f + gemType * 0.9f, 2.0f * PI);
    float shimmerIntensity = (sinf(shimmerPhase) + 1.0f) * 0.5f;
    if (shimmerIntensity > 0.6f) {
        float shimmerAngle = shimmerPhase * 2.0f;
        float shimmerX = cx + gemSize * 0.25f * cosf(shimmerAngle);
        float shimmerY = cy + gemSize * 0.15f * sinf(shimmerAngle);
        Color shimmer = WHITE;
        shimmer.a = (unsigned char)((shimmerIntensity - 0.6f) * 2.5f * 255 * alpha);
        DrawPoly((Vector2){shimmerX, shimmerY}, 4, gemSize * 0.12f, 45.0f, shimmer);
    }

    /* Edge highlight for extra sparkle */
    Color edgeHighlight = WHITE;
    edgeHighlight.a = (unsigned char)(80 * alpha);
    for (int i = 5; i <= 7; i++) { /* Top-left edges */
        float angle = (i * 45.0f + 22.5f) * DEG2RAD;
        float nextAngle = ((i + 1) * 45.0f + 22.5f) * DEG2RAD;
        Vector2 p1 = {cx + cosf(angle) * gemSize * 0.98f, cy + sinf(angle) * gemSize * 0.98f};
        Vector2 p2 = {cx + cosf(nextAngle) * gemSize * 0.98f, cy + sinf(nextAngle) * gemSize * 0.98f};
        DrawLineEx(p1, p2, 2.0f, edgeHighlight);
    }
}

static void DrawGemSelection(int gx, int gy) {
    float sx, sy;
    GridToScreen(gx, gy, &sx, &sy);

    sx += g_shakeOffset.x;
    sy += g_shakeOffset.y;

    float pulse = 1.0f + sinf(g_animTimer * 8.0f) * 0.1f;
    float selSize = g_cellSize * 0.5f * pulse;

    /* Animated selection ring */
    DrawRing((Vector2){sx, sy}, selSize - 3, selSize, 0, 360, 36, COLOR_HIGHLIGHT);

    /* Glow effect */
    Color glow = COLOR_HIGHLIGHT;
    glow.a = 60;
    DrawCircle((int)sx, (int)sy, selSize * 1.3f, glow);
}

static void DrawHintHighlight(int x1, int y1, int x2, int y2) {
    float pulse = (sinf(g_animTimer * 6.0f) + 1.0f) * 0.5f;

    for (int i = 0; i < 2; i++) {
        int gx = (i == 0) ? x1 : x2;
        int gy = (i == 0) ? y1 : y2;

        float sx, sy;
        GridToScreen(gx, gy, &sx, &sy);
        sx += g_shakeOffset.x;
        sy += g_shakeOffset.y;

        Color hintColor = (Color){100, 255, 100, (unsigned char)(100 + 100 * pulse)};
        float hintSize = g_cellSize * 0.45f;

        DrawRing((Vector2){sx, sy}, hintSize - 2, hintSize, 0, 360, 36, hintColor);
    }
}

/* ============================================================================
 * ANIMATED BACKGROUND SYSTEM
 * ============================================================================ */

static void InitBackgroundStars(void) {
    for (int i = 0; i < MAX_BG_STARS; i++) {
        g_bgStars[i].pos = (Vector2){
            (float)GetRandomValue(0, g_screenWidth),
            (float)GetRandomValue(0, g_screenHeight)
        };
        g_bgStars[i].depth = 0.3f + (float)GetRandomValue(0, 100) / 100.0f * 0.7f;
        g_bgStars[i].brightness = 0.3f + (float)GetRandomValue(0, 100) / 100.0f * 0.7f;
        g_bgStars[i].twinklePhase = (float)GetRandomValue(0, 628) / 100.0f;
        g_bgStars[i].size = 1.0f + (float)GetRandomValue(0, 100) / 100.0f * 1.5f;
    }
}

static void DrawAnimatedBackground(void) {
    /* Simple gradient - fewer iterations for performance */
    float pulse = g_bgPulseIntensity * sinf(g_animTimer * 2.0f);
    Color topColor = {
        (unsigned char)(15 + pulse * 15),
        (unsigned char)(15 + pulse * 8),
        (unsigned char)(25 + pulse * 20),
        255
    };
    Color bottomColor = {
        (unsigned char)(25 + pulse * 10),
        (unsigned char)(20 + pulse * 5),
        (unsigned char)(40 + pulse * 15),
        255
    };

    /* Draw gradient with just 4 bands instead of many lines */
    for (int i = 0; i < 4; i++) {
        float t = (float)i / 4.0f;
        Color bandColor = LerpColor(topColor, bottomColor, t);
        DrawRectangle(0, i * g_screenHeight / 4, g_screenWidth, g_screenHeight / 4 + 1, bandColor);
    }

    /* Simplified grid - larger spacing, fewer lines */
    Color gridColor = {40, 45, 60, (unsigned char)(30 + g_bgPulseIntensity * 20)};
    int gridSize = 80;
    float gridOffset = fmodf(g_bgGridOffset, (float)gridSize);

    for (int x = (int)(-gridOffset); x < g_screenWidth + gridSize; x += gridSize) {
        DrawLineV((Vector2){(float)x, 0}, (Vector2){(float)x, (float)g_screenHeight}, gridColor);
    }
    for (int y = 0; y < g_screenHeight; y += gridSize) {
        DrawLineV((Vector2){0, (float)y}, (Vector2){(float)g_screenWidth, (float)y}, gridColor);
    }

    /* Simple twinkling stars - no glow layer */
    for (int i = 0; i < MAX_BG_STARS; i++) {
        BackgroundStar *star = &g_bgStars[i];
        float driftX = fmodf(star->pos.x + g_bgGridOffset * (1.0f - star->depth) * 0.1f, (float)g_screenWidth);
        float twinkle = (sinf(g_animTimer * 2.5f + star->twinklePhase) + 1.0f) * 0.5f;
        float finalBrightness = star->brightness * (0.5f + twinkle * 0.5f);

        Color starCore = {255, 255, 255, (unsigned char)(200 * finalBrightness)};
        DrawCircle((int)driftX, (int)star->pos.y, star->size, starCore);
    }

    /* Cascade flash overlay */
    if (g_cascadeFlashTimer > 0) {
        Color flashColor = g_cascadeFlashColor;
        flashColor.a = (unsigned char)(g_cascadeFlashTimer * 50);
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight, flashColor);
    }
}

static void TriggerCascadeFlash(int cascadeLevel) {
    g_cascadeFlashTimer = 0.4f;
    if (cascadeLevel >= 5) {
        g_cascadeFlashColor = (Color){255, 100, 50, 255};  /* Hot orange */
    } else if (cascadeLevel >= 3) {
        g_cascadeFlashColor = (Color){50, 255, 100, 255};  /* Lime green */
    } else {
        g_cascadeFlashColor = (Color){100, 150, 255, 255}; /* Cool blue */
    }
    g_bgPulseIntensity = fminf(1.0f, g_bgPulseIntensity + 0.3f);
}

/* ============================================================================
 * COMBO ANNOUNCEMENT SYSTEM
 * ============================================================================ */

static void SpawnComboAnnouncement(int comboLevel) {
    if (comboLevel < 2) return;

    /* Find inactive slot */
    for (int i = 0; i < MAX_COMBO_ANNOUNCEMENTS; i++) {
        if (!g_comboAnnouncements[i].active) {
            ComboAnnouncement *ann = &g_comboAnnouncements[i];
            ann->active = true;
            ann->life = 0.0f;
            ann->maxLife = 1.5f + (comboLevel >= 5 ? 0.5f : 0.0f);

            /* Text based on combo level */
            int textIdx = (comboLevel > 7) ? 7 : comboLevel;
            snprintf(ann->text, sizeof(ann->text), "%s", COMBO_TEXTS[textIdx]);

            /* Position - center of board with slight randomness */
            ann->x = g_boardX + g_boardSize / 2.0f + (float)GetRandomValue(-30, 30);
            ann->y = g_boardY + g_boardSize / 2.0f - 50;

            /* Visual properties based on combo level */
            ann->scale = 1.0f + (comboLevel - 2) * 0.15f;
            ann->rotation = (float)GetRandomValue(-10, 10);
            ann->color = COMBO_COLORS[textIdx];

            /* Rainbow effect for max combo */
            if (comboLevel >= 7) {
                ann->color = (Color){255, 255, 255, 255};
            }
            break;
        }
    }
}

static void UpdateComboAnnouncements(float dt) {
    for (int i = 0; i < MAX_COMBO_ANNOUNCEMENTS; i++) {
        ComboAnnouncement *ann = &g_comboAnnouncements[i];
        if (!ann->active) continue;

        ann->life += dt;
        if (ann->life >= ann->maxLife) {
            ann->active = false;
        } else {
            /* Float upward */
            ann->y -= dt * 30.0f;
            /* Wobble rotation */
            ann->rotation = sinf(ann->life * 8.0f) * 5.0f;
        }
    }
}

static void DrawComboAnnouncements(void) {
    for (int i = 0; i < MAX_COMBO_ANNOUNCEMENTS; i++) {
        ComboAnnouncement *ann = &g_comboAnnouncements[i];
        if (!ann->active) continue;

        float progress = ann->life / ann->maxLife;

        /* Scale animation: bounce in, then shrink out */
        float scaleAnim;
        if (progress < 0.2f) {
            scaleAnim = EaseOutBack(progress / 0.2f) * ann->scale;
        } else if (progress > 0.7f) {
            scaleAnim = ann->scale * (1.0f - (progress - 0.7f) / 0.3f);
        } else {
            scaleAnim = ann->scale;
        }

        /* Alpha: fade in fast, hold, fade out */
        float alpha;
        if (progress < 0.1f) {
            alpha = progress / 0.1f;
        } else if (progress > 0.8f) {
            alpha = 1.0f - (progress - 0.8f) / 0.2f;
        } else {
            alpha = 1.0f;
        }

        int fontSize = (int)(36 * scaleAnim);
        if (fontSize < 8) continue;

        Vector2 textSize = MeasureTextEx(g_font, ann->text, (float)fontSize, 1);
        float tx = ann->x - textSize.x / 2.0f;
        float ty = ann->y - textSize.y / 2.0f;

        /* Rainbow effect for high combos */
        Color textColor = ann->color;
        if (ann->color.r == 255 && ann->color.g == 255 && ann->color.b == 255) {
            float hue = fmodf(g_animTimer * 150.0f + i * 60, 360.0f);
            textColor = ColorFromHSV(hue, 0.8f, 1.0f);
        }
        textColor.a = (unsigned char)(255 * alpha);

        /* Simple shadow - just one draw call */
        Color shadowColor = {0, 0, 0, (unsigned char)(150 * alpha)};
        DrawTextEx(g_font, ann->text, (Vector2){tx + 2, ty + 2}, (float)fontSize, 1, shadowColor);

        /* Main text */
        DrawTextEx(g_font, ann->text, (Vector2){tx, ty}, (float)fontSize, 1, textColor);
    }
}

/* ============================================================================
 * LEVEL UP CELEBRATION
 * ============================================================================ */

static void TriggerLevelUpCelebration(int level) {
    g_levelUpActive = true;
    g_levelUpTimer = 0.0f;
    g_levelUpLevel = level;
    g_screenFlashTimer = 0.4f;
    g_screenFlashColor = (Color){100, 200, 255, 255};

    /* Spawn celebration particles - reduced for performance */
    for (int i = 0; i < 16; i++) {
        float angle = (float)i / 16.0f * PI * 2.0f;
        float speed = 180.0f + (float)GetRandomValue(0, 100);
        float cx = g_boardX + g_boardSize / 2.0f;
        float cy = g_boardY + g_boardSize / 2.0f;

        /* Find a free particle slot */
        for (int p = 0; p < MAX_PARTICLES; p++) {
            if (g_particles[p].life <= 0) {
                g_particles[p].pos = (Vector2){cx, cy};
                g_particles[p].vel = (Vector2){cosf(angle) * speed, sinf(angle) * speed - 100};
                g_particles[p].color = ColorFromHSV((float)i * 9.0f, 0.8f, 1.0f);
                g_particles[p].life = 1.5f;
                g_particles[p].maxLife = 1.5f;
                g_particles[p].size = 6.0f + (float)GetRandomValue(0, 8);
                g_particles[p].rotation = (float)GetRandomValue(0, 360);
                g_particles[p].rotSpeed = (float)GetRandomValue(-200, 200);
                g_particleCount++;
                break;
            }
        }
    }
}

static void UpdateLevelUpCelebration(float dt) {
    if (!g_levelUpActive) return;

    g_levelUpTimer += dt;
    if (g_levelUpTimer >= 2.0f) {
        g_levelUpActive = false;
    }
}

static void DrawLevelUpCelebration(void) {
    if (!g_levelUpActive) return;

    float progress = g_levelUpTimer / 2.0f;  /* Shorter duration */

    /* Scale animation */
    float scale;
    if (progress < 0.15f) {
        scale = EaseOutBack(progress / 0.15f);
    } else if (progress > 0.75f) {
        scale = 1.0f - (progress - 0.75f) / 0.25f;
    } else {
        scale = 1.0f;
    }

    /* Alpha */
    float alpha;
    if (progress < 0.1f) {
        alpha = progress / 0.1f;
    } else if (progress > 0.8f) {
        alpha = 1.0f - (progress - 0.8f) / 0.2f;
    } else {
        alpha = 1.0f;
    }

    /* Center of screen */
    float cx = g_screenWidth / 2.0f;
    float cy = g_screenHeight / 2.0f - 30;

    /* Simple background glow - just one circle */
    Color glowColor = {100, 200, 255, (unsigned char)(60 * alpha)};
    DrawCircle((int)cx, (int)cy, 120 * scale, glowColor);

    /* Level up text */
    char levelText[32];
    snprintf(levelText, sizeof(levelText), "LEVEL %d!", g_levelUpLevel);

    int fontSize = (int)(44 * scale);
    Vector2 textSize = MeasureTextEx(g_font, levelText, (float)fontSize, 1);
    float tx = cx - textSize.x / 2.0f;
    float ty = cy - textSize.y / 2.0f;

    /* Simple gold color with subtle pulse */
    float pulse = sinf(g_animTimer * 6.0f) * 0.15f + 0.85f;
    Color textColor = {255, (unsigned char)(200 * pulse), 50, (unsigned char)(255 * alpha)};

    /* Shadow */
    DrawTextEx(g_font, levelText, (Vector2){tx + 2, ty + 2}, (float)fontSize, 1, (Color){0, 0, 0, (unsigned char)(180 * alpha)});

    /* Main text */
    DrawTextEx(g_font, levelText, (Vector2){tx, ty}, (float)fontSize, 1, textColor);
}

/* ============================================================================
 * BOARD RENDERING
 * ============================================================================ */

static void DrawBoard(void) {
    float bx = g_boardX + g_shakeOffset.x;
    float by = g_boardY + g_shakeOffset.y;

    /* Board background with rounded corners */
    Rectangle boardRect = {bx - 8, by - 8, g_boardSize + 16, g_boardSize + 16};
    DrawRectangleRounded(boardRect, 0.03f, 16, COLOR_BOARD_BG);

    /* Draw empty cell backgrounds - octagonal gem sockets */
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            float cx = bx + x * g_cellSize + g_cellSize / 2.0f;
            float cy = by + y * g_cellSize + g_cellSize / 2.0f;

            /* Cell background - octagonal socket shape */
            DrawPoly((Vector2){cx, cy}, 8, g_cellSize * 0.44f, 22.5f, COLOR_CELL_EMPTY);
            /* Inner shadow for depth */
            Color innerShadow = {20, 22, 32, 255};
            DrawPoly((Vector2){cx + 1, cy + 1}, 8, g_cellSize * 0.38f, 22.5f, innerShadow);
        }
    }

    /* Draw gems with animation */
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            int gemType = GetBoardGem(x, y);
            if (gemType == GEM_EMPTY) continue;

            GemAnimation *anim = GetGemAnimation(x, y);

            float cx, cy;
            GridToScreen(x, y, &cx, &cy);

            /* Apply animation offsets */
            if (anim) {
                cx += anim->offsetX * g_cellSize;
                cy += anim->offsetY * g_cellSize;
            }

            float scale = 1.0f;
            float alpha = 1.0f;

            if (anim) {
                if (anim->isRemoving) {
                    scale = anim->scale;
                    alpha = anim->scale;
                } else if (anim->isSpawning) {
                    scale = anim->scale;
                    alpha = anim->scale * 0.8f + 0.2f;
                }
            }

            DrawGem(gemType, cx, cy, g_cellSize, scale, alpha);
        }
    }

    /* Draw selection */
    Position sel = GetSelectedGem();
    if (sel.x >= 0 && sel.y >= 0) {
        DrawGemSelection(sel.x, sel.y);
    }

    /* Draw hint if active */
    if (g_showHint && g_hintX1 >= 0) {
        DrawHintHighlight(g_hintX1, g_hintY1, g_hintX2, g_hintY2);
    }

    /* Draw cursor in idle state */
    if (GetGameState() == GAME_STATE_IDLE && !HasSelection()) {
        float cx, cy;
        GridToScreen(g_cursorX, g_cursorY, &cx, &cy);
        cx += g_shakeOffset.x;
        cy += g_shakeOffset.y;

        Color cursorColor = COLOR_TEXT;
        cursorColor.a = 100;
        float cursorSize = g_cellSize * 0.48f;
        DrawRectangleLinesEx(
            (Rectangle){cx - cursorSize, cy - cursorSize, cursorSize * 2, cursorSize * 2},
            2, cursorColor);
    }
}

/* ============================================================================
 * HUD RENDERING
 * ============================================================================ */

static void DrawHUD(void) {
    /* Level overlay - top left */
    Rectangle levelPanel = {12, 12, 100, 44};
    DrawRectangleRounded(levelPanel, 0.2f, 8, COLOR_BOARD_BG);
    DrawRectangleRoundedLines(levelPanel, 0.2f, 8, (Color){60, 65, 80, 255});

    DrawTextEx(g_font, "LVL", (Vector2){levelPanel.x + 10, levelPanel.y + 6}, 14, 1, COLOR_TEXT_MUTED);
    char levelText[32];
    snprintf(levelText, sizeof(levelText), "%d", GetLevel());
    DrawTextEx(g_font, levelText, (Vector2){levelPanel.x + 50, levelPanel.y + 10}, 26, 1, COLOR_TEXT);

    /* Level progress bar */
    int currentLevelScore, nextLevelScore;
    GetLevelProgress(&currentLevelScore, &nextLevelScore);
    int scoreInLevel = GetScore() - currentLevelScore;
    int levelRange = nextLevelScore - currentLevelScore;
    float progress = (levelRange > 0) ? (float)scoreInLevel / levelRange : 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    if (progress < 0.0f) progress = 0.0f;

    float barX = levelPanel.x + 8;
    float barY = levelPanel.y + 36;
    float barWidth = levelPanel.width - 16;
    float barHeight = 4;

    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, (Color){30, 35, 50, 255});
    Color progressColor = LerpColor((Color){60, 120, 230, 255}, (Color){100, 255, 150, 255}, progress);
    DrawRectangle((int)barX, (int)barY, (int)(barWidth * progress), (int)barHeight, progressColor);

    /* Score overlay - top right */
    Rectangle scorePanel = {g_screenWidth - 112, 12, 100, 44};

    /* Panel glow when score is animating */
    if (g_scorePulse > 0.1f) {
        Color panelGlow = {255, 215, 0, (unsigned char)(40 * g_scorePulse)};
        DrawRectangleRounded((Rectangle){scorePanel.x - 3, scorePanel.y - 3,
                            scorePanel.width + 6, scorePanel.height + 6}, 0.2f, 8, panelGlow);
    }
    DrawRectangleRounded(scorePanel, 0.2f, 8, COLOR_BOARD_BG);
    DrawRectangleRoundedLines(scorePanel, 0.2f, 8, (Color){60, 65, 80, 255});

    /* Score with animated counter */
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "%d", g_displayScore);

    /* Pulse effect on score change */
    float scoreFontSize = 22 + g_scorePulse * 3;
    Color scoreColor = LerpColor(COLOR_TEXT, COLOR_HIGHLIGHT, g_scorePulse);

    /* Right-align score text */
    Vector2 scoreSize = MeasureTextEx(g_font, scoreText, scoreFontSize, 1);
    float scoreX = scorePanel.x + scorePanel.width - scoreSize.x - 10;
    DrawTextEx(g_font, scoreText, (Vector2){scoreX, scorePanel.y + 12}, scoreFontSize, 1, scoreColor);

    /* Cascade/Combo indicator - only show on 2nd+ match in chain */
    int cascade = GetCascadeLevel();
    if (cascade > 1) {
        /* cascade=2 means first cascade (x2), cascade=3 means second cascade (x3), etc. */
        Color cascadeColor = LerpColor(COLOR_HIGHLIGHT, (Color){255, 100, 50, 255}, (float)(cascade - 1) / 5.0f);
        char cascadeText[32];
        snprintf(cascadeText, sizeof(cascadeText), "x%d COMBO!", cascade);

        float pulse = 1.0f + sinf(g_animTimer * 10.0f) * 0.1f;
        int fontSize = (int)(24 * pulse);

        Vector2 textSize = MeasureTextEx(g_font, cascadeText, fontSize, 1);
        float cx = g_boardX + g_boardSize / 2.0f - textSize.x / 2.0f;
        float cy = g_boardY - 30;

        DrawTextEx(g_font, cascadeText, (Vector2){cx + 2, cy + 2}, fontSize, 1, (Color){0, 0, 0, 180});
        DrawTextEx(g_font, cascadeText, (Vector2){cx, cy}, fontSize, 1, cascadeColor);
    }

    /* Game over overlay - enhanced with animations */
    if (GetGameState() == GAME_STATE_GAME_OVER) {
        float bx = g_boardX + g_shakeOffset.x;
        float by = g_boardY + g_shakeOffset.y;
        float centerX = bx + g_boardSize / 2.0f;
        float centerY = by + g_boardSize / 2.0f;

        /* Animated gradient overlay */
        for (int i = 0; i < 8; i++) {
            float alpha = 180 - i * 15;
            float offset = i * 10.0f * sinf(g_animTimer * 0.5f);
            DrawRectangle((int)(bx + offset / 2), (int)(by + offset / 2),
                         (int)(g_boardSize - offset), (int)(g_boardSize - offset),
                         (Color){0, 0, 0, (unsigned char)alpha});
        }

        /* Pulsing border glow */
        float glowPulse = (sinf(g_animTimer * 3.0f) + 1.0f) * 0.5f;
        Color borderGlow = {255, 100, 100, (unsigned char)(60 + 40 * glowPulse)};
        DrawRectangleLinesEx((Rectangle){bx - 2, by - 2, g_boardSize + 4, g_boardSize + 4}, 3, borderGlow);

        /* GAME OVER with glow effect */
        const char *gameOverText = "GAME OVER";
        float pulse = 1.0f + sinf(g_animTimer * 4.0f) * 0.05f;
        int fontSize = (int)(52 * pulse);
        Vector2 textSize = MeasureTextEx(g_font, gameOverText, (float)fontSize, 1);
        float tx = centerX - textSize.x / 2.0f;
        float ty = centerY - 70;

        /* Text glow layers */
        Color glowColor = {255, 80, 80, 60};
        for (int g = 5; g >= 1; g--) {
            DrawTextEx(g_font, gameOverText, (Vector2){tx - g, ty}, (float)fontSize, 1, glowColor);
            DrawTextEx(g_font, gameOverText, (Vector2){tx + g, ty}, (float)fontSize, 1, glowColor);
            DrawTextEx(g_font, gameOverText, (Vector2){tx, ty - g}, (float)fontSize, 1, glowColor);
            DrawTextEx(g_font, gameOverText, (Vector2){tx, ty + g}, (float)fontSize, 1, glowColor);
        }

        /* Shadow */
        DrawTextEx(g_font, gameOverText, (Vector2){tx + 3, ty + 3}, (float)fontSize, 1, (Color){0, 0, 0, 200});

        /* Main text with gradient color */
        Color textColor = LerpColor((Color){255, 100, 100, 255}, (Color){255, 200, 100, 255}, glowPulse);
        DrawTextEx(g_font, gameOverText, (Vector2){tx, ty}, (float)fontSize, 1, textColor);

        /* Stats panel */
        float panelY = ty + 70;
        Rectangle statsPanel = {centerX - 100, panelY, 200, 110};
        DrawRectangleRounded(statsPanel, 0.1f, 12, (Color){20, 25, 40, 230});
        DrawRectangleRoundedLines(statsPanel, 0.1f, 12, (Color){80, 85, 100, 255});

        /* Final Score */
        const char *scoreLabel = "FINAL SCORE";
        Vector2 scoreLabelSize = MeasureTextEx(g_font, scoreLabel, 16, 1);
        DrawTextEx(g_font, scoreLabel, (Vector2){centerX - scoreLabelSize.x / 2, panelY + 12}, 16, 1, COLOR_TEXT_MUTED);

        char finalScore[32];
        snprintf(finalScore, sizeof(finalScore), "%d", GetScore());
        float scorePulse = (sinf(g_animTimer * 5.0f) + 1.0f) * 0.5f;
        int scoreFontSize = (int)(38 + scorePulse * 4);
        Vector2 finalScoreSize = MeasureTextEx(g_font, finalScore, (float)scoreFontSize, 1);

        /* Score with rainbow shimmer for high scores */
        Color scoreColor;
        if (GetScore() > 5000) {
            scoreColor = ColorFromHSV(fmodf(g_animTimer * 60, 360), 0.7f, 1.0f);
        } else {
            scoreColor = COLOR_HIGHLIGHT;
        }
        DrawTextEx(g_font, finalScore, (Vector2){centerX - finalScoreSize.x / 2, panelY + 32}, (float)scoreFontSize, 1, scoreColor);

        /* Level reached */
        char levelText[32];
        snprintf(levelText, sizeof(levelText), "LEVEL %d", GetLevel());
        Vector2 levelSize = MeasureTextEx(g_font, levelText, 20, 1);
        DrawTextEx(g_font, levelText, (Vector2){centerX - levelSize.x / 2, panelY + 78}, 20, 1, COLOR_TEXT);

        /* Restart instruction with pulsing */
        const char *restartText = "PRESS SELECT TO PLAY AGAIN";
        float restartAlpha = 150 + 105 * sinf(g_animTimer * 2.5f);
        Vector2 restartSize = MeasureTextEx(g_font, restartText, 16, 1);
        DrawTextEx(g_font, restartText, (Vector2){centerX - restartSize.x / 2, panelY + 130},
                  16, 1, (Color){240, 240, 250, (unsigned char)restartAlpha});
    }
}

/* ============================================================================
 * ANIMATION UPDATE
 * ============================================================================ */

static void UpdateAnimations(float deltaTime) {
    g_animTimer += deltaTime;
    g_shimmerTime += deltaTime;

    /* Update background animation */
    g_bgGridOffset += deltaTime * 15.0f;
    g_bgPulseIntensity *= (1.0f - deltaTime * 2.0f);  /* Decay pulse */
    if (g_bgPulseIntensity < 0.01f) g_bgPulseIntensity = 0;

    /* Update cascade flash */
    if (g_cascadeFlashTimer > 0) {
        g_cascadeFlashTimer -= deltaTime;
    }

    /* Update screen flash */
    if (g_screenFlashTimer > 0) {
        g_screenFlashTimer -= deltaTime;
    }

    /* Update combo announcements */
    UpdateComboAnnouncements(deltaTime);

    /* Update level up celebration */
    UpdateLevelUpCelebration(deltaTime);

    /* Animated score counter - smoothly count toward actual score */
    int actualScore = GetScore();
    if (g_displayScore < actualScore) {
        int diff = actualScore - g_displayScore;
        int increment = (diff > 100) ? diff / 10 : (diff > 10) ? 5 : 1;
        g_displayScore += increment;
        if (g_displayScore > actualScore) g_displayScore = actualScore;
        g_scorePulse = 1.0f;
    }
    g_scorePulse *= (1.0f - deltaTime * 4.0f);

    /* Check for level up */
    int currentLevel = GetLevel();
    if (currentLevel > g_previousLevel) {
        TriggerLevelUpCelebration(currentLevel);
        g_previousLevel = currentLevel;
    }

    /* Check for cascade changes */
    int currentCascade = GetCascadeLevel();
    if (currentCascade > g_lastCascadeLevel && currentCascade >= 2) {
        SpawnComboAnnouncement(currentCascade);
        TriggerCascadeFlash(currentCascade);
    }
    g_lastCascadeLevel = currentCascade;

    bool anyAnimating = false;

    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            GemAnimation *anim = GetGemAnimation(x, y);
            if (!anim) continue;

            /* Update swap/fall animations */
            if (anim->offsetX != 0 || anim->offsetY != 0) {
                float speed = (anim->fallDistance > 0) ? ANIM_FALL_SPEED : ANIM_SWAP_SPEED;
                anim->offsetX = Lerp(anim->offsetX, 0, speed * deltaTime);
                anim->offsetY = Lerp(anim->offsetY, 0, speed * deltaTime);

                if (fabsf(anim->offsetX) < 0.01f) anim->offsetX = 0;
                if (fabsf(anim->offsetY) < 0.01f) anim->offsetY = 0;

                if (anim->offsetX != 0 || anim->offsetY != 0) anyAnimating = true;
            }

            /* Update removal animation */
            if (anim->isRemoving) {
                anim->scale -= ANIM_REMOVE_SPEED * deltaTime;
                if (anim->scale <= 0) {
                    anim->scale = 0;
                    anim->isRemoving = false;
                } else {
                    anyAnimating = true;
                }
            }

            /* Update spawn animation */
            if (anim->isSpawning) {
                anim->scale += ANIM_SPAWN_SPEED * deltaTime;
                if (anim->scale >= 1.0f) {
                    anim->scale = 1.0f;
                    anim->isSpawning = false;
                } else {
                    anyAnimating = true;
                }
            }
        }
    }

    /* State machine for game flow */
    GameState state = GetGameState();

    if (!anyAnimating && state != GAME_STATE_IDLE && state != GAME_STATE_GAME_OVER) {
        g_stateTimer += deltaTime;

        if (g_stateTimer > 0.05f) {
            g_stateTimer = 0;

            switch (state) {
                case GAME_STATE_SWAPPING:
                    SetGameState(GAME_STATE_CHECKING);
                    break;

                case GAME_STATE_CHECKING: {
                    int matches = CheckMatches();
                    if (matches > 0) {
                        SetGameState(GAME_STATE_REMOVING);

                        /* Check for 5+ match lightning strike */
                        LightningInfo lightning = GetLightningInfo();
                        if (lightning.active) {
                            /* Trigger lightning visual effect */
                            TriggerLightning(lightning.isHorizontal,
                                           lightning.isHorizontal ? lightning.row : lightning.col,
                                           lightning.centerX, lightning.centerY);

                            /* Execute lightning strike - clears row/column */
                            int lightningGems = ExecuteLightningStrike(lightning.isHorizontal,
                                                lightning.isHorizontal ? lightning.row : lightning.col);

                            /* Spawn lightning score popup */
                            float lx, ly;
                            GridToScreen(lightning.centerX, lightning.centerY, &lx, &ly);
                            int cascadeMult = GetCascadeLevel();
                            if (cascadeMult < 1) cascadeMult = 1;
                            int lightningScore = lightningGems * SCORE_MATCH_3 * cascadeMult;
                            SpawnScorePopup(lx, ly - 30, lightningScore, (Color){150, 200, 255, 255});

                            ClearLightningInfo();
                        }

                        /* Spawn particles for matched gems and score popup */
                        int scoreEarned = RemoveMatches();

                        /* Find center of matches for popup */
                        float avgX = 0, avgY = 0;
                        int count = 0;
                        for (int gy = 0; gy < BOARD_HEIGHT; gy++) {
                            for (int gx = 0; gx < BOARD_WIDTH; gx++) {
                                GemAnimation *a = GetGemAnimation(gx, gy);
                                if (a && a->isRemoving) {
                                    float sx, sy;
                                    GridToScreen(gx, gy, &sx, &sy);
                                    avgX += sx;
                                    avgY += sy;
                                    count++;
                                    SpawnMatchParticles(gx, gy, GetBoardGem(gx, gy) ? GetBoardGem(gx, gy) : 1);
                                }
                            }
                        }

                        if (count > 0) {
                            avgX /= count;
                            avgY /= count;
                            SpawnScorePopup(avgX, avgY, scoreEarned, COLOR_HIGHLIGHT);

                            /* Shake based on match size */
                            TriggerShake(3.0f + count * 1.5f);
                        }

                        IncrementCascade();
                    } else {
                        /* No matches - check for game over or return to idle */
                        if (CheckGameOver()) {
                            SetGameState(GAME_STATE_GAME_OVER);
                            TriggerShake(15.0f);
                        } else {
                            SetGameState(GAME_STATE_IDLE);
                            ResetCascade();
                        }
                    }
                    break;
                }

                case GAME_STATE_REMOVING:
                    SetGameState(GAME_STATE_FALLING);
                    ApplyGravity();
                    break;

                case GAME_STATE_FALLING:
                    SetGameState(GAME_STATE_FILLING);
                    FillBoard();
                    break;

                case GAME_STATE_FILLING:
                    SetGameState(GAME_STATE_CHECKING);
                    break;

                default:
                    break;
            }
        }
    }
}

/* ============================================================================
 * INPUT HANDLING
 * ============================================================================ */

static void HandleInput(const LlzInputState *input) {
    if (!input) return;

    if (input->backReleased) {
        g_wantsClose = true;
        return;
    }

    GameState state = GetGameState();

    /* Handle game over - restart on select */
    if (state == GAME_STATE_GAME_OVER) {
        if (input->selectPressed) {
            InitGame();
            g_cursorX = BOARD_WIDTH / 2;
            g_cursorY = BOARD_HEIGHT / 2;
        }
        return;
    }

    /* Only handle input in idle state */
    if (state != GAME_STATE_IDLE) return;

    /* Reset hint timer on any input */
    g_hintTimer = 0;
    g_showHint = false;

    /* Button navigation - up/down buttons move columns, scroll wheel moves rows */
    if (input->upPressed) {
        g_cursorX = (g_cursorX + BOARD_WIDTH - 1) % BOARD_WIDTH;
    }
    if (input->downPressed) {
        g_cursorX = (g_cursorX + 1) % BOARD_WIDTH;
    }
    /* Scroll wheel moves cursor up/down through rows */
    if (input->scrollDelta > 0.5f) {
        g_cursorY = (g_cursorY + 1) % BOARD_HEIGHT;
    }
    if (input->scrollDelta < -0.5f) {
        g_cursorY = (g_cursorY + BOARD_HEIGHT - 1) % BOARD_HEIGHT;
    }

    /* Select button for gem selection/swap */
    if (input->selectPressed) {
        Position sel = GetSelectedGem();

        if (sel.x < 0) {
            /* Nothing selected - select cursor position */
            SetSelectedGem(g_cursorX, g_cursorY);
        } else if (sel.x == g_cursorX && sel.y == g_cursorY) {
            /* Same gem - deselect */
            ClearSelection();
        } else {
            /* Try to swap */
            int dx = abs(g_cursorX - sel.x);
            int dy = abs(g_cursorY - sel.y);

            if ((dx == 1 && dy == 0) || (dx == 0 && dy == 1)) {
                if (SwapGems(sel.x, sel.y, g_cursorX, g_cursorY)) {
                    SetGameState(GAME_STATE_SWAPPING);
                    ClearSelection();
                } else {
                    /* Invalid swap - visual feedback */
                    TriggerShake(3.0f);
                    ClearSelection();
                }
            } else {
                /* Not adjacent - select new gem */
                SetSelectedGem(g_cursorX, g_cursorY);
            }
        }
    }

    /* Touch input */
    if (input->tap) {
        int gx, gy;
        if (ScreenToGrid(input->tapPosition.x, input->tapPosition.y, &gx, &gy)) {
            Position sel = GetSelectedGem();

            if (sel.x < 0) {
                SetSelectedGem(gx, gy);
                g_cursorX = gx;
                g_cursorY = gy;
            } else if (sel.x == gx && sel.y == gy) {
                ClearSelection();
            } else {
                int dx = abs(gx - sel.x);
                int dy = abs(gy - sel.y);

                if ((dx == 1 && dy == 0) || (dx == 0 && dy == 1)) {
                    if (SwapGems(sel.x, sel.y, gx, gy)) {
                        SetGameState(GAME_STATE_SWAPPING);
                        ClearSelection();
                    } else {
                        TriggerShake(3.0f);
                        ClearSelection();
                    }
                } else {
                    SetSelectedGem(gx, gy);
                    g_cursorX = gx;
                    g_cursorY = gy;
                }
            }
        }
    }

    /* Swipe input for quick swap */
    if (HasSelection()) {
        Position sel = GetSelectedGem();
        int targetX = sel.x;
        int targetY = sel.y;

        if (input->swipeLeft && sel.x > 0) targetX = sel.x - 1;
        else if (input->swipeRight && sel.x < BOARD_WIDTH - 1) targetX = sel.x + 1;
        else if (input->swipeUp && sel.y > 0) targetY = sel.y - 1;
        else if (input->swipeDown && sel.y < BOARD_HEIGHT - 1) targetY = sel.y + 1;

        if (targetX != sel.x || targetY != sel.y) {
            if (SwapGems(sel.x, sel.y, targetX, targetY)) {
                SetGameState(GAME_STATE_SWAPPING);
            } else {
                TriggerShake(3.0f);
            }
            ClearSelection();
        }
    }
}

/* ============================================================================
 * HINT SYSTEM
 * ============================================================================ */

static void UpdateHintSystem(float deltaTime) {
    if (GetGameState() != GAME_STATE_IDLE) {
        g_hintTimer = 0;
        g_showHint = false;
        return;
    }

    g_hintTimer += deltaTime;

    /* Show hint after 5 seconds of inactivity */
    if (g_hintTimer > 5.0f && !g_showHint) {
        if (GetHint(&g_hintX1, &g_hintY1, &g_hintX2, &g_hintY2)) {
            g_showHint = true;
        }
    }
}

/* ============================================================================
 * NOTIFICATION CALLBACK
 * ============================================================================ */

static void OnNotificationTap(void *userData) {
    (void)userData;
    g_wantsClose = true;
}

static void OnTrackChanged(const char *track, const char *artist, const char *album, void *userData) {
    (void)album;
    (void)userData;

    char message[256];
    if (artist && artist[0] && track && track[0]) {
        snprintf(message, sizeof(message), "%s - %s", artist, track);
    } else if (track && track[0]) {
        snprintf(message, sizeof(message), "%s", track);
    } else {
        return;
    }

    LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_BANNER);
    strncpy(config.message, message, LLZ_NOTIFY_TEXT_MAX - 1);
    strncpy(config.iconText, "~", LLZ_NOTIFY_ICON_MAX - 1);
    config.duration = 4.0f;
    config.position = LLZ_NOTIFY_POS_TOP;
    strncpy(config.openPluginOnTap, "Now Playing", LLZ_NOTIFY_PLUGIN_NAME_MAX - 1);
    config.onTap = OnNotificationTap;

    LlzNotifyShow(&config);
}

/* ============================================================================
 * PLUGIN API
 * ============================================================================ */

static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;

    /* Load display font (Quincy Caps - all uppercase decorative font) */
    g_font = LlzFontGet(LLZ_FONT_DISPLAY, 48);
    if (g_font.texture.id == 0) {
        g_font = GetFontDefault();
    }

    /* Calculate layout */
    CalculateLayout();

    /* Initialize game */
    InitGame();

    /* Reset state */
    g_cursorX = BOARD_WIDTH / 2;
    g_cursorY = BOARD_HEIGHT / 2;
    g_animTimer = 0;
    g_stateTimer = 0;
    g_shimmerTime = 0;
    g_shakeIntensity = 0;
    g_shakeOffset = (Vector2){0, 0};
    g_particleCount = 0;
    g_popupCount = 0;
    g_hintTimer = 0;
    g_showHint = false;

    /* Initialize enhanced visual systems */
    InitBackgroundStars();
    g_bgPulseIntensity = 0;
    g_bgGridOffset = 0;
    g_cascadeFlashTimer = 0;
    g_lastCascadeLevel = 0;
    g_displayScore = 0;
    g_scorePulse = 0;
    g_previousLevel = 1;
    g_levelUpActive = false;
    g_screenFlashTimer = 0;

    /* Reset combo announcements */
    for (int i = 0; i < MAX_COMBO_ANNOUNCEMENTS; i++) {
        g_comboAnnouncements[i].active = false;
    }

    /* Initialize notifications */
    LlzNotifyInit(width, height);

    /* Subscribe to media changes */
    if (LlzMediaInit(NULL)) {
        g_mediaInitialized = true;
        g_trackSubId = LlzSubscribeTrackChanged(OnTrackChanged, NULL);
    }

    printf("[BEJEWELED] Plugin initialized (%dx%d)\n", width, height);
}

static void PluginUpdate(const LlzInputState *input, float deltaTime) {
    /* Poll media subscriptions */
    if (g_mediaInitialized) {
        LlzSubscriptionPoll();
    }

    /* Update notification system */
    bool notifyBlocking = LlzNotifyUpdate(input, deltaTime);

    if (notifyBlocking && LlzNotifyIsBlocking()) {
        return;
    }

    /* Update game systems */
    UpdateAnimations(deltaTime);
    UpdateParticles(deltaTime);
    UpdatePopups(deltaTime);
    UpdateShake(deltaTime);
    UpdateLightning(deltaTime);
    UpdateHintSystem(deltaTime);

    /* Handle input */
    HandleInput(input);
}

static void PluginDraw(void) {
    ClearBackground(COLOR_BG);

    /* Animated background layer */
    DrawAnimatedBackground();

    /* Game elements */
    DrawBoard();
    DrawLightning();
    DrawParticles();
    DrawPopups();

    /* Combo announcements over gameplay */
    DrawComboAnnouncements();

    /* Level up celebration overlay */
    DrawLevelUpCelebration();

    /* HUD on top */
    DrawHUD();

    /* Screen flash effect */
    if (g_screenFlashTimer > 0) {
        Color flashColor = g_screenFlashColor;
        flashColor.a = (unsigned char)(g_screenFlashTimer * 150);
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight, flashColor);
    }

    /* Draw notification overlay */
    LlzNotifyDraw();
}

static void PluginShutdown(void) {
    LlzNotifyShutdown();

    if (g_trackSubId != 0) {
        LlzUnsubscribe(g_trackSubId);
        g_trackSubId = 0;
    }

    if (g_mediaInitialized) {
        LlzMediaShutdown();
        g_mediaInitialized = false;
    }

    g_wantsClose = false;
    printf("[BEJEWELED] Plugin shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

static LlzPluginAPI g_api = {
    .name = "Bejeweled",
    .description = "Match-3 puzzle game with flashy effects",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .category = LLZ_CATEGORY_GAMES
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
