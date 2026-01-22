// LLZ Solipskier - A line-drawing ski game
// Inspired by Solipskier - draw snow lines for a skier to ride!
// Designed for CarThing's 800x480 display

#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "llz_sdk_input.h"
#include "llz_sdk_config.h"
#include "raylib.h"
#include "rlgl.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// =============================================================================
// CONSTANTS AND DEFINITIONS
// =============================================================================

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Line segment system
#define MAX_LINE_SEGMENTS 256
#define LINE_SEGMENT_MIN_LENGTH 10.0f
#define LINE_CLEANUP_BEHIND 250.0f

// Gates and obstacles
#define MAX_GATES 24
#define MAX_TUNNELS 6
#define GATE_WIDTH 55.0f
#define GATE_HEIGHT 75.0f

// Particles
#define MAX_PARTICLES 200
#define MAX_TRAIL_POINTS 40

// Physics - tuned for Solipskier feel
#define GRAVITY 720.0f              // Snappier gravity
#define BASE_CAMERA_SPEED 200.0f    // Faster base speed
#define MAX_CAMERA_SPEED 650.0f     // Higher top speed
#define SKIER_SCREEN_X 140.0f       // Skier position on screen
#define SLOPE_SPEED_FACTOR 450.0f   // More responsive to slopes
#define MIN_SKIER_SPEED 150.0f      // Higher minimum speed
#define MAX_SKIER_SPEED 800.0f      // Much faster max
#define LAUNCH_BOOST 0.45f          // Better launches
#define AIR_CONTROL 0.15f           // Slight air control

// Scoring
#define MIN_AIR_TIME_FOR_TRICK 0.2f
#define TRICK_SCORE_PER_SECOND 200
#define SPEED_SCORE_MULTIPLIER 0.12f

// =============================================================================
// COLOR DEFINITIONS - Solipskier aesthetic (white skier, neon accents)
// =============================================================================

#define COLOR_BG_TOP        (Color){8, 12, 25, 255}
#define COLOR_BG_BOTTOM     (Color){18, 28, 50, 255}
#define COLOR_MOUNTAIN1     (Color){25, 35, 55, 200}
#define COLOR_MOUNTAIN2     (Color){35, 45, 70, 160}
#define COLOR_MOUNTAIN3     (Color){45, 55, 85, 120}

// Snow/Line colors - crisp white
#define COLOR_SNOW_LINE     (Color){255, 255, 255, 255}
#define COLOR_SNOW_HIGHLIGHT (Color){255, 255, 255, 255}
#define COLOR_SNOW_SHADOW   (Color){200, 210, 230, 180}
#define COLOR_SNOW_PREVIEW  (Color){255, 255, 255, 100}

// Skier colors - white/bright like original Solipskier
#define COLOR_SKIER_WHITE   (Color){255, 255, 255, 255}
#define COLOR_SKIER_BODY    (Color){240, 245, 255, 255}
#define COLOR_SKIER_OUTLINE (Color){60, 80, 120, 255}
#define COLOR_SKIER_SKIS    (Color){40, 50, 70, 255}
#define COLOR_HEADPHONES    (Color){255, 100, 50, 255}   // Orange headphones
#define COLOR_HEADPHONE_WIRE (Color){255, 80, 40, 200}

// Gate colors - neon vibrant
#define COLOR_GATE_GREEN    (Color){50, 255, 120, 255}
#define COLOR_GATE_RED      (Color){255, 40, 70, 255}
#define COLOR_GATE_GOLD     (Color){255, 220, 50, 255}

// Speed/effect colors
#define COLOR_SPEED_TRAIL   (Color){100, 200, 255, 255}
#define COLOR_TRICK_GLOW    (Color){255, 200, 50, 255}
#define COLOR_BOOST_GLOW    (Color){50, 255, 150, 255}

#define COLOR_TUNNEL_BG     (Color){5, 8, 15, 240}
#define COLOR_TUNNEL_BORDER (Color){40, 55, 85, 255}

#define COLOR_TEXT_PRIMARY  (Color){255, 255, 255, 255}
#define COLOR_TEXT_MUTED    (Color){150, 165, 190, 255}
#define COLOR_TEXT_DIM      (Color){70, 85, 110, 255}
#define COLOR_PANEL         (Color){22, 28, 45, 220}
#define COLOR_ACCENT        (Color){80, 180, 255, 255}
#define COLOR_MULTIPLIER    (Color){255, 195, 70, 255}
#define COLOR_DANGER        (Color){255, 70, 90, 255}

// =============================================================================
// ENUMERATIONS
// =============================================================================

typedef enum {
    GAME_STATE_MENU = 0,
    GAME_STATE_READY,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER
} GameState;

typedef enum {
    SKIER_GROUNDED = 0,
    SKIER_AIRBORNE,
    SKIER_CRASHED
} SkierState;

typedef enum {
    GATE_GREEN = 0,
    GATE_RED,
    GATE_GOLD
} GateType;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

typedef struct {
    Vector2 start;
    Vector2 end;
    float angle;
    float length;
    bool active;
    float creationTime;
} LineSegment;

typedef struct {
    LineSegment segments[MAX_LINE_SEGMENTS];
    int count;
    float currentY;              // Current Y position controlled by scroll
    float targetY;               // Target Y for smooth interpolation
    float lastWorldX;            // Last X position where segment was created
    float lastY;                 // Y value at last segment position
    float scrollVelocity;        // For momentum-based scrolling
} LineSystem;

typedef struct {
    Vector2 worldPos;
    Vector2 velocity;
    float rotation;
    float targetRotation;        // For smooth rotation
    float angularVel;
    SkierState state;
    float airTime;
    float totalAirTime;
    float scaleX;                // For squash/stretch
    float scaleY;
    float speedBoost;
    float boostTimer;
    int currentSegment;
    float segmentT;

    // Solipskier visual state
    bool hasHeadphones;          // Lose at very high speed
    float headphoneWiggle;       // Animation for headphone wire
    float trickRotation;         // Total rotation during current air time
    int trickCount;              // Flips done this jump
    float landingImpact;         // Visual squash on landing
    float speedStretch;          // Stretch based on velocity
    float glowIntensity;         // Glow when boosting/fast

    // Physics helpers
    float groundSpeed;           // Speed along the ground
    float lastGroundAngle;       // For smooth transitions
} Skier;

typedef struct {
    Vector2 worldPos;
    GateType type;
    float width;
    float height;
    bool active;
    bool passed;
    float animTimer;
} Gate;

typedef struct {
    float startX;
    float endX;
    float bottomY;
    bool active;
} Tunnel;

typedef struct {
    Gate gates[MAX_GATES];
    int gateCount;
    Tunnel tunnels[MAX_TUNNELS];
    int tunnelCount;
    float nextGateX;
    float nextTunnelX;
} ObstacleSystem;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float maxLife;
    float size;
    bool active;
} Particle;

typedef struct {
    Vector2 points[MAX_TRAIL_POINTS];
    int head;
    int count;
} SkierTrail;

typedef struct {
    Particle particles[MAX_PARTICLES];
    int activeCount;
    SkierTrail trail;
} ParticleSystem;

typedef struct {
    float worldX;
    float speed;
    float targetSpeed;
    float shakeIntensity;
    Vector2 shakeOffset;
} GameCamera;

typedef struct {
    int score;
    int highScore;
    int distance;
    int multiplier;
    float multiplierTimer;
    int gatesGreen;
    int gatesGold;
    int combo;
    int maxCombo;
    float longestAir;
} ScoreSystem;

typedef struct {
    char text[32];
    Vector2 pos;
    float timer;
    Color color;
    int value;
} ScorePopup;

#define MAX_POPUPS 6

typedef struct {
    float bgTime;
    float readyTimer;
    int menuIndex;
    float menuAnim;
    float screenShake;
    float shakeX;
    float shakeY;
    float crashFlash;
    float speedFlash;
    ScorePopup popups[MAX_POPUPS];
    int popupCount;
    float tunnelDarken;
} AnimState;

typedef struct {
    GameState state;
    Skier skier;
    GameCamera camera;
    LineSystem lines;
    ObstacleSystem obstacles;
    ParticleSystem particles;
    ScoreSystem score;
    AnimState anim;
    float gameTime;
    float difficulty;
    bool canDraw;
    Vector2 touchWorld;
} Game;

// =============================================================================
// GLOBALS
// =============================================================================

static Game g_game;
static int g_screenWidth = SCREEN_WIDTH;
static int g_screenHeight = SCREEN_HEIGHT;
static bool g_wantsClose = false;
static LlzPluginConfig g_config;
static bool g_configInit = false;
static Font g_font;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void GameReset(void);
static void StartNewGame(void);
static void CrashSkier(const char *reason);
static void AddScore(int points, const char *text, Vector2 worldPos);
static void SpawnParticle(Vector2 pos, Vector2 vel, Color color, float life, float size);
static bool IsInTunnel(float worldX);
static Vector2 ScreenToWorld(Vector2 screen);
static Vector2 WorldToScreen(Vector2 world);
static void SaveConfig(void);
static void LoadConfig(void);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static float Clampf(float v, float min, float max) {
    return (v < min) ? min : (v > max) ? max : v;
}

static float Lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static Vector2 ScreenToWorld(Vector2 screen) {
    return (Vector2){screen.x + g_game.camera.worldX, screen.y};
}

static Vector2 WorldToScreen(Vector2 world) {
    return (Vector2){world.x - g_game.camera.worldX, world.y};
}

static float PointToSegmentDist(Vector2 p, Vector2 a, Vector2 b, float *outT) {
    Vector2 ab = {b.x - a.x, b.y - a.y};
    Vector2 ap = {p.x - a.x, p.y - a.y};
    float dot = ap.x * ab.x + ap.y * ab.y;
    float lenSq = ab.x * ab.x + ab.y * ab.y;
    float t = (lenSq > 0) ? Clampf(dot / lenSq, 0.0f, 1.0f) : 0.0f;
    if (outT) *outT = t;
    Vector2 closest = {a.x + ab.x * t, a.y + ab.y * t};
    float dx = p.x - closest.x;
    float dy = p.y - closest.y;
    return sqrtf(dx * dx + dy * dy);
}

// =============================================================================
// PARTICLE SYSTEM
// =============================================================================

static void SpawnParticle(Vector2 pos, Vector2 vel, Color color, float life, float size) {
    ParticleSystem *ps = &g_game.particles;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!ps->particles[i].active) {
            Particle *p = &ps->particles[i];
            p->pos = pos;
            p->vel = vel;
            p->color = color;
            p->life = life;
            p->maxLife = life;
            p->size = size;
            p->active = true;
            ps->activeCount++;
            return;
        }
    }
}

static void SpawnSnowSpray(float speed) {
    if (GetRandomValue(0, 100) > (int)(speed / 4.0f)) return;
    Skier *skier = &g_game.skier;
    Vector2 vel = {-speed * 0.25f + GetRandomValue(-25, 25), GetRandomValue(-60, -20)};
    SpawnParticle(skier->worldPos, vel, COLOR_SNOW_LINE,
                  0.25f + GetRandomValue(0, 20) / 100.0f,
                  2.0f + GetRandomValue(0, 25) / 10.0f);
}

static void SpawnGateParticles(Gate *gate, Color color) {
    for (int i = 0; i < 15; i++) {
        Vector2 pos = {
            gate->worldPos.x + GetRandomValue(-25, 25),
            gate->worldPos.y - gate->height / 2 + GetRandomValue(-35, 35)
        };
        Vector2 vel = {GetRandomValue(-80, 80), GetRandomValue(-120, 40)};
        SpawnParticle(pos, vel, color, 0.4f + GetRandomValue(0, 25) / 100.0f,
                      3.0f + GetRandomValue(0, 25) / 10.0f);
    }
}

static void SpawnCrashParticles(void) {
    Skier *skier = &g_game.skier;
    for (int i = 0; i < 25; i++) {
        Vector2 vel = {GetRandomValue(-150, 150), GetRandomValue(-200, 50)};
        Color c = (i % 3 == 0) ? COLOR_SKIER_BODY : COLOR_SNOW_LINE;
        SpawnParticle(skier->worldPos, vel, c, 0.6f + GetRandomValue(0, 30) / 100.0f,
                      3.0f + GetRandomValue(0, 35) / 10.0f);
    }
}

static void SpawnLandingParticles(void) {
    Skier *skier = &g_game.skier;
    for (int i = 0; i < 10; i++) {
        Vector2 vel = {GetRandomValue(-60, 60), GetRandomValue(-80, -30)};
        SpawnParticle(skier->worldPos, vel, COLOR_SNOW_LINE,
                      0.2f + GetRandomValue(0, 15) / 100.0f,
                      2.0f + GetRandomValue(0, 20) / 10.0f);
    }
}

static void UpdateParticles(float dt) {
    ParticleSystem *ps = &g_game.particles;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &ps->particles[i];
        if (!p->active) continue;

        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->vel.y += 180.0f * dt;
        p->life -= dt;

        if (p->life <= 0) {
            p->active = false;
            ps->activeCount--;
        }
    }
}

static void UpdateSkierTrail(float dt) {
    SkierTrail *trail = &g_game.particles.trail;
    Skier *skier = &g_game.skier;

    static float trailTimer = 0;
    trailTimer += dt;

    if (trailTimer > 0.02f && skier->state != SKIER_CRASHED) {
        trailTimer = 0;
        trail->points[trail->head] = skier->worldPos;
        trail->head = (trail->head + 1) % MAX_TRAIL_POINTS;
        if (trail->count < MAX_TRAIL_POINTS) trail->count++;
    }
}

static void DrawParticles(void) {
    ParticleSystem *ps = &g_game.particles;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &ps->particles[i];
        if (!p->active) continue;

        Vector2 screen = WorldToScreen(p->pos);
        float alpha = p->life / p->maxLife;
        Color c = p->color;
        c.a = (unsigned char)(255 * alpha);
        float sz = p->size * (0.5f + alpha * 0.5f);
        DrawCircleV(screen, sz, c);
    }
}

static void DrawSkierTrail(void) {
    SkierTrail *trail = &g_game.particles.trail;
    if (trail->count < 2) return;

    for (int i = 0; i < trail->count - 1; i++) {
        int idx = (trail->head - trail->count + i + MAX_TRAIL_POINTS) % MAX_TRAIL_POINTS;
        int nextIdx = (idx + 1) % MAX_TRAIL_POINTS;

        Vector2 p1 = WorldToScreen(trail->points[idx]);
        Vector2 p2 = WorldToScreen(trail->points[nextIdx]);

        float alpha = (float)i / trail->count * 0.4f;
        Color c = COLOR_SNOW_LINE;
        c.a = (unsigned char)(255 * alpha);

        DrawLineEx(p1, p2, 2.0f, c);
    }
}

// =============================================================================
// LINE SYSTEM
// =============================================================================

static void CreateLineSegment(Vector2 start, Vector2 end) {
    LineSystem *lines = &g_game.lines;

    int index = -1;
    for (int i = 0; i < MAX_LINE_SEGMENTS; i++) {
        if (!lines->segments[i].active) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        // Find oldest and overwrite
        float oldest = g_game.gameTime;
        for (int i = 0; i < MAX_LINE_SEGMENTS; i++) {
            if (lines->segments[i].creationTime < oldest) {
                oldest = lines->segments[i].creationTime;
                index = i;
            }
        }
    }
    if (index < 0) index = 0;

    LineSegment *seg = &lines->segments[index];
    seg->start = start;
    seg->end = end;
    seg->active = true;
    seg->creationTime = g_game.gameTime;

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    seg->angle = atan2f(dy, dx) * RAD2DEG;
    seg->length = sqrtf(dx * dx + dy * dy);

    lines->count++;
}

static void UpdateLineDrawing(const LlzInputState *input, float dt) {
    LineSystem *lines = &g_game.lines;

    // Scroll wheel controls the Y position of the snow line
    // Scroll up = line goes up (negative Y), scroll down = line goes down
    float scrollSensitivity = 35.0f;  // How much each scroll unit moves the line
    float scrollInput = input->scrollDelta;

    // Add scroll velocity (with momentum)
    lines->scrollVelocity += scrollInput * scrollSensitivity * 8.0f;

    // Apply velocity to target Y
    lines->targetY += lines->scrollVelocity * dt;

    // Dampen velocity for momentum feel
    lines->scrollVelocity *= powf(0.92f, dt * 60.0f);

    // Also allow direct drag control as alternative
    if (input->dragActive) {
        lines->targetY += input->dragDelta.y * 1.5f;
        lines->scrollVelocity = 0;  // Cancel momentum during drag
    }

    // Clamp Y to screen bounds with padding
    float minY = 60.0f;
    float maxY = g_screenHeight - 40.0f;
    lines->targetY = Clampf(lines->targetY, minY, maxY);

    // Smooth interpolation to target
    float smoothSpeed = 12.0f;
    lines->currentY = Lerpf(lines->currentY, lines->targetY, dt * smoothSpeed);

    // Check if we can draw (not in tunnel)
    float drawX = g_game.camera.worldX + SKIER_SCREEN_X + 80.0f;  // Draw ahead of skier
    g_game.canDraw = !IsInTunnel(drawX);

    // Create line segments as the camera moves forward
    float segmentSpacing = 15.0f;  // Distance between segment points

    if (g_game.canDraw) {
        while (drawX > lines->lastWorldX + segmentSpacing) {
            float nextX = lines->lastWorldX + segmentSpacing;

            Vector2 start = {lines->lastWorldX, lines->lastY};
            Vector2 end = {nextX, lines->currentY};

            // Only create if we have a valid last position
            if (lines->lastWorldX > 0) {
                CreateLineSegment(start, end);
            }

            lines->lastWorldX = nextX;
            lines->lastY = lines->currentY;  // Update lastY for next segment
        }
    } else {
        // In tunnel - just advance position without drawing
        if (drawX > lines->lastWorldX) {
            lines->lastWorldX = drawX;
            lines->lastY = lines->currentY;  // Keep Y in sync
        }
    }

    // Cleanup old segments
    float cleanupX = g_game.camera.worldX - LINE_CLEANUP_BEHIND;
    for (int i = 0; i < MAX_LINE_SEGMENTS; i++) {
        LineSegment *seg = &lines->segments[i];
        if (seg->active && seg->end.x < cleanupX) {
            seg->active = false;
            lines->count--;
        }
    }
}

static void DrawLines(void) {
    LineSystem *lines = &g_game.lines;

    for (int i = 0; i < MAX_LINE_SEGMENTS; i++) {
        LineSegment *seg = &lines->segments[i];
        if (!seg->active) continue;

        Vector2 start = WorldToScreen(seg->start);
        Vector2 end = WorldToScreen(seg->end);

        // Skip if off screen
        if (end.x < -50 || start.x > g_screenWidth + 50) continue;

        // Fade in
        float age = g_game.gameTime - seg->creationTime;
        float alpha = Clampf(age * 6.0f, 0.0f, 1.0f);

        // Main line
        Color snow = COLOR_SNOW_LINE;
        snow.a = (unsigned char)(255 * alpha);
        DrawLineEx(start, end, 7.0f, snow);

        // Highlight
        Vector2 off = {0, -2};
        Color hi = COLOR_SNOW_HIGHLIGHT;
        hi.a = (unsigned char)(180 * alpha);
        DrawLineEx((Vector2){start.x + off.x, start.y + off.y},
                   (Vector2){end.x + off.x, end.y + off.y}, 2.5f, hi);

        // Shadow
        off = (Vector2){0, 3};
        Color sh = COLOR_SNOW_SHADOW;
        sh.a = (unsigned char)(100 * alpha);
        DrawLineEx((Vector2){start.x + off.x, start.y + off.y},
                   (Vector2){end.x + off.x, end.y + off.y}, 4.0f, sh);
    }

    // Draw cursor/preview showing where line will be drawn
    if (g_game.canDraw && g_game.state == GAME_STATE_PLAYING) {
        float cursorX = SKIER_SCREEN_X + 80.0f;  // Ahead of skier
        float cursorY = lines->currentY;

        // Draw line from last segment to cursor
        Vector2 lastPoint = WorldToScreen((Vector2){lines->lastWorldX, lines->lastY});
        Vector2 cursor = {cursorX, cursorY};

        // Preview line
        if (lastPoint.x < cursorX) {
            DrawLineEx(lastPoint, cursor, 4.0f, COLOR_SNOW_PREVIEW);
        }

        // Cursor indicator (circle with crosshair)
        float pulse = sinf(g_game.anim.bgTime * 6.0f) * 0.2f + 0.8f;
        Color cursorColor = COLOR_ACCENT;
        cursorColor.a = (unsigned char)(200 * pulse);
        DrawCircleLines((int)cursorX, (int)cursorY, 12, cursorColor);
        DrawLine((int)cursorX - 18, (int)cursorY, (int)cursorX + 18, (int)cursorY, cursorColor);
        DrawLine((int)cursorX, (int)cursorY - 18, (int)cursorX, (int)cursorY + 18, cursorColor);

        // Small arrow indicators showing scroll direction
        Color arrowColor = COLOR_TEXT_MUTED;
        arrowColor.a = 120;
        DrawTriangle(
            (Vector2){cursorX - 25, cursorY - 25},
            (Vector2){cursorX - 20, cursorY - 35},
            (Vector2){cursorX - 15, cursorY - 25},
            arrowColor
        );
        DrawTriangle(
            (Vector2){cursorX - 25, cursorY + 25},
            (Vector2){cursorX - 15, cursorY + 25},
            (Vector2){cursorX - 20, cursorY + 35},
            arrowColor
        );
    }

    // Show "NO DRAW" indicator in tunnels
    if (!g_game.canDraw && g_game.state == GAME_STATE_PLAYING) {
        float cursorX = SKIER_SCREEN_X + 80.0f;
        float flash = sinf(g_game.anim.bgTime * 8.0f) * 0.3f + 0.7f;
        Color noDrawColor = COLOR_DANGER;
        noDrawColor.a = (unsigned char)(180 * flash);
        DrawCircle((int)cursorX, (int)lines->currentY, 15, noDrawColor);
        DrawLine((int)cursorX - 10, (int)lines->currentY - 10,
                 (int)cursorX + 10, (int)lines->currentY + 10, WHITE);
        DrawLine((int)cursorX - 10, (int)lines->currentY + 10,
                 (int)cursorX + 10, (int)lines->currentY - 10, WHITE);
    }
}

// =============================================================================
// OBSTACLES
// =============================================================================

static bool IsInTunnel(float worldX) {
    for (int i = 0; i < MAX_TUNNELS; i++) {
        Tunnel *t = &g_game.obstacles.tunnels[i];
        if (t->active && worldX >= t->startX && worldX <= t->endX) {
            return true;
        }
    }
    return false;
}

static Tunnel *GetCurrentTunnel(float worldX) {
    for (int i = 0; i < MAX_TUNNELS; i++) {
        Tunnel *t = &g_game.obstacles.tunnels[i];
        if (t->active && worldX >= t->startX && worldX <= t->endX) {
            return t;
        }
    }
    return NULL;
}

static void SpawnGate(float worldX) {
    ObstacleSystem *obs = &g_game.obstacles;

    int index = -1;
    for (int i = 0; i < MAX_GATES; i++) {
        if (!obs->gates[i].active) {
            index = i;
            break;
        }
    }
    if (index < 0) return;

    Gate *gate = &obs->gates[index];

    // Determine type
    int roll = GetRandomValue(0, 100);
    if (g_game.difficulty > 0.4f && roll < (int)(15 + g_game.difficulty * 25)) {
        gate->type = GATE_RED;
    } else if (roll > 96) {
        gate->type = GATE_GOLD;
    } else {
        gate->type = GATE_GREEN;
    }

    // Position - red gates are lower
    float baseY = g_screenHeight * 0.45f;
    if (gate->type == GATE_RED) {
        baseY = g_screenHeight * 0.6f;
    }
    baseY += GetRandomValue(-70, 70);

    gate->worldPos = (Vector2){worldX, baseY};
    gate->width = GATE_WIDTH;
    gate->height = GATE_HEIGHT;
    gate->active = true;
    gate->passed = false;
    gate->animTimer = 0;

    obs->gateCount++;
}

static void SpawnTunnel(float startX) {
    ObstacleSystem *obs = &g_game.obstacles;

    int index = -1;
    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (!obs->tunnels[i].active) {
            index = i;
            break;
        }
    }
    if (index < 0) return;

    Tunnel *t = &obs->tunnels[index];
    t->startX = startX;
    t->endX = startX + GetRandomValue(180, 350);
    t->bottomY = g_screenHeight * 0.35f + GetRandomValue(0, 80);
    t->active = true;

    obs->tunnelCount++;
}

static void HandleGatePass(Gate *gate) {
    Skier *skier = &g_game.skier;
    gate->passed = true;

    switch (gate->type) {
        case GATE_GREEN:
            if (skier->state == SKIER_AIRBORNE) {
                int bonus = 500 * g_game.score.multiplier;
                AddScore(bonus, "AIRBORNE!", skier->worldPos);
                g_game.score.multiplier = (g_game.score.multiplier < 8) ?
                    g_game.score.multiplier + 1 : 8;
                g_game.score.multiplierTimer = 5.0f;
                skier->speedBoost = 1.4f;
                skier->boostTimer = 2.0f;
                g_game.score.gatesGreen++;
            } else {
                AddScore(100, "PASS", skier->worldPos);
            }
            SpawnGateParticles(gate, COLOR_GATE_GREEN);
            break;

        case GATE_RED:
            CrashSkier("Red gate!");
            SpawnGateParticles(gate, COLOR_GATE_RED);
            g_game.anim.crashFlash = 0.35f;
            break;

        case GATE_GOLD:
            {
                int goldBonus = 2000 * g_game.score.multiplier;
                AddScore(goldBonus, "GOLD!", skier->worldPos);
                g_game.score.multiplier = (g_game.score.multiplier < 8) ?
                    g_game.score.multiplier + 2 : 8;
                g_game.score.multiplierTimer = 7.0f;
                skier->speedBoost = 1.8f;
                skier->boostTimer = 3.0f;
                g_game.score.gatesGold++;
                SpawnGateParticles(gate, COLOR_GATE_GOLD);
                g_game.anim.screenShake = 0.15f;
            }
            break;
    }
}

static void UpdateObstacles(float dt) {
    ObstacleSystem *obs = &g_game.obstacles;
    Skier *skier = &g_game.skier;

    // Spawn gates ahead
    float spawnX = g_game.camera.worldX + g_screenWidth + 80;
    while (obs->nextGateX < spawnX) {
        // Don't spawn in tunnels
        if (!IsInTunnel(obs->nextGateX)) {
            SpawnGate(obs->nextGateX);
        }
        float interval = 280.0f - g_game.difficulty * 80.0f;
        interval = Clampf(interval, 160.0f, 280.0f);
        interval += GetRandomValue(-40, 40);
        obs->nextGateX += interval;
    }

    // Spawn tunnels occasionally
    if (obs->nextTunnelX < spawnX && GetRandomValue(0, 100) < 8) {
        SpawnTunnel(obs->nextTunnelX);
        obs->nextTunnelX += GetRandomValue(700, 1200);
    }

    // Update gates
    for (int i = 0; i < MAX_GATES; i++) {
        Gate *gate = &obs->gates[i];
        if (!gate->active) continue;

        gate->animTimer += dt;

        // Check collision
        if (!gate->passed && skier->state != SKIER_CRASHED) {
            float dx = skier->worldPos.x - gate->worldPos.x;
            if (fabsf(dx) < gate->width / 2) {
                float dy = skier->worldPos.y - gate->worldPos.y;
                if (dy > -gate->height && dy < 15) {
                    HandleGatePass(gate);
                }
            }
            if (skier->worldPos.x > gate->worldPos.x + gate->width) {
                gate->passed = true;
            }
        }

        // Cleanup
        if (gate->worldPos.x < g_game.camera.worldX - 100) {
            gate->active = false;
            obs->gateCount--;
        }
    }

    // Cleanup tunnels
    for (int i = 0; i < MAX_TUNNELS; i++) {
        Tunnel *t = &obs->tunnels[i];
        if (t->active && t->endX < g_game.camera.worldX - 100) {
            t->active = false;
            obs->tunnelCount--;
        }
    }

    // Update tunnel darkening
    Tunnel *current = GetCurrentTunnel(skier->worldPos.x);
    g_game.anim.tunnelDarken = Lerpf(g_game.anim.tunnelDarken,
                                      current ? 0.65f : 0.0f, dt * 6.0f);
}

static void DrawGates(void) {
    ObstacleSystem *obs = &g_game.obstacles;

    for (int i = 0; i < MAX_GATES; i++) {
        Gate *gate = &obs->gates[i];
        if (!gate->active) continue;

        Vector2 screen = WorldToScreen(gate->worldPos);
        if (screen.x < -60 || screen.x > g_screenWidth + 60) continue;

        Color color;
        switch (gate->type) {
            case GATE_GREEN: color = COLOR_GATE_GREEN; break;
            case GATE_RED: color = COLOR_GATE_RED; break;
            case GATE_GOLD: color = COLOR_GATE_GOLD; break;
        }

        // Pulsing
        float pulse = sinf(gate->animTimer * 4.0f) * 0.15f + 0.85f;
        color.r = (unsigned char)(color.r * pulse);
        color.g = (unsigned char)(color.g * pulse);
        color.b = (unsigned char)(color.b * pulse);

        // Draw gate posts
        float hw = gate->width / 2;
        float h = gate->height;

        // Left post
        DrawRectangle((int)(screen.x - hw - 6), (int)(screen.y - h), 6, (int)h, color);
        // Right post
        DrawRectangle((int)(screen.x + hw), (int)(screen.y - h), 6, (int)h, color);
        // Top bar
        DrawRectangle((int)(screen.x - hw - 6), (int)(screen.y - h - 6),
                      (int)(gate->width + 12), 6, color);

        // Glow effect
        Color glow = color;
        glow.a = 60;
        DrawCircleV((Vector2){screen.x, screen.y - h / 2}, hw + 15, glow);
    }
}

static void DrawTunnels(void) {
    ObstacleSystem *obs = &g_game.obstacles;

    for (int i = 0; i < MAX_TUNNELS; i++) {
        Tunnel *t = &obs->tunnels[i];
        if (!t->active) continue;

        float startScreen = t->startX - g_game.camera.worldX;
        float endScreen = t->endX - g_game.camera.worldX;

        if (endScreen < 0 || startScreen > g_screenWidth) continue;

        float width = endScreen - startScreen;

        // Ceiling
        DrawRectangle((int)startScreen, 0, (int)width, (int)t->bottomY, COLOR_TUNNEL_BG);
        // Border
        DrawRectangle((int)startScreen, (int)t->bottomY - 4, (int)width, 4, COLOR_TUNNEL_BORDER);

        // Entry/exit highlights
        DrawRectangle((int)startScreen - 2, 0, 4, (int)t->bottomY, COLOR_TUNNEL_BORDER);
        DrawRectangle((int)endScreen - 2, 0, 4, (int)t->bottomY, COLOR_TUNNEL_BORDER);
    }
}

// =============================================================================
// SKIER PHYSICS
// =============================================================================

static int FindNearestSegment(void) {
    Skier *skier = &g_game.skier;
    LineSystem *lines = &g_game.lines;

    int best = -1;
    float bestDist = 1000.0f;

    for (int i = 0; i < MAX_LINE_SEGMENTS; i++) {
        LineSegment *seg = &lines->segments[i];
        if (!seg->active) continue;

        // Only check segments in range
        if (seg->end.x < skier->worldPos.x - 30) continue;
        if (seg->start.x > skier->worldPos.x + 30) continue;

        float t;
        float dist = PointToSegmentDist(skier->worldPos, seg->start, seg->end, &t);

        // Must be close and below skier
        Vector2 point = {
            seg->start.x + (seg->end.x - seg->start.x) * t,
            seg->start.y + (seg->end.y - seg->start.y) * t
        };

        if (dist < bestDist && point.y >= skier->worldPos.y - 20) {
            bestDist = dist;
            best = i;
        }
    }

    return best;
}

static void LaunchSkier(void) {
    Skier *skier = &g_game.skier;

    float groundAngle = skier->lastGroundAngle;
    float launchAngle = groundAngle * DEG2RAD;
    float speed = skier->groundSpeed > 0 ? skier->groundSpeed : skier->velocity.x;

    // === VELOCITY CALCULATION ===
    // Convert ground speed to launch velocity along the angle
    // Preserve more speed for satisfying launches
    float speedPreservation = 0.92f;  // Keep most of the speed

    // Going downhill (negative angle) = more horizontal speed
    // Going uphill (positive angle) = more vertical component
    if (groundAngle < -10.0f) {
        // Steep downhill = pop up with good speed
        skier->velocity.x = speed * speedPreservation;
        skier->velocity.y = sinf(launchAngle) * speed * 0.6f;
        // Add upward pop for jumps off downhills
        skier->velocity.y -= fabsf(speed) * LAUNCH_BOOST * 0.8f;
    } else if (groundAngle > 15.0f) {
        // Uphill ramp = launch upward
        float rampBoost = Clampf((groundAngle - 15.0f) / 30.0f, 0.0f, 1.0f);
        skier->velocity.x = cosf(launchAngle) * speed * speedPreservation;
        skier->velocity.y = sinf(launchAngle) * speed * speedPreservation;
        // Extra upward boost from ramps
        skier->velocity.y -= speed * LAUNCH_BOOST * (1.0f + rampBoost);
    } else {
        // Relatively flat - standard launch
        skier->velocity.x = cosf(launchAngle) * speed * speedPreservation;
        skier->velocity.y = sinf(launchAngle) * speed * 0.5f;
        // Small upward boost if going fast
        if (speed > 250.0f) {
            skier->velocity.y -= speed * LAUNCH_BOOST * 0.5f;
        }
    }

    // === STATE TRANSITION ===
    skier->state = SKIER_AIRBORNE;
    skier->currentSegment = -1;

    // === RESET TRICK TRACKING ===
    skier->airTime = 0.0f;
    skier->trickRotation = 0.0f;
    skier->trickCount = 0;

    // === ANGULAR VELOCITY ===
    // Spin based on ground angle and speed - steeper = more spin potential
    float spinFactor = Clampf(speed / MAX_SKIER_SPEED, 0.3f, 1.0f);
    skier->angularVel = groundAngle * spinFactor * 2.0f;

    // Add extra spin if launching off steep slopes at high speed
    if (fabsf(groundAngle) > 20.0f && speed > 400.0f) {
        skier->angularVel += (groundAngle > 0 ? -1 : 1) * 60.0f;
    }

    // === VISUAL EFFECTS ===
    // Slight vertical stretch on launch
    skier->scaleY = 1.15f;
    skier->scaleX = 0.9f;

    // Glow boost for high-speed launches
    if (speed > 350.0f) {
        skier->glowIntensity = 0.5f + (speed - 350.0f) / 400.0f;
    }
}

static void LandSkier(int segIndex, float t) {
    Skier *skier = &g_game.skier;
    LineSegment *seg = &g_game.lines.segments[segIndex];

    // === CALCULATE LANDING QUALITY ===
    float landAngle = skier->rotation - seg->angle;
    while (landAngle > 180) landAngle -= 360;
    while (landAngle < -180) landAngle += 360;

    float quality = 1.0f - fabsf(landAngle) / 90.0f;
    quality = Clampf(quality, 0.0f, 1.0f);

    // Bad landing = crash
    if (quality < 0.25f) {
        CrashSkier("Bad landing!");
        return;
    }

    // === TRICK SCORING (before resetting) ===
    int totalTrickScore = 0;

    // Score for tricks (flips)
    if (skier->trickCount > 0) {
        int trickScore = skier->trickCount * 500 * g_game.score.multiplier;
        const char *trickText = skier->trickCount == 1 ? "FLIP!" :
                                skier->trickCount == 2 ? "DOUBLE!" :
                                skier->trickCount >= 3 ? "INSANE!" : "TRICK!";
        AddScore(trickScore, trickText, skier->worldPos);
        totalTrickScore += trickScore;

        // Big boost for multiple flips
        if (skier->trickCount >= 2) {
            skier->speedBoost = 1.5f + skier->trickCount * 0.2f;
            skier->boostTimer = 2.0f + skier->trickCount * 0.5f;
            skier->glowIntensity = 1.0f;
        }
    }

    // Score for air time
    if (skier->airTime >= MIN_AIR_TIME_FOR_TRICK) {
        int airScore = (int)(skier->airTime * TRICK_SCORE_PER_SECOND * g_game.score.multiplier);
        // Bonus for landing quality
        airScore = (int)(airScore * (0.5f + quality * 0.5f));
        AddScore(airScore, "AIR TIME", skier->worldPos);
        totalTrickScore += airScore;
        skier->totalAirTime += skier->airTime;

        if (skier->airTime > g_game.score.longestAir) {
            g_game.score.longestAir = skier->airTime;
        }
    }

    // Perfect landing bonus
    if (quality > 0.9f && skier->airTime > 0.5f) {
        int perfectBonus = 200 * g_game.score.multiplier;
        AddScore(perfectBonus, "PERFECT!", skier->worldPos);
        skier->glowIntensity = 0.8f;
    }

    // === STATE TRANSITION ===
    skier->state = SKIER_GROUNDED;
    skier->currentSegment = segIndex;
    skier->segmentT = t;
    skier->worldPos.x = seg->start.x + (seg->end.x - seg->start.x) * t;
    skier->worldPos.y = seg->start.y + (seg->end.y - seg->start.y) * t;
    skier->rotation = seg->angle;
    skier->angularVel = 0;
    skier->lastGroundAngle = seg->angle;

    // Reset air state
    skier->airTime = 0;
    skier->trickRotation = 0;
    skier->trickCount = 0;

    // === VELOCITY PRESERVATION ===
    float speed = sqrtf(skier->velocity.x * skier->velocity.x +
                        skier->velocity.y * skier->velocity.y);

    // Better landings preserve more speed
    float speedPreservation = 0.7f + quality * 0.25f;

    // Landing on downhill = speed boost
    if (seg->angle < -5.0f) {
        speedPreservation += fabsf(seg->angle) * 0.005f;
    }

    skier->velocity.x = speed * speedPreservation;
    skier->velocity.y = 0;
    skier->groundSpeed = skier->velocity.x;

    // === VISUAL EFFECTS ===
    // Landing squash - more squash for harder landings
    float impactStrength = 1.0f - quality * 0.4f;
    skier->scaleY = 0.55f + quality * 0.15f;  // Range: 0.55 to 0.70
    skier->scaleX = 1.2f - quality * 0.1f;    // Range: 1.10 to 1.20
    skier->landingImpact = impactStrength;

    // Particles based on landing intensity
    SpawnLandingParticles();
    if (totalTrickScore > 500) {
        // Extra particles for big tricks
        for (int i = 0; i < 5; i++) {
            SpawnLandingParticles();
        }
    }

    // === COMBO SYSTEM ===
    if (quality >= 0.65f) {
        g_game.score.combo++;
        if (g_game.score.combo > g_game.score.maxCombo) {
            g_game.score.maxCombo = g_game.score.combo;
        }
        // Combo multiplier boost
        if (g_game.score.combo >= 3 && g_game.score.multiplier < 8) {
            g_game.score.multiplier++;
            g_game.score.multiplierTimer = 4.0f;
        }
    } else {
        g_game.score.combo = 0;
    }

    // Screen shake based on landing impact
    g_game.anim.screenShake = (1.0f - quality) * 0.2f + (totalTrickScore > 1000 ? 0.1f : 0);
}

static void CheckGroundCollision(void) {
    Skier *skier = &g_game.skier;
    LineSystem *lines = &g_game.lines;

    for (int i = 0; i < MAX_LINE_SEGMENTS; i++) {
        LineSegment *seg = &lines->segments[i];
        if (!seg->active) continue;

        if (seg->end.x < skier->worldPos.x - 25) continue;
        if (seg->start.x > skier->worldPos.x + 25) continue;

        float t;
        float dist = PointToSegmentDist(skier->worldPos, seg->start, seg->end, &t);

        if (dist < 18.0f && skier->velocity.y > 0) {
            Vector2 point = {
                seg->start.x + (seg->end.x - seg->start.x) * t,
                seg->start.y + (seg->end.y - seg->start.y) * t
            };
            if (skier->worldPos.y < point.y + 12.0f) {
                LandSkier(i, t);
                return;
            }
        }
    }
}

static void UpdateGroundedSkier(float dt) {
    Skier *skier = &g_game.skier;

    if (skier->currentSegment < 0 ||
        !g_game.lines.segments[skier->currentSegment].active) {
        LaunchSkier();
        return;
    }

    LineSegment *seg = &g_game.lines.segments[skier->currentSegment];

    // === IMPROVED SLOPE PHYSICS ===
    float slopeAngle = seg->angle;
    float slopeRad = slopeAngle * DEG2RAD;

    // Gravity component along slope (stronger effect)
    float gravityAlongSlope = -sinf(slopeRad) * SLOPE_SPEED_FACTOR;

    // Steeper slopes = more acceleration
    float slopeSteepness = fabsf(sinf(slopeRad));
    float slopeMultiplier = 1.0f + slopeSteepness * 0.5f;

    skier->velocity.x += gravityAlongSlope * slopeMultiplier * dt;

    // Less friction on steeper downhills (more speed)
    float frictionFactor = 0.997f - slopeSteepness * 0.003f;
    if (slopeAngle > 0) frictionFactor = 0.992f;  // More friction on uphills
    skier->velocity.x *= powf(frictionFactor, dt * 60.0f);

    // Apply boost (more responsive)
    if (skier->boostTimer > 0 && skier->speedBoost > 1.0f) {
        float boostAccel = (skier->speedBoost - 1.0f) * 150.0f;
        skier->velocity.x += boostAccel * dt;
    }

    // Store ground speed
    skier->groundSpeed = skier->velocity.x;

    // Clamp speed
    skier->velocity.x = Clampf(skier->velocity.x, MIN_SKIER_SPEED, MAX_SKIER_SPEED);

    // Check if going too fast - might lose headphones!
    if (skier->velocity.x > MAX_SKIER_SPEED * 0.9f && skier->hasHeadphones) {
        // Small chance to lose headphones at extreme speed
        if (GetRandomValue(0, 1000) < 2) {
            skier->hasHeadphones = false;
            // Could add particle effect here
        }
    }

    // Move along segment
    float move = skier->velocity.x * dt;
    skier->segmentT += move / seg->length;

    // Check end of segment - find next segment to continue
    while (skier->segmentT >= 1.0f) {
        skier->segmentT -= 1.0f;

        int nextSeg = FindNearestSegment();
        if (nextSeg < 0 || nextSeg == skier->currentSegment) {
            LaunchSkier();
            return;
        }

        // Check angle change for smooth transitions
        float prevAngle = seg->angle;
        skier->currentSegment = nextSeg;
        seg = &g_game.lines.segments[nextSeg];

        // Big angle changes should launch
        float angleDiff = fabsf(seg->angle - prevAngle);
        if (angleDiff > 35.0f && prevAngle < seg->angle) {
            // Hit a ramp!
            LaunchSkier();
            return;
        }
    }

    // Update position on segment
    skier->worldPos.x = seg->start.x + (seg->end.x - seg->start.x) * skier->segmentT;
    skier->worldPos.y = seg->start.y + (seg->end.y - seg->start.y) * skier->segmentT;

    // Smooth rotation towards ground angle
    skier->targetRotation = seg->angle;
    skier->rotation = Lerpf(skier->rotation, skier->targetRotation, dt * 15.0f);
    skier->lastGroundAngle = seg->angle;

    // Headphone wire wiggle based on speed
    skier->headphoneWiggle += skier->velocity.x * dt * 0.01f;

    // Spawn snow spray (more at higher speeds)
    SpawnSnowSpray(skier->velocity.x);
}

static void UpdateAirborneSkier(float dt) {
    Skier *skier = &g_game.skier;

    // === GRAVITY with realistic arc ===
    skier->velocity.y += GRAVITY * dt;

    // === AIR DRAG (less on horizontal for speed preservation) ===
    skier->velocity.x *= powf(0.992f, dt * 60.0f);
    skier->velocity.y *= powf(0.985f, dt * 60.0f);

    // === POSITION UPDATE ===
    skier->worldPos.x += skier->velocity.x * dt;
    skier->worldPos.y += skier->velocity.y * dt;

    // === ROTATION AND TRICKS ===
    float prevRotation = skier->rotation;
    skier->rotation += skier->angularVel * dt;

    // Track cumulative trick rotation
    float rotationDelta = skier->rotation - prevRotation;
    skier->trickRotation += rotationDelta;

    // Count full flips (360 degrees = 1 trick)
    int newTrickCount = (int)(fabsf(skier->trickRotation) / 360.0f);
    if (newTrickCount > skier->trickCount) {
        skier->trickCount = newTrickCount;
        // Bonus for tricks!
        int trickBonus = 300 * g_game.score.multiplier * skier->trickCount;
        AddScore(trickBonus, "FLIP!", skier->worldPos);
        skier->glowIntensity = 1.0f;  // Flash on trick completion
    }

    // Angular velocity damping (natural air rotation slowing)
    skier->angularVel *= powf(0.97f, dt * 60.0f);

    // Natural rotation towards velocity direction when falling
    float velocityAngle = atan2f(skier->velocity.y, skier->velocity.x) * RAD2DEG;
    float angleDiff = velocityAngle - skier->rotation;
    while (angleDiff > 180) angleDiff -= 360;
    while (angleDiff < -180) angleDiff += 360;

    // Gentle pull towards velocity direction (air physics)
    skier->angularVel += angleDiff * AIR_CONTROL * dt * 60.0f;

    // === AIR TIME ===
    skier->airTime += dt;

    // === VISUAL EFFECTS ===
    // Speed stretch based on velocity magnitude
    float speed = sqrtf(skier->velocity.x * skier->velocity.x + skier->velocity.y * skier->velocity.y);
    skier->speedStretch = 1.0f + Clampf(speed / MAX_SKIER_SPEED, 0.0f, 1.0f) * 0.3f;

    // Glow intensity fades
    skier->glowIntensity *= powf(0.92f, dt * 60.0f);

    // Headphone wire wiggle in air (faster when spinning)
    skier->headphoneWiggle += (fabsf(skier->angularVel) * 0.002f + speed * 0.005f) * dt;

    // Build up glow during extended air time
    if (skier->airTime > 0.8f) {
        skier->glowIntensity = fmaxf(skier->glowIntensity, (skier->airTime - 0.8f) * 0.3f);
    }

    // === COLLISION CHECKS ===
    CheckGroundCollision();

    // Check death (fell off screen)
    if (skier->worldPos.y > g_screenHeight + 60) {
        CrashSkier("Fell off!");
    }

    // Check tunnel ceiling collision when airborne
    Tunnel *tunnel = GetCurrentTunnel(skier->worldPos.x);
    if (tunnel && skier->worldPos.y < tunnel->bottomY + 15) {
        CrashSkier("Hit tunnel ceiling!");
    }
}

static void UpdateCrashedSkier(float dt) {
    Skier *skier = &g_game.skier;

    skier->velocity.y += GRAVITY * 0.7f * dt;
    skier->velocity.x *= 0.96f;
    skier->worldPos.x += skier->velocity.x * dt;
    skier->worldPos.y += skier->velocity.y * dt;
    skier->rotation += skier->angularVel * dt;
}

static void UpdateSkier(float dt) {
    Skier *skier = &g_game.skier;

    // Boost timer
    if (skier->boostTimer > 0) {
        skier->boostTimer -= dt;
        if (skier->boostTimer <= 0) {
            skier->speedBoost = 1.0f;
        }
    }

    switch (skier->state) {
        case SKIER_GROUNDED:
            UpdateGroundedSkier(dt);
            break;
        case SKIER_AIRBORNE:
            UpdateAirborneSkier(dt);
            break;
        case SKIER_CRASHED:
            UpdateCrashedSkier(dt);
            break;
    }

    // === VISUAL STATE RECOVERY ===
    // Scale recovery (squash/stretch returns to normal)
    float scaleRecoverySpeed = 10.0f;
    skier->scaleY = Lerpf(skier->scaleY, 1.0f, dt * scaleRecoverySpeed);
    skier->scaleX = Lerpf(skier->scaleX, 1.0f, dt * scaleRecoverySpeed);

    // Landing impact fades
    skier->landingImpact *= powf(0.9f, dt * 60.0f);

    // Glow fades (handled in UpdateAirborneSkier for airborne, but fade when grounded too)
    if (skier->state == SKIER_GROUNDED) {
        skier->glowIntensity *= powf(0.95f, dt * 60.0f);
    }

    // Update speed stretch based on current velocity
    float currentSpeed = sqrtf(skier->velocity.x * skier->velocity.x +
                               skier->velocity.y * skier->velocity.y);
    float targetStretch = 1.0f + Clampf(currentSpeed / MAX_SKIER_SPEED, 0.0f, 1.0f) * 0.2f;
    skier->speedStretch = Lerpf(skier->speedStretch, targetStretch, dt * 8.0f);
}

static void DrawSkier(void) {
    Skier *skier = &g_game.skier;
    Vector2 screen = WorldToScreen(skier->worldPos);
    float time = g_game.anim.bgTime;

    // Calculate visual effects
    float speedRatio = g_game.camera.speed / MAX_CAMERA_SPEED;
    float stretchX = 1.0f + speedRatio * 0.25f;  // Stretch horizontally when fast
    float stretchY = 1.0f - speedRatio * 0.1f;   // Compress vertically

    // Landing impact squash
    stretchY *= skier->scaleY;
    stretchX *= skier->scaleX;

    // Glow effect when boosting or fast
    if (skier->glowIntensity > 0.01f || speedRatio > 0.6f) {
        float glowSize = 35.0f + speedRatio * 25.0f + skier->glowIntensity * 20.0f;
        float glowAlpha = (speedRatio * 0.3f + skier->glowIntensity * 0.5f);
        Color glowColor = skier->speedBoost > 1.0f ? COLOR_BOOST_GLOW : COLOR_SPEED_TRAIL;
        glowColor.a = (unsigned char)(glowAlpha * 120);
        DrawCircleV(screen, glowSize, glowColor);
    }

    rlPushMatrix();
    rlTranslatef(screen.x, screen.y, 0);
    rlRotatef(skier->rotation, 0, 0, 1);
    rlScalef(stretchX, stretchY, 1.0f);

    // === SOLIPSKIER STYLE SKIER ===
    // White stick figure with headphones

    // Skis (behind everything)
    Color skiColor = COLOR_SKIER_SKIS;
    float skiLength = 38.0f;
    float skiThickness = 4.0f;
    DrawRectangle((int)(-skiLength/2), 2, (int)skiLength, (int)skiThickness, skiColor);
    // Ski tips (curved up)
    DrawCircle((int)(-skiLength/2), 4, 3, skiColor);
    DrawCircle((int)(skiLength/2), 4, 3, skiColor);

    // Legs - bent skiing pose
    Color bodyColor = COLOR_SKIER_WHITE;
    float legThickness = 3.5f;
    // Back leg
    DrawLineEx((Vector2){-3, 0}, (Vector2){-8, -12}, legThickness, bodyColor);
    DrawLineEx((Vector2){-8, -12}, (Vector2){-4, -22}, legThickness, bodyColor);
    // Front leg
    DrawLineEx((Vector2){3, 0}, (Vector2){6, -14}, legThickness, bodyColor);
    DrawLineEx((Vector2){6, -14}, (Vector2){2, -22}, legThickness, bodyColor);

    // Body/torso - leaning forward
    DrawLineEx((Vector2){0, -22}, (Vector2){4, -38}, 4.0f, bodyColor);

    // Arms - tucked skiing pose
    // Back arm
    DrawLineEx((Vector2){2, -34}, (Vector2){-8, -28}, 3.0f, bodyColor);
    DrawLineEx((Vector2){-8, -28}, (Vector2){-14, -20}, 2.5f, bodyColor);
    // Front arm
    DrawLineEx((Vector2){4, -36}, (Vector2){12, -30}, 3.0f, bodyColor);
    DrawLineEx((Vector2){12, -30}, (Vector2){16, -22}, 2.5f, bodyColor);

    // Ski poles
    Color poleColor = {180, 190, 210, 255};
    DrawLineEx((Vector2){-14, -20}, (Vector2){-18, 5}, 1.5f, poleColor);
    DrawLineEx((Vector2){16, -22}, (Vector2){20, 5}, 1.5f, poleColor);

    // Head
    float headY = -44.0f;
    float headRadius = 8.0f;
    DrawCircle(5, (int)headY, (int)headRadius, bodyColor);

    // Headphones (if still has them)
    if (skier->hasHeadphones) {
        Color hpColor = COLOR_HEADPHONES;
        float wiggle = sinf(time * 8.0f + skier->headphoneWiggle) * 2.0f;

        // Headphone band (over head)
        DrawLineEx((Vector2){5 - 10, headY - 3}, (Vector2){5 + 10, headY - 3}, 3.0f, hpColor);
        DrawLineEx((Vector2){5 - 10, headY - 3}, (Vector2){5 - 10, headY + 2}, 3.0f, hpColor);
        DrawLineEx((Vector2){5 + 10, headY - 3}, (Vector2){5 + 10, headY + 2}, 3.0f, hpColor);

        // Ear cups
        DrawCircle(5 - 10, (int)(headY + 2), 5, hpColor);
        DrawCircle(5 + 10, (int)(headY + 2), 5, hpColor);

        // Wire flowing behind
        Color wireColor = COLOR_HEADPHONE_WIRE;
        Vector2 wireStart = {5, headY + 8};
        Vector2 wireEnd = {-15 + wiggle, headY + 25};
        DrawLineEx(wireStart, wireEnd, 2.0f, wireColor);
        DrawLineEx(wireEnd, (Vector2){-25 + wiggle * 1.5f, headY + 40}, 2.0f, wireColor);
    }

    rlPopMatrix();

    // === SPEED EFFECTS (drawn in screen space) ===
    if (speedRatio > 0.4f) {
        float effectAlpha = (speedRatio - 0.4f) / 0.6f;

        // Speed lines behind skier
        for (int i = 0; i < 5; i++) {
            float lineY = screen.y - 20 + i * 10;
            float lineLen = 20 + speedRatio * 40 + GetRandomValue(0, 20);
            float lineX = screen.x - 30 - i * 8;

            Color lineColor = COLOR_SPEED_TRAIL;
            lineColor.a = (unsigned char)(effectAlpha * 100 * (1.0f - i * 0.15f));

            DrawLineEx(
                (Vector2){lineX, lineY},
                (Vector2){lineX - lineLen, lineY + GetRandomValue(-3, 3)},
                2.0f - i * 0.3f,
                lineColor
            );
        }
    }

    // === TRICK EFFECTS ===
    if (skier->state == SKIER_AIRBORNE && skier->trickCount > 0) {
        // Spin trail
        float trailAlpha = 0.6f;
        Color trickColor = COLOR_TRICK_GLOW;
        trickColor.a = (unsigned char)(trailAlpha * 150);

        for (int i = 0; i < 8; i++) {
            float angle = skier->trickRotation * DEG2RAD + i * 0.4f;
            float dist = 25.0f - i * 2.0f;
            Vector2 point = {
                screen.x + cosf(angle) * dist,
                screen.y + sinf(angle) * dist
            };
            float size = 4.0f - i * 0.4f;
            trickColor.a = (unsigned char)(trailAlpha * 150 * (1.0f - i * 0.1f));
            DrawCircleV(point, size, trickColor);
        }
    }

    // === CRASH EFFECT ===
    if (skier->state == SKIER_CRASHED) {
        float flash = sinf(time * 20) * 0.4f + 0.4f;
        // Explosion rings
        for (int i = 0; i < 3; i++) {
            float ringSize = 30 + i * 15 + (1.0f - flash) * 20;
            Color ringColor = COLOR_GATE_RED;
            ringColor.a = (unsigned char)((0.5f - i * 0.15f) * 255 * flash);
            DrawCircleLines((int)screen.x, (int)screen.y, ringSize, ringColor);
        }
    }

    // === BOOST EFFECT ===
    if (skier->speedBoost > 1.0f && skier->boostTimer > 0) {
        float boostPulse = sinf(time * 15) * 0.3f + 0.7f;
        Color boostColor = COLOR_BOOST_GLOW;
        boostColor.a = (unsigned char)(boostPulse * 100);

        // Boost particles behind
        for (int i = 0; i < 4; i++) {
            float px = screen.x - 25 - i * 12;
            float py = screen.y + sinf(time * 10 + i) * 8;
            DrawCircleV((Vector2){px, py}, 4 - i * 0.5f, boostColor);
        }
    }
}

// =============================================================================
// CAMERA
// =============================================================================

static void UpdateGameCamera(float dt) {
    GameCamera *cam = &g_game.camera;
    Skier *skier = &g_game.skier;

    // Target speed based on skier
    if (skier->state != SKIER_CRASHED) {
        cam->targetSpeed = BASE_CAMERA_SPEED + skier->velocity.x * 0.45f;
        if (skier->speedBoost > 1.0f) {
            cam->targetSpeed *= skier->speedBoost * 0.5f;
        }
        cam->targetSpeed = Clampf(cam->targetSpeed, BASE_CAMERA_SPEED, MAX_CAMERA_SPEED);
    } else {
        cam->targetSpeed *= 0.97f;
    }

    // Smooth interpolation
    cam->speed = Lerpf(cam->speed, cam->targetSpeed, dt * 3.0f);

    // Move camera
    cam->worldX += cam->speed * dt;

    // Shake
    if (cam->shakeIntensity > 0) {
        cam->shakeIntensity -= dt * 4.0f;
        if (cam->shakeIntensity < 0) cam->shakeIntensity = 0;
        cam->shakeOffset.x = sinf(g_game.gameTime * 55) * cam->shakeIntensity * 12;
        cam->shakeOffset.y = cosf(g_game.gameTime * 65) * cam->shakeIntensity * 8;
    }
}

// =============================================================================
// SCORING
// =============================================================================

static void AddScore(int points, const char *text, Vector2 worldPos) {
    g_game.score.score += points;

    if (g_game.anim.popupCount < MAX_POPUPS) {
        ScorePopup *popup = &g_game.anim.popups[g_game.anim.popupCount++];
        strncpy(popup->text, text, sizeof(popup->text) - 1);
        popup->pos = WorldToScreen(worldPos);
        popup->timer = 1.4f;
        popup->value = points;

        if (points >= 1000) {
            popup->color = COLOR_GATE_GOLD;
        } else if (points >= 400) {
            popup->color = COLOR_GATE_GREEN;
        } else {
            popup->color = COLOR_TEXT_PRIMARY;
        }
    }
}

static void UpdateScoring(float dt) {
    ScoreSystem *score = &g_game.score;

    // Distance scoring
    score->distance = (int)(g_game.camera.worldX / 10.0f);
    score->score += (int)(g_game.camera.speed * dt * 0.08f);

    // Multiplier decay
    if (score->multiplierTimer > 0) {
        score->multiplierTimer -= dt;
        if (score->multiplierTimer <= 0) {
            score->multiplier = 1;
        }
    }

    // Difficulty
    g_game.difficulty = g_game.gameTime / 100.0f;
    g_game.difficulty = Clampf(g_game.difficulty, 0.0f, 1.0f);

    // Update popups
    for (int i = 0; i < g_game.anim.popupCount; i++) {
        ScorePopup *popup = &g_game.anim.popups[i];
        popup->timer -= dt;
        popup->pos.y -= 55 * dt;

        if (popup->timer <= 0) {
            for (int j = i; j < g_game.anim.popupCount - 1; j++) {
                g_game.anim.popups[j] = g_game.anim.popups[j + 1];
            }
            g_game.anim.popupCount--;
            i--;
        }
    }
}

static void DrawScoreUI(void) {
    ScoreSystem *score = &g_game.score;
    char buf[32];

    // Score (top left)
    snprintf(buf, sizeof(buf), "%d", score->score);
    DrawTextEx(g_font, buf, (Vector2){18, 12}, 34, 1, COLOR_TEXT_PRIMARY);

    // Multiplier
    if (score->multiplier > 1) {
        snprintf(buf, sizeof(buf), "x%d", score->multiplier);
        float flash = sinf(g_game.gameTime * 9) * 0.2f + 0.8f;
        Color mc = COLOR_MULTIPLIER;
        mc.a = (unsigned char)(255 * flash);
        DrawTextEx(g_font, buf, (Vector2){18, 50}, 26, 1, mc);

        // Timer bar
        float barW = 75 * (score->multiplierTimer / 5.0f);
        DrawRectangle(18, 78, (int)barW, 4, COLOR_MULTIPLIER);
    }

    // Distance (top right)
    snprintf(buf, sizeof(buf), "%dm", score->distance);
    int tw = (int)MeasureTextEx(g_font, buf, 22, 1).x;
    DrawTextEx(g_font, buf, (Vector2){g_screenWidth - tw - 18, 12}, 22, 1, COLOR_TEXT_MUTED);

    // Speed bar (right side)
    float speedNorm = (g_game.camera.speed - BASE_CAMERA_SPEED) / (MAX_CAMERA_SPEED - BASE_CAMERA_SPEED);
    speedNorm = Clampf(speedNorm, 0.0f, 1.0f);

    int barH = 120;
    int barY = g_screenHeight / 2 - barH / 2;
    DrawRectangle(g_screenWidth - 14, barY, 7, barH, COLOR_PANEL);

    Color speedColor = {
        (unsigned char)(80 + speedNorm * 175),
        (unsigned char)(180 - speedNorm * 80),
        (unsigned char)(255 - speedNorm * 200),
        255
    };
    DrawRectangle(g_screenWidth - 14, barY + barH - (int)(barH * speedNorm),
                  7, (int)(barH * speedNorm), speedColor);

    // Popups
    for (int i = 0; i < g_game.anim.popupCount; i++) {
        ScorePopup *popup = &g_game.anim.popups[i];
        float alpha = popup->timer / 1.4f;
        Color c = popup->color;
        c.a = (unsigned char)(255 * alpha);

        float scale = 1.0f + (1.0f - alpha) * 0.25f;
        int fontSize = (int)(18 * scale);
        DrawTextEx(g_font, popup->text, (Vector2){(float)popup->pos.x, (float)popup->pos.y}, fontSize, 1, c);

        snprintf(buf, sizeof(buf), "+%d", popup->value);
        DrawTextEx(g_font, buf, (Vector2){(float)popup->pos.x, (float)popup->pos.y + fontSize + 2},
                 fontSize - 3, 1, ColorAlpha(c, alpha * 0.85f));
    }

    // Scroll control hint
    if (g_game.gameTime < 4.0f && g_game.state == GAME_STATE_PLAYING) {
        float alpha = (4.0f - g_game.gameTime) / 4.0f;
        Color hint = COLOR_ACCENT;
        hint.a = (unsigned char)(255 * alpha);
        const char *drawHint = "SCROLL TO DRAW SNOW!";
        int hw = (int)MeasureTextEx(g_font, drawHint, 26, 1).x;
        DrawTextEx(g_font, drawHint, (Vector2){g_screenWidth / 2 - hw / 2, 95}, 26, 1, hint);

        // Secondary hint
        Color hint2 = COLOR_TEXT_MUTED;
        hint2.a = (unsigned char)(200 * alpha);
        const char *scrollHint = "Up = Higher, Down = Lower";
        int sw = (int)MeasureTextEx(g_font, scrollHint, 16, 1).x;
        DrawTextEx(g_font, scrollHint, (Vector2){g_screenWidth / 2 - sw / 2, 125}, 16, 1, hint2);
    }

    // Persistent mini-hint at bottom
    if (g_game.state == GAME_STATE_PLAYING) {
        const char *ctrlHint = "Scroll: Snow Height | Hold: Pause";
        int chw = (int)MeasureTextEx(g_font, ctrlHint, 12, 1).x;
        DrawTextEx(g_font, ctrlHint, (Vector2){g_screenWidth / 2 - chw / 2, g_screenHeight - 22}, 12, 1, COLOR_TEXT_DIM);
    }
}

// =============================================================================
// GAME STATE
// =============================================================================

static void CrashSkier(const char *reason) {
    (void)reason;
    Skier *skier = &g_game.skier;
    skier->state = SKIER_CRASHED;
    skier->angularVel = 450.0f * ((GetRandomValue(0, 1) * 2) - 1);

    g_game.anim.crashFlash = 0.45f;
    g_game.anim.screenShake = 0.35f;

    SpawnCrashParticles();

    // Delay to game over
    // We'll handle this in update
}

static void StartNewGame(void) {
    // Reset skier with all Solipskier visual state
    g_game.skier = (Skier){
        .worldPos = {SKIER_SCREEN_X + 150, g_screenHeight * 0.55f},
        .velocity = {BASE_CAMERA_SPEED, 0},
        .rotation = 0.0f,
        .targetRotation = 0.0f,
        .angularVel = 0.0f,
        .state = SKIER_AIRBORNE,
        .airTime = 0.0f,
        .totalAirTime = 0.0f,
        .scaleX = 1.0f,
        .scaleY = 1.0f,
        .speedBoost = 1.0f,
        .boostTimer = 0.0f,
        .currentSegment = -1,
        .segmentT = 0.0f,
        // Solipskier visual state
        .hasHeadphones = true,
        .headphoneWiggle = 0.0f,
        .trickRotation = 0.0f,
        .trickCount = 0,
        .landingImpact = 0.0f,
        .speedStretch = 1.0f,
        .glowIntensity = 0.0f,
        .groundSpeed = BASE_CAMERA_SPEED,
        .lastGroundAngle = 0.0f
    };

    // Reset camera
    g_game.camera = (GameCamera){
        .worldX = 0,
        .speed = BASE_CAMERA_SPEED,
        .targetSpeed = BASE_CAMERA_SPEED
    };

    // Clear lines and initialize scroll-based drawing
    memset(&g_game.lines, 0, sizeof(g_game.lines));
    g_game.lines.currentY = g_screenHeight * 0.55f;  // Start at middle-low
    g_game.lines.targetY = g_game.lines.currentY;
    g_game.lines.lastY = g_game.lines.currentY;
    g_game.lines.lastWorldX = SKIER_SCREEN_X + 50.0f;  // Start just ahead of skier
    g_game.lines.scrollVelocity = 0;

    // Reset obstacles
    g_game.obstacles = (ObstacleSystem){
        .nextGateX = 450,
        .nextTunnelX = 900
    };

    // Reset score
    g_game.score.score = 0;
    g_game.score.distance = 0;
    g_game.score.multiplier = 1;
    g_game.score.multiplierTimer = 0;
    g_game.score.combo = 0;
    g_game.score.gatesGreen = 0;
    g_game.score.gatesGold = 0;
    g_game.score.longestAir = 0;

    // Clear particles
    memset(&g_game.particles, 0, sizeof(g_game.particles));

    // Reset anim
    g_game.anim.popupCount = 0;
    g_game.anim.readyTimer = 1.5f;
    g_game.anim.tunnelDarken = 0;
    g_game.anim.crashFlash = 0;

    g_game.gameTime = 0;
    g_game.difficulty = 0;

    g_game.state = GAME_STATE_READY;
}

static void GameReset(void) {
    memset(&g_game, 0, sizeof(g_game));
    g_game.state = GAME_STATE_MENU;
    g_game.anim.menuIndex = 0;
}

// =============================================================================
// DRAWING
// =============================================================================

static void DrawBackground(void) {
    float time = g_game.anim.bgTime;

    // Gradient sky
    float hue = sinf(time * 0.08f) * 8.0f;
    Color top = ColorFromHSV(220.0f + hue, 0.55f, 0.08f);
    Color bottom = ColorFromHSV(225.0f + hue, 0.45f, 0.18f);
    DrawRectangleGradientV(0, 0, g_screenWidth, g_screenHeight, top, bottom);

    // Mountains (parallax)
    float offset1 = fmodf(g_game.camera.worldX * 0.08f, 350);
    float offset2 = fmodf(g_game.camera.worldX * 0.15f, 300);
    float offset3 = fmodf(g_game.camera.worldX * 0.25f, 250);

    for (int i = -1; i < 4; i++) {
        float x = i * 350 - offset1;
        float h = 90 + sinf(i * 2.3f) * 35;
        Vector2 p1 = {x, g_screenHeight};
        Vector2 p2 = {x + 175, g_screenHeight * 0.38f - h};
        Vector2 p3 = {x + 350, g_screenHeight};
        DrawTriangle(p1, p2, p3, COLOR_MOUNTAIN1);
    }

    for (int i = -1; i < 5; i++) {
        float x = i * 300 - offset2;
        float h = 70 + cosf(i * 1.8f) * 25;
        Vector2 p1 = {x, g_screenHeight};
        Vector2 p2 = {x + 150, g_screenHeight * 0.5f - h};
        Vector2 p3 = {x + 300, g_screenHeight};
        DrawTriangle(p1, p2, p3, COLOR_MOUNTAIN2);
    }

    for (int i = -1; i < 6; i++) {
        float x = i * 250 - offset3;
        float h = 50 + sinf(i * 3.1f) * 20;
        Vector2 p1 = {x, g_screenHeight};
        Vector2 p2 = {x + 125, g_screenHeight * 0.62f - h};
        Vector2 p3 = {x + 250, g_screenHeight};
        DrawTriangle(p1, p2, p3, COLOR_MOUNTAIN3);
    }

    // Stars
    for (int i = 0; i < 40; i++) {
        float sx = fmodf(i * 101.7f + time * 3, g_screenWidth);
        float sy = fmodf(i * 67.3f, g_screenHeight * 0.35f);
        float twinkle = (sinf(time * (1.5f + i * 0.08f) + i) + 1) * 0.5f;
        Color star = {255, 255, 255, (unsigned char)(40 + twinkle * 50)};
        DrawCircleV((Vector2){sx, sy}, 1.0f + twinkle * 0.5f, star);
    }
}

static void DrawMenu(void) {
    // Title
    const char *title = "LLZ SOLIPSKIER";
    int tw = (int)MeasureTextEx(g_font, title, 42, 1).x;
    DrawTextEx(g_font, title, (Vector2){g_screenWidth / 2 - tw / 2, 40}, 42, 1, COLOR_ACCENT);

    const char *sub = "Scroll to draw snow for the skier!";
    int sw = (int)MeasureTextEx(g_font, sub, 18, 1).x;
    DrawTextEx(g_font, sub, (Vector2){g_screenWidth / 2 - sw / 2, 90}, 18, 1, COLOR_TEXT_MUTED);

    // Menu options
    const char *opts[] = {"PLAY", "HIGH SCORE", "EXIT"};
    float menuY = 160;
    float menuH = 55;
    float menuW = 280;
    float menuX = g_screenWidth / 2 - menuW / 2;

    for (int i = 0; i < 3; i++) {
        Rectangle box = {menuX, menuY + i * menuH, menuW, menuH - 8};
        bool sel = (i == g_game.anim.menuIndex);

        Color bg = sel ? COLOR_ACCENT : COLOR_PANEL;
        if (sel) {
            float pulse = sinf(g_game.anim.bgTime * 5) * 0.1f + 0.9f;
            bg.r = (unsigned char)(bg.r * pulse);
            bg.g = (unsigned char)(bg.g * pulse);
            bg.b = (unsigned char)(bg.b * pulse);
        }
        DrawRectangleRounded(box, 0.15f, 8, bg);

        if (sel) {
            DrawRectangleRoundedLines(box, 0.15f, 8, COLOR_TEXT_PRIMARY);
        }

        Color tc = sel ? COLOR_BG_TOP : COLOR_TEXT_PRIMARY;
        int optW = (int)MeasureTextEx(g_font, opts[i], 24, 1).x;
        DrawTextEx(g_font, opts[i], (Vector2){(int)(box.x + box.width / 2 - optW / 2),
                 (int)(box.y + 12)}, 24, 1, tc);
    }

    // High score display
    if (g_game.anim.menuIndex == 1) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Best: %d", g_game.score.highScore);
        int bw = MeasureText(buf, 22);
        DrawText(buf, g_screenWidth / 2 - bw / 2, menuY + 3 * menuH + 20, 22, COLOR_ACCENT);
    }

    // Controls hint
    const char *hint = "Scroll: Navigate | Select: Play | Back: Exit";
    int hw = MeasureText(hint, 14);
    DrawText(hint, g_screenWidth / 2 - hw / 2, g_screenHeight - 35, 14, COLOR_TEXT_DIM);

    // Decorative skier
    float skierY = 380 + sinf(g_game.anim.bgTime * 2) * 8;
    DrawEllipse(g_screenWidth - 100, (int)skierY, 14, 20, COLOR_SKIER_BODY);
    DrawCircle(g_screenWidth - 100, (int)skierY - 18, 9, COLOR_SKIER_BODY);
    DrawRectangle(g_screenWidth - 118, (int)skierY + 12, 36, 5, COLOR_SKIER_SKIS);
}

static void DrawReadyGo(void) {
    float t = g_game.anim.readyTimer;
    const char *text;
    Color color;

    if (t > 0.7f) {
        text = "READY";
        color = COLOR_TEXT_PRIMARY;
    } else if (t > 0) {
        text = "GO!";
        color = COLOR_GATE_GREEN;
    } else {
        return;
    }

    float scale = 1.0f + (1.5f - t) * 0.25f;
    float alpha = (t > 0.25f) ? 1.0f : t / 0.25f;

    int fontSize = (int)(44 * scale);
    int textW = MeasureText(text, fontSize);
    Color c = color;
    c.a = (unsigned char)(255 * alpha);

    DrawText(text, g_screenWidth / 2 - textW / 2, g_screenHeight / 2 - fontSize / 2, fontSize, c);
}

static void DrawPaused(void) {
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 150});

    const char *text = "PAUSED";
    int tw = MeasureText(text, 44);
    DrawText(text, g_screenWidth / 2 - tw / 2, g_screenHeight / 2 - 45, 44, COLOR_TEXT_PRIMARY);

    const char *hint = "Tap to resume | Back: Menu";
    int hw = MeasureText(hint, 16);
    DrawText(hint, g_screenWidth / 2 - hw / 2, g_screenHeight / 2 + 15, 16, COLOR_TEXT_MUTED);
}

static void DrawGameOver(void) {
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 180});

    Rectangle panel = {g_screenWidth / 2 - 170, g_screenHeight / 2 - 115, 340, 230};
    DrawRectangleRounded(panel, 0.08f, 12, COLOR_PANEL);
    DrawRectangleRoundedLines(panel, 0.08f, 12, COLOR_DANGER);

    const char *title = "GAME OVER";
    int titleW = MeasureText(title, 34);
    DrawText(title, (int)(panel.x + panel.width / 2 - titleW / 2), (int)(panel.y + 22), 34, COLOR_DANGER);

    char buf[64];

    // Score
    snprintf(buf, sizeof(buf), "Score: %d", g_game.score.score);
    int scoreW = MeasureText(buf, 26);
    DrawText(buf, (int)(panel.x + panel.width / 2 - scoreW / 2), (int)(panel.y + 70), 26, COLOR_ACCENT);

    // Stats
    snprintf(buf, sizeof(buf), "Distance: %dm", g_game.score.distance);
    int distW = MeasureText(buf, 18);
    DrawText(buf, (int)(panel.x + panel.width / 2 - distW / 2), (int)(panel.y + 110), 18, COLOR_TEXT_MUTED);

    snprintf(buf, sizeof(buf), "Gates: %d green, %d gold", g_game.score.gatesGreen, g_game.score.gatesGold);
    int gatesW = MeasureText(buf, 16);
    DrawText(buf, (int)(panel.x + panel.width / 2 - gatesW / 2), (int)(panel.y + 135), 16, COLOR_TEXT_MUTED);

    // New best?
    if (g_game.score.score >= g_game.score.highScore && g_game.score.score > 0) {
        const char *best = "NEW BEST!";
        int bestW = MeasureText(best, 20);
        float flash = sinf(g_game.anim.bgTime * 7) * 0.3f + 0.7f;
        Color bc = COLOR_GATE_GOLD;
        bc.a = (unsigned char)(255 * flash);
        DrawText(best, (int)(panel.x + panel.width / 2 - bestW / 2), (int)(panel.y + 165), 20, bc);
    }

    const char *hint = "Tap to continue";
    int hintW = MeasureText(hint, 14);
    DrawText(hint, (int)(panel.x + panel.width / 2 - hintW / 2), (int)(panel.y + 198), 14, COLOR_TEXT_DIM);
}

// =============================================================================
// UPDATE
// =============================================================================

static void HandleMenuInput(const LlzInputState *input) {
    if (input->backReleased) {
        g_wantsClose = true;
        return;
    }

    if (input->downPressed || input->scrollDelta > 0.5f || input->swipeDown) {
        g_game.anim.menuIndex = (g_game.anim.menuIndex + 1) % 3;
    }
    if (input->upPressed || input->scrollDelta < -0.5f || input->swipeUp) {
        g_game.anim.menuIndex = (g_game.anim.menuIndex + 2) % 3;
    }

    if (input->tap || input->selectPressed) {
        switch (g_game.anim.menuIndex) {
            case 0:
                StartNewGame();
                break;
            case 1:
                // Just show high score on menu
                break;
            case 2:
                g_wantsClose = true;
                break;
        }
    }
}

static void HandlePlayInput(const LlzInputState *input, float dt) {
    (void)dt;

    // Pause
    if (input->hold || input->backReleased) {
        g_game.state = GAME_STATE_PAUSED;
        return;
    }

    // Line drawing is handled separately
}

// =============================================================================
// CONFIG
// =============================================================================

static void SaveConfig(void) {
    if (!g_configInit) return;
    LlzPluginConfigSetInt(&g_config, "high_score", g_game.score.highScore);
    LlzPluginConfigSave(&g_config);
}

static void LoadConfig(void) {
    if (!g_configInit) return;
    g_game.score.highScore = LlzPluginConfigGetInt(&g_config, "high_score", 0);
}

// =============================================================================
// PLUGIN CALLBACKS
// =============================================================================

static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;

    // Initialize font
    g_font = LlzFontGet(LLZ_FONT_UI, 32);
    if (g_font.texture.id == 0) {
        g_font = GetFontDefault();
    }

    LlzPluginConfigEntry defaults[] = {
        {"high_score", "0"}
    };
    g_configInit = LlzPluginConfigInit(&g_config, "llzsolipskier", defaults, 1);

    GameReset();
    LoadConfig();

    printf("[LLZSOLIPSKIER] Initialized %dx%d\n", width, height);
}

static void PluginUpdate(const LlzInputState *input, float dt) {
    // Background animation
    g_game.anim.bgTime += dt;

    // Animation timers
    if (g_game.anim.screenShake > 0) {
        g_game.anim.screenShake -= dt * 4.5f;
        if (g_game.anim.screenShake < 0) g_game.anim.screenShake = 0;
        g_game.anim.shakeX = sinf(g_game.anim.bgTime * 55) * g_game.anim.screenShake * 14;
        g_game.anim.shakeY = cosf(g_game.anim.bgTime * 65) * g_game.anim.screenShake * 10;
    }
    if (g_game.anim.crashFlash > 0) g_game.anim.crashFlash -= dt * 2.5f;

    switch (g_game.state) {
        case GAME_STATE_MENU:
            HandleMenuInput(input);
            break;

        case GAME_STATE_READY:
            g_game.anim.readyTimer -= dt;
            if (g_game.anim.readyTimer <= 0) {
                g_game.state = GAME_STATE_PLAYING;
            }
            break;

        case GAME_STATE_PLAYING:
            HandlePlayInput(input, dt);
            if (g_game.state != GAME_STATE_PLAYING) break;

            g_game.gameTime += dt;

            UpdateGameCamera(dt);
            UpdateLineDrawing(input, dt);
            UpdateSkier(dt);
            UpdateObstacles(dt);
            UpdateScoring(dt);
            UpdateParticles(dt);
            UpdateSkierTrail(dt);

            // Check for game over after crash
            if (g_game.skier.state == SKIER_CRASHED) {
                static float crashTimer = 0;
                crashTimer += dt;
                if (crashTimer > 1.2f) {
                    crashTimer = 0;
                    // Update high score
                    if (g_game.score.score > g_game.score.highScore) {
                        g_game.score.highScore = g_game.score.score;
                        SaveConfig();
                    }
                    g_game.state = GAME_STATE_GAME_OVER;
                }
            }
            break;

        case GAME_STATE_PAUSED:
            if (input->tap || input->selectPressed) {
                g_game.state = GAME_STATE_PLAYING;
            }
            if (input->backReleased) {
                g_game.state = GAME_STATE_MENU;
            }
            break;

        case GAME_STATE_GAME_OVER:
            if (input->tap || input->selectPressed || input->backReleased) {
                g_game.state = GAME_STATE_MENU;
            }
            break;
    }
}

static void PluginDraw(void) {
    DrawBackground();

    // Screen shake
    bool shaking = g_game.anim.screenShake > 0;
    if (shaking) {
        rlPushMatrix();
        rlTranslatef(g_game.anim.shakeX, g_game.anim.shakeY, 0);
    }

    if (g_game.state == GAME_STATE_MENU) {
        DrawMenu();
    } else {
        DrawTunnels();
        DrawLines();
        DrawGates();
        DrawSkierTrail();
        DrawSkier();
        DrawParticles();
        DrawScoreUI();

        if (g_game.state == GAME_STATE_READY) {
            DrawReadyGo();
        }
        if (g_game.state == GAME_STATE_PAUSED) {
            DrawPaused();
        }
        if (g_game.state == GAME_STATE_GAME_OVER) {
            DrawGameOver();
        }
    }

    if (shaking) {
        rlPopMatrix();
    }

    // Crash flash
    if (g_game.anim.crashFlash > 0) {
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight,
                      (Color){255, 50, 70, (unsigned char)(g_game.anim.crashFlash * 100)});
    }

    // Tunnel darkness overlay
    if (g_game.anim.tunnelDarken > 0.01f) {
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight,
                      (Color){0, 0, 0, (unsigned char)(g_game.anim.tunnelDarken * 180)});
    }
}

static void PluginShutdown(void) {
    if (g_configInit) {
        SaveConfig();
        LlzPluginConfigFree(&g_config);
        g_configInit = false;
    }
    g_wantsClose = false;
    printf("[LLZSOLIPSKIER] Shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

static LlzPluginAPI g_api = {
    .name = "LLZ Solipskier",
    .description = "Draw snow lines for a skier to ride!",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .handles_back_button = true,
    .category = LLZ_CATEGORY_GAMES
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
