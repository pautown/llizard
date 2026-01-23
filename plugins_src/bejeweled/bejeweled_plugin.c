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

/* Mapping from bejeweled gem types to SDK gem colors */
static const LlzGemColor GEM_TO_SDK_COLOR[] = {
    LLZ_GEM_RUBY,       /* GEM_EMPTY (0) - fallback */
    LLZ_GEM_RUBY,       /* GEM_RED (1) */
    LLZ_GEM_AMBER,      /* GEM_ORANGE (2) */
    LLZ_GEM_TOPAZ,      /* GEM_YELLOW (3) */
    LLZ_GEM_EMERALD,    /* GEM_GREEN (4) */
    LLZ_GEM_SAPPHIRE,   /* GEM_BLUE (5) */
    LLZ_GEM_AMETHYST,   /* GEM_PURPLE (6) */
    LLZ_GEM_DIAMOND     /* GEM_WHITE (7) */
};

/* Mapping from bejeweled gem types to SDK shapes */
static const LlzShapeType GEM_TO_SDK_SHAPE[] = {
    LLZ_SHAPE_CIRCLE,       /* GEM_EMPTY (0) - fallback */
    LLZ_SHAPE_DIAMOND,      /* GEM_RED (1) - Ruby */
    LLZ_SHAPE_HEXAGON,      /* GEM_ORANGE (2) - Amber */
    LLZ_SHAPE_KITE,         /* GEM_YELLOW (3) - Topaz */
    LLZ_SHAPE_OCTAGON,      /* GEM_GREEN (4) - Emerald */
    LLZ_SHAPE_TALL_DIAMOND, /* GEM_BLUE (5) - Sapphire */
    LLZ_SHAPE_TRIANGLE,     /* GEM_PURPLE (6) - Amethyst */
    LLZ_SHAPE_CIRCLE        /* GEM_WHITE (7) - Diamond */
};

/* Helper to get gem color from bejeweled gem type */
static Color GetGemBaseColor(int gemType) {
    if (gemType <= 0 || gemType > GEM_TYPE_COUNT) return (Color){0, 0, 0, 0};
    return LlzGetGemColor(GEM_TO_SDK_COLOR[gemType]);
}

static Color GetGemLightColor(int gemType) {
    if (gemType <= 0 || gemType > GEM_TYPE_COUNT) return (Color){0, 0, 0, 0};
    return LlzGetGemColorLight(GEM_TO_SDK_COLOR[gemType]);
}

static Color GetGemDarkColor(int gemType) {
    if (gemType <= 0 || gemType > GEM_TYPE_COUNT) return (Color){0, 0, 0, 0};
    return LlzGetGemColorDark(GEM_TO_SDK_COLOR[gemType]);
}

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

/* Plugin screen states */
typedef enum {
    PLUGIN_STATE_TITLE_SCREEN,  /* Title/splash screen */
    PLUGIN_STATE_MODE_SELECT,   /* Mode selection menu */
    PLUGIN_STATE_PLAYING        /* Playing the game */
} PluginState;

static PluginState g_pluginState = PLUGIN_STATE_TITLE_SCREEN;
static int g_selectedMode = 0;  /* Currently highlighted mode in menu */

static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;
static Font g_font;
static Font g_accentFont;  /* Flange Bold - for combos, scores, powerups */

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

/* Carousel mode selector */
static float g_carouselPosition = 0.0f;      /* Current animated position */
static float g_carouselTarget = 0.0f;        /* Target position */
static bool g_carouselAnimating = false;
static float g_modeGlowIntensity[5] = {0};   /* Glow for each mode */

/* Twist mode cursor (top-left of 2x2 selection) */
static int g_twistCursorX = 0;
static int g_twistCursorY = 0;

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

/* Title screen state */
static float g_titleTimer = 0.0f;
static float g_titlePulse = 0.0f;

/* Title screen floating gem particles */
#define MAX_TITLE_GEMS 24
typedef struct {
    Vector2 pos;
    Vector2 vel;
    int gemType;
    float rotation;
    float rotSpeed;
    float size;
    float alpha;
} TitleGem;
static TitleGem g_titleGems[MAX_TITLE_GEMS];

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

    Color baseColor = GetGemBaseColor(gemType);

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

        int fontSize = 28 + (int)(10.0f * lifeRatio);
        float scale = 0.5f + 0.5f * EaseOutBack(1.0f - lifeRatio + 0.5f);
        if (scale > 1.0f) scale = 1.0f;

        Color color = p->color;
        color.a = (unsigned char)(255 * lifeRatio);

        Vector2 textSize = MeasureTextEx(g_accentFont, text, fontSize, 1);
        float x = p->x - textSize.x / 2.0f + g_shakeOffset.x;
        float y = p->y - textSize.y / 2.0f + g_shakeOffset.y;

        /* Shadow */
        DrawTextEx(g_accentFont, text, (Vector2){x + 2, y + 2}, fontSize, 1, (Color){0, 0, 0, color.a / 2});
        /* Main text */
        DrawTextEx(g_accentFont, text, (Vector2){x, y}, fontSize, 1, color);
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
 * GEM RENDERING - Using SDK Shapes
 * ============================================================================ */

static void DrawSpecialGemGlow(int specialType, float cx, float cy, float size, float scale, float alpha);

/* Draw a gem using SDK shapes and colors */
static void DrawGem(int gemType, float cx, float cy, float size, float scale, float alpha) {
    if (gemType == GEM_EMPTY || gemType < 0 || gemType > GEM_TYPE_COUNT) return;

    /* Apply shake offset */
    cx += g_shakeOffset.x;
    cy += g_shakeOffset.y;

    /* Get SDK shape and color for this gem type */
    LlzShapeType shape = GEM_TO_SDK_SHAPE[gemType];
    LlzGemColor gemColor = GEM_TO_SDK_COLOR[gemType];

    /* Calculate gem size */
    float gemSize = size * 0.38f * scale;

    /* Draw using SDK shape function */
    LlzDrawGemShape(shape, cx, cy, gemSize, gemColor);
}

/* Draw special gem BACKGROUND glow effects (rendered BEHIND the gem) */
static void DrawSpecialGemGlow(int specialType, float cx, float cy, float size, float scale, float alpha) {
    if (specialType == SPECIAL_NONE) return;

    float gemSize = size * 0.40f * scale;
    cx += g_shakeOffset.x;
    cy += g_shakeOffset.y;

    float animPhase = g_shimmerTime * 3.0f;

    switch (specialType) {
        case SPECIAL_FLAME: {
            /* Soft orange/red glow behind flame gems */
            float pulseScale = 1.0f + sinf(animPhase * 2.0f) * 0.1f;
            Color glowOuter = {255, 80, 0, (unsigned char)(40 * alpha * pulseScale)};
            Color glowInner = {255, 150, 50, (unsigned char)(60 * alpha * pulseScale)};
            DrawCircle((int)cx, (int)cy, gemSize * 1.8f * pulseScale, glowOuter);
            DrawCircle((int)cx, (int)cy, gemSize * 1.4f * pulseScale, glowInner);
            break;
        }
        case SPECIAL_STAR: {
            /* Soft yellow/white cross glow behind star gems */
            float pulseScale = 1.0f + sinf(animPhase * 2.5f) * 0.08f;
            Color glowColor = {255, 255, 150, (unsigned char)(50 * alpha * pulseScale)};
            float lineLen = gemSize * 1.6f * pulseScale;
            float lineWidth = gemSize * 0.25f;
            /* Horizontal glow */
            DrawRectanglePro(
                (Rectangle){cx - lineLen, cy - lineWidth / 2, lineLen * 2, lineWidth},
                (Vector2){0, 0}, 0, glowColor);
            /* Vertical glow */
            DrawRectanglePro(
                (Rectangle){cx - lineWidth / 2, cy - lineLen, lineWidth, lineLen * 2},
                (Vector2){0, 0}, 0, glowColor);
            break;
        }
        case SPECIAL_HYPERCUBE: {
            /* Soft rainbow cycling glow behind hypercube */
            float hue = fmodf(animPhase * 60.0f, 360.0f);
            Color rainbowGlow = ColorFromHSV(hue, 0.5f, 1.0f);
            rainbowGlow.a = (unsigned char)(50 * alpha);
            DrawCircle((int)cx, (int)cy, gemSize * 1.8f, rainbowGlow);
            /* White mist layer */
            Color mist = {255, 255, 255, (unsigned char)(30 * alpha)};
            DrawCircle((int)cx, (int)cy, gemSize * 1.5f, mist);
            break;
        }
        case SPECIAL_SUPERNOVA: {
            /* Intense blue-white glow behind supernova */
            float pulseScale = 1.0f + sinf(animPhase * 4.0f) * 0.15f;
            Color blueGlow = {100, 180, 255, (unsigned char)(40 * alpha * pulseScale)};
            Color whiteGlow = {220, 240, 255, (unsigned char)(60 * alpha * pulseScale)};
            DrawCircle((int)cx, (int)cy, gemSize * 2.2f * pulseScale, blueGlow);
            DrawCircle((int)cx, (int)cy, gemSize * 1.6f * pulseScale, whiteGlow);
            break;
        }
    }
}

/* Draw special gem overlay effects */
static void DrawSpecialGemOverlay(int specialType, float cx, float cy, float size, float scale, float alpha) {
    if (specialType == SPECIAL_NONE) return;

    float gemSize = size * 0.40f * scale;
    cx += g_shakeOffset.x;
    cy += g_shakeOffset.y;

    float animPhase = g_shimmerTime * 3.0f;

    switch (specialType) {
        case SPECIAL_FLAME: {
            /* Flame Gem: Intense flickering fire with particles and orange glow */
            float pulseScale = 1.0f + sinf(animPhase * 2.0f) * 0.15f;
            float flickerPhase = animPhase * 5.0f;

            /* Outer flame glow - layered for intensity */
            Color flameOuter = {255, 30, 0, (unsigned char)(50 * alpha * pulseScale)};
            Color flameMid = {255, 80, 0, (unsigned char)(80 * alpha * pulseScale)};
            Color flameInner = {255, 140, 0, (unsigned char)(120 * alpha * pulseScale)};

            DrawCircle((int)cx, (int)cy, gemSize * 1.6f * pulseScale, flameOuter);
            DrawCircle((int)cx, (int)cy, gemSize * 1.4f * pulseScale, flameMid);
            DrawCircle((int)cx, (int)cy, gemSize * 1.2f * pulseScale, flameInner);

            /* Flickering fire particles - 8 particles with random-ish movement */
            for (int i = 0; i < 8; i++) {
                float particlePhase = flickerPhase + i * 0.785f;  /* Offset by ~PI/4 */
                float flickerX = sinf(particlePhase * 2.3f + i) * gemSize * 0.2f;
                float flickerY = -fabs(sinf(particlePhase * 1.7f)) * gemSize * 0.4f;

                /* Rising flame particle */
                float baseAngle = i * PI / 4.0f;
                float dist = gemSize * (0.7f + sinf(particlePhase * 3.0f) * 0.15f);
                float px = cx + cosf(baseAngle) * dist * 0.6f + flickerX;
                float py = cy + sinf(baseAngle) * dist * 0.4f + flickerY;

                /* Color gradient from yellow to red based on position */
                float heat = (sinf(particlePhase * 2.0f) + 1.0f) * 0.5f;
                Color particleColor = {
                    255,
                    (unsigned char)(200 * heat + 50),
                    (unsigned char)(50 * heat),
                    (unsigned char)(180 * alpha * (0.5f + heat * 0.5f))
                };
                float particleSize = gemSize * (0.1f + heat * 0.08f);
                DrawCircle((int)px, (int)py, particleSize, particleColor);
            }

            /* Rising heat wisps */
            for (int i = 0; i < 4; i++) {
                float wispPhase = animPhase * 2.0f + i * 1.57f;
                float wispY = cy - gemSize * (0.5f + fmodf(wispPhase * 0.3f, 1.0f) * 0.8f);
                float wispX = cx + sinf(wispPhase * 3.0f + i) * gemSize * 0.25f;
                float wispAlpha = 1.0f - fmodf(wispPhase * 0.3f, 1.0f);
                Color wisp = {255, 220, 100, (unsigned char)(100 * alpha * wispAlpha)};
                DrawCircle((int)wispX, (int)wispY, gemSize * 0.08f * wispAlpha, wisp);
            }

            /* Center hot core */
            Color hotCore = {255, 255, 200, (unsigned char)(255 * alpha)};
            Color hotMid = {255, 200, 100, (unsigned char)(200 * alpha)};
            DrawCircle((int)cx, (int)(cy - gemSize * 0.05f), gemSize * 0.25f, hotMid);
            DrawCircle((int)cx, (int)(cy - gemSize * 0.08f), gemSize * 0.15f, hotCore);
            break;
        }

        case SPECIAL_STAR: {
            /* Star Gem: Electrical sparks with radiant cross glow */
            float pulseScale = 1.0f + sinf(animPhase * 2.5f) * 0.1f;
            float sparkPhase = animPhase * 8.0f;

            /* Radiant cross glow - gradient from center */
            float lineLen = gemSize * 1.4f * pulseScale;
            float lineWidth = gemSize * 0.15f;

            /* Outer glow layer */
            Color glowOuter = {255, 255, 150, (unsigned char)(60 * alpha * pulseScale)};
            DrawRectanglePro(
                (Rectangle){cx - lineLen * 1.1f, cy - lineWidth * 0.8f, lineLen * 2.2f, lineWidth * 1.6f},
                (Vector2){0, 0}, 0, glowOuter);
            DrawRectanglePro(
                (Rectangle){cx - lineWidth * 0.8f, cy - lineLen * 1.1f, lineWidth * 1.6f, lineLen * 2.2f},
                (Vector2){0, 0}, 0, glowOuter);

            /* Inner bright cross */
            Color starGlow = {255, 255, 100, (unsigned char)(140 * alpha * pulseScale)};
            DrawRectanglePro(
                (Rectangle){cx - lineLen, cy - lineWidth / 2, lineLen * 2, lineWidth},
                (Vector2){0, 0}, 0, starGlow);
            DrawRectanglePro(
                (Rectangle){cx - lineWidth / 2, cy - lineLen, lineWidth, lineLen * 2},
                (Vector2){0, 0}, 0, starGlow);

            /* Electrical sparks - random jagged lines */
            for (int i = 0; i < 6; i++) {
                float sparkAngle = i * PI / 3.0f + sinf(sparkPhase + i * 2.0f) * 0.3f;
                float sparkLen = gemSize * (0.6f + sinf(sparkPhase * 2.0f + i) * 0.3f);

                /* Spark origin point */
                float ox = cx + cosf(sparkAngle) * gemSize * 0.3f;
                float oy = cy + sinf(sparkAngle) * gemSize * 0.3f;

                /* Create jagged lightning bolt effect */
                Color sparkColor = {200, 220, 255, (unsigned char)(200 * alpha)};
                float px = ox, py = oy;
                for (int seg = 0; seg < 3; seg++) {
                    float segLen = sparkLen / 3.0f;
                    float jitter = (sinf(sparkPhase * 5.0f + i + seg * 3.0f) * 0.5f) * gemSize * 0.15f;
                    float nx = px + cosf(sparkAngle) * segLen + cosf(sparkAngle + PI/2) * jitter;
                    float ny = py + sinf(sparkAngle) * segLen + sinf(sparkAngle + PI/2) * jitter;
                    DrawLineEx((Vector2){px, py}, (Vector2){nx, ny}, 2.0f, sparkColor);
                    px = nx;
                    py = ny;
                }

                /* Spark endpoint glow */
                if (sinf(sparkPhase * 3.0f + i * 1.5f) > 0.3f) {
                    Color sparkTip = {255, 255, 255, (unsigned char)(255 * alpha)};
                    DrawCircle((int)px, (int)py, gemSize * 0.05f, sparkTip);
                }
            }

            /* Corner diamond sparkles with pulsing */
            Color sparkleColor = {255, 255, 255, (unsigned char)(220 * alpha)};
            for (int i = 0; i < 4; i++) {
                float angle = PI / 4.0f + i * PI / 2.0f;
                float sparkDist = gemSize * 0.7f;
                float sx = cx + cosf(angle) * sparkDist;
                float sy = cy + sinf(angle) * sparkDist;
                float sparkSize = gemSize * 0.1f * (1.0f + sinf(animPhase * 4.0f + i * PI / 2.0f) * 0.5f);
                DrawPoly((Vector2){sx, sy}, 4, sparkSize, 45.0f, sparkleColor);
            }

            /* Center bright point */
            Color centerGlow = {255, 255, 255, (unsigned char)(255 * alpha)};
            DrawCircle((int)cx, (int)cy, gemSize * 0.12f, centerGlow);
            break;
        }

        case SPECIAL_HYPERCUBE: {
            /* Hypercube: Spinning rainbow cube with white mist aura */
            float rotation = animPhase * 40.0f;  /* Rotation in degrees */
            float rotationZ = animPhase * 25.0f; /* Secondary rotation */

            /* White mist aura - multiple soft layers */
            for (int layer = 4; layer >= 0; layer--) {
                float mistOffset = sinf(animPhase + layer * 0.5f) * gemSize * 0.1f;
                float mistSize = gemSize * (1.3f + layer * 0.15f);
                Color mist = {255, 255, 255, (unsigned char)(30 - layer * 5) * alpha};
                DrawCircle((int)(cx + mistOffset), (int)(cy + mistOffset * 0.5f), mistSize, mist);
            }

            /* Rainbow glow cycling through colors - more vibrant */
            float hue = fmodf(animPhase * 80.0f, 360.0f);
            Color rainbowColor = ColorFromHSV(hue, 0.9f, 1.0f);
            rainbowColor.a = (unsigned char)(120 * alpha);
            DrawCircle((int)cx, (int)cy, gemSize * 1.1f, rainbowColor);

            /* Secondary rainbow trail */
            float hue2 = fmodf(hue + 120.0f, 360.0f);
            Color rainbow2 = ColorFromHSV(hue2, 0.8f, 1.0f);
            rainbow2.a = (unsigned char)(80 * alpha);
            DrawCircle((int)cx, (int)cy, gemSize * 1.0f, rainbow2);

            /* Outer cube - larger, slower rotation */
            float cubeSize = gemSize * 0.55f;
            Color cubeColor = WHITE;
            cubeColor.a = (unsigned char)(220 * alpha);

            float cosR = cosf(rotation * DEG2RAD);
            float sinR = sinf(rotation * DEG2RAD);
            Vector2 corners[4];
            for (int i = 0; i < 4; i++) {
                float ax = (i == 0 || i == 3) ? -cubeSize : cubeSize;
                float ay = (i < 2) ? -cubeSize : cubeSize;
                corners[i].x = cx + ax * cosR - ay * sinR;
                corners[i].y = cy + ax * sinR + ay * cosR;
            }
            for (int i = 0; i < 4; i++) {
                DrawLineEx(corners[i], corners[(i + 1) % 4], 3.0f, cubeColor);
            }

            /* Inner cube - smaller, opposite rotation */
            Color innerColor = rainbowColor;
            innerColor.a = (unsigned char)(180 * alpha);
            cubeSize = gemSize * 0.35f;
            float cosR2 = cosf(-rotationZ * DEG2RAD);
            float sinR2 = sinf(-rotationZ * DEG2RAD);
            for (int i = 0; i < 4; i++) {
                float ax = (i == 0 || i == 3) ? -cubeSize : cubeSize;
                float ay = (i < 2) ? -cubeSize : cubeSize;
                corners[i].x = cx + ax * cosR2 - ay * sinR2;
                corners[i].y = cy + ax * sinR2 + ay * cosR2;
            }
            for (int i = 0; i < 4; i++) {
                DrawLineEx(corners[i], corners[(i + 1) % 4], 2.0f, innerColor);
            }

            /* Connecting lines between cubes (3D illusion) */
            float smallCube = gemSize * 0.2f;
            float cosR3 = cosf((rotation + 45.0f) * DEG2RAD);
            float sinR3 = sinf((rotation + 45.0f) * DEG2RAD);
            Color connectColor = {255, 255, 255, (unsigned char)(100 * alpha)};
            for (int i = 0; i < 4; i++) {
                float ax = (i == 0 || i == 3) ? -smallCube : smallCube;
                float ay = (i < 2) ? -smallCube : smallCube;
                float px = cx + ax * cosR3 - ay * sinR3;
                float py = cy + ax * sinR3 + ay * cosR3;
                DrawCircle((int)px, (int)py, 2.0f, connectColor);
            }

            /* Center prismatic core */
            Color coreColor = {255, 255, 255, (unsigned char)(255 * alpha)};
            DrawCircle((int)cx, (int)cy, gemSize * 0.15f, coreColor);
            break;
        }

        case SPECIAL_SUPERNOVA: {
            /* Supernova: Combined flame + star effects with white-hot energy and blue tint */
            float pulseScale = 1.0f + sinf(animPhase * 4.0f) * 0.25f;
            float sparkPhase = animPhase * 6.0f;

            /* Blue-tinted outer aura */
            Color blueAura = {80, 150, 255, (unsigned char)(40 * alpha * pulseScale)};
            DrawCircle((int)cx, (int)cy, gemSize * 2.0f * pulseScale, blueAura);

            /* Intense white/cyan glow layers */
            Color outerGlow = {100, 180, 255, (unsigned char)(60 * alpha * pulseScale)};
            Color midGlow = {180, 220, 255, (unsigned char)(100 * alpha * pulseScale)};
            Color coreGlow = {220, 240, 255, (unsigned char)(150 * alpha * pulseScale)};

            DrawCircle((int)cx, (int)cy, gemSize * 1.7f * pulseScale, outerGlow);
            DrawCircle((int)cx, (int)cy, gemSize * 1.4f * pulseScale, midGlow);
            DrawCircle((int)cx, (int)cy, gemSize * 1.1f * pulseScale, coreGlow);

            /* Radiating energy rays - 3-wide cross pattern */
            float rayLen = gemSize * 2.0f * pulseScale;
            float rayWidth = gemSize * 0.12f;

            /* Outer glow for rays */
            Color rayGlow = {200, 230, 255, (unsigned char)(120 * alpha)};
            Color rayCore = {255, 255, 255, (unsigned char)(200 * alpha)};

            /* Horizontal rays with glow */
            for (int i = -1; i <= 1; i++) {
                float yOff = i * gemSize * 0.35f;
                float thisWidth = rayWidth * (1.0f - fabs((float)i) * 0.3f);
                DrawRectanglePro(
                    (Rectangle){cx - rayLen * 1.1f, cy + yOff - thisWidth, rayLen * 2.2f, thisWidth * 2},
                    (Vector2){0, 0}, 0, rayGlow);
                DrawRectanglePro(
                    (Rectangle){cx - rayLen, cy + yOff - thisWidth / 2, rayLen * 2, thisWidth},
                    (Vector2){0, 0}, 0, rayCore);
            }
            /* Vertical rays with glow */
            for (int i = -1; i <= 1; i++) {
                float xOff = i * gemSize * 0.35f;
                float thisWidth = rayWidth * (1.0f - fabs((float)i) * 0.3f);
                DrawRectanglePro(
                    (Rectangle){cx + xOff - thisWidth, cy - rayLen * 1.1f, thisWidth * 2, rayLen * 2.2f},
                    (Vector2){0, 0}, 0, rayGlow);
                DrawRectanglePro(
                    (Rectangle){cx + xOff - thisWidth / 2, cy - rayLen, thisWidth, rayLen * 2},
                    (Vector2){0, 0}, 0, rayCore);
            }

            /* Fire particles around the edge - flame effect */
            for (int i = 0; i < 8; i++) {
                float particleAngle = i * PI / 4.0f + sparkPhase * 0.5f;
                float flicker = sinf(sparkPhase * 2.0f + i * 1.5f);
                float dist = gemSize * (0.9f + flicker * 0.15f);
                float px = cx + cosf(particleAngle) * dist;
                float py = cy + sinf(particleAngle) * dist;

                Color fireColor = {255, (unsigned char)(200 + flicker * 55), (unsigned char)(100 + flicker * 50), (unsigned char)(180 * alpha)};
                DrawCircle((int)px, (int)py, gemSize * 0.1f, fireColor);
            }

            /* Electrical sparks - star effect */
            for (int i = 0; i < 4; i++) {
                float sparkAngle = i * PI / 2.0f + PI / 4.0f;
                if (sinf(sparkPhase + i * 2.0f) > 0.2f) {
                    float sparkDist = gemSize * 1.2f;
                    float sx = cx + cosf(sparkAngle) * sparkDist;
                    float sy = cy + sinf(sparkAngle) * sparkDist;

                    Color sparkColor = {200, 220, 255, (unsigned char)(200 * alpha)};
                    DrawLineEx((Vector2){cx + cosf(sparkAngle) * gemSize * 0.5f, cy + sinf(sparkAngle) * gemSize * 0.5f},
                              (Vector2){sx, sy}, 2.0f, sparkColor);

                    Color sparkTip = {255, 255, 255, (unsigned char)(255 * alpha)};
                    DrawCircle((int)sx, (int)sy, gemSize * 0.06f, sparkTip);
                }
            }

            /* White-hot central core with blue tint ring */
            Color blueRing = {150, 200, 255, (unsigned char)(200 * alpha)};
            DrawRing((Vector2){cx, cy}, gemSize * 0.25f, gemSize * 0.35f, 0, 360, 24, blueRing);

            Color brightCore = {255, 255, 255, (unsigned char)(255 * alpha)};
            DrawCircle((int)cx, (int)cy, gemSize * 0.25f, brightCore);

            /* Pulsing highlight */
            float highlightPulse = (sinf(animPhase * 6.0f) + 1.0f) * 0.5f;
            Color highlight = {255, 255, 255, (unsigned char)(150 * highlightPulse * alpha)};
            DrawCircle((int)(cx - gemSize * 0.1f), (int)(cy - gemSize * 0.1f), gemSize * 0.12f, highlight);
            break;
        }
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
    GameMode mode = GetCurrentGameMode();

    if (mode == GAME_MODE_TWIST) {
        /* Twist mode: highlight all 4 cells in the 2x2 grid */
        float sx1, sy1, sx2, sy2;
        GridToScreen(x1, y1, &sx1, &sy1);
        GridToScreen(x2, y2, &sx2, &sy2);

        sx1 += g_shakeOffset.x - g_cellSize / 2.0f;
        sy1 += g_shakeOffset.y - g_cellSize / 2.0f;
        sx2 += g_shakeOffset.x + g_cellSize / 2.0f;
        sy2 += g_shakeOffset.y + g_cellSize / 2.0f;

        Color hintColor = (Color){100, 255, 100, (unsigned char)(100 + 100 * pulse)};
        DrawRectangleLinesEx((Rectangle){sx1, sy1, sx2 - sx1, sy2 - sy1}, 3, hintColor);

        /* Add glow effect */
        Color glowColor = {100, 255, 100, (unsigned char)(40 * pulse)};
        DrawRectangleRounded((Rectangle){sx1 - 4, sy1 - 4, (sx2 - sx1) + 8, (sy2 - sy1) + 8},
                            0.1f, 8, glowColor);
    } else {
        /* Classic/Blitz mode: highlight the two swap positions */
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

        int fontSize = (int)(42 * scaleAnim);
        if (fontSize < 8) continue;

        Vector2 textSize = MeasureTextEx(g_accentFont, ann->text, (float)fontSize, 1);
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
        DrawTextEx(g_accentFont, ann->text, (Vector2){tx + 3, ty + 3}, (float)fontSize, 1, shadowColor);

        /* Main text */
        DrawTextEx(g_accentFont, ann->text, (Vector2){tx, ty}, (float)fontSize, 1, textColor);
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

    int fontSize = (int)(48 * scale);
    Vector2 textSize = MeasureTextEx(g_accentFont, levelText, (float)fontSize, 1);
    float tx = cx - textSize.x / 2.0f;
    float ty = cy - textSize.y / 2.0f;

    /* Simple gold color with subtle pulse */
    float pulse = sinf(g_animTimer * 6.0f) * 0.15f + 0.85f;
    Color textColor = {255, (unsigned char)(200 * pulse), 50, (unsigned char)(255 * alpha)};

    /* Shadow */
    DrawTextEx(g_accentFont, levelText, (Vector2){tx + 3, ty + 3}, (float)fontSize, 1, (Color){0, 0, 0, (unsigned char)(180 * alpha)});

    /* Main text */
    DrawTextEx(g_accentFont, levelText, (Vector2){tx, ty}, (float)fontSize, 1, textColor);
}

/* ============================================================================
 * TITLE SCREEN
 * ============================================================================ */

/* Initialize floating title gems */
static void InitTitleGems(void) {
    for (int i = 0; i < MAX_TITLE_GEMS; i++) {
        g_titleGems[i].pos.x = (float)GetRandomValue(0, g_screenWidth);
        g_titleGems[i].pos.y = (float)GetRandomValue(0, g_screenHeight);
        g_titleGems[i].vel.x = (float)GetRandomValue(-30, 30);
        g_titleGems[i].vel.y = (float)GetRandomValue(-20, -60);  /* Float upward */
        g_titleGems[i].gemType = GetRandomValue(1, 7);  /* Random gem type */
        g_titleGems[i].rotation = (float)GetRandomValue(0, 360);
        g_titleGems[i].rotSpeed = (float)GetRandomValue(-60, 60);
        g_titleGems[i].size = (float)GetRandomValue(20, 50);
        g_titleGems[i].alpha = (float)GetRandomValue(30, 80) / 100.0f;
    }
}

/* Update title screen animations */
static void UpdateTitleScreen(const LlzInputState *input, float deltaTime) {
    g_titleTimer += deltaTime;
    g_titlePulse = (sinf(g_titleTimer * 2.5f) + 1.0f) * 0.5f;

    /* Update floating gems */
    for (int i = 0; i < MAX_TITLE_GEMS; i++) {
        g_titleGems[i].pos.x += g_titleGems[i].vel.x * deltaTime;
        g_titleGems[i].pos.y += g_titleGems[i].vel.y * deltaTime;
        g_titleGems[i].rotation += g_titleGems[i].rotSpeed * deltaTime;

        /* Wrap around screen edges */
        if (g_titleGems[i].pos.y < -g_titleGems[i].size) {
            g_titleGems[i].pos.y = g_screenHeight + g_titleGems[i].size;
            g_titleGems[i].pos.x = (float)GetRandomValue(0, g_screenWidth);
            g_titleGems[i].gemType = GetRandomValue(1, 7);
        }
        if (g_titleGems[i].pos.x < -g_titleGems[i].size) {
            g_titleGems[i].pos.x = g_screenWidth + g_titleGems[i].size;
        }
        if (g_titleGems[i].pos.x > g_screenWidth + g_titleGems[i].size) {
            g_titleGems[i].pos.x = -g_titleGems[i].size;
        }
    }

    /* Handle input */
    if (!input) return;

    /* SELECT to proceed to mode select */
    if (input->selectPressed || input->tap) {
        g_pluginState = PLUGIN_STATE_MODE_SELECT;
        g_selectedMode = 0;
    }

    /* BACK to close plugin */
    if (input->backReleased) {
        g_wantsClose = true;
    }
}

/* Draw the stunning title screen */
static void DrawTitleScreen(void) {
    float centerX = g_screenWidth / 2.0f;
    float centerY = g_screenHeight / 2.0f;

    /* ====== BACKGROUND: Dark purple to dark blue gradient ====== */
    for (int y = 0; y < g_screenHeight; y++) {
        float t = (float)y / g_screenHeight;
        /* Dark purple at top, dark blue at bottom */
        Color gradColor = {
            (unsigned char)(25 + 15 * (1.0f - t)),   /* R: 40 -> 25 */
            (unsigned char)(15 + 10 * t),             /* G: 15 -> 25 */
            (unsigned char)(45 + 35 * t),             /* B: 45 -> 80 */
            255
        };
        DrawRectangle(0, y, g_screenWidth, 1, gradColor);
    }

    /* ====== FLOATING GEM PARTICLES (behind title) ====== */
    for (int i = 0; i < MAX_TITLE_GEMS; i++) {
        float cx = g_titleGems[i].pos.x;
        float cy = g_titleGems[i].pos.y;
        int gemType = g_titleGems[i].gemType;
        float alpha = g_titleGems[i].alpha;
        float size = g_titleGems[i].size;

        /* Draw simplified gem shapes with low alpha */
        Color gemColor = GetGemBaseColor(gemType);
        gemColor.a = (unsigned char)(gemColor.a * alpha * 0.6f);

        /* Outer glow */
        Color glowColor = gemColor;
        glowColor.a = (unsigned char)(glowColor.a * 0.3f);
        DrawPoly((Vector2){cx, cy}, 8, size * 0.6f, g_titleGems[i].rotation, glowColor);

        /* Main gem body */
        DrawPoly((Vector2){cx, cy}, 8, size * 0.4f, g_titleGems[i].rotation + 22.5f, gemColor);

        /* Highlight */
        Color highlight = WHITE;
        highlight.a = (unsigned char)(60 * alpha);
        DrawCircle((int)(cx - size * 0.1f), (int)(cy - size * 0.1f), size * 0.15f, highlight);
    }

    /* ====== DECORATIVE CORNER GEMS ====== */
    float cornerOffset = 60.0f;
    float cornerSize = 40.0f;
    float cornerPulse = (sinf(g_titleTimer * 3.0f) + 1.0f) * 0.5f;

    /* Top-left corner gem (Ruby) */
    DrawGem(GEM_RED, cornerOffset, cornerOffset, cornerSize * 2, 1.0f + cornerPulse * 0.1f, 0.8f);

    /* Top-right corner gem (Sapphire) */
    DrawGem(GEM_BLUE, g_screenWidth - cornerOffset, cornerOffset, cornerSize * 2, 1.0f + cornerPulse * 0.1f, 0.8f);

    /* Bottom-left corner gem (Emerald) */
    DrawGem(GEM_GREEN, cornerOffset, g_screenHeight - cornerOffset, cornerSize * 2, 1.0f + cornerPulse * 0.1f, 0.8f);

    /* Bottom-right corner gem (Amethyst) */
    DrawGem(GEM_PURPLE, g_screenWidth - cornerOffset, g_screenHeight - cornerOffset, cornerSize * 2, 1.0f + cornerPulse * 0.1f, 0.8f);

    /* ====== MAIN TITLE: "BEJEWELED" with multi-layer glow ====== */
    const char* title = "BEJEWELED";
    int titleFontSize = 72;
    Vector2 titleSize = MeasureTextEx(g_font, title, titleFontSize, 2);

    /* Calculate title position with pulsing scale */
    float titleScale = 1.0f + g_titlePulse * 0.03f;
    float scaledWidth = titleSize.x * titleScale;
    float titleX = centerX - scaledWidth / 2.0f;
    float titleY = centerY - 80;

    /* ====== RAINBOW GLOW LAYERS (cycling through gem colors) ====== */
    /* Layer 1: Outermost glow - cycles through colors */
    for (int layer = 5; layer >= 0; layer--) {
        float hueOffset = fmodf(g_titleTimer * 60.0f + layer * 60.0f, 360.0f);
        Color glowColor = ColorFromHSV(hueOffset, 0.8f, 1.0f);
        float glowAlpha = 25.0f - layer * 4.0f;
        glowColor.a = (unsigned char)(glowAlpha + glowAlpha * g_titlePulse * 0.5f);

        float offset = (layer + 1) * 3.0f;
        DrawTextEx(g_font, title, (Vector2){titleX - offset, titleY - offset},
                  titleFontSize * titleScale, 2, glowColor);
        DrawTextEx(g_font, title, (Vector2){titleX + offset, titleY - offset},
                  titleFontSize * titleScale, 2, glowColor);
        DrawTextEx(g_font, title, (Vector2){titleX - offset, titleY + offset},
                  titleFontSize * titleScale, 2, glowColor);
        DrawTextEx(g_font, title, (Vector2){titleX + offset, titleY + offset},
                  titleFontSize * titleScale, 2, glowColor);
    }

    /* Layer 2: Colored underglow (gem color cycle) */
    int colorIndex = (int)(g_titleTimer * 2.0f) % 7 + 1;
    Color underGlow = GetGemBaseColor(colorIndex);
    underGlow.a = (unsigned char)(100 + 50 * g_titlePulse);
    DrawTextEx(g_font, title, (Vector2){titleX + 2, titleY + 4}, titleFontSize * titleScale, 2, underGlow);

    /* Layer 3: Dark shadow for depth */
    Color shadowColor = {10, 5, 30, 200};
    DrawTextEx(g_font, title, (Vector2){titleX + 4, titleY + 6}, titleFontSize * titleScale, 2, shadowColor);

    /* Layer 4: Gold base text */
    Color goldBase = {255, 215, 0, 255};
    DrawTextEx(g_font, title, (Vector2){titleX, titleY}, titleFontSize * titleScale, 2, goldBase);

    /* Layer 5: Bright highlight on top portion (faceted look) */
    /* Draw top half brighter for faceted gem appearance */
    Color brightGold = {255, 240, 150, (unsigned char)(200 + 55 * g_titlePulse)};
    DrawTextEx(g_font, title, (Vector2){titleX - 1, titleY - 1}, titleFontSize * titleScale, 2, brightGold);

    /* Layer 6: White specular highlight */
    Color specular = {255, 255, 255, (unsigned char)(120 + 80 * g_titlePulse)};
    DrawTextEx(g_font, title, (Vector2){titleX - 2, titleY - 2}, titleFontSize * titleScale * 0.98f, 2, specular);

    /* ====== ANIMATED SPARKLES on title ====== */
    for (int i = 0; i < 5; i++) {
        float sparklePhase = fmodf(g_titleTimer * 3.0f + i * 1.2f, 3.0f);
        if (sparklePhase < 0.8f) {
            float sparkleX = titleX + (titleSize.x * titleScale) * (0.1f + i * 0.2f);
            float sparkleY = titleY + titleFontSize * 0.3f;
            float sparkleAlpha = sinf(sparklePhase * PI / 0.8f);
            float sparkleSize = 6.0f + 4.0f * sparkleAlpha;

            Color sparkleColor = WHITE;
            sparkleColor.a = (unsigned char)(255 * sparkleAlpha);

            /* 4-pointed star sparkle */
            DrawPoly((Vector2){sparkleX, sparkleY}, 4, sparkleSize, 45.0f, sparkleColor);
            DrawPoly((Vector2){sparkleX, sparkleY}, 4, sparkleSize * 0.6f, 0.0f, sparkleColor);
        }
    }

    /* ====== SUBTITLE: "LLIZARDOS EDITION" ====== */
    const char* subtitle = "LLIZARDOS EDITION";
    int subtitleFontSize = 24;
    Vector2 subtitleSize = MeasureTextEx(g_font, subtitle, subtitleFontSize, 1);
    float subtitleX = centerX - subtitleSize.x / 2.0f;
    float subtitleY = titleY + titleFontSize + 20;

    /* Subtitle shadow */
    DrawTextEx(g_font, subtitle, (Vector2){subtitleX + 2, subtitleY + 2}, subtitleFontSize, 1, (Color){0, 0, 0, 150});

    /* Subtitle in lighter gold/amber */
    Color subtitleColor = {255, 200, 100, (unsigned char)(200 + 55 * g_titlePulse)};
    DrawTextEx(g_font, subtitle, (Vector2){subtitleX, subtitleY}, subtitleFontSize, 1, subtitleColor);

    /* ====== PRESS SELECT prompt with blinking ====== */
    const char* prompt = "PRESS SELECT TO PLAY";
    int promptFontSize = 20;
    Vector2 promptSize = MeasureTextEx(g_font, prompt, promptFontSize, 1);
    float promptX = centerX - promptSize.x / 2.0f;
    float promptY = g_screenHeight - 80;

    /* Blinking alpha */
    float blinkAlpha = (sinf(g_titleTimer * 4.0f) + 1.0f) * 0.5f;
    blinkAlpha = 0.3f + blinkAlpha * 0.7f;  /* Range from 0.3 to 1.0 */

    /* Prompt shadow */
    DrawTextEx(g_font, prompt, (Vector2){promptX + 1, promptY + 1}, promptFontSize, 1,
              (Color){0, 0, 0, (unsigned char)(150 * blinkAlpha)});

    /* Prompt text */
    Color promptColor = {240, 240, 250, (unsigned char)(255 * blinkAlpha)};
    DrawTextEx(g_font, prompt, (Vector2){promptX, promptY}, promptFontSize, 1, promptColor);

    /* ====== DECORATIVE LINE below subtitle ====== */
    float lineY = subtitleY + subtitleFontSize + 15;
    float lineWidth = 200.0f + 50.0f * g_titlePulse;
    float lineX = centerX - lineWidth / 2.0f;

    /* Gradient line with gem colors */
    for (int i = 0; i < (int)lineWidth; i++) {
        float t = (float)i / lineWidth;
        float hue = fmodf(g_titleTimer * 40.0f + t * 180.0f, 360.0f);
        Color lineColor = ColorFromHSV(hue, 0.7f, 1.0f);
        /* Fade at edges */
        float edgeFade = 1.0f - fabsf(t - 0.5f) * 2.0f;
        edgeFade = edgeFade * edgeFade;  /* Quadratic falloff */
        lineColor.a = (unsigned char)(200 * edgeFade);
        DrawRectangle((int)(lineX + i), (int)lineY, 1, 3, lineColor);
    }

    /* Center gem on the line */
    float lineCenterY = lineY + 1.5f;
    DrawGem(GEM_WHITE, centerX, lineCenterY, 30.0f, 0.8f + g_titlePulse * 0.2f, 1.0f);
}

/* ============================================================================
 * MODE SELECTION MENU
 * ============================================================================ */

/* Mode descriptions for 5 game modes */
static const char* MODE_NAMES[] = {
    "CLASSIC", "BLITZ", "TWIST", "CASCADE RUSH", "GEM SURGE"
};
static const char* MODE_DESCRIPTIONS[] = {
    "NO TIME LIMIT",
    "60 SECONDS",
    "ROTATE 2X2 GEMS",
    "CASCADE FOR TIME!",
    "WAVE-BASED ACTION"
};
static const Color MODE_COLORS[] = {
    {100, 200, 255, 255},  /* Classic - Blue */
    {255, 150, 50, 255},   /* Blitz - Orange */
    {150, 100, 255, 255},  /* Twist - Purple */
    {100, 255, 150, 255},  /* Cascade Rush - Green */
    {255, 100, 200, 255}   /* Gem Surge - Pink */
};

/* Update carousel animation - smooth position interpolation */
static void UpdateModeCarousel(float deltaTime) {
    /* Smooth animation using exponential ease-out */
    float diff = g_carouselTarget - g_carouselPosition;

    /* Handle wrap-around for smooth transitions */
    if (diff > 2.5f) diff -= 5.0f;
    if (diff < -2.5f) diff += 5.0f;

    /* Exponential ease-out with ~0.3 second animation */
    float speed = 10.0f;
    g_carouselPosition += diff * speed * deltaTime;

    /* Normalize position to 0-5 range */
    while (g_carouselPosition < 0.0f) g_carouselPosition += 5.0f;
    while (g_carouselPosition >= 5.0f) g_carouselPosition -= 5.0f;

    /* Check if animation is complete */
    if (fabsf(diff) < 0.01f) {
        g_carouselPosition = g_carouselTarget;
        while (g_carouselPosition < 0.0f) g_carouselPosition += 5.0f;
        while (g_carouselPosition >= 5.0f) g_carouselPosition -= 5.0f;
        g_carouselAnimating = false;
    }

    /* Update glow intensity for each mode */
    for (int i = 0; i < 5; i++) {
        float targetGlow = (i == g_selectedMode) ? 1.0f : 0.0f;
        g_modeGlowIntensity[i] += (targetGlow - g_modeGlowIntensity[i]) * 8.0f * deltaTime;
    }
}

/* Draw carousel-style mode selector */
static void DrawModeCarousel(void) {
    float centerX = g_screenWidth / 2.0f;
    float centerY = g_screenHeight / 2.0f;

    /* Card dimensions */
    float baseCardWidth = 180;
    float baseCardHeight = 220;
    float cardSpacing = 160;

    /* Draw each mode card with carousel positioning */
    /* Draw in order from back to front (farthest first) */
    int drawOrder[5];
    float distances[5];

    for (int i = 0; i < 5; i++) {
        /* Calculate position offset from center (selected mode) */
        float offset = (float)i - g_carouselPosition;

        /* Handle wrap-around */
        while (offset > 2.5f) offset -= 5.0f;
        while (offset < -2.5f) offset += 5.0f;

        distances[i] = fabsf(offset);
        drawOrder[i] = i;
    }

    /* Sort by distance (farthest first for proper z-ordering) */
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 5; j++) {
            if (distances[drawOrder[i]] < distances[drawOrder[j]]) {
                int temp = drawOrder[i];
                drawOrder[i] = drawOrder[j];
                drawOrder[j] = temp;
            }
        }
    }

    /* Draw cards in sorted order */
    for (int d = 0; d < 5; d++) {
        int i = drawOrder[d];

        /* Calculate position offset from center */
        float offset = (float)i - g_carouselPosition;

        /* Handle wrap-around */
        while (offset > 2.5f) offset -= 5.0f;
        while (offset < -2.5f) offset += 5.0f;

        float absOffset = fabsf(offset);

        /* Scale based on distance from center */
        /* Center: 1.0, Adjacent (offset=1): 0.7, Far (offset=2): 0.5 */
        float scale;
        if (absOffset < 0.1f) {
            scale = 1.0f;
        } else if (absOffset < 1.5f) {
            scale = 1.0f - 0.3f * absOffset;
        } else {
            scale = 0.5f;
        }

        /* Add pulse effect for selected mode */
        bool isSelected = (i == g_selectedMode);
        float selectionPulse = isSelected ? (sinf(g_animTimer * 6.0f) + 1.0f) * 0.5f * 0.05f : 0.0f;
        scale += selectionPulse;

        /* Calculate alpha based on distance */
        float alpha = 1.0f;
        if (absOffset > 1.5f) {
            alpha = 0.4f;
        } else if (absOffset > 0.5f) {
            alpha = 1.0f - 0.4f * (absOffset - 0.5f);
        }

        /* Calculate card position */
        float cardWidth = baseCardWidth * scale;
        float cardHeight = baseCardHeight * scale;
        float cardX = centerX + offset * cardSpacing - cardWidth / 2.0f;
        float cardY = centerY - cardHeight / 2.0f + 30;

        /* Glow effect for selected card */
        float glowIntensity = g_modeGlowIntensity[i];
        if (glowIntensity > 0.01f) {
            Color glowColor = MODE_COLORS[i];
            float glowPulse = (sinf(g_animTimer * 6.0f) + 1.0f) * 0.5f;
            glowColor.a = (unsigned char)((80 + 60 * glowPulse) * glowIntensity * alpha);
            DrawRectangleRounded((Rectangle){cardX - 8, cardY - 8, cardWidth + 16, cardHeight + 16},
                                0.12f, 8, glowColor);
        }

        /* Card background */
        Color cardBg = isSelected ? (Color){45, 50, 70, (unsigned char)(255 * alpha)}
                                  : (Color){30, 33, 48, (unsigned char)(255 * alpha)};
        DrawRectangleRounded((Rectangle){cardX, cardY, cardWidth, cardHeight}, 0.12f, 8, cardBg);

        /* Card border */
        Color borderColor = MODE_COLORS[i];
        borderColor.a = (unsigned char)((isSelected ? 255 : 120) * alpha);
        DrawRectangleRoundedLines((Rectangle){cardX, cardY, cardWidth, cardHeight}, 0.12f, 8, borderColor);

        /* Mode name */
        float nameX = cardX + cardWidth / 2.0f;
        float nameY = cardY + 25 * scale;
        int nameFontSize = (int)(32 * scale);
        if (nameFontSize < 14) nameFontSize = 14;
        Vector2 nameSize = MeasureTextEx(g_font, MODE_NAMES[i], nameFontSize, 1);

        Color nameColor = MODE_COLORS[i];
        nameColor.a = (unsigned char)(255 * alpha);
        DrawTextEx(g_font, MODE_NAMES[i], (Vector2){nameX - nameSize.x / 2.0f, nameY}, nameFontSize, 1, nameColor);

        /* Mode icon (decorative gem shape) */
        float iconY = cardY + cardHeight * 0.45f;
        float iconSize = 50 * scale;
        Color iconColor = MODE_COLORS[i];
        iconColor.a = (unsigned char)((isSelected ? 255 : 180) * alpha);
        DrawPoly((Vector2){nameX, iconY}, 8, iconSize * 0.5f, 22.5f, iconColor);

        /* Inner gem facet */
        Color innerColor = {255, 255, 255, (unsigned char)((isSelected ? 150 : 80) * alpha)};
        DrawPoly((Vector2){nameX - 4 * scale, iconY - 4 * scale}, 8, iconSize * 0.25f, 22.5f, innerColor);

        /* Description - only show for visible cards */
        if (alpha > 0.3f) {
            float descY = cardY + cardHeight * 0.72f;
            int descFontSize = (int)(18 * scale);
            if (descFontSize < 10) descFontSize = 10;
            Vector2 descSize = MeasureTextEx(g_font, MODE_DESCRIPTIONS[i], descFontSize, 1);

            Color descColor = isSelected ? (Color){240, 240, 250, (unsigned char)(255 * alpha)}
                                         : (Color){180, 185, 200, (unsigned char)(200 * alpha)};
            DrawTextEx(g_font, MODE_DESCRIPTIONS[i], (Vector2){nameX - descSize.x / 2.0f, descY}, descFontSize, 1, descColor);
        }
    }
}

static void DrawModeSelectMenu(void) {
    /* Draw animated background */
    DrawAnimatedBackground();

    float centerX = g_screenWidth / 2.0f;

    /* Draw dark overlay with subtle gradient for carousel area */
    DrawRectangleGradientV(0, 80, g_screenWidth, g_screenHeight - 130,
                           (Color){10, 12, 20, 180}, (Color){20, 22, 35, 180});

    /* Title */
    const char* title = "SELECT MODE";
    int titleFontSize = 52;
    Vector2 titleSize = MeasureTextEx(g_font, title, titleFontSize, 1);
    float titleX = centerX - titleSize.x / 2.0f;
    float titleY = 25;

    /* Title glow */
    float pulse = (sinf(g_animTimer * 3.0f) + 1.0f) * 0.5f;
    Color glowColor = {255, 215, 0, (unsigned char)(60 + 40 * pulse)};
    DrawTextEx(g_font, title, (Vector2){titleX + 2, titleY + 2}, titleFontSize, 1, (Color){0, 0, 0, 180});
    DrawTextEx(g_font, title, (Vector2){titleX, titleY}, titleFontSize, 1, glowColor);
    DrawTextEx(g_font, title, (Vector2){titleX, titleY}, titleFontSize, 1, COLOR_HIGHLIGHT);

    /* Draw carousel */
    DrawModeCarousel();

    /* Instructions */
    const char* instructions = "SCROLL TO SELECT - PRESS TO PLAY";
    int instrFontSize = 18;
    Vector2 instrSize = MeasureTextEx(g_font, instructions, instrFontSize, 1);
    float instrAlpha = 150 + 105 * sinf(g_animTimer * 2.5f);
    DrawTextEx(g_font, instructions, (Vector2){centerX - instrSize.x / 2.0f, g_screenHeight - 45},
              instrFontSize, 1, (Color){240, 240, 250, (unsigned char)instrAlpha});

    /* Mode indicator dots */
    float dotY = g_screenHeight - 75;
    float dotSpacing = 18;
    float totalDotWidth = 4 * dotSpacing;
    float dotStartX = centerX - totalDotWidth / 2.0f;

    for (int i = 0; i < 5; i++) {
        float dotX = dotStartX + i * dotSpacing;
        bool isSelected = (i == g_selectedMode);

        Color dotColor = isSelected ? MODE_COLORS[i] : (Color){80, 85, 100, 200};
        float dotSize = isSelected ? 6.0f : 4.0f;

        DrawCircleV((Vector2){dotX, dotY}, dotSize, dotColor);
    }
}

static void HandleModeSelectInput(const LlzInputState *input) {
    if (!input) return;

    /* BACK to return to title screen / close plugin */
    if (input->backReleased) {
        g_wantsClose = true;
        return;
    }

    /* Navigate carousel - Up/Down or scroll wheel */
    if (input->downPressed || input->scrollDelta > 0.5f) {
        /* Move to next mode with wrap-around (0 to 4, then back to 0) */
        g_selectedMode = (g_selectedMode + 1) % 5;
        g_carouselTarget = (float)g_selectedMode;
        g_carouselAnimating = true;
    }
    if (input->upPressed || input->scrollDelta < -0.5f) {
        /* Move to previous mode with wrap-around (4 to 0, then back to 4) */
        g_selectedMode = (g_selectedMode + 4) % 5;
        g_carouselTarget = (float)g_selectedMode;
        g_carouselAnimating = true;
    }

    /* SELECT to start game with selected mode */
    if (input->selectPressed) {
        InitGameMode((GameMode)g_selectedMode);

        /* Reset cursor positions */
        g_cursorX = BOARD_WIDTH / 2;
        g_cursorY = BOARD_HEIGHT / 2;
        g_twistCursorX = BOARD_WIDTH / 2 - 1;
        g_twistCursorY = BOARD_HEIGHT / 2 - 1;
        if (g_twistCursorX < 0) g_twistCursorX = 0;
        if (g_twistCursorY < 0) g_twistCursorY = 0;

        /* Reset display state */
        g_displayScore = 0;
        g_scorePulse = 0;
        g_previousLevel = 1;
        g_lastCascadeLevel = 0;

        g_pluginState = PLUGIN_STATE_PLAYING;
    }

    /* Touch input - tap center card to start, tap side cards to navigate */
    if (input->tap) {
        float centerX = g_screenWidth / 2.0f;
        float tapX = input->tapPosition.x;
        float cardSpacing = 160;

        /* Check if tap is in the center card area (start game) */
        float centerCardLeft = centerX - 90;
        float centerCardRight = centerX + 90;

        if (tapX >= centerCardLeft && tapX <= centerCardRight) {
            /* Tapped center card - start game */
            InitGameMode((GameMode)g_selectedMode);

            g_cursorX = BOARD_WIDTH / 2;
            g_cursorY = BOARD_HEIGHT / 2;
            g_twistCursorX = BOARD_WIDTH / 2 - 1;
            g_twistCursorY = BOARD_HEIGHT / 2 - 1;
            if (g_twistCursorX < 0) g_twistCursorX = 0;
            if (g_twistCursorY < 0) g_twistCursorY = 0;

            g_displayScore = 0;
            g_scorePulse = 0;
            g_previousLevel = 1;
            g_lastCascadeLevel = 0;

            g_pluginState = PLUGIN_STATE_PLAYING;
        } else if (tapX < centerCardLeft) {
            /* Tapped left side - navigate to previous mode */
            g_selectedMode = (g_selectedMode + 4) % 5;
            g_carouselTarget = (float)g_selectedMode;
            g_carouselAnimating = true;
        } else if (tapX > centerCardRight) {
            /* Tapped right side - navigate to next mode */
            g_selectedMode = (g_selectedMode + 1) % 5;
            g_carouselTarget = (float)g_selectedMode;
            g_carouselAnimating = true;
        }
    }

    /* Swipe gestures for navigation */
    if (input->swipeLeft) {
        g_selectedMode = (g_selectedMode + 1) % 5;
        g_carouselTarget = (float)g_selectedMode;
        g_carouselAnimating = true;
    }
    if (input->swipeRight) {
        g_selectedMode = (g_selectedMode + 4) % 5;
        g_carouselTarget = (float)g_selectedMode;
        g_carouselAnimating = true;
    }
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

    /* PASS 1: Draw special gem GLOWS (behind gems) */
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            int specialType = GetBoardSpecial(x, y);
            if (specialType == SPECIAL_NONE) continue;

            GemAnimation *anim = GetGemAnimation(x, y);
            float cx, cy;
            GridToScreen(x, y, &cx, &cy);

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

            DrawSpecialGemGlow(specialType, cx, cy, g_cellSize, scale, alpha);
        }
    }

    /* PASS 2: Draw gems (on top of glows) */
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
        GameMode mode = GetCurrentGameMode();

        if (mode == GAME_MODE_TWIST) {
            /* Twist mode: Draw 2x2 selection cursor */
            float cx1, cy1, cx2, cy2;
            GridToScreen(g_twistCursorX, g_twistCursorY, &cx1, &cy1);
            GridToScreen(g_twistCursorX + 1, g_twistCursorY + 1, &cx2, &cy2);

            cx1 += g_shakeOffset.x - g_cellSize / 2.0f;
            cy1 += g_shakeOffset.y - g_cellSize / 2.0f;
            cx2 += g_shakeOffset.x + g_cellSize / 2.0f;
            cy2 += g_shakeOffset.y + g_cellSize / 2.0f;

            float pulse = 1.0f + sinf(g_animTimer * 6.0f) * 0.1f;

            /* Outer glow */
            Color glowColor = {150, 100, 255, (unsigned char)(60 * pulse)};
            DrawRectangleRounded(
                (Rectangle){cx1 - 4, cy1 - 4, (cx2 - cx1) + 8, (cy2 - cy1) + 8},
                0.1f, 8, glowColor);

            /* Selection border */
            Color cursorColor = {150, 100, 255, (unsigned char)(200 * pulse)};
            DrawRectangleLinesEx(
                (Rectangle){cx1, cy1, cx2 - cx1, cy2 - cy1},
                3, cursorColor);

            /* Rotation indicator arrows */
            float arrowSize = g_cellSize * 0.15f;
            float centerX = (cx1 + cx2) / 2.0f;
            float centerY = (cy1 + cy2) / 2.0f;
            Color arrowColor = {200, 180, 255, (unsigned char)(180 * pulse)};

            /* Draw curved arrow hint */
            DrawCircleSector((Vector2){centerX, centerY}, arrowSize * 2.5f, 45, 135, 12, arrowColor);
        } else {
            /* Classic/Blitz mode: Standard single-cell cursor */
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
}

/* ============================================================================
 * HUD RENDERING
 * ============================================================================ */

static void DrawHUD(void) {
    GameMode currentMode = GetCurrentGameMode();

    /* Level overlay - top left */
    Rectangle levelPanel = {12, 12, 100, 44};
    DrawRectangleRounded(levelPanel, 0.2f, 8, COLOR_BOARD_BG);
    DrawRectangleRoundedLines(levelPanel, 0.2f, 8, (Color){60, 65, 80, 255});

    DrawTextEx(g_font, "LVL", (Vector2){levelPanel.x + 10, levelPanel.y + 6}, 14, 1, COLOR_TEXT_MUTED);
    char levelText[32];
    snprintf(levelText, sizeof(levelText), "%d", GetLevel());
    DrawTextEx(g_font, levelText, (Vector2){levelPanel.x + 50, levelPanel.y + 10}, 26, 1, COLOR_TEXT);

    /* Blitz timer - center top (only in Blitz mode) */
    if (currentMode == GAME_MODE_BLITZ) {
        float timeRemaining = GetTimeRemaining();
        Rectangle timerPanel = {g_screenWidth / 2.0f - 60, 8, 120, 52};

        /* Timer background with urgency-based color */
        Color timerBg = COLOR_BOARD_BG;
        Color timerBorder = {60, 65, 80, 255};
        Color timerText = COLOR_TEXT;

        if (timeRemaining <= 10.0f && timeRemaining > 0) {
            /* Urgent - red pulsing */
            float urgencyPulse = (sinf(g_animTimer * 10.0f) + 1.0f) * 0.5f;
            timerBg = (Color){60 + (int)(40 * urgencyPulse), 20, 20, 255};
            timerBorder = (Color){255, 80, 80, 255};
            timerText = (Color){255, (unsigned char)(100 + 155 * urgencyPulse), (unsigned char)(100 + 155 * urgencyPulse), 255};
        } else if (timeRemaining <= 30.0f) {
            /* Warning - yellow */
            timerBorder = (Color){255, 200, 50, 255};
            timerText = (Color){255, 220, 100, 255};
        } else {
            /* Normal - green */
            timerBorder = (Color){50, 200, 100, 255};
            timerText = (Color){100, 255, 150, 255};
        }

        DrawRectangleRounded(timerPanel, 0.2f, 8, timerBg);
        DrawRectangleRoundedLines(timerPanel, 0.2f, 8, timerBorder);

        /* Timer value */
        int seconds = (int)timeRemaining;
        int tenths = (int)((timeRemaining - seconds) * 10);
        char timerStr[32];
        if (timeRemaining <= 10.0f) {
            snprintf(timerStr, sizeof(timerStr), "%d.%d", seconds, tenths);
        } else {
            snprintf(timerStr, sizeof(timerStr), "%d", seconds);
        }

        int timerFontSize = 38;
        Vector2 timerSize = MeasureTextEx(g_accentFont, timerStr, timerFontSize, 1);
        float timerX = timerPanel.x + (timerPanel.width - timerSize.x) / 2.0f;
        DrawTextEx(g_accentFont, timerStr, (Vector2){timerX, timerPanel.y + 6}, timerFontSize, 1, timerText);

        /* "BLITZ" label */
        DrawTextEx(g_font, "BLITZ", (Vector2){timerPanel.x + 38, timerPanel.y + 42}, 10, 1, timerBorder);
    }

    /* Mode indicator - below level panel for non-Blitz modes */
    if (currentMode != GAME_MODE_BLITZ) {
        const char* modeLabel = (currentMode == GAME_MODE_TWIST) ? "TWIST" : "CLASSIC";
        Color modeColor = MODE_COLORS[currentMode];
        modeColor.a = 180;
        DrawTextEx(g_font, modeLabel, (Vector2){levelPanel.x + 10, levelPanel.y + levelPanel.height + 4}, 12, 1, modeColor);
    }

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
        int fontSize = (int)(28 * pulse);

        Vector2 textSize = MeasureTextEx(g_accentFont, cascadeText, fontSize, 1);
        float cx = g_boardX + g_boardSize / 2.0f - textSize.x / 2.0f;
        float cy = g_boardY - 30;

        DrawTextEx(g_accentFont, cascadeText, (Vector2){cx + 2, cy + 2}, fontSize, 1, (Color){0, 0, 0, 180});
        DrawTextEx(g_accentFont, cascadeText, (Vector2){cx, cy}, fontSize, 1, cascadeColor);
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

    /* Update Blitz timer */
    GameMode mode = GetCurrentGameMode();
    if (mode == GAME_MODE_BLITZ && GetGameState() != GAME_STATE_GAME_OVER) {
        UpdateTime(deltaTime);

        /* Check for time-based game over */
        if (GetTimeRemaining() <= 0.0f) {
            SetGameState(GAME_STATE_GAME_OVER);
            TriggerShake(15.0f);
        }
    }

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
                        GameMode mode = GetCurrentGameMode();
                        bool isGameOver = (mode == GAME_MODE_TWIST) ? CheckTwistGameOver() : CheckGameOver();

                        if (isGameOver) {
                            SetGameState(GAME_STATE_GAME_OVER);
                            TriggerShake(15.0f);
                        } else {
                            SetGameState(GAME_STATE_IDLE);
                            ResetCascade();
                        }
                    }
                    break;
                }

                case GAME_STATE_REMOVING: {
                    /* Process any queued special effects before falling */
                    if (ProcessQueuedEffects()) {
                        /* More effects to process - spawn particles and stay in removing */
                        for (int gy = 0; gy < BOARD_HEIGHT; gy++) {
                            for (int gx = 0; gx < BOARD_WIDTH; gx++) {
                                GemAnimation *a = GetGemAnimation(gx, gy);
                                if (a && a->isRemoving) {
                                    SpawnMatchParticles(gx, gy, 1);  /* Effect particles */
                                }
                            }
                        }
                        TriggerShake(8.0f);  /* Extra shake for special effects */
                        /* Stay in removing state to process more effects */
                    } else {
                        /* No more effects - transition to falling */
                        SetGameState(GAME_STATE_FALLING);
                        ApplyGravity();
                    }
                    break;
                }

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

/**
 * Attempt to swap gems, handling Hypercube special swaps.
 * Returns true if a valid swap was made.
 */
static bool TrySmartSwap(int x1, int y1, int x2, int y2) {
    /* Check if either gem is a Hypercube */
    bool hc1 = IsHypercube(x1, y1);
    bool hc2 = IsHypercube(x2, y2);

    if (hc1 || hc2) {
        /* Use Hypercube swap - doesn't require normal match */
        if (hc1) {
            return SwapHypercube(x1, y1, x2, y2);
        } else {
            return SwapHypercube(x2, y2, x1, y1);
        }
    }

    /* Normal swap */
    return SwapGems(x1, y1, x2, y2);
}

static void HandleInput(const LlzInputState *input) {
    if (!input) return;

    if (input->backReleased) {
        /* Return to mode select instead of closing */
        g_pluginState = PLUGIN_STATE_MODE_SELECT;
        /* Reset carousel to current mode position */
        g_carouselPosition = (float)g_selectedMode;
        g_carouselTarget = (float)g_selectedMode;
        return;
    }

    GameState state = GetGameState();
    GameMode mode = GetCurrentGameMode();

    /* Handle game over - restart on select */
    if (state == GAME_STATE_GAME_OVER) {
        if (input->selectPressed) {
            /* Return to mode selection */
            g_pluginState = PLUGIN_STATE_MODE_SELECT;
            /* Reset carousel to current mode position */
            g_carouselPosition = (float)g_selectedMode;
            g_carouselTarget = (float)g_selectedMode;
        }
        return;
    }

    /* Only handle input in idle state */
    if (state != GAME_STATE_IDLE) return;

    /* Reset hint timer on any input */
    g_hintTimer = 0;
    g_showHint = false;

    if (mode == GAME_MODE_TWIST) {
        /* ============= TWIST MODE INPUT ============= */
        /* Navigate 2x2 cursor */
        if (input->upPressed) {
            g_twistCursorX = (g_twistCursorX + BOARD_WIDTH - 2) % (BOARD_WIDTH - 1);
        }
        if (input->downPressed) {
            g_twistCursorX = (g_twistCursorX + 1) % (BOARD_WIDTH - 1);
        }
        if (input->scrollDelta > 0.5f) {
            g_twistCursorY = (g_twistCursorY + 1) % (BOARD_HEIGHT - 1);
        }
        if (input->scrollDelta < -0.5f) {
            g_twistCursorY = (g_twistCursorY + BOARD_HEIGHT - 2) % (BOARD_HEIGHT - 1);
        }

        /* Select button rotates the 2x2 grid */
        if (input->selectPressed) {
            if (RotateGems(g_twistCursorX, g_twistCursorY)) {
                SetGameState(GAME_STATE_SWAPPING);
            } else {
                /* Invalid rotation - visual feedback */
                TriggerShake(3.0f);
            }
        }

        /* Touch input for Twist mode */
        if (input->tap) {
            int gx, gy;
            if (ScreenToGrid(input->tapPosition.x, input->tapPosition.y, &gx, &gy)) {
                /* Position cursor so tapped cell is in the 2x2 */
                g_twistCursorX = gx;
                g_twistCursorY = gy;
                /* Clamp to valid range */
                if (g_twistCursorX >= BOARD_WIDTH - 1) g_twistCursorX = BOARD_WIDTH - 2;
                if (g_twistCursorY >= BOARD_HEIGHT - 1) g_twistCursorY = BOARD_HEIGHT - 2;
            }
        }

        /* Double tap to rotate */
        if (input->doubleTap) {
            int gx, gy;
            if (ScreenToGrid(input->tapPosition.x, input->tapPosition.y, &gx, &gy)) {
                /* Try to rotate at tapped position */
                int rotX = gx;
                int rotY = gy;
                if (rotX >= BOARD_WIDTH - 1) rotX = BOARD_WIDTH - 2;
                if (rotY >= BOARD_HEIGHT - 1) rotY = BOARD_HEIGHT - 2;

                if (RotateGems(rotX, rotY)) {
                    SetGameState(GAME_STATE_SWAPPING);
                    g_twistCursorX = rotX;
                    g_twistCursorY = rotY;
                } else {
                    TriggerShake(3.0f);
                }
            }
        }
    } else {
        /* ============= CLASSIC/BLITZ MODE INPUT ============= */
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
                    if (TrySmartSwap(sel.x, sel.y, g_cursorX, g_cursorY)) {
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
                        if (TrySmartSwap(sel.x, sel.y, gx, gy)) {
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
                if (TrySmartSwap(sel.x, sel.y, targetX, targetY)) {
                    SetGameState(GAME_STATE_SWAPPING);
                } else {
                    TriggerShake(3.0f);
                }
                ClearSelection();
            }
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
        GameMode mode = GetCurrentGameMode();
        if (mode == GAME_MODE_TWIST) {
            /* Twist mode: find a valid rotation */
            if (GetTwistHint(&g_hintX1, &g_hintY1)) {
                g_hintX2 = g_hintX1 + 1;
                g_hintY2 = g_hintY1 + 1;
                g_showHint = true;
            }
        } else {
            /* Classic/Blitz mode: find a valid swap */
            if (GetHint(&g_hintX1, &g_hintY1, &g_hintX2, &g_hintY2)) {
                g_showHint = true;
            }
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

    /* Start in title screen state */
    g_pluginState = PLUGIN_STATE_TITLE_SCREEN;
    g_selectedMode = 0;

    /* Initialize title screen state */
    g_titleTimer = 0.0f;
    g_titlePulse = 0.0f;
    InitTitleGems();

    /* Initialize carousel state */
    g_carouselPosition = 0.0f;
    g_carouselTarget = 0.0f;
    g_carouselAnimating = false;
    for (int i = 0; i < 5; i++) {
        g_modeGlowIntensity[i] = (i == 0) ? 1.0f : 0.0f;
    }

    /* Load display font (Quincy Caps - all uppercase decorative font) */
    g_font = LlzFontGet(LLZ_FONT_DISPLAY, 48);
    if (g_font.texture.id == 0) {
        g_font = GetFontDefault();
    }

    /* Load accent font (Flange Bold - for combos, scores, powerups) */
    g_accentFont = LlzFontGet(LLZ_FONT_ACCENT, 36);
    if (g_accentFont.texture.id == 0) {
        g_accentFont = g_font;  /* Fall back to display font */
    }

    /* Calculate layout */
    CalculateLayout();

    /* Reset state (game will be initialized when mode is selected) */
    g_cursorX = BOARD_WIDTH / 2;
    g_cursorY = BOARD_HEIGHT / 2;
    g_twistCursorX = BOARD_WIDTH / 2 - 1;
    g_twistCursorY = BOARD_HEIGHT / 2 - 1;
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

    /* Always update animation timer for menu */
    g_animTimer += deltaTime;

    if (g_pluginState == PLUGIN_STATE_TITLE_SCREEN) {
        /* Title screen */
        UpdateTitleScreen(input, deltaTime);
    } else if (g_pluginState == PLUGIN_STATE_MODE_SELECT) {
        /* Mode selection menu */
        g_bgGridOffset += deltaTime * 15.0f;  /* Keep background animating */
        UpdateModeCarousel(deltaTime);        /* Update carousel animation */
        HandleModeSelectInput(input);
    } else {
        /* Playing the game */
        /* Update game systems (animTimer already updated above, so subtract to avoid double) */
        g_animTimer -= deltaTime;
        UpdateAnimations(deltaTime);
        UpdateParticles(deltaTime);
        UpdatePopups(deltaTime);
        UpdateShake(deltaTime);
        UpdateLightning(deltaTime);
        UpdateHintSystem(deltaTime);

        /* Handle input */
        HandleInput(input);
    }
}

static void PluginDraw(void) {
    ClearBackground(COLOR_BG);

    if (g_pluginState == PLUGIN_STATE_TITLE_SCREEN) {
        /* Draw title screen */
        DrawTitleScreen();
    } else if (g_pluginState == PLUGIN_STATE_MODE_SELECT) {
        /* Draw mode selection menu */
        DrawModeSelectMenu();
    } else {
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
    .handles_back_button = true,  /* Plugin handles back for mode selection navigation */
    .category = LLZ_CATEGORY_GAMES
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
