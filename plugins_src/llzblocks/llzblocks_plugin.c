// LLZ Blocks - A feature-rich block-stacking puzzle game
// Inspired by Apotris and classic falling block games
// Designed for CarThing's 800x480 display

#include "llizard_plugin.h"
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

#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define BUFFER_HEIGHT 4
#define TOTAL_HEIGHT (BOARD_HEIGHT + BUFFER_HEIGHT)

// Tetromino types
typedef enum {
    PIECE_I = 0, PIECE_O, PIECE_T, PIECE_S, PIECE_Z, PIECE_J, PIECE_L,
    PIECE_COUNT, PIECE_NONE = -1
} PieceType;

// Game modes
typedef enum {
    MODE_MARATHON = 0,
    MODE_SPRINT_40,   // Clear 40 lines
    MODE_SPRINT_100,  // Clear 100 lines
    MODE_ULTRA_3,     // 3 minute time attack
    MODE_ULTRA_5,     // 5 minute time attack
    MODE_ZEN,         // No game over, relaxed
    MODE_COUNT
} GameMode;

// Game states
typedef enum {
    STATE_MENU = 0,
    STATE_OPTIONS,    // Mode-specific options screen
    STATE_READY,      // Ready-Go countdown
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAMEOVER,
    STATE_COMPLETE    // Sprint/Ultra completed
} GameState;

// Marathon goal types
typedef enum {
    MARATHON_ENDLESS = 0,
    MARATHON_150_LINES,
    MARATHON_200_LINES,
    MARATHON_GOAL_COUNT
} MarathonGoal;

static const char *MARATHON_GOAL_NAMES[] = {"Endless", "150 Lines", "200 Lines"};
static const int MARATHON_GOAL_VALUES[] = {0, 150, 200};

// Clear types for scoring and display
typedef enum {
    CLEAR_NONE = 0,
    CLEAR_SINGLE,
    CLEAR_DOUBLE,
    CLEAR_TRIPLE,
    CLEAR_TETRIS,
    CLEAR_TSPIN_MINI,
    CLEAR_TSPIN_SINGLE,
    CLEAR_TSPIN_DOUBLE,
    CLEAR_TSPIN_TRIPLE,
    CLEAR_PERFECT
} ClearType;

// Rotation states
#define ROTATION_COUNT 4

// Tetromino shapes (4x4 grids for each piece and rotation)
static const int SHAPES[PIECE_COUNT][ROTATION_COUNT][4][4] = {
    // I piece
    {{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
     {{0,0,0,0},{0,0,0,0},{1,1,1,1},{0,0,0,0}},
     {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}}},
    // O piece
    {{{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}},
    // T piece
    {{{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
     {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
     {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}},
    // S piece
    {{{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
     {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}},
    // Z piece
    {{{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
     {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
     {{0,1,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}}},
    // J piece
    {{{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
     {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}},
    // L piece
    {{{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
     {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}}
};

// SRS wall kick data
static const int KICKS_JLSTZ[4][5][2] = {
    {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}},
    {{0,0},{1,0},{1,-1},{0,2},{1,2}},
    {{0,0},{1,0},{1,1},{0,-2},{1,-2}},
    {{0,0},{-1,0},{-1,-1},{0,2},{-1,2}}
};

static const int KICKS_I[4][5][2] = {
    {{0,0},{-2,0},{1,0},{-2,-1},{1,2}},
    {{0,0},{-1,0},{2,0},{-1,2},{2,-1}},
    {{0,0},{2,0},{-1,0},{2,1},{-1,-2}},
    {{0,0},{1,0},{-2,0},{1,-2},{-2,1}}
};

// Piece colors (vibrant, Apotris-style)
static const Color PIECE_COLORS[PIECE_COUNT] = {
    {0, 240, 240, 255},   // I - Cyan
    {240, 240, 0, 255},   // O - Yellow
    {180, 0, 255, 255},   // T - Purple
    {0, 255, 0, 255},     // S - Green
    {255, 0, 0, 255},     // Z - Red
    {0, 0, 255, 255},     // J - Blue
    {255, 165, 0, 255}    // L - Orange
};

// UI Colors
#define COLOR_BG            (Color){8, 10, 16, 255}
#define COLOR_BG_GRADIENT   (Color){16, 20, 32, 255}
#define COLOR_PANEL         (Color){20, 24, 36, 255}
#define COLOR_PANEL_LIGHT   (Color){32, 38, 54, 255}
#define COLOR_PANEL_ACCENT  (Color){40, 48, 68, 255}
#define COLOR_GRID          (Color){28, 32, 44, 255}
#define COLOR_GRID_LINE     (Color){36, 42, 58, 255}
#define COLOR_TEXT_PRIMARY  (Color){245, 245, 250, 255}
#define COLOR_TEXT_MUTED    (Color){120, 130, 150, 255}
#define COLOR_TEXT_DIM      (Color){70, 80, 100, 255}
#define COLOR_ACCENT        (Color){80, 180, 255, 255}
#define COLOR_ACCENT_BRIGHT (Color){120, 200, 255, 255}
#define COLOR_SUCCESS       (Color){80, 255, 120, 255}
#define COLOR_WARNING       (Color){255, 200, 80, 255}
#define COLOR_DANGER        (Color){255, 80, 100, 255}

// Mode names and descriptions
static const char *MODE_NAMES[] = {"MARATHON", "SPRINT 40", "SPRINT 100", "ULTRA 3MIN", "ULTRA 5MIN", "ZEN"};
static const char *MODE_DESCS[] = {
    "Endless survival - level up every 10 lines",
    "Clear 40 lines as fast as possible",
    "Clear 100 lines - the endurance test",
    "Score attack - 3 minute time limit",
    "Score attack - 5 minute time limit",
    "Relaxed mode - no game over, just chill"
};

// =============================================================================
// PARTICLE SYSTEM
// =============================================================================

#define MAX_PARTICLES 200

typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float maxLife;
    float size;
    bool active;
} Particle;

static Particle g_particles[MAX_PARTICLES];
static int g_particleCount = 0;

static void SpawnParticle(float x, float y, Color color, float speed) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g_particles[i].active) {
            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float vel = speed * (0.5f + (float)GetRandomValue(0, 100) / 100.0f);
            g_particles[i].pos = (Vector2){x, y};
            g_particles[i].vel = (Vector2){cosf(angle) * vel, sinf(angle) * vel};
            g_particles[i].color = color;
            g_particles[i].life = 0.5f + (float)GetRandomValue(0, 50) / 100.0f;
            g_particles[i].maxLife = g_particles[i].life;
            g_particles[i].size = 2.0f + (float)GetRandomValue(0, 40) / 10.0f;
            g_particles[i].active = true;
            g_particleCount++;
            return;
        }
    }
}

static void SpawnLineClearParticles(float boardX, float y, float width, Color color) {
    for (int i = 0; i < 30; i++) {
        float x = boardX + (float)GetRandomValue(0, (int)width);
        SpawnParticle(x, y, color, 150.0f);
    }
}

static void UpdateParticles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_particles[i].active) {
            g_particles[i].pos.x += g_particles[i].vel.x * dt;
            g_particles[i].pos.y += g_particles[i].vel.y * dt;
            g_particles[i].vel.y += 300.0f * dt;  // Gravity
            g_particles[i].life -= dt;
            if (g_particles[i].life <= 0) {
                g_particles[i].active = false;
                g_particleCount--;
            }
        }
    }
}

static void DrawParticles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_particles[i].active) {
            float alpha = g_particles[i].life / g_particles[i].maxLife;
            Color c = g_particles[i].color;
            c.a = (unsigned char)(255 * alpha);
            DrawCircleV(g_particles[i].pos, g_particles[i].size * alpha, c);
        }
    }
}

// =============================================================================
// GAME STATE
// =============================================================================

typedef struct {
    int board[TOTAL_HEIGHT][BOARD_WIDTH];
    PieceType currentPiece;
    int currentX, currentY, currentRotation;
    PieceType holdPiece;
    bool holdUsed;
    PieceType nextPieces[6];
    int score;
    int highScores[MODE_COUNT];
    int lines;
    int level;
    int combo;
    bool backToBack;
    ClearType lastClearType;
    bool lastWasTSpin;
    bool lastMoveWasRotation;
    GameMode mode;
    GameState state;
    float gameTime;
    float dropTimer;
    float lockTimer;
    float lockMoves;  // Moves/rotations during lock delay
    bool locking;
    int sprintTarget;
    float ultraTimeLimit;
    // Marathon options
    int startLevel;           // Starting level (0-19)
    MarathonGoal marathonGoal; // Line goal for marathon
    int marathonLineTarget;   // Actual line target (0 = endless)
    // Options screen state
    int optionSelected;       // Which option is selected (0=level, 1=goal)
} Game;

typedef struct {
    float readyTimer;
    float lineClearTimer;
    int clearingLines[4];
    int clearingCount;
    float clearFlashTimer;
    float lineClearProgress;     // 0 to 1 for directional clear animation
    float screenShake;
    float screenShakeX, screenShakeY;
    float dirShakeX;             // Directional shake for flick moves
    float dirShakeTimer;
    float clearTextTimer;
    char clearText[32];
    Color clearTextColor;
    int clearTextScore;
    float menuSelection;
    int menuIndex;
    float lockFlashTimer;
    float perfectClearTimer;
    float bgTime;               // Background animation time
    float bgHue;                // Background gradient hue shift
    float pieceFlashTimer;      // Flash when piece moves after flick
    float dragAccumX;           // Accumulated drag distance for movement
    // Grid lighting effects
    float gridPulseLeft;        // Left edge flash
    float gridPulseRight;       // Right edge flash
    float gridPulseRow;         // Row flash (hard drop landing zone)
    int gridPulseRowY;          // Which row to pulse (visible row, 0-19)
    float gridMoveGlow;         // General movement glow intensity
    int lastPieceX;             // Track piece position for grid lighting
    int lastPieceY;
} AnimState;

typedef struct {
    float dasTimer;
    float arrTimer;
    int dasDirection;
    bool softDropHeld;
    float scrollAccum;
    float das;  // Configurable DAS
    float arr;  // Configurable ARR
} InputState;

// Globals
static Game g_game;
static AnimState g_anim;
static InputState g_input;
static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;
static LlzPluginConfig g_config;
static bool g_configInitialized = false;

// 7-bag randomizer
static PieceType g_bag[7];
static int g_bagIndex = 7;

// Timing constants
static const float LOCK_DELAY = 0.5f;
static const float MAX_LOCK_MOVES = 15;
static const float LINE_CLEAR_TIME = 0.35f;  // Slightly longer for visual effect
static const float READY_TIME = 2.0f;
static const float CLEAR_TEXT_TIME = 1.5f;

// Default handling values (guideline: DAS=10 frames, ARR=2 frames at 60fps)
static const float DEFAULT_DAS = 0.167f;  // ~10 frames at 60fps
static const float DEFAULT_ARR = 0.033f;  // ~2 frames at 60fps

// Speed curve (seconds per drop) - Tetris Guideline gravity
// Level 0: 1G, Level 1: 1.26G, ... Level 19+: 20G (instant)
static const float SPEED_CURVE[] = {
    1.000f,   // Level 0
    0.793f,   // Level 1
    0.618f,   // Level 2
    0.473f,   // Level 3
    0.355f,   // Level 4
    0.262f,   // Level 5
    0.190f,   // Level 6
    0.135f,   // Level 7
    0.094f,   // Level 8
    0.064f,   // Level 9
    0.043f,   // Level 10
    0.028f,   // Level 11
    0.018f,   // Level 12
    0.011f,   // Level 13
    0.007f,   // Level 14
    0.005f,   // Level 15
    0.004f,   // Level 16
    0.003f,   // Level 17
    0.002f,   // Level 18
    0.001f    // Level 19+ (almost instant)
};
static const int SPEED_CURVE_COUNT = 20;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void GameReset(void);
static void SpawnPiece(void);
static bool TryMove(int dx, int dy);
static bool TryRotate(int dir);
static void HardDrop(void);
static void HoldPiece(void);
static void LockPiece(void);
static void ClearLines(void);
static int CalculateGhostY(void);
static bool CheckCollision(PieceType piece, int x, int y, int rot);
static bool IsTSpin(void);
static void SaveConfig(void);
static void LoadConfig(void);

// =============================================================================
// RANDOMIZER
// =============================================================================

static void ShuffleBag(void) {
    for (int i = 0; i < 7; i++) g_bag[i] = (PieceType)i;
    for (int i = 6; i > 0; i--) {
        int j = GetRandomValue(0, i);
        PieceType t = g_bag[i];
        g_bag[i] = g_bag[j];
        g_bag[j] = t;
    }
    g_bagIndex = 0;
}

static PieceType GetNextPiece(void) {
    if (g_bagIndex >= 7) ShuffleBag();
    return g_bag[g_bagIndex++];
}

static void FillNextQueue(void) {
    for (int i = 0; i < 6; i++) g_game.nextPieces[i] = GetNextPiece();
}

static PieceType PopNextPiece(void) {
    PieceType p = g_game.nextPieces[0];
    for (int i = 0; i < 5; i++) g_game.nextPieces[i] = g_game.nextPieces[i + 1];
    g_game.nextPieces[5] = GetNextPiece();
    return p;
}

// =============================================================================
// COLLISION AND MOVEMENT
// =============================================================================

static bool CheckCollision(PieceType piece, int x, int y, int rot) {
    if (piece == PIECE_NONE) return true;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (SHAPES[piece][rot][py][px]) {
                int bx = x + px, by = y + py;
                if (bx < 0 || bx >= BOARD_WIDTH || by >= TOTAL_HEIGHT) return true;
                if (by >= 0 && g_game.board[by][bx] != 0) return true;
            }
        }
    }
    return false;
}

static int CalculateGhostY(void) {
    int y = g_game.currentY;
    while (!CheckCollision(g_game.currentPiece, g_game.currentX, y + 1, g_game.currentRotation)) y++;
    return y;
}

static bool TryMove(int dx, int dy) {
    if (g_game.currentPiece == PIECE_NONE) return false;
    int nx = g_game.currentX + dx, ny = g_game.currentY + dy;
    if (!CheckCollision(g_game.currentPiece, nx, ny, g_game.currentRotation)) {
        g_game.currentX = nx;
        g_game.currentY = ny;
        g_game.lastMoveWasRotation = false;
        if (g_game.locking && dy == 0 && g_game.lockMoves < MAX_LOCK_MOVES) {
            g_game.lockTimer = 0;
            g_game.lockMoves++;
        }
        return true;
    }
    return false;
}

static bool TryRotate(int dir) {
    if (g_game.currentPiece == PIECE_NONE || g_game.currentPiece == PIECE_O) return false;
    int newRot = (g_game.currentRotation + dir + 4) % 4;
    int kickIdx = (dir > 0) ? g_game.currentRotation : newRot;
    const int (*kicks)[5][2] = (g_game.currentPiece == PIECE_I) ? KICKS_I : KICKS_JLSTZ;

    for (int i = 0; i < 5; i++) {
        int kx = kicks[kickIdx][i][0] * dir;
        int ky = -kicks[kickIdx][i][1] * dir;
        int nx = g_game.currentX + kx, ny = g_game.currentY + ky;
        if (!CheckCollision(g_game.currentPiece, nx, ny, newRot)) {
            g_game.currentX = nx;
            g_game.currentY = ny;
            g_game.currentRotation = newRot;
            g_game.lastMoveWasRotation = true;
            if (g_game.locking && g_game.lockMoves < MAX_LOCK_MOVES) {
                g_game.lockTimer = 0;
                g_game.lockMoves++;
            }
            return true;
        }
    }
    return false;
}

// T-spin detection
static bool IsTSpin(void) {
    if (g_game.currentPiece != PIECE_T || !g_game.lastMoveWasRotation) return false;

    // Check 3 of 4 corners occupied
    int corners[4][2] = {{0,0}, {2,0}, {0,2}, {2,2}};
    int filled = 0;
    for (int i = 0; i < 4; i++) {
        int cx = g_game.currentX + corners[i][0];
        int cy = g_game.currentY + corners[i][1];
        if (cx < 0 || cx >= BOARD_WIDTH || cy < 0 || cy >= TOTAL_HEIGHT) {
            filled++;
        } else if (g_game.board[cy][cx] != 0) {
            filled++;
        }
    }
    return filled >= 3;
}

// =============================================================================
// PIECE ACTIONS
// =============================================================================

static void SpawnPiece(void) {
    g_game.currentPiece = PopNextPiece();
    // Apotris/Guideline spawn position:
    // - Horizontally centered (column 3 for most pieces)
    // - Spawn so piece appears at top of visible area immediately
    // - Buffer rows are 0-3, visible rows start at 4
    g_game.currentX = 3;
    g_game.currentY = 2;  // Spawn just above visible area (pieces appear immediately)
    g_game.currentRotation = 0;
    g_game.holdUsed = false;
    g_game.locking = false;
    g_game.lockTimer = 0;
    g_game.lockMoves = 0;
    g_game.dropTimer = 0;
    g_game.lastMoveWasRotation = false;

    // Check for game over (can't spawn piece)
    if (CheckCollision(g_game.currentPiece, g_game.currentX, g_game.currentY, g_game.currentRotation)) {
        // Try spawning one row higher
        g_game.currentY = 1;
        if (CheckCollision(g_game.currentPiece, g_game.currentX, g_game.currentY, g_game.currentRotation)) {
            g_game.currentY = 0;
            if (CheckCollision(g_game.currentPiece, g_game.currentX, g_game.currentY, g_game.currentRotation)) {
                if (g_game.mode == MODE_ZEN) {
                    // In Zen mode, just clear some lines and continue
                    for (int y = TOTAL_HEIGHT - 5; y < TOTAL_HEIGHT; y++) {
                        for (int x = 0; x < BOARD_WIDTH; x++) g_game.board[y][x] = 0;
                    }
                    g_game.currentY = 2;
                } else {
                    g_game.state = STATE_GAMEOVER;
                    if (g_game.score > g_game.highScores[g_game.mode]) {
                        g_game.highScores[g_game.mode] = g_game.score;
                    }
                    SaveConfig();
                }
            }
        }
    }
}

static void HardDrop(void) {
    if (g_game.currentPiece == PIECE_NONE) return;
    int dist = 0;
    while (TryMove(0, 1)) dist++;
    g_game.score += dist * 2;

    // Find the lowest row of the landed piece (for grid pulse effect)
    int lowestRow = 0;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (SHAPES[g_game.currentPiece][g_game.currentRotation][py][px]) {
                int boardRow = g_game.currentY + py - BUFFER_HEIGHT;
                if (boardRow > lowestRow) lowestRow = boardRow;
            }
        }
    }

    // Flash the landing row area on hard drop
    if (dist > 2) {
        g_anim.gridPulseRow = 1.0f;
        g_anim.gridPulseRowY = lowestRow;
        g_anim.screenShake = 0.08f;  // Small shake on hard drop
    }
    LockPiece();
}

static void HoldPiece(void) {
    if (g_game.holdUsed || g_game.currentPiece == PIECE_NONE) return;
    PieceType temp = g_game.holdPiece;
    g_game.holdPiece = g_game.currentPiece;
    g_game.holdUsed = true;
    if (temp == PIECE_NONE) {
        SpawnPiece();
    } else {
        g_game.currentPiece = temp;
        g_game.currentX = 3;
        g_game.currentY = 2;  // Same spawn position as SpawnPiece
        g_game.currentRotation = 0;
        g_game.locking = false;
        g_game.lockTimer = 0;
        g_game.lockMoves = 0;
        g_game.lastMoveWasRotation = false;
    }
}

static void LockPiece(void) {
    if (g_game.currentPiece == PIECE_NONE) return;

    bool wasTSpin = IsTSpin();
    g_game.lastWasTSpin = wasTSpin;

    // Place on board
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (SHAPES[g_game.currentPiece][g_game.currentRotation][py][px]) {
                int bx = g_game.currentX + px, by = g_game.currentY + py;
                if (by >= 0 && by < TOTAL_HEIGHT && bx >= 0 && bx < BOARD_WIDTH) {
                    g_game.board[by][bx] = g_game.currentPiece + 1;
                }
            }
        }
    }

    g_anim.lockFlashTimer = 0.1f;
    g_game.currentPiece = PIECE_NONE;
    ClearLines();
}

// =============================================================================
// LINE CLEARING AND SCORING
// =============================================================================

static const char *CLEAR_NAMES[] = {
    "", "SINGLE", "DOUBLE", "TRIPLE", "TETRIS",
    "T-SPIN MINI", "T-SPIN SINGLE", "T-SPIN DOUBLE", "T-SPIN TRIPLE",
    "PERFECT CLEAR"
};

static const int CLEAR_SCORES[] = {
    0, 100, 300, 500, 800,
    100, 800, 1200, 1600,
    3000
};

static void ClearLines(void) {
    g_anim.clearingCount = 0;

    for (int y = BUFFER_HEIGHT; y < TOTAL_HEIGHT; y++) {
        bool full = true;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (g_game.board[y][x] == 0) { full = false; break; }
        }
        if (full && g_anim.clearingCount < 4) {
            g_anim.clearingLines[g_anim.clearingCount++] = y;
        }
    }

    if (g_anim.clearingCount > 0) {
        g_anim.lineClearTimer = LINE_CLEAR_TIME;
        g_anim.clearFlashTimer = 0;

        // Determine clear type
        ClearType clearType;
        if (g_game.lastWasTSpin) {
            switch (g_anim.clearingCount) {
                case 1: clearType = CLEAR_TSPIN_SINGLE; break;
                case 2: clearType = CLEAR_TSPIN_DOUBLE; break;
                case 3: clearType = CLEAR_TSPIN_TRIPLE; break;
                default: clearType = CLEAR_TSPIN_MINI; break;
            }
        } else {
            switch (g_anim.clearingCount) {
                case 1: clearType = CLEAR_SINGLE; break;
                case 2: clearType = CLEAR_DOUBLE; break;
                case 3: clearType = CLEAR_TRIPLE; break;
                default: clearType = CLEAR_TETRIS; break;
            }
        }
        g_game.lastClearType = clearType;

        // Calculate score
        int baseScore = CLEAR_SCORES[clearType];
        bool isDifficult = (clearType >= CLEAR_TETRIS);

        // Back-to-back bonus
        if (isDifficult && g_game.backToBack) {
            baseScore = (int)(baseScore * 1.5f);
        }
        g_game.backToBack = isDifficult;

        // Combo
        g_game.combo++;
        int comboBonus = 50 * g_game.combo * (g_game.level + 1);

        int totalScore = baseScore * (g_game.level + 1) + comboBonus;
        g_game.score += totalScore;
        g_game.lines += g_anim.clearingCount;

        // Level up (every 10 lines, cap at level 19 for speed curve)
        int newLevel = g_game.lines / 10;
        if (newLevel >= SPEED_CURVE_COUNT) newLevel = SPEED_CURVE_COUNT - 1;
        if (newLevel > g_game.level) {
            g_game.level = newLevel;
        }

        // Visual feedback
        snprintf(g_anim.clearText, sizeof(g_anim.clearText), "%s", CLEAR_NAMES[clearType]);
        g_anim.clearTextScore = totalScore;
        g_anim.clearTextTimer = CLEAR_TEXT_TIME;

        // Set color based on clear type
        if (clearType == CLEAR_TETRIS || clearType >= CLEAR_TSPIN_SINGLE) {
            g_anim.clearTextColor = COLOR_ACCENT_BRIGHT;
            g_anim.screenShake = 0.15f;
        } else if (clearType == CLEAR_TRIPLE) {
            g_anim.clearTextColor = COLOR_SUCCESS;
            g_anim.screenShake = 0.08f;
        } else {
            g_anim.clearTextColor = COLOR_TEXT_PRIMARY;
        }

        // Check for Sprint/Marathon line goal completion
        if ((g_game.mode == MODE_SPRINT_40 || g_game.mode == MODE_SPRINT_100 ||
             (g_game.mode == MODE_MARATHON && g_game.sprintTarget > 0)) &&
            g_game.lines >= g_game.sprintTarget) {
            g_game.state = STATE_COMPLETE;
            if (g_game.score > g_game.highScores[g_game.mode]) {
                g_game.highScores[g_game.mode] = g_game.score;
            }
            SaveConfig();
        }

        // Check for perfect clear
        bool perfect = true;
        for (int y = 0; y < TOTAL_HEIGHT && perfect; y++) {
            for (int x = 0; x < BOARD_WIDTH && perfect; x++) {
                // Check if any cell outside clearing lines has blocks
                bool isClearing = false;
                for (int i = 0; i < g_anim.clearingCount; i++) {
                    if (g_anim.clearingLines[i] == y) { isClearing = true; break; }
                }
                if (!isClearing && g_game.board[y][x] != 0) perfect = false;
            }
        }
        if (perfect) {
            g_anim.perfectClearTimer = 2.0f;
            g_game.score += CLEAR_SCORES[CLEAR_PERFECT] * (g_game.level + 1);
            snprintf(g_anim.clearText, sizeof(g_anim.clearText), "PERFECT CLEAR!");
            g_anim.clearTextColor = COLOR_WARNING;
            g_anim.screenShake = 0.25f;
        }

        // Spawn particles
        for (int i = 0; i < g_anim.clearingCount; i++) {
            float blockSize = (g_screenHeight - 40) / (float)BOARD_HEIGHT;
            float boardX = (g_screenWidth - BOARD_WIDTH * blockSize) / 2;
            float y = (g_anim.clearingLines[i] - BUFFER_HEIGHT) * blockSize + 20;
            SpawnLineClearParticles(boardX, y, BOARD_WIDTH * blockSize, g_anim.clearTextColor);
        }

    } else {
        g_game.combo = 0;
        SpawnPiece();
    }
}

static void FinishLineClear(void) {
    // Single-pass compaction: copy non-cleared rows from bottom to top
    int writeRow = TOTAL_HEIGHT - 1;  // Start writing at bottom

    for (int readRow = TOTAL_HEIGHT - 1; readRow >= 0; readRow--) {
        // Check if this row is being cleared
        bool isClearing = false;
        for (int i = 0; i < g_anim.clearingCount; i++) {
            if (g_anim.clearingLines[i] == readRow) {
                isClearing = true;
                break;
            }
        }

        if (!isClearing) {
            // Copy this row to the write position (if different)
            if (writeRow != readRow) {
                for (int x = 0; x < BOARD_WIDTH; x++) {
                    g_game.board[writeRow][x] = g_game.board[readRow][x];
                }
            }
            writeRow--;
        }
    }

    // Clear remaining rows at the top
    while (writeRow >= 0) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            g_game.board[writeRow][x] = 0;
        }
        writeRow--;
    }

    g_anim.clearingCount = 0;
    SpawnPiece();
    SaveConfig();
}

// =============================================================================
// GAME RESET
// =============================================================================

static void GameReset(void) {
    memset(g_game.board, 0, sizeof(g_game.board));
    g_game.currentPiece = PIECE_NONE;
    g_game.holdPiece = PIECE_NONE;
    g_game.holdUsed = false;
    g_game.score = 0;
    g_game.lines = 0;
    g_game.level = 0;
    g_game.combo = 0;
    g_game.backToBack = false;
    g_game.lastClearType = CLEAR_NONE;
    g_game.lastWasTSpin = false;
    g_game.dropTimer = 0;
    g_game.lockTimer = 0;
    g_game.locking = false;
    g_game.lockMoves = 0;
    g_game.gameTime = 0;

    // Set mode-specific targets
    switch (g_game.mode) {
        case MODE_MARATHON:
            g_game.sprintTarget = g_game.marathonLineTarget;  // 0 = endless
            g_game.ultraTimeLimit = 0;
            g_game.level = g_game.startLevel;  // Apply starting level
            break;
        case MODE_SPRINT_40:
            g_game.sprintTarget = 40;
            g_game.ultraTimeLimit = 0;
            break;
        case MODE_SPRINT_100:
            g_game.sprintTarget = 100;
            g_game.ultraTimeLimit = 0;
            break;
        case MODE_ULTRA_3:
            g_game.sprintTarget = 0;
            g_game.ultraTimeLimit = 180.0f;  // 3 minutes
            break;
        case MODE_ULTRA_5:
            g_game.sprintTarget = 0;
            g_game.ultraTimeLimit = 300.0f;  // 5 minutes
            break;
        default:
            g_game.sprintTarget = 0;
            g_game.ultraTimeLimit = 0;
            break;
    }

    memset(&g_anim, 0, sizeof(g_anim));
    g_anim.readyTimer = READY_TIME;

    g_input.dasTimer = 0;
    g_input.arrTimer = 0;
    g_input.dasDirection = 0;
    g_input.scrollAccum = 0;

    // Clear particles
    for (int i = 0; i < MAX_PARTICLES; i++) g_particles[i].active = false;
    g_particleCount = 0;

    g_bagIndex = 7;
    FillNextQueue();

    g_game.state = STATE_READY;
}

static float GetDropSpeed(void) {
    int idx = g_game.level;
    if (idx >= SPEED_CURVE_COUNT) idx = SPEED_CURVE_COUNT - 1;
    return SPEED_CURVE[idx];
}

// =============================================================================
// DRAWING FUNCTIONS
// =============================================================================

// Draw animated gradient background like Apotris
static void DrawAnimatedBackground(float time) {
    // Slowly shifting hue for subtle color change
    float hue1 = fmodf(time * 5.0f, 360.0f);
    float hue2 = fmodf(time * 5.0f + 30.0f, 360.0f);

    // Very dark, subtle gradient colors
    Color color1 = ColorFromHSV(hue1, 0.4f, 0.06f);  // Top - very dark
    Color color2 = ColorFromHSV(hue2, 0.5f, 0.10f);  // Bottom - slightly brighter

    DrawRectangleGradientV(0, 0, g_screenWidth, g_screenHeight, color1, color2);

    // Subtle grid pattern overlay
    Color gridColor = {255, 255, 255, 8};  // Very subtle white lines
    int gridSize = 40;

    for (int x = 0; x < g_screenWidth; x += gridSize) {
        DrawLine(x, 0, x, g_screenHeight, gridColor);
    }
    for (int y = 0; y < g_screenHeight; y += gridSize) {
        DrawLine(0, y, g_screenWidth, y, gridColor);
    }

    // Subtle vignette effect (darker corners)
    for (int i = 0; i < 4; i++) {
        int alpha = 15 - i * 3;
        int size = 80 + i * 40;
        DrawRectangle(0, 0, size, g_screenHeight, (Color){0, 0, 0, alpha});
        DrawRectangle(g_screenWidth - size, 0, size, g_screenHeight, (Color){0, 0, 0, alpha});
    }
}

static void DrawBlock(float x, float y, float size, Color color, bool ghost) {
    if (ghost) {
        DrawRectangleRec((Rectangle){x, y, size - 1, size - 1}, ColorAlpha(color, 0.15f));
        DrawRectangleLinesEx((Rectangle){x, y, size - 1, size - 1}, 1, ColorAlpha(color, 0.4f));
    } else {
        // Main block with gradient
        DrawRectangleGradientV((int)x, (int)y, (int)size - 1, (int)size - 1,
                               ColorBrightness(color, 0.1f), ColorBrightness(color, -0.1f));

        // Inner highlight
        Color hi = ColorBrightness(color, 0.4f);
        DrawRectangle((int)x + 1, (int)y + 1, (int)size - 3, 2, hi);
        DrawRectangle((int)x + 1, (int)y + 1, 2, (int)size - 3, hi);

        // Shadow
        Color sh = ColorBrightness(color, -0.4f);
        DrawRectangle((int)x + 1, (int)(y + size - 3), (int)size - 3, 2, sh);
        DrawRectangle((int)(x + size - 3), (int)y + 1, 2, (int)size - 3, sh);
    }
}

static void DrawPiecePreview(PieceType piece, float cx, float cy, float blockSize, float alpha) {
    if (piece == PIECE_NONE) return;
    Color color = PIECE_COLORS[piece];
    color.a = (unsigned char)(255 * alpha);

    int minX = 4, maxX = -1, minY = 4, maxY = -1;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (SHAPES[piece][0][py][px]) {
                if (px < minX) minX = px;
                if (px > maxX) maxX = px;
                if (py < minY) minY = py;
                if (py > maxY) maxY = py;
            }
        }
    }

    float pw = (maxX - minX + 1) * blockSize;
    float ph = (maxY - minY + 1) * blockSize;
    float ox = cx - pw / 2, oy = cy - ph / 2;

    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (SHAPES[piece][0][py][px]) {
                DrawBlock(ox + (px - minX) * blockSize, oy + (py - minY) * blockSize,
                          blockSize, color, false);
            }
        }
    }
}

static void DrawBoard(float boardX, float boardY, float blockSize) {
    float bw = BOARD_WIDTH * blockSize;
    float bh = BOARD_HEIGHT * blockSize;

    // Board background
    DrawRectangle((int)boardX, (int)boardY, (int)bw, (int)bh, COLOR_BG);

    // Draw grid of perfect squares with visible cell outlines
    float baseAlpha = 0.12f + g_anim.gridMoveGlow * 0.15f;

    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            float cellX = boardX + x * blockSize;
            float cellY = boardY + y * blockSize;
            int cellSize = (int)blockSize;

            // Calculate cell glow based on position and current effects
            float glow = 0.0f;

            // Left edge glow (columns 0-1)
            if (x <= 1 && g_anim.gridPulseLeft > 0) {
                float edgeFactor = (2 - x) / 2.0f;
                glow += g_anim.gridPulseLeft * edgeFactor * 0.4f;
            }

            // Right edge glow (columns 8-9)
            if (x >= BOARD_WIDTH - 2 && g_anim.gridPulseRight > 0) {
                float edgeFactor = (x - (BOARD_WIDTH - 3)) / 2.0f;
                glow += g_anim.gridPulseRight * edgeFactor * 0.4f;
            }

            // Landing row glow (3 rows around where piece landed)
            if (g_anim.gridPulseRow > 0) {
                int rowDist = abs(y - g_anim.gridPulseRowY);
                if (rowDist <= 2) {
                    float rowFactor = 1.0f - (rowDist / 3.0f);
                    glow += g_anim.gridPulseRow * rowFactor * 0.5f;
                }
            }

            // General movement glow (subtle)
            glow += g_anim.gridMoveGlow * 0.1f;

            // Draw cell fill with glow effect
            if (glow > 0.01f) {
                Color glowColor = ColorAlpha(COLOR_ACCENT, glow);
                DrawRectangle((int)cellX + 1, (int)cellY + 1, cellSize - 2, cellSize - 2, glowColor);
            }

            // Draw cell outline (perfect square) - 1px border for each cell
            float cellAlpha = baseAlpha;
            if (glow > 0.01f) {
                cellAlpha += glow * 0.3f;
            }
            Color cellOutline = ColorAlpha(WHITE, cellAlpha);
            DrawRectangleLines((int)cellX, (int)cellY, cellSize, cellSize, cellOutline);
        }
    }

    // Draw outer border slightly brighter
    Color borderColor = ColorAlpha(WHITE, baseAlpha + 0.1f);
    DrawRectangleLinesEx((Rectangle){boardX, boardY, bw, bh}, 1.0f, borderColor);

    // Ghost piece
    if (g_game.currentPiece != PIECE_NONE && g_game.state == STATE_PLAYING && g_anim.clearingCount == 0) {
        int ghostY = CalculateGhostY();
        Color gc = PIECE_COLORS[g_game.currentPiece];
        for (int py = 0; py < 4; py++) {
            for (int px = 0; px < 4; px++) {
                if (SHAPES[g_game.currentPiece][g_game.currentRotation][py][px]) {
                    int by = ghostY + py - BUFFER_HEIGHT;
                    int bx = g_game.currentX + px;
                    if (by >= 0 && by < BOARD_HEIGHT) {
                        DrawBlock(boardX + bx * blockSize, boardY + by * blockSize, blockSize, gc, true);
                    }
                }
            }
        }
    }

    // Placed blocks
    for (int y = BUFFER_HEIGHT; y < TOTAL_HEIGHT; y++) {
        int vy = y - BUFFER_HEIGHT;
        bool clearing = false;
        for (int i = 0; i < g_anim.clearingCount; i++) {
            if (g_anim.clearingLines[i] == y) { clearing = true; break; }
        }

        for (int x = 0; x < BOARD_WIDTH; x++) {
            int cell = g_game.board[y][x];
            if (cell > 0) {
                Color c = PIECE_COLORS[cell - 1];
                if (clearing) {
                    // Directional wipe effect like Apotris
                    float progress = g_anim.lineClearProgress;
                    int wipePos = (int)(progress * (BOARD_WIDTH + 2));
                    int distFromCenter = abs(x - BOARD_WIDTH / 2);

                    if (distFromCenter < wipePos) {
                        // Block is being wiped - flash white then fade
                        float localProgress = (float)(wipePos - distFromCenter) / 3.0f;
                        if (localProgress > 1.0f) localProgress = 1.0f;

                        if (localProgress < 0.5f) {
                            // Flash to white
                            float flash = localProgress * 2.0f;
                            c.r = (unsigned char)(c.r + (255 - c.r) * flash);
                            c.g = (unsigned char)(c.g + (255 - c.g) * flash);
                            c.b = (unsigned char)(c.b + (255 - c.b) * flash);
                        } else {
                            // Fade out
                            float fade = (localProgress - 0.5f) * 2.0f;
                            c = ColorAlpha(WHITE, 1.0f - fade);
                        }
                    }
                }
                DrawBlock(boardX + x * blockSize, boardY + vy * blockSize, blockSize, c, false);
            }
        }
    }

    // Current piece
    if (g_game.currentPiece != PIECE_NONE && g_game.state == STATE_PLAYING && g_anim.clearingCount == 0) {
        Color pc = PIECE_COLORS[g_game.currentPiece];

        // Lock flash
        if (g_anim.lockFlashTimer > 0) {
            pc = ColorBrightness(pc, 0.5f * (g_anim.lockFlashTimer / 0.1f));
        }

        // Flick move flash - makes piece bright white briefly so player can see where it landed
        if (g_anim.pieceFlashTimer > 0) {
            float flashIntensity = g_anim.pieceFlashTimer / 0.2f;
            // Pulse effect
            float pulse = sinf(g_anim.pieceFlashTimer * 30.0f) * 0.3f + 0.7f;
            flashIntensity *= pulse;
            pc.r = (unsigned char)fminf(255, pc.r + (255 - pc.r) * flashIntensity);
            pc.g = (unsigned char)fminf(255, pc.g + (255 - pc.g) * flashIntensity);
            pc.b = (unsigned char)fminf(255, pc.b + (255 - pc.b) * flashIntensity);
        }

        for (int py = 0; py < 4; py++) {
            for (int px = 0; px < 4; px++) {
                if (SHAPES[g_game.currentPiece][g_game.currentRotation][py][px]) {
                    int by = g_game.currentY + py - BUFFER_HEIGHT;
                    int bx = g_game.currentX + px;
                    if (by >= 0 && by < BOARD_HEIGHT) {
                        DrawBlock(boardX + bx * blockSize, boardY + by * blockSize, blockSize, pc, false);
                    }
                }
            }
        }
    }

    // Board border
    DrawRectangleLinesEx((Rectangle){boardX - 3, boardY - 3, bw + 6, bh + 6}, 2, COLOR_ACCENT);

    // Lock progress indicator
    if (g_game.locking && g_game.state == STATE_PLAYING) {
        float lockProgress = g_game.lockTimer / LOCK_DELAY;
        Color lockColor = ColorAlpha(COLOR_WARNING, 0.8f);
        DrawRectangle((int)boardX - 3, (int)(boardY + bh + 4), (int)(bw * lockProgress), 3, lockColor);
    }
}

static void DrawUI(float boardX, float boardY, float blockSize) {
    float bw = BOARD_WIDTH * blockSize;
    float bh = BOARD_HEIGHT * blockSize;

    // === RIGHT PANEL: Next & Hold ===
    float rpX = boardX + bw + 20;
    float rpW = g_screenWidth - rpX - 15;

    // Next pieces
    DrawText("NEXT", (int)rpX, (int)boardY, 16, COLOR_TEXT_MUTED);
    for (int i = 0; i < 5; i++) {
        Rectangle box = {rpX, boardY + 24 + i * 58, 75, 52};
        DrawRectangleRounded(box, 0.15f, 6, COLOR_PANEL);
        float alpha = 1.0f - i * 0.12f;
        DrawPiecePreview(g_game.nextPieces[i], box.x + 38, box.y + 26, 13, alpha);
    }

    // Hold piece
    float holdY = boardY + 320;
    DrawText("HOLD", (int)rpX, (int)holdY, 16, COLOR_TEXT_MUTED);
    Rectangle holdBox = {rpX, holdY + 24, 75, 52};
    DrawRectangleRounded(holdBox, 0.15f, 6, g_game.holdUsed ? COLOR_GRID : COLOR_PANEL);
    if (g_game.holdPiece != PIECE_NONE) {
        float alpha = g_game.holdUsed ? 0.35f : 1.0f;
        DrawPiecePreview(g_game.holdPiece, holdBox.x + 38, holdBox.y + 26, 13, alpha);
    }

    // === LEFT PANEL: Stats ===
    float lpX = 15;
    float lpW = boardX - 30;

    // Mode indicator
    DrawText(MODE_NAMES[g_game.mode], (int)lpX, (int)boardY, 20, COLOR_ACCENT);

    // Score
    float sy = boardY + 35;
    DrawText("SCORE", (int)lpX, (int)sy, 14, COLOR_TEXT_MUTED);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", g_game.score);
    DrawText(buf, (int)lpX, (int)sy + 18, 28, COLOR_TEXT_PRIMARY);

    // High score
    sy += 60;
    DrawText("BEST", (int)lpX, (int)sy, 14, COLOR_TEXT_MUTED);
    snprintf(buf, sizeof(buf), "%d", g_game.highScores[g_game.mode]);
    DrawText(buf, (int)lpX, (int)sy + 18, 22, COLOR_ACCENT);

    // Lines
    sy += 55;
    DrawText("LINES", (int)lpX, (int)sy, 14, COLOR_TEXT_MUTED);
    if (g_game.mode == MODE_SPRINT_40 || g_game.mode == MODE_SPRINT_100 ||
        (g_game.mode == MODE_MARATHON && g_game.sprintTarget > 0)) {
        snprintf(buf, sizeof(buf), "%d/%d", g_game.lines, g_game.sprintTarget);
    } else {
        snprintf(buf, sizeof(buf), "%d", g_game.lines);
    }
    DrawText(buf, (int)lpX, (int)sy + 18, 22, COLOR_TEXT_PRIMARY);

    // Level
    sy += 50;
    DrawText("LEVEL", (int)lpX, (int)sy, 14, COLOR_TEXT_MUTED);
    snprintf(buf, sizeof(buf), "%d", g_game.level + 1);
    DrawText(buf, (int)lpX, (int)sy + 18, 22, COLOR_TEXT_PRIMARY);

    // Time (for Ultra mode or general)
    sy += 50;
    if (g_game.mode == MODE_ULTRA_3 || g_game.mode == MODE_ULTRA_5) {
        float remaining = g_game.ultraTimeLimit - g_game.gameTime;
        if (remaining < 0) remaining = 0;
        DrawText("TIME", (int)lpX, (int)sy, 14, COLOR_TEXT_MUTED);
        int mins = (int)remaining / 60;
        int secs = (int)remaining % 60;
        snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
        Color timeColor = (remaining < 30) ? COLOR_DANGER : COLOR_TEXT_PRIMARY;
        DrawText(buf, (int)lpX, (int)sy + 18, 22, timeColor);
    } else {
        DrawText("TIME", (int)lpX, (int)sy, 14, COLOR_TEXT_MUTED);
        int mins = (int)g_game.gameTime / 60;
        int secs = (int)g_game.gameTime % 60;
        snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
        DrawText(buf, (int)lpX, (int)sy + 18, 22, COLOR_TEXT_PRIMARY);
    }

    // Combo
    if (g_game.combo > 1) {
        sy += 50;
        DrawText("COMBO", (int)lpX, (int)sy, 14, COLOR_TEXT_MUTED);
        snprintf(buf, sizeof(buf), "x%d", g_game.combo);
        DrawText(buf, (int)lpX, (int)sy + 18, 22, COLOR_WARNING);
    }

    // Back-to-back indicator
    if (g_game.backToBack) {
        sy += (g_game.combo > 1) ? 50 : 50;
        DrawText("B2B", (int)lpX, (int)sy, 14, COLOR_ACCENT_BRIGHT);
    }

    // Clear text animation
    if (g_anim.clearTextTimer > 0) {
        float alpha = g_anim.clearTextTimer / CLEAR_TEXT_TIME;
        float scale = 1.0f + (1.0f - alpha) * 0.3f;
        float yOffset = (1.0f - alpha) * -30;

        Color textColor = g_anim.clearTextColor;
        textColor.a = (unsigned char)(255 * alpha);

        int fontSize = (int)(24 * scale);
        int textW = MeasureText(g_anim.clearText, fontSize);
        DrawText(g_anim.clearText, (int)(boardX + bw/2 - textW/2),
                 (int)(boardY + bh/2 + yOffset), fontSize, textColor);

        // Score popup
        snprintf(buf, sizeof(buf), "+%d", g_anim.clearTextScore);
        int scoreW = MeasureText(buf, 20);
        DrawText(buf, (int)(boardX + bw/2 - scoreW/2),
                 (int)(boardY + bh/2 + 30 + yOffset), 20, ColorAlpha(COLOR_TEXT_PRIMARY, alpha));
    }

    // Controls hint
    float hintY = g_screenHeight - 35;
    DrawText("Drag/Scroll: Move | Flick: Slam | Tap: Rotate | Swipe Down: Drop | Back: Hold",
             (int)lpX, (int)hintY, 11, COLOR_TEXT_DIM);
}

static void DrawMenu(void) {
    // Title
    const char *title = "LLZ BLOCKS";
    int titleW = MeasureText(title, 40);
    DrawText(title, g_screenWidth/2 - titleW/2, 25, 40, COLOR_ACCENT_BRIGHT);

    const char *subtitle = "A Block-Stacking Puzzle Game";
    int subW = MeasureText(subtitle, 16);
    DrawText(subtitle, g_screenWidth/2 - subW/2, 70, 16, COLOR_TEXT_MUTED);

    // Mode selection - compact layout for 6 modes
    float menuY = 100;
    float menuH = 52;  // Smaller height per item
    float menuW = 340;
    float menuX = 40;

    for (int i = 0; i < MODE_COUNT; i++) {
        Rectangle box = {menuX, menuY + i * menuH, menuW, menuH - 6};
        bool selected = (i == g_anim.menuIndex);

        Color bgColor = selected ? COLOR_ACCENT : COLOR_PANEL;
        if (selected) {
            // Pulsing effect
            float pulse = sinf(g_anim.bgTime * 4) * 0.1f + 0.9f;
            bgColor = ColorBrightness(bgColor, pulse - 1.0f);
        }
        DrawRectangleRounded(box, 0.15f, 8, bgColor);

        if (selected) {
            DrawRectangleRoundedLines(box, 0.15f, 8, COLOR_ACCENT_BRIGHT);
        }

        Color textColor = selected ? COLOR_BG : COLOR_TEXT_PRIMARY;
        DrawText(MODE_NAMES[i], (int)(box.x + 15), (int)(box.y + 8), 20, textColor);

        Color descColor = selected ? ColorAlpha(COLOR_BG, 0.8f) : COLOR_TEXT_MUTED;
        DrawText(MODE_DESCS[i], (int)(box.x + 15), (int)(box.y + 30), 12, descColor);
    }

    // Right panel - High scores and best times
    float rpX = menuX + menuW + 30;
    float rpY = menuY;

    DrawText("HIGH SCORES", (int)rpX, (int)rpY, 18, COLOR_ACCENT);
    rpY += 28;

    for (int i = 0; i < MODE_COUNT; i++) {
        char buf[64];
        Color scoreColor = (i == g_anim.menuIndex) ? COLOR_ACCENT_BRIGHT : COLOR_TEXT_MUTED;

        // Shorten mode names for display
        const char *shortNames[] = {"Marathon", "Sprint 40", "Sprint 100", "Ultra 3m", "Ultra 5m", "Zen"};
        snprintf(buf, sizeof(buf), "%s", shortNames[i]);
        DrawText(buf, (int)rpX, (int)rpY + i * 44, 14, scoreColor);

        snprintf(buf, sizeof(buf), "%d", g_game.highScores[i]);
        DrawText(buf, (int)rpX, (int)rpY + i * 44 + 16, 22, scoreColor);
    }

    // Instructions at bottom
    float instY = g_screenHeight - 40;
    DrawText("Scroll: Navigate | Select: Play | Back: Exit",
             (int)(g_screenWidth/2 - 180), (int)instY, 16, COLOR_TEXT_DIM);

    // Decorative tetromino preview on right side
    float previewX = g_screenWidth - 120;
    float previewY = 150;
    float time = g_anim.bgTime;
    int previewPiece = ((int)(time / 2.0f)) % PIECE_COUNT;
    DrawPiecePreview((PieceType)previewPiece, previewX, previewY, 20, 0.6f);

    // Another piece below
    previewPiece = (previewPiece + 3) % PIECE_COUNT;
    DrawPiecePreview((PieceType)previewPiece, previewX, previewY + 100, 18, 0.4f);
}

static void DrawOptions(void) {
    // Title
    const char *title = "MARATHON OPTIONS";
    int titleW = MeasureText(title, 36);
    DrawText(title, g_screenWidth/2 - titleW/2, 40, 36, COLOR_ACCENT_BRIGHT);

    float optY = 120;
    float optH = 70;
    float optW = 500;
    float optX = (g_screenWidth - optW) / 2;
    char buf[64];

    // Option 0: Starting Level
    {
        Rectangle box = {optX, optY, optW, optH - 8};
        bool selected = (g_game.optionSelected == 0);

        Color bgColor = selected ? COLOR_ACCENT : COLOR_PANEL;
        if (selected) {
            float pulse = sinf(g_anim.bgTime * 4) * 0.1f + 0.9f;
            bgColor = ColorBrightness(bgColor, pulse - 1.0f);
        }
        DrawRectangleRounded(box, 0.1f, 8, bgColor);
        if (selected) {
            DrawRectangleRoundedLines(box, 0.1f, 8, COLOR_ACCENT_BRIGHT);
        }

        Color textColor = selected ? COLOR_BG : COLOR_TEXT_PRIMARY;
        DrawText("Starting Level", (int)(box.x + 20), (int)(box.y + 12), 22, textColor);

        // Level value with arrows
        snprintf(buf, sizeof(buf), "< %d >", g_game.startLevel);
        int valW = MeasureText(buf, 28);
        DrawText(buf, (int)(box.x + box.width - valW - 20), (int)(box.y + 18), 28, textColor);

        Color hintColor = selected ? ColorAlpha(COLOR_BG, 0.7f) : COLOR_TEXT_MUTED;
        DrawText("Swipe left/right to change", (int)(box.x + 20), (int)(box.y + 42), 14, hintColor);
    }

    optY += optH;

    // Option 1: Goal
    {
        Rectangle box = {optX, optY, optW, optH - 8};
        bool selected = (g_game.optionSelected == 1);

        Color bgColor = selected ? COLOR_ACCENT : COLOR_PANEL;
        if (selected) {
            float pulse = sinf(g_anim.bgTime * 4) * 0.1f + 0.9f;
            bgColor = ColorBrightness(bgColor, pulse - 1.0f);
        }
        DrawRectangleRounded(box, 0.1f, 8, bgColor);
        if (selected) {
            DrawRectangleRoundedLines(box, 0.1f, 8, COLOR_ACCENT_BRIGHT);
        }

        Color textColor = selected ? COLOR_BG : COLOR_TEXT_PRIMARY;
        DrawText("Line Goal", (int)(box.x + 20), (int)(box.y + 12), 22, textColor);

        // Goal value with arrows
        snprintf(buf, sizeof(buf), "< %s >", MARATHON_GOAL_NAMES[g_game.marathonGoal]);
        int valW = MeasureText(buf, 24);
        DrawText(buf, (int)(box.x + box.width - valW - 20), (int)(box.y + 20), 24, textColor);

        Color hintColor = selected ? ColorAlpha(COLOR_BG, 0.7f) : COLOR_TEXT_MUTED;
        DrawText("Swipe left/right to change", (int)(box.x + 20), (int)(box.y + 42), 14, hintColor);
    }

    optY += optH + 20;

    // Option 2: Start Button
    {
        Rectangle box = {optX + 100, optY, optW - 200, 55};
        bool selected = (g_game.optionSelected == 2);

        Color bgColor = selected ? COLOR_SUCCESS : COLOR_ACCENT;
        if (selected) {
            float pulse = sinf(g_anim.bgTime * 6) * 0.15f + 0.85f;
            bgColor = ColorBrightness(bgColor, pulse - 1.0f);
        }
        DrawRectangleRounded(box, 0.2f, 8, bgColor);
        if (selected) {
            DrawRectangleRoundedLines(box, 0.2f, 8, WHITE);
        }

        const char *startText = "START GAME";
        int startW = MeasureText(startText, 26);
        Color startColor = selected ? WHITE : COLOR_BG;
        DrawText(startText, (int)(box.x + (box.width - startW) / 2), (int)(box.y + 14), 26, startColor);
    }

    // Preview of settings
    optY += 80;
    snprintf(buf, sizeof(buf), "Level %d  |  %s", g_game.startLevel, MARATHON_GOAL_NAMES[g_game.marathonGoal]);
    int previewW = MeasureText(buf, 18);
    DrawText(buf, (g_screenWidth - previewW) / 2, (int)optY, 18, COLOR_TEXT_MUTED);

    // Instructions
    float instY = g_screenHeight - 45;
    DrawText("Scroll: Navigate | Swipe: Adjust | Tap: Start | Back: Return",
             (int)(g_screenWidth/2 - 240), (int)instY, 16, COLOR_TEXT_DIM);

    // Speed preview for selected level
    optY += 35;
    int levelIdx = g_game.startLevel;
    if (levelIdx >= SPEED_CURVE_COUNT) levelIdx = SPEED_CURVE_COUNT - 1;
    float speed = SPEED_CURVE[levelIdx];
    snprintf(buf, sizeof(buf), "Drop speed: %.2fs per row", speed);
    int speedW = MeasureText(buf, 14);
    DrawText(buf, (g_screenWidth - speedW) / 2, (int)optY, 14, COLOR_TEXT_DIM);
}

static void DrawReadyGo(void) {
    float t = g_anim.readyTimer;
    const char *text;
    Color color;

    if (t > 1.0f) {
        text = "READY";
        color = COLOR_TEXT_PRIMARY;
    } else if (t > 0) {
        text = "GO!";
        color = COLOR_SUCCESS;
    } else {
        return;
    }

    float scale = 1.0f + (2.0f - t) * 0.2f;
    if (scale > 1.5f) scale = 1.5f;
    float alpha = (t > 0.3f) ? 1.0f : t / 0.3f;

    int fontSize = (int)(48 * scale);
    int textW = MeasureText(text, fontSize);
    Color c = color;
    c.a = (unsigned char)(255 * alpha);

    DrawText(text, g_screenWidth/2 - textW/2, g_screenHeight/2 - fontSize/2, fontSize, c);
}

static void DrawGameOver(void) {
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, ColorAlpha(BLACK, 0.75f));

    Rectangle panel = {g_screenWidth/2 - 180, g_screenHeight/2 - 130, 360, 260};
    DrawRectangleRounded(panel, 0.08f, 12, COLOR_PANEL);
    DrawRectangleRoundedLines(panel, 0.08f, 12, COLOR_ACCENT);

    const char *title = (g_game.state == STATE_COMPLETE) ? "COMPLETE!" : "GAME OVER";
    Color titleColor = (g_game.state == STATE_COMPLETE) ? COLOR_SUCCESS : COLOR_DANGER;
    int titleW = MeasureText(title, 36);
    DrawText(title, (int)(panel.x + panel.width/2 - titleW/2), (int)(panel.y + 25), 36, titleColor);

    char buf[64];

    // Score
    snprintf(buf, sizeof(buf), "Score: %d", g_game.score);
    int scoreW = MeasureText(buf, 28);
    DrawText(buf, (int)(panel.x + panel.width/2 - scoreW/2), (int)(panel.y + 80), 28, COLOR_ACCENT);

    // Stats
    snprintf(buf, sizeof(buf), "Lines: %d   Level: %d", g_game.lines, g_game.level + 1);
    int statsW = MeasureText(buf, 20);
    DrawText(buf, (int)(panel.x + panel.width/2 - statsW/2), (int)(panel.y + 120), 20, COLOR_TEXT_MUTED);

    // Time
    int mins = (int)g_game.gameTime / 60;
    int secs = (int)g_game.gameTime % 60;
    snprintf(buf, sizeof(buf), "Time: %d:%02d", mins, secs);
    int timeW = MeasureText(buf, 20);
    DrawText(buf, (int)(panel.x + panel.width/2 - timeW/2), (int)(panel.y + 148), 20, COLOR_TEXT_MUTED);

    // New high score?
    if (g_game.score >= g_game.highScores[g_game.mode] && g_game.score > 0) {
        const char *newBest = "NEW BEST!";
        int bestW = MeasureText(newBest, 22);
        float flash = sinf(GetTime() * 6) * 0.3f + 0.7f;
        DrawText(newBest, (int)(panel.x + panel.width/2 - bestW/2), (int)(panel.y + 185),
                 22, ColorAlpha(COLOR_WARNING, flash));
    }

    const char *hint = "Tap to return to menu";
    int hintW = MeasureText(hint, 16);
    DrawText(hint, (int)(panel.x + panel.width/2 - hintW/2), (int)(panel.y + 225), 16, COLOR_TEXT_DIM);
}

static void DrawPaused(void) {
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, ColorAlpha(BLACK, 0.6f));

    const char *text = "PAUSED";
    int textW = MeasureText(text, 48);
    DrawText(text, g_screenWidth/2 - textW/2, g_screenHeight/2 - 50, 48, COLOR_TEXT_PRIMARY);

    const char *hint = "Tap to resume | Back to menu";
    int hintW = MeasureText(hint, 18);
    DrawText(hint, g_screenWidth/2 - hintW/2, g_screenHeight/2 + 20, 18, COLOR_TEXT_MUTED);
}

// =============================================================================
// CONFIG
// =============================================================================

static void SaveConfig(void) {
    if (!g_configInitialized) return;
    for (int i = 0; i < MODE_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "high_score_%d", i);
        LlzPluginConfigSetInt(&g_config, key, g_game.highScores[i]);
    }
    LlzPluginConfigSave(&g_config);
}

static void LoadConfig(void) {
    if (!g_configInitialized) return;
    for (int i = 0; i < MODE_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "high_score_%d", i);
        g_game.highScores[i] = LlzPluginConfigGetInt(&g_config, key, 0);
    }
}

// =============================================================================
// INPUT HANDLING
// =============================================================================

static void HandleMenuInput(const LlzInputState *input, bool backJustReleased) {
    // Back button exits to host menu
    if (backJustReleased) {
        g_wantsClose = true;
        return;
    }

    // Navigation
    if (input->scrollDelta > 0.5f || input->swipeDown || input->downPressed) {
        g_anim.menuIndex = (g_anim.menuIndex + 1) % MODE_COUNT;
    }
    if (input->scrollDelta < -0.5f || input->swipeUp || input->upPressed) {
        g_anim.menuIndex = (g_anim.menuIndex - 1 + MODE_COUNT) % MODE_COUNT;
    }

    // Selection
    if (input->tap || input->selectPressed) {
        g_game.mode = (GameMode)g_anim.menuIndex;
        // Marathon mode goes to options screen first
        if (g_game.mode == MODE_MARATHON) {
            g_game.state = STATE_OPTIONS;
            g_game.optionSelected = 0;
        } else {
            GameReset();
        }
    }
}

static void HandleOptionsInput(const LlzInputState *input, bool backJustReleased) {
    // Back returns to menu
    if (backJustReleased) {
        g_game.state = STATE_MENU;
        return;
    }

    // Navigate between options (up/down or scroll)
    if (input->downPressed || input->scrollDelta > 0.5f) {
        g_game.optionSelected = (g_game.optionSelected + 1) % 3;  // 0=level, 1=goal, 2=start
    }
    if (input->upPressed || input->scrollDelta < -0.5f) {
        g_game.optionSelected = (g_game.optionSelected + 2) % 3;
    }

    // Adjust selected option value (left/right via swipe)
    if (g_game.optionSelected == 0) {
        // Level selection (0-19)
        if (input->swipeRight) {
            g_game.startLevel = (g_game.startLevel + 1) % 20;
        }
        if (input->swipeLeft) {
            g_game.startLevel = (g_game.startLevel + 19) % 20;
        }
    } else if (g_game.optionSelected == 1) {
        // Goal selection
        if (input->swipeRight) {
            g_game.marathonGoal = (MarathonGoal)((g_game.marathonGoal + 1) % MARATHON_GOAL_COUNT);
        }
        if (input->swipeLeft) {
            g_game.marathonGoal = (MarathonGoal)((g_game.marathonGoal + MARATHON_GOAL_COUNT - 1) % MARATHON_GOAL_COUNT);
        }
    }

    // Start game (tap on Start button or select button when on Start)
    if (input->tap || (input->selectPressed && g_game.optionSelected == 2)) {
        if (g_game.optionSelected == 2 || input->tap) {
            g_game.marathonLineTarget = MARATHON_GOAL_VALUES[g_game.marathonGoal];
            GameReset();
        }
    }

    // Select button changes value when on option rows
    if (input->selectPressed && g_game.optionSelected < 2) {
        if (g_game.optionSelected == 0) {
            g_game.startLevel = (g_game.startLevel + 1) % 20;
        } else {
            g_game.marathonGoal = (MarathonGoal)((g_game.marathonGoal + 1) % MARATHON_GOAL_COUNT);
        }
    }
}

static void HandlePlayInput(const LlzInputState *input, float dt, bool backJustReleased) {
    // New control scheme (Apotris-style):
    // Back = Hold piece
    // Select = Rotate clockwise
    // Up/Tap = Hard drop
    // Down = Rotate counter-clockwise
    // Swipe down = Hard drop
    // Hold down key = Soft drop (speed up)

    if (g_anim.clearingCount > 0) return;

    // Back button = Hold piece (during gameplay)
    if (backJustReleased) {
        HoldPiece();
        return;
    }

    // Tap or Select = Rotate clockwise
    if (input->selectPressed || (input->tap && !input->hold)) {
        TryRotate(1);
    }

    // Down = Rotate counter-clockwise
    if (input->downPressed) {
        TryRotate(-1);
    }

    // Up/SwipeDown = Hard drop
    if (input->upPressed || input->swipeDown) {
        HardDrop();
        return;
    }

    // Horizontal movement via scroll
    g_input.scrollAccum += input->scrollDelta;
    while (g_input.scrollAccum >= 1.0f) {
        TryMove(1, 0);
        g_input.scrollAccum -= 1.0f;
    }
    while (g_input.scrollAccum <= -1.0f) {
        TryMove(-1, 0);
        g_input.scrollAccum += 1.0f;
    }

    // Touch drag to move block - accumulate drag distance (more sensitive)
    if (input->dragActive) {
        float dragThreshold = 18.0f;  // Pixels per cell movement (more sensitive)
        g_anim.dragAccumX += input->dragDelta.x;

        while (g_anim.dragAccumX >= dragThreshold) {
            if (TryMove(1, 0)) {
                g_anim.gridMoveGlow = 0.3f;  // Light up grid on movement
            }
            g_anim.dragAccumX -= dragThreshold;
        }
        while (g_anim.dragAccumX <= -dragThreshold) {
            if (TryMove(-1, 0)) {
                g_anim.gridMoveGlow = 0.3f;
            }
            g_anim.dragAccumX += dragThreshold;
        }
    } else {
        g_anim.dragAccumX = 0;  // Reset when not dragging
    }

    // Quick flick/swipe to slam piece to edge
    if (input->swipeLeft) {
        int movesMade = 0;
        while (TryMove(-1, 0)) {
            movesMade++;
        }
        if (movesMade > 0) {
            // Directional shake to the left
            g_anim.dirShakeX = -8.0f;
            g_anim.dirShakeTimer = 0.15f;
            // Flash the piece so player can see where it went
            g_anim.pieceFlashTimer = 0.2f;
            // Flash left edge of grid
            g_anim.gridPulseLeft = 1.0f;
        }
    }
    if (input->swipeRight) {
        int movesMade = 0;
        while (TryMove(1, 0)) {
            movesMade++;
        }
        if (movesMade > 0) {
            // Directional shake to the right
            g_anim.dirShakeX = 8.0f;
            g_anim.dirShakeTimer = 0.15f;
            // Flash the piece
            g_anim.pieceFlashTimer = 0.2f;
            // Flash right edge of grid
            g_anim.gridPulseRight = 1.0f;
        }
    }

    // Keyboard
    if (IsKeyPressed(KEY_LEFT)) TryMove(-1, 0);
    if (IsKeyPressed(KEY_RIGHT)) TryMove(1, 0);
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X)) TryRotate(1);
    if (IsKeyPressed(KEY_Z)) TryRotate(-1);
    if (IsKeyPressed(KEY_SPACE)) HardDrop();
    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_SHIFT)) HoldPiece();

    // DAS
    int moveDir = 0;
    if (IsKeyDown(KEY_LEFT)) moveDir = -1;
    else if (IsKeyDown(KEY_RIGHT)) moveDir = 1;

    if (moveDir != 0) {
        if (moveDir != g_input.dasDirection) {
            g_input.dasDirection = moveDir;
            g_input.dasTimer = 0;
            g_input.arrTimer = 0;
        } else {
            g_input.dasTimer += dt;
            if (g_input.dasTimer >= g_input.das) {
                g_input.arrTimer += dt;
                while (g_input.arrTimer >= g_input.arr) {
                    TryMove(moveDir, 0);
                    g_input.arrTimer -= g_input.arr;
                }
            }
        }
    } else {
        g_input.dasDirection = 0;
        g_input.dasTimer = 0;
        g_input.arrTimer = 0;
    }

    // Soft drop
    g_input.softDropHeld = IsKeyDown(KEY_DOWN);
}

// =============================================================================
// PLUGIN CALLBACKS
// =============================================================================

static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;

    LlzPluginConfigEntry defaults[] = {
        {"high_score_0", "0"}, {"high_score_1", "0"},
        {"high_score_2", "0"}, {"high_score_3", "0"},
        {"high_score_4", "0"}, {"high_score_5", "0"}
    };
    g_configInitialized = LlzPluginConfigInit(&g_config, "llzblocks", defaults, 6);

    memset(&g_game, 0, sizeof(g_game));
    memset(&g_anim, 0, sizeof(g_anim));
    memset(&g_input, 0, sizeof(g_input));

    g_input.das = DEFAULT_DAS;
    g_input.arr = DEFAULT_ARR;

    LoadConfig();
    g_game.state = STATE_MENU;
    g_anim.menuIndex = 0;

    printf("[LLZBLOCKS] Initialized\n");
}

static void PluginUpdate(const LlzInputState *input, float dt) {
    UpdateParticles(dt);

    // Background animation time
    g_anim.bgTime += dt;

    // Use SDK's backReleased for proper release detection
    bool backJustReleased = input->backReleased;

    // Screen shake decay (smooth bounce like Apotris)
    if (g_anim.screenShake > 0) {
        g_anim.screenShake -= dt * 3;
        if (g_anim.screenShake < 0) g_anim.screenShake = 0;
        float shake = g_anim.screenShake * 10.0f;
        g_anim.screenShakeX = sinf(g_anim.bgTime * 50) * shake;
        g_anim.screenShakeY = cosf(g_anim.bgTime * 60) * shake * 0.7f;
    }

    // Timer updates
    if (g_anim.clearTextTimer > 0) g_anim.clearTextTimer -= dt;
    if (g_anim.lockFlashTimer > 0) g_anim.lockFlashTimer -= dt;
    if (g_anim.perfectClearTimer > 0) g_anim.perfectClearTimer -= dt;
    if (g_anim.pieceFlashTimer > 0) g_anim.pieceFlashTimer -= dt;

    // Grid lighting decay
    float gridDecay = dt * 5.0f;  // Fast decay
    if (g_anim.gridPulseLeft > 0) g_anim.gridPulseLeft -= gridDecay;
    if (g_anim.gridPulseRight > 0) g_anim.gridPulseRight -= gridDecay;
    if (g_anim.gridPulseRow > 0) g_anim.gridPulseRow -= gridDecay;
    if (g_anim.gridMoveGlow > 0) g_anim.gridMoveGlow -= gridDecay;

    // Directional shake decay (for flick moves)
    if (g_anim.dirShakeTimer > 0) {
        g_anim.dirShakeTimer -= dt;
        if (g_anim.dirShakeTimer <= 0) {
            g_anim.dirShakeX = 0;
        }
    }

    switch (g_game.state) {
        case STATE_MENU:
            HandleMenuInput(input, backJustReleased);
            break;

        case STATE_OPTIONS:
            HandleOptionsInput(input, backJustReleased);
            break;

        case STATE_READY:
            g_anim.readyTimer -= dt;
            if (g_anim.readyTimer <= 0) {
                g_game.state = STATE_PLAYING;
                SpawnPiece();
            }
            break;

        case STATE_PLAYING:
            // Long hold gesture = pause
            if (input->hold) {
                g_game.state = STATE_PAUSED;
                break;
            }
            HandlePlayInput(input, dt, backJustReleased);

            // Game time
            g_game.gameTime += dt;

            // Ultra time limit
            if ((g_game.mode == MODE_ULTRA_3 || g_game.mode == MODE_ULTRA_5) &&
                g_game.gameTime >= g_game.ultraTimeLimit) {
                g_game.state = STATE_COMPLETE;
                if (g_game.score > g_game.highScores[g_game.mode]) {
                    g_game.highScores[g_game.mode] = g_game.score;
                }
                SaveConfig();
            }

            // Line clear animation
            if (g_anim.clearingCount > 0) {
                g_anim.lineClearTimer -= dt;
                // Calculate progress 0 to 1 for directional wipe
                g_anim.lineClearProgress = 1.0f - (g_anim.lineClearTimer / LINE_CLEAR_TIME);
                if (g_anim.lineClearProgress > 1.0f) g_anim.lineClearProgress = 1.0f;
                if (g_anim.lineClearTimer <= 0) {
                    FinishLineClear();
                }
                break;
            }

            if (g_game.currentPiece == PIECE_NONE) break;

            // Gravity
            float speed = GetDropSpeed();
            if (g_input.softDropHeld) {
                speed *= 0.05f;
                g_game.score += 1;  // Soft drop score
            }

            g_game.dropTimer += dt;
            if (g_game.dropTimer >= speed) {
                g_game.dropTimer = 0;
                if (!TryMove(0, 1)) {
                    g_game.locking = true;
                }
            }

            // Lock delay
            if (g_game.locking) {
                if (!CheckCollision(g_game.currentPiece, g_game.currentX, g_game.currentY + 1, g_game.currentRotation)) {
                    g_game.locking = false;
                    g_game.lockTimer = 0;
                    g_game.lockMoves = 0;
                } else {
                    g_game.lockTimer += dt;
                    if (g_game.lockTimer >= LOCK_DELAY || g_game.lockMoves >= MAX_LOCK_MOVES) {
                        LockPiece();
                    }
                }
            }
            break;

        case STATE_PAUSED:
            // Tap or select resumes game
            if (input->tap || input->selectPressed) {
                g_game.state = STATE_PLAYING;
            }
            // Back button goes to menu
            if (backJustReleased) {
                g_game.state = STATE_MENU;
            }
            break;

        case STATE_GAMEOVER:
        case STATE_COMPLETE:
            // Any input goes back to menu
            if (input->tap || input->selectPressed || backJustReleased) {
                g_game.state = STATE_MENU;
            }
            break;
    }
}

static void PluginDraw(void) {
    // Animated background like Apotris
    DrawAnimatedBackground(g_anim.bgTime);

    // Apply screen shake (including directional shake from flicks)
    float totalShakeX = g_anim.screenShakeX + g_anim.dirShakeX * (g_anim.dirShakeTimer / 0.15f);
    float totalShakeY = g_anim.screenShakeY;
    if (g_anim.screenShake > 0 || g_anim.dirShakeTimer > 0) {
        rlPushMatrix();
        rlTranslatef(totalShakeX, totalShakeY, 0);
    }

    if (g_game.state == STATE_MENU) {
        DrawMenu();
    } else if (g_game.state == STATE_OPTIONS) {
        DrawOptions();
    } else {
        float blockSize = (g_screenHeight - 40) / (float)BOARD_HEIGHT;
        float boardW = BOARD_WIDTH * blockSize;
        float boardX = (g_screenWidth - boardW) / 2;
        float boardY = 20;

        DrawBoard(boardX, boardY, blockSize);
        DrawUI(boardX, boardY, blockSize);
        DrawParticles();

        if (g_game.state == STATE_READY) DrawReadyGo();
        if (g_game.state == STATE_PAUSED) DrawPaused();
        if (g_game.state == STATE_GAMEOVER || g_game.state == STATE_COMPLETE) DrawGameOver();
    }

    if (g_anim.screenShake > 0 || g_anim.dirShakeTimer > 0) {
        rlPopMatrix();
    }
}

static void PluginShutdown(void) {
    if (g_configInitialized) {
        SaveConfig();
        LlzPluginConfigFree(&g_config);
        g_configInitialized = false;
    }
    g_wantsClose = false;
    printf("[LLZBLOCKS] Shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

static LlzPluginAPI g_api = {
    .name = "LLZ Blocks",
    .description = "Block-stacking puzzle with Marathon, Sprint, Ultra & Zen modes",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .handles_back_button = true  // Back button = Hold piece during gameplay
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
