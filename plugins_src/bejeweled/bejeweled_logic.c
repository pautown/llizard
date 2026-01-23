/**
 * bejeweled_logic.c - Core game logic for a Bejeweled-style match-3 game
 *
 * This file contains only the game logic without any rendering or input handling.
 * It provides functions for board manipulation, match detection, gravity, and scoring.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * GAME CONSTANTS
 * ============================================================================ */

#define BOARD_WIDTH     8       /* Number of columns */
#define BOARD_HEIGHT    8       /* Number of rows */
#define GEM_TYPE_COUNT  7       /* Number of different gem colors/types */
#define SCREEN_WIDTH    800     /* Screen width in pixels */
#define SCREEN_HEIGHT   480     /* Screen height in pixels */

/* Gem type constants (0 = empty) */
#define GEM_EMPTY       0
#define GEM_RED         1
#define GEM_ORANGE      2
#define GEM_YELLOW      3
#define GEM_GREEN       4
#define GEM_BLUE        5
#define GEM_PURPLE      6
#define GEM_WHITE       7

/* Special gem type constants */
#define SPECIAL_NONE        0
#define SPECIAL_FLAME       1
#define SPECIAL_STAR        2
#define SPECIAL_HYPERCUBE   3
#define SPECIAL_SUPERNOVA   4
#define SPECIAL_COUNT       5

/* Max queued effects for chain reactions */
#define MAX_QUEUED_EFFECTS  16
#define MAX_PENDING_SPECIALS 16

/* Scoring constants */
#define SCORE_MATCH_3   50      /* Points for 3-gem match */
#define SCORE_MATCH_4   100     /* Points for 4-gem match */
#define SCORE_MATCH_5   200     /* Points for 5+ gem match */
#define CASCADE_BONUS   25      /* Bonus per cascade level */

/* Blitz mode constants */
#define BLITZ_TIME_LIMIT    60.0f   /* 60 seconds for Blitz mode */
#define BLITZ_SCORE_MULT    2       /* Score multiplier for Blitz mode */

/* CASCADE RUSH mode constants */
#define CASCADE_RUSH_START_TIME     30.0f   /* 30 seconds initial time */
#define RUSH_ZONE_SPAWN_INTERVAL    8.0f    /* Seconds between zone spawns */
#define RUSH_ZONE_DURATION          5.0f    /* Seconds zone stays active */
#define CASCADE_RUSH_SCORE_MULT     3       /* 3x cascade multiplier */

/* GEM SURGE mode constants */
#define SURGE_MAX_LINES     8       /* Maximum active surge lines */
#define SURGE_LINE_DURATION 8.0f    /* Duration surge lines stay active (seconds) */
#define SURGE_INITIAL_TIME  45.0f   /* Initial time for Gem Surge mode */
#define SURGE_INITIAL_TARGET 1000   /* Initial wave target score */

/* Game modes */
typedef enum {
    GAME_MODE_CLASSIC,
    GAME_MODE_BLITZ,
    GAME_MODE_TWIST,
    GAME_MODE_CASCADE_RUSH,
    GAME_MODE_GEM_SURGE,
    GAME_MODE_COUNT
} GameMode;

/* ============================================================================
 * GAME STATE STRUCTURES
 * ============================================================================ */

/* Game state enumeration */
typedef enum {
    GAME_STATE_IDLE,            /* Waiting for player input */
    GAME_STATE_SWAPPING,        /* Gems are swapping animation */
    GAME_STATE_CHECKING,        /* Checking for matches */
    GAME_STATE_REMOVING,        /* Matched gems are being removed */
    GAME_STATE_FALLING,         /* Gems are falling due to gravity */
    GAME_STATE_FILLING,         /* New gems are being added at top */
    GAME_STATE_GAME_OVER,       /* No valid moves remain */
    GAME_STATE_PAUSED           /* Game is paused */
} GameState;

/* Position structure for grid coordinates */
typedef struct {
    int x;
    int y;
} Position;

/* Match information structure */
typedef struct {
    Position positions[BOARD_WIDTH * BOARD_HEIGHT];  /* Positions of matched gems */
    int count;                                        /* Number of gems in this match */
    bool isHorizontal;                                /* True if horizontal match */
} MatchInfo;

/* Animation state for a single gem */
typedef struct {
    float offsetX;              /* Current X offset for animation */
    float offsetY;              /* Current Y offset for animation */
    float targetOffsetX;        /* Target X offset */
    float targetOffsetY;        /* Target Y offset */
    float scale;                /* Scale factor for spawn/remove animation */
    bool isRemoving;            /* True if gem is being removed */
    bool isSpawning;            /* True if gem is spawning */
    int fallDistance;           /* Number of cells this gem needs to fall */
} GemAnimation;

/* Pending special gem info */
typedef struct {
    int x, y;
    int color;
    int specialType;
} PendingSpecialGem;

/* Queued effect for chain reactions */
typedef struct {
    int x, y;
    int effectType;
    int targetColor;
    bool active;
} QueuedEffect;

/* Main game state structure */
typedef struct {
    /* Board state - gem types at each position (0 = empty) */
    int board[BOARD_HEIGHT][BOARD_WIDTH];

    /* Special gem types at each position (SPECIAL_NONE = normal) */
    int special[BOARD_HEIGHT][BOARD_WIDTH];

    /* Animation states for each gem */
    GemAnimation animations[BOARD_HEIGHT][BOARD_WIDTH];

    /* Currently selected gem position (-1, -1 if none) */
    Position selectedGem;

    /* Second gem position for swap operation */
    Position swapGem;

    /* Scoring and progression */
    int score;
    int level;
    int movesRemaining;         /* For move-limited mode */
    float timeRemaining;        /* For time-limited mode */

    /* Cascade tracking */
    int cascadeLevel;           /* Current cascade depth */
    int cascadeScore;           /* Score accumulated in current cascade */

    /* Current game state */
    GameState state;

    /* Match tracking for current frame */
    MatchInfo matches[BOARD_WIDTH * BOARD_HEIGHT];
    int matchCount;

    /* Statistics */
    int totalMatches;
    int largestMatch;
    int longestCascade;
    int gemsDestroyed;

    /* Lightning strike tracking for 5+ matches */
    bool lightningActive;
    bool lightningHorizontal;
    int lightningRow;
    int lightningCol;
    int lightningCenterX;
    int lightningCenterY;

    /* Game mode */
    GameMode gameMode;

    /* Pending special gems (to be created after match removal) */
    PendingSpecialGem pendingSpecials[MAX_PENDING_SPECIALS];
    int pendingSpecialCount;

    /* Queued effects for chain reactions */
    QueuedEffect queuedEffects[MAX_QUEUED_EFFECTS];
    int queuedEffectCount;

    /* CASCADE RUSH Mode State */
    bool rushZoneActive;
    int rushZoneX, rushZoneY;
    float rushZoneTimer;
    float rushZoneMaxDuration;  /* Max duration for this zone (for progress bar) */
    float rushZoneSpawnTimer;
    int rushZonesCaptured;
    float timeAddedFromCascades;

    /* GEM SURGE Mode State */
    int surgeCurrentWave;
    int surgeWaveTarget;
    int surgeWaveScore;
    int surgeFeaturedGem;
    bool surgeLines[SURGE_MAX_LINES];
    bool surgeLinesHorizontal[SURGE_MAX_LINES];
    int surgeLinePositions[SURGE_MAX_LINES];
    float surgeLineTimers[SURGE_MAX_LINES];
    int surgeActiveLineCount;
    int surgeLinesCleared;

} BejeweledState;

/* ============================================================================
 * GLOBAL GAME STATE
 * ============================================================================ */

static BejeweledState g_game;
static bool g_initialized = false;

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

static int GetRandomGemType(void);
static bool HasMatchAt(int x, int y);
static bool HasHorizontalMatchAt(int x, int y, int minLength);
static bool HasVerticalMatchAt(int x, int y, int minLength);
static void ClearMatchInfo(void);
static int CalculateMatchScore(int matchLength, int cascadeLevel, int gemType);

/* Forward declarations for Twist mode */
bool CheckTwistGameOver(void);

/* Forward declarations for CASCADE RUSH mode */
void InitCascadeRush(void);
void SpawnRushZone(void);

/* Forward declarations for GEM SURGE mode */
static void InitGemSurge(void);
static void AdvanceToNextWave(void);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * Get a random gem type (1 to GEM_TYPE_COUNT)
 */
static int GetRandomGemType(void)
{
    return (rand() % GEM_TYPE_COUNT) + 1;
}

/**
 * Check if position is within board bounds
 */
static bool IsValidPosition(int x, int y)
{
    return x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT;
}

/**
 * Check if two positions are adjacent (horizontally or vertically)
 */
static bool AreAdjacent(Position a, Position b)
{
    int dx = abs(a.x - b.x);
    int dy = abs(a.y - b.y);
    return (dx == 1 && dy == 0) || (dx == 0 && dy == 1);
}

/**
 * Get gem type at position, returns GEM_EMPTY if out of bounds
 */
static int GetGemAt(int x, int y)
{
    if (!IsValidPosition(x, y)) {
        return GEM_EMPTY;
    }
    return g_game.board[y][x];
}

/**
 * Set gem type at position
 */
static void SetGemAt(int x, int y, int gemType)
{
    if (IsValidPosition(x, y)) {
        g_game.board[y][x] = gemType;
    }
}

/* ============================================================================
 * MATCH DETECTION
 * ============================================================================ */

/**
 * Check if there's a horizontal match of at least minLength at position (x, y)
 * Returns the length of the match (0 if no match)
 */
static int GetHorizontalMatchLength(int x, int y)
{
    int gemType = GetGemAt(x, y);
    if (gemType == GEM_EMPTY) {
        return 0;
    }

    /* Find leftmost gem of same type */
    int left = x;
    while (left > 0 && GetGemAt(left - 1, y) == gemType) {
        left--;
    }

    /* Find rightmost gem of same type */
    int right = x;
    while (right < BOARD_WIDTH - 1 && GetGemAt(right + 1, y) == gemType) {
        right++;
    }

    return right - left + 1;
}

/**
 * Check if there's a vertical match of at least minLength at position (x, y)
 * Returns the length of the match (0 if no match)
 */
static int GetVerticalMatchLength(int x, int y)
{
    int gemType = GetGemAt(x, y);
    if (gemType == GEM_EMPTY) {
        return 0;
    }

    /* Find topmost gem of same type */
    int top = y;
    while (top > 0 && GetGemAt(x, top - 1) == gemType) {
        top--;
    }

    /* Find bottommost gem of same type */
    int bottom = y;
    while (bottom < BOARD_HEIGHT - 1 && GetGemAt(x, bottom + 1) == gemType) {
        bottom++;
    }

    return bottom - top + 1;
}

/**
 * Check if there's a horizontal match of at least minLength starting at (x, y)
 */
static bool HasHorizontalMatchAt(int x, int y, int minLength)
{
    return GetHorizontalMatchLength(x, y) >= minLength;
}

/**
 * Check if there's a vertical match of at least minLength starting at (x, y)
 */
static bool HasVerticalMatchAt(int x, int y, int minLength)
{
    return GetVerticalMatchLength(x, y) >= minLength;
}

/**
 * Check if there's any match (horizontal or vertical) of 3+ at position
 */
static bool HasMatchAt(int x, int y)
{
    return HasHorizontalMatchAt(x, y, 3) || HasVerticalMatchAt(x, y, 3);
}

/**
 * Clear the match tracking arrays
 */
static void ClearMatchInfo(void)
{
    g_game.matchCount = 0;
    memset(g_game.matches, 0, sizeof(g_game.matches));
}

/**
 * Check entire board for matches and populate match info
 * Returns the total number of matched positions
 */
int CheckMatches(void)
{
    ClearMatchInfo();

    /* Clear any previous lightning info */
    g_game.lightningActive = false;

    /* Track which positions have been marked as matched */
    bool matched[BOARD_HEIGHT][BOARD_WIDTH];
    memset(matched, false, sizeof(matched));

    /* Check for horizontal matches */
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH - 2; x++) {
            int gemType = GetGemAt(x, y);
            if (gemType == GEM_EMPTY) {
                continue;
            }

            /* Count consecutive gems of same type */
            int matchLen = 1;
            while (x + matchLen < BOARD_WIDTH && GetGemAt(x + matchLen, y) == gemType) {
                matchLen++;
            }

            if (matchLen >= 3) {
                /* Record this horizontal match */
                MatchInfo *match = &g_game.matches[g_game.matchCount];
                match->count = 0;
                match->isHorizontal = true;

                for (int i = 0; i < matchLen; i++) {
                    if (!matched[y][x + i]) {
                        matched[y][x + i] = true;
                        match->positions[match->count].x = x + i;
                        match->positions[match->count].y = y;
                        match->count++;
                    }
                }

                if (match->count > 0) {
                    g_game.matchCount++;
                }

                /* 5+ match triggers lightning strike through center column */
                if (matchLen >= 5 && !g_game.lightningActive) {
                    g_game.lightningActive = true;
                    g_game.lightningHorizontal = false;  /* Strike goes vertical (up/down) */
                    g_game.lightningCol = x + matchLen / 2;  /* Center column of match */
                    g_game.lightningRow = y;
                    g_game.lightningCenterX = x + matchLen / 2;
                    g_game.lightningCenterY = y;
                }

                /* Skip past this match */
                x += matchLen - 1;
            }
        }
    }

    /* Check for vertical matches */
    for (int x = 0; x < BOARD_WIDTH; x++) {
        for (int y = 0; y < BOARD_HEIGHT - 2; y++) {
            int gemType = GetGemAt(x, y);
            if (gemType == GEM_EMPTY) {
                continue;
            }

            /* Count consecutive gems of same type */
            int matchLen = 1;
            while (y + matchLen < BOARD_HEIGHT && GetGemAt(x, y + matchLen) == gemType) {
                matchLen++;
            }

            if (matchLen >= 3) {
                /* Record this vertical match */
                MatchInfo *match = &g_game.matches[g_game.matchCount];
                match->count = 0;
                match->isHorizontal = false;

                for (int i = 0; i < matchLen; i++) {
                    if (!matched[y + i][x]) {
                        matched[y + i][x] = true;
                        match->positions[match->count].x = x;
                        match->positions[match->count].y = y + i;
                        match->count++;
                    }
                }

                if (match->count > 0) {
                    g_game.matchCount++;
                }

                /* 5+ match triggers lightning strike through center row */
                if (matchLen >= 5 && !g_game.lightningActive) {
                    g_game.lightningActive = true;
                    g_game.lightningHorizontal = true;  /* Strike goes horizontal (left/right) */
                    g_game.lightningRow = y + matchLen / 2;  /* Center row of match */
                    g_game.lightningCol = x;
                    g_game.lightningCenterX = x;
                    g_game.lightningCenterY = y + matchLen / 2;
                }

                /* Skip past this match */
                y += matchLen - 1;
            }
        }
    }

    /* Count total matched positions */
    int totalMatched = 0;
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (matched[y][x]) {
                totalMatched++;
            }
        }
    }

    return totalMatched;
}

/* ============================================================================
 * SCORING
 * ============================================================================ */

/**
 * Get the score threshold required to reach a given level
 * Level 1 = 0 points
 * Level 2 = 1000 points
 * Level 3 = 2500 points (1000 + 1500)
 * Level 4 = 4500 points (2500 + 2000)
 * etc. (increases by 500 each level)
 */
static int GetScoreForLevel(int level)
{
    if (level <= 1) return 0;

    /* Sum of arithmetic series: 1000 + 1500 + 2000 + ... */
    /* First term a=1000, common difference d=500 */
    /* For level n, we need sum of first (n-1) terms */
    int n = level - 1;
    /* Sum = n/2 * (2*a + (n-1)*d) = n/2 * (2000 + (n-1)*500) */
    return n * (2000 + (n - 1) * 500) / 2;
}

/**
 * Update the level based on current score
 */
static void UpdateLevel(void)
{
    int oldLevel = g_game.level;

    /* Find the highest level the current score qualifies for */
    while (g_game.score >= GetScoreForLevel(g_game.level + 1)) {
        g_game.level++;
    }

    /* Cap at a reasonable max level */
    if (g_game.level > 99) {
        g_game.level = 99;
    }
}

/**
 * Calculate score for a match based on length and cascade level
 * Score = baseScore * matchLength * comboMultiplier * modeMultiplier
 */
static int CalculateMatchScore(int matchLength, int cascadeLevel, int gemType)
{
    int baseScore;

    if (matchLength >= 5) {
        baseScore = SCORE_MATCH_5;
    } else if (matchLength == 4) {
        baseScore = SCORE_MATCH_4;
    } else {
        baseScore = SCORE_MATCH_3;
    }

    /* Apply combo multiplier (cascade=1 is 1x, cascade=2 is 2x, etc.) */
    int comboMultiplier = (cascadeLevel < 1) ? 1 : cascadeLevel;

    /* Apply mode multiplier */
    int modeMultiplier = 1;
    if (g_game.gameMode == GAME_MODE_BLITZ) {
        modeMultiplier = BLITZ_SCORE_MULT;
    } else if (g_game.gameMode == GAME_MODE_CASCADE_RUSH) {
        /* CASCADE RUSH: 3x cascade multiplier */
        modeMultiplier = CASCADE_RUSH_SCORE_MULT;
    }

    /* Calculate base score */
    int score = baseScore * matchLength * comboMultiplier * modeMultiplier / 3;

    /* GEM SURGE: Apply 2x bonus for featured gem matches */
    if (g_game.gameMode == GAME_MODE_GEM_SURGE && gemType == g_game.surgeFeaturedGem) {
        score *= 2;
    }

    return score;
}

/**
 * Remove matched gems from the board and calculate score
 * Returns the score earned from this removal
 */
int RemoveMatches(void)
{
    if (g_game.matchCount == 0) {
        return 0;
    }

    int scoreEarned = 0;
    int gemsRemoved = 0;

    /* First, analyze matches for special gem creation */
    AnalyzeForSpecialGems();

    /* Process each match */
    for (int m = 0; m < g_game.matchCount; m++) {
        MatchInfo *match = &g_game.matches[m];

        /* Get gem type from first position in match */
        int matchGemType = GEM_EMPTY;
        if (match->count > 0) {
            matchGemType = GetGemAt(match->positions[0].x, match->positions[0].y);
        }

        /* Calculate score for this match */
        int matchScore = CalculateMatchScore(match->count, g_game.cascadeLevel, matchGemType);
        scoreEarned += matchScore;

        /* Track statistics */
        if (match->count > g_game.largestMatch) {
            g_game.largestMatch = match->count;
        }

        /* Remove gems and set animation state */
        for (int i = 0; i < match->count; i++) {
            int x = match->positions[i].x;
            int y = match->positions[i].y;

            /* Check if this position should become a special gem */
            bool becomesSpecial = false;
            int specialType = SPECIAL_NONE;
            int gemColor = GetGemAt(x, y);

            for (int p = 0; p < g_game.pendingSpecialCount; p++) {
                if (g_game.pendingSpecials[p].x == x && g_game.pendingSpecials[p].y == y) {
                    becomesSpecial = true;
                    specialType = g_game.pendingSpecials[p].specialType;
                    gemColor = g_game.pendingSpecials[p].color;
                    break;
                }
            }

            if (GetGemAt(x, y) != GEM_EMPTY) {
                /* Check if this gem is already special - queue its effect */
                int existingSpecial = GetBoardSpecial(x, y);
                if (existingSpecial != SPECIAL_NONE && !becomesSpecial) {
                    QueueSpecialEffect(x, y, existingSpecial, GetGemAt(x, y));
                }

                if (becomesSpecial) {
                    /* Create special gem instead of removing */
                    SetGemAt(x, y, gemColor);
                    SetBoardSpecial(x, y, specialType);
                    g_game.animations[y][x].isSpawning = true;
                    g_game.animations[y][x].scale = 0.0f;
                } else {
                    /* Normal removal */
                    SetGemAt(x, y, GEM_EMPTY);
                    SetBoardSpecial(x, y, SPECIAL_NONE);
                    g_game.animations[y][x].isRemoving = true;
                    g_game.animations[y][x].scale = 1.0f;
                    gemsRemoved++;
                }
            }
        }
    }

    /* Clear pending specials */
    g_game.pendingSpecialCount = 0;

    /* Update game state */
    g_game.score += scoreEarned;
    g_game.cascadeScore += scoreEarned;
    g_game.gemsDestroyed += gemsRemoved;
    g_game.totalMatches += g_game.matchCount;

    /* GEM SURGE: Add to wave score */
    if (g_game.gameMode == GAME_MODE_GEM_SURGE) {
        g_game.surgeWaveScore += scoreEarned;
    }

    /* Check for level up */
    UpdateLevel();

    /* Clear match info for next check */
    ClearMatchInfo();

    return scoreEarned;
}

/* ============================================================================
 * GRAVITY AND FILLING
 * ============================================================================ */

/**
 * Apply gravity - make gems fall down to fill empty spaces
 * Returns true if any gems moved
 */
bool ApplyGravity(void)
{
    bool anyMoved = false;

    /* Process each column from bottom to top */
    for (int x = 0; x < BOARD_WIDTH; x++) {
        /* Start from second-to-bottom row and work up */
        int writePos = BOARD_HEIGHT - 1;

        /* Find the bottom-most empty space */
        while (writePos >= 0 && GetGemAt(x, writePos) != GEM_EMPTY) {
            writePos--;
        }

        /* Now scan from writePos upward, moving gems down */
        for (int readPos = writePos - 1; readPos >= 0; readPos--) {
            int gemType = GetGemAt(x, readPos);

            if (gemType != GEM_EMPTY) {
                /* Move this gem down to writePos */
                SetGemAt(x, writePos, gemType);
                SetGemAt(x, readPos, GEM_EMPTY);

                /* Set up falling animation */
                int fallDist = writePos - readPos;
                g_game.animations[writePos][x].fallDistance = fallDist;
                g_game.animations[writePos][x].offsetY = (float)(-fallDist);
                g_game.animations[writePos][x].targetOffsetY = 0.0f;

                /* Clear the animation at old position */
                memset(&g_game.animations[readPos][x], 0, sizeof(GemAnimation));

                anyMoved = true;
                writePos--;
            }
        }
    }

    return anyMoved;
}

/**
 * Fill empty spaces at the top of each column with new random gems
 * Returns the number of new gems added
 */
int FillBoard(void)
{
    int newGems = 0;

    for (int x = 0; x < BOARD_WIDTH; x++) {
        /* Count empty spaces from top */
        int emptyCount = 0;
        for (int y = 0; y < BOARD_HEIGHT; y++) {
            if (GetGemAt(x, y) == GEM_EMPTY) {
                emptyCount++;
            } else {
                break;  /* Stop at first non-empty */
            }
        }

        /* Fill empty spaces from top with new gems */
        for (int i = 0; i < emptyCount; i++) {
            int y = emptyCount - 1 - i;  /* Fill from bottom of empty section */
            int newGem = GetRandomGemType();
            SetGemAt(x, y, newGem);

            /* Set up falling animation (gems fall from above screen) */
            g_game.animations[y][x].isSpawning = true;
            g_game.animations[y][x].fallDistance = emptyCount;
            g_game.animations[y][x].offsetY = (float)(-(emptyCount - i));
            g_game.animations[y][x].targetOffsetY = 0.0f;
            g_game.animations[y][x].scale = 1.0f;

            newGems++;
        }
    }

    return newGems;
}

/* ============================================================================
 * GEM SWAPPING
 * ============================================================================ */

/**
 * Check if swapping gems at (x1,y1) and (x2,y2) would create a match
 * Does not modify the board
 */
bool IsValidSwap(int x1, int y1, int x2, int y2)
{
    /* Validate positions */
    if (!IsValidPosition(x1, y1) || !IsValidPosition(x2, y2)) {
        return false;
    }

    /* Check adjacency */
    Position p1 = {x1, y1};
    Position p2 = {x2, y2};
    if (!AreAdjacent(p1, p2)) {
        return false;
    }

    /* Temporarily swap gems */
    int gem1 = GetGemAt(x1, y1);
    int gem2 = GetGemAt(x2, y2);

    if (gem1 == GEM_EMPTY || gem2 == GEM_EMPTY) {
        return false;
    }

    SetGemAt(x1, y1, gem2);
    SetGemAt(x2, y2, gem1);

    /* Check if either position now has a match */
    bool hasMatch = HasMatchAt(x1, y1) || HasMatchAt(x2, y2);

    /* Restore original state */
    SetGemAt(x1, y1, gem1);
    SetGemAt(x2, y2, gem2);

    return hasMatch;
}

/**
 * Swap two adjacent gems
 * Returns true if the swap was valid and executed
 */
bool SwapGems(int x1, int y1, int x2, int y2)
{
    /* Validate the swap would create a match */
    if (!IsValidSwap(x1, y1, x2, y2)) {
        return false;
    }

    /* Perform the swap */
    int gem1 = GetGemAt(x1, y1);
    int gem2 = GetGemAt(x2, y2);

    SetGemAt(x1, y1, gem2);
    SetGemAt(x2, y2, gem1);

    /* Set up swap animation */
    float dx = (float)(x2 - x1);
    float dy = (float)(y2 - y1);

    g_game.animations[y1][x1].offsetX = dx;
    g_game.animations[y1][x1].offsetY = dy;
    g_game.animations[y1][x1].targetOffsetX = 0.0f;
    g_game.animations[y1][x1].targetOffsetY = 0.0f;

    g_game.animations[y2][x2].offsetX = -dx;
    g_game.animations[y2][x2].offsetY = -dy;
    g_game.animations[y2][x2].targetOffsetX = 0.0f;
    g_game.animations[y2][x2].targetOffsetY = 0.0f;

    /* Reset cascade tracking for new move */
    g_game.cascadeLevel = 0;
    g_game.cascadeScore = 0;

    /* Decrement moves if in move-limited mode */
    if (g_game.movesRemaining > 0) {
        g_game.movesRemaining--;
    }

    return true;
}

/**
 * Attempt to swap gems but revert if no match is created
 * Used for animation feedback on invalid moves
 * Returns true if swap was valid, false if reverted
 */
bool TrySwapGems(int x1, int y1, int x2, int y2)
{
    /* Validate positions and adjacency */
    if (!IsValidPosition(x1, y1) || !IsValidPosition(x2, y2)) {
        return false;
    }

    Position p1 = {x1, y1};
    Position p2 = {x2, y2};
    if (!AreAdjacent(p1, p2)) {
        return false;
    }

    int gem1 = GetGemAt(x1, y1);
    int gem2 = GetGemAt(x2, y2);

    if (gem1 == GEM_EMPTY || gem2 == GEM_EMPTY) {
        return false;
    }

    /* This function is used for the swap animation even on invalid moves */
    /* The actual validity is determined by SwapGems */
    return SwapGems(x1, y1, x2, y2);
}

/* ============================================================================
 * GAME OVER DETECTION
 * ============================================================================ */

/**
 * Check if any valid moves remain on the board
 * Returns true if the game is over (no valid moves)
 */
bool CheckGameOver(void)
{
    /* Check every possible swap */
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            /* Check horizontal swap (with gem to the right) */
            if (x < BOARD_WIDTH - 1) {
                if (IsValidSwap(x, y, x + 1, y)) {
                    return false;  /* Found a valid move */
                }
            }

            /* Check vertical swap (with gem below) */
            if (y < BOARD_HEIGHT - 1) {
                if (IsValidSwap(x, y, x, y + 1)) {
                    return false;  /* Found a valid move */
                }
            }
        }
    }

    /* Also check if time or moves have run out */
    if (g_game.movesRemaining == 0 && g_game.movesRemaining != -1) {
        return true;
    }

    if (g_game.timeRemaining <= 0.0f && g_game.timeRemaining != -1.0f) {
        return true;
    }

    return true;  /* No valid moves found */
}

/**
 * Count the number of valid moves available
 */
int CountValidMoves(void)
{
    int count = 0;

    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (x < BOARD_WIDTH - 1 && IsValidSwap(x, y, x + 1, y)) {
                count++;
            }
            if (y < BOARD_HEIGHT - 1 && IsValidSwap(x, y, x, y + 1)) {
                count++;
            }
        }
    }

    return count;
}

/* ============================================================================
 * BOARD INITIALIZATION
 * ============================================================================ */

/**
 * Generate a random gem that won't create a match at position (x, y)
 */
static int GetSafeGemAt(int x, int y)
{
    int attempts = 0;
    int maxAttempts = 100;

    while (attempts < maxAttempts) {
        int gemType = GetRandomGemType();

        /* Temporarily place gem */
        SetGemAt(x, y, gemType);

        /* Check if it creates a match */
        bool createsMatch = false;

        /* Check horizontal (need at least 2 gems to the left of same type) */
        if (x >= 2) {
            if (GetGemAt(x - 1, y) == gemType && GetGemAt(x - 2, y) == gemType) {
                createsMatch = true;
            }
        }

        /* Check vertical (need at least 2 gems above of same type) */
        if (y >= 2) {
            if (GetGemAt(x, y - 1) == gemType && GetGemAt(x, y - 2) == gemType) {
                createsMatch = true;
            }
        }

        if (!createsMatch) {
            return gemType;  /* This gem is safe */
        }

        /* Clear and try again */
        SetGemAt(x, y, GEM_EMPTY);
        attempts++;
    }

    /* Fallback: return any gem (shouldn't happen often) */
    return GetRandomGemType();
}

/**
 * Internal function to initialize board (shared by all init functions)
 */
static void InitBoardInternal(void)
{
    /* Seed random number generator if not already done */
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    /* Clear entire game state (preserves mode-specific settings set before) */
    GameMode savedMode = g_game.gameMode;
    int savedMoves = g_game.movesRemaining;
    float savedTime = g_game.timeRemaining;

    memset(&g_game, 0, sizeof(g_game));

    /* Restore mode-specific settings */
    g_game.gameMode = savedMode;
    g_game.movesRemaining = savedMoves;
    g_game.timeRemaining = savedTime;

    /* Initialize selection to none */
    g_game.selectedGem.x = -1;
    g_game.selectedGem.y = -1;
    g_game.swapGem.x = -1;
    g_game.swapGem.y = -1;

    /* Fill board with random gems, ensuring no initial matches */
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            int safeGem = GetSafeGemAt(x, y);
            SetGemAt(x, y, safeGem);

            /* Initialize animation state */
            g_game.animations[y][x].scale = 1.0f;
        }
    }

    /* Verify no matches exist (defensive check) */
    while (CheckMatches() > 0) {
        /* If somehow matches exist, remove and regenerate */
        RemoveMatches();
        ApplyGravity();
        FillBoard();
    }

    /* Ensure at least one valid move exists based on game mode */
    bool gameOver = (g_game.gameMode == GAME_MODE_TWIST) ? CheckTwistGameOver() : CheckGameOver();
    if (gameOver) {
        /* Rare case: regenerate entire board */
        InitBoardInternal();
        return;
    }

    /* Set initial state */
    g_game.state = GAME_STATE_IDLE;
    g_game.level = 1;

    g_initialized = true;
}

/**
 * Initialize the game board with random gems, ensuring no initial matches.
 * Uses Classic mode by default.
 */
void InitGame(void)
{
    g_game.gameMode = GAME_MODE_CLASSIC;
    g_game.movesRemaining = -1;    /* -1 = unlimited */
    g_game.timeRemaining = -1.0f;  /* -1 = unlimited */
    InitBoardInternal();
}

/**
 * Initialize game with a specific game mode.
 */
void InitGameMode(GameMode mode)
{
    g_game.gameMode = mode;

    switch (mode) {
        case GAME_MODE_CLASSIC:
            g_game.movesRemaining = -1;
            g_game.timeRemaining = -1.0f;
            break;

        case GAME_MODE_BLITZ:
            g_game.movesRemaining = -1;
            g_game.timeRemaining = BLITZ_TIME_LIMIT;
            break;

        case GAME_MODE_TWIST:
            g_game.movesRemaining = -1;
            g_game.timeRemaining = -1.0f;
            break;

        case GAME_MODE_CASCADE_RUSH:
            g_game.movesRemaining = -1;
            g_game.timeRemaining = CASCADE_RUSH_START_TIME;
            break;

        case GAME_MODE_GEM_SURGE:
            g_game.movesRemaining = -1;
            g_game.timeRemaining = SURGE_INITIAL_TIME;
            break;

        default:
            g_game.movesRemaining = -1;
            g_game.timeRemaining = -1.0f;
            break;
    }

    InitBoardInternal();

    /* Mode-specific post-initialization */
    if (mode == GAME_MODE_CASCADE_RUSH) {
        InitCascadeRush();
    } else if (mode == GAME_MODE_GEM_SURGE) {
        InitGemSurge();
    }
}

/**
 * Get the current game mode.
 */
GameMode GetCurrentGameMode(void)
{
    return g_game.gameMode;
}

/**
 * Initialize game with specific mode settings
 */
void InitGameWithMode(int moves, float timeLimit)
{
    InitGame();
    g_game.movesRemaining = moves;
    g_game.timeRemaining = timeLimit;
}

/* ============================================================================
 * CASCADE HANDLING
 * ============================================================================ */

/**
 * Increment cascade level (called after each gravity/fill cycle finds more matches)
 */
void IncrementCascade(void)
{
    g_game.cascadeLevel++;
    if (g_game.cascadeLevel > g_game.longestCascade) {
        g_game.longestCascade = g_game.cascadeLevel;
    }
}

/**
 * Reset cascade tracking (called when player makes a new move)
 */
void ResetCascade(void)
{
    g_game.cascadeLevel = 0;
    g_game.cascadeScore = 0;
}

/* ============================================================================
 * STATE ACCESSORS
 * ============================================================================ */

/**
 * Get current game state
 */
GameState GetGameState(void)
{
    return g_game.state;
}

/**
 * Set game state
 */
void SetGameState(GameState state)
{
    g_game.state = state;
}

/**
 * Get current score
 */
int GetScore(void)
{
    return g_game.score;
}

/**
 * Get current level
 */
int GetLevel(void)
{
    return g_game.level;
}

/**
 * Get level progress information for HUD display
 */
void GetLevelProgress(int *currentLevelScore, int *nextLevelScore)
{
    if (currentLevelScore) {
        *currentLevelScore = GetScoreForLevel(g_game.level);
    }
    if (nextLevelScore) {
        *nextLevelScore = GetScoreForLevel(g_game.level + 1);
    }
}

/**
 * Get remaining moves (-1 if unlimited)
 */
int GetMovesRemaining(void)
{
    return g_game.movesRemaining;
}

/**
 * Get remaining time (-1 if unlimited)
 */
float GetTimeRemaining(void)
{
    return g_game.timeRemaining;
}

/**
 * Update time remaining
 */
void UpdateTime(float deltaTime)
{
    if (g_game.timeRemaining > 0.0f) {
        g_game.timeRemaining -= deltaTime;
        if (g_game.timeRemaining < 0.0f) {
            g_game.timeRemaining = 0.0f;
        }
    }
}

void AddTime(float seconds)
{
    g_game.timeRemaining += seconds;
}

/**
 * Get cascade level
 */
int GetCascadeLevel(void)
{
    return g_game.cascadeLevel;
}

/**
 * Get selected gem position
 */
Position GetSelectedGem(void)
{
    return g_game.selectedGem;
}

/**
 * Set selected gem position
 */
void SetSelectedGem(int x, int y)
{
    g_game.selectedGem.x = x;
    g_game.selectedGem.y = y;
}

/**
 * Clear gem selection
 */
void ClearSelection(void)
{
    g_game.selectedGem.x = -1;
    g_game.selectedGem.y = -1;
}

/**
 * Check if a gem is selected
 */
bool HasSelection(void)
{
    return g_game.selectedGem.x >= 0 && g_game.selectedGem.y >= 0;
}

/**
 * Get the gem type at a position (for rendering)
 */
int GetBoardGem(int x, int y)
{
    return GetGemAt(x, y);
}

/**
 * Get the special type at a position
 */
int GetBoardSpecial(int x, int y)
{
    if (!IsValidPosition(x, y)) {
        return SPECIAL_NONE;
    }
    return g_game.special[y][x];
}

/**
 * Set the special type at a position
 */
void SetBoardSpecial(int x, int y, int specialType)
{
    if (IsValidPosition(x, y)) {
        g_game.special[y][x] = specialType;
    }
}

/**
 * Check if a gem at position is special
 */
bool IsSpecialGem(int x, int y)
{
    return GetBoardSpecial(x, y) != SPECIAL_NONE;
}

/**
 * Check if position has a Hypercube
 */
bool IsHypercube(int x, int y)
{
    return GetBoardSpecial(x, y) == SPECIAL_HYPERCUBE;
}

/**
 * Get animation state for a gem (for rendering)
 */
GemAnimation* GetGemAnimation(int x, int y)
{
    if (!IsValidPosition(x, y)) {
        return NULL;
    }
    return &g_game.animations[y][x];
}

/**
 * Check if game is initialized
 */
bool IsGameInitialized(void)
{
    return g_initialized;
}

/**
 * Get game statistics
 */
void GetGameStats(int *totalMatches, int *largestMatch, int *longestCascade, int *gemsDestroyed)
{
    if (totalMatches) *totalMatches = g_game.totalMatches;
    if (largestMatch) *largestMatch = g_game.largestMatch;
    if (longestCascade) *longestCascade = g_game.longestCascade;
    if (gemsDestroyed) *gemsDestroyed = g_game.gemsDestroyed;
}

/* ============================================================================
 * ANIMATION HELPERS
 * ============================================================================ */

/**
 * Clear all animation states (call after animations complete)
 */
void ClearAnimations(void)
{
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            g_game.animations[y][x].offsetX = 0.0f;
            g_game.animations[y][x].offsetY = 0.0f;
            g_game.animations[y][x].targetOffsetX = 0.0f;
            g_game.animations[y][x].targetOffsetY = 0.0f;
            g_game.animations[y][x].scale = 1.0f;
            g_game.animations[y][x].isRemoving = false;
            g_game.animations[y][x].isSpawning = false;
            g_game.animations[y][x].fallDistance = 0;
        }
    }
}

/**
 * Check if any animations are currently active
 */
bool HasActiveAnimations(void)
{
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            GemAnimation *anim = &g_game.animations[y][x];
            if (anim->offsetX != 0.0f || anim->offsetY != 0.0f ||
                anim->isRemoving || anim->isSpawning) {
                return true;
            }
        }
    }
    return false;
}

/* ============================================================================
 * HINT SYSTEM
 * ============================================================================ */

/**
 * Find a valid move and return the positions
 * Returns true if a hint was found
 */
bool GetHint(int *x1, int *y1, int *x2, int *y2)
{
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            /* Check horizontal swap */
            if (x < BOARD_WIDTH - 1 && IsValidSwap(x, y, x + 1, y)) {
                *x1 = x;
                *y1 = y;
                *x2 = x + 1;
                *y2 = y;
                return true;
            }

            /* Check vertical swap */
            if (y < BOARD_HEIGHT - 1 && IsValidSwap(x, y, x, y + 1)) {
                *x1 = x;
                *y1 = y;
                *x2 = x;
                *y2 = y + 1;
                return true;
            }
        }
    }

    return false;
}

/* ============================================================================
 * BOARD SHUFFLE
 * ============================================================================ */

/**
 * Shuffle the board when no moves are available
 * Preserves gem counts but randomizes positions
 */
void ShuffleBoard(void)
{
    /* Collect all gems */
    int gems[BOARD_WIDTH * BOARD_HEIGHT];
    int gemCount = 0;

    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            int gem = GetGemAt(x, y);
            if (gem != GEM_EMPTY) {
                gems[gemCount++] = gem;
            }
        }
    }

    /* Fisher-Yates shuffle */
    for (int i = gemCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = gems[i];
        gems[i] = gems[j];
        gems[j] = temp;
    }

    /* Place gems back on board */
    int idx = 0;
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (idx < gemCount) {
                SetGemAt(x, y, gems[idx++]);
            }
        }
    }

    /* Clear any matches created by shuffle */
    while (CheckMatches() > 0) {
        RemoveMatches();
        ApplyGravity();
        FillBoard();
    }

    /* If still no valid moves, shuffle again */
    if (CheckGameOver()) {
        ShuffleBoard();
    }
}

/* ============================================================================
 * LIGHTNING STRIKE FUNCTIONS
 * ============================================================================ */

/* LightningInfo structure (matches header) */
typedef struct {
    bool active;
    bool isHorizontal;
    int row;
    int col;
    int centerX;
    int centerY;
} LightningInfo;

/**
 * Get pending lightning strike info
 */
LightningInfo GetLightningInfo(void)
{
    LightningInfo info;
    info.active = g_game.lightningActive;
    info.isHorizontal = g_game.lightningHorizontal;
    info.row = g_game.lightningRow;
    info.col = g_game.lightningCol;
    info.centerX = g_game.lightningCenterX;
    info.centerY = g_game.lightningCenterY;
    return info;
}

/**
 * Clear the lightning info after processing
 */
void ClearLightningInfo(void)
{
    g_game.lightningActive = false;
}

/**
 * Execute a lightning strike - clears an entire row or column
 * Returns number of gems destroyed
 */
int ExecuteLightningStrike(bool isHorizontal, int index)
{
    int gemsDestroyed = 0;

    if (isHorizontal) {
        /* Clear entire row */
        if (index >= 0 && index < BOARD_HEIGHT) {
            for (int x = 0; x < BOARD_WIDTH; x++) {
                if (GetGemAt(x, index) != GEM_EMPTY) {
                    SetGemAt(x, index, GEM_EMPTY);
                    g_game.animations[index][x].isRemoving = true;
                    g_game.animations[index][x].scale = 1.0f;
                    gemsDestroyed++;
                }
            }
        }
    } else {
        /* Clear entire column */
        if (index >= 0 && index < BOARD_WIDTH) {
            for (int y = 0; y < BOARD_HEIGHT; y++) {
                if (GetGemAt(index, y) != GEM_EMPTY) {
                    SetGemAt(index, y, GEM_EMPTY);
                    g_game.animations[y][index].isRemoving = true;
                    g_game.animations[y][index].scale = 1.0f;
                    gemsDestroyed++;
                }
            }
        }
    }

    /* Add score for lightning destruction */
    int cascadeMult = (g_game.cascadeLevel < 1) ? 1 : g_game.cascadeLevel;
    int lightningScore = gemsDestroyed * SCORE_MATCH_3 * cascadeMult;
    g_game.score += lightningScore;
    g_game.gemsDestroyed += gemsDestroyed;

    /* Check for level up */
    UpdateLevel();

    return gemsDestroyed;
}

/* ============================================================================
 * TWIST MODE FUNCTIONS
 * ============================================================================ */

/**
 * Check if rotating a 2x2 grid would create a match.
 * Does not modify the board.
 */
bool IsValidRotation(int topLeftX, int topLeftY)
{
    /* Validate position - need a full 2x2 grid */
    if (topLeftX < 0 || topLeftX >= BOARD_WIDTH - 1 ||
        topLeftY < 0 || topLeftY >= BOARD_HEIGHT - 1) {
        return false;
    }

    /* Get the 4 gems in the 2x2 grid */
    int gem_tl = GetGemAt(topLeftX, topLeftY);         /* Top-left */
    int gem_tr = GetGemAt(topLeftX + 1, topLeftY);     /* Top-right */
    int gem_bl = GetGemAt(topLeftX, topLeftY + 1);     /* Bottom-left */
    int gem_br = GetGemAt(topLeftX + 1, topLeftY + 1); /* Bottom-right */

    /* All gems must be non-empty */
    if (gem_tl == GEM_EMPTY || gem_tr == GEM_EMPTY ||
        gem_bl == GEM_EMPTY || gem_br == GEM_EMPTY) {
        return false;
    }

    /* Temporarily rotate clockwise: tl->tr, tr->br, br->bl, bl->tl */
    SetGemAt(topLeftX, topLeftY, gem_bl);         /* tl = old bl */
    SetGemAt(topLeftX + 1, topLeftY, gem_tl);     /* tr = old tl */
    SetGemAt(topLeftX + 1, topLeftY + 1, gem_tr); /* br = old tr */
    SetGemAt(topLeftX, topLeftY + 1, gem_br);     /* bl = old br */

    /* Check if any of the 4 positions now creates a match */
    bool hasMatch = HasMatchAt(topLeftX, topLeftY) ||
                    HasMatchAt(topLeftX + 1, topLeftY) ||
                    HasMatchAt(topLeftX, topLeftY + 1) ||
                    HasMatchAt(topLeftX + 1, topLeftY + 1);

    /* Restore original state */
    SetGemAt(topLeftX, topLeftY, gem_tl);
    SetGemAt(topLeftX + 1, topLeftY, gem_tr);
    SetGemAt(topLeftX, topLeftY + 1, gem_bl);
    SetGemAt(topLeftX + 1, topLeftY + 1, gem_br);

    return hasMatch;
}

/**
 * Rotate a 2x2 grid of gems clockwise.
 * Returns true if rotation created a match.
 */
bool RotateGems(int topLeftX, int topLeftY)
{
    /* Validate the rotation would create a match */
    if (!IsValidRotation(topLeftX, topLeftY)) {
        return false;
    }

    /* Get the 4 gems in the 2x2 grid */
    int gem_tl = GetGemAt(topLeftX, topLeftY);
    int gem_tr = GetGemAt(topLeftX + 1, topLeftY);
    int gem_bl = GetGemAt(topLeftX, topLeftY + 1);
    int gem_br = GetGemAt(topLeftX + 1, topLeftY + 1);

    /* Perform clockwise rotation: tl->tr, tr->br, br->bl, bl->tl */
    SetGemAt(topLeftX, topLeftY, gem_bl);         /* tl = old bl */
    SetGemAt(topLeftX + 1, topLeftY, gem_tl);     /* tr = old tl */
    SetGemAt(topLeftX + 1, topLeftY + 1, gem_tr); /* br = old tr */
    SetGemAt(topLeftX, topLeftY + 1, gem_br);     /* bl = old br */

    /* Set up rotation animation for all 4 gems */
    /* The visual animation will be handled by the plugin */
    g_game.animations[topLeftY][topLeftX].offsetX = -1.0f;
    g_game.animations[topLeftY][topLeftX].offsetY = 0.0f;

    g_game.animations[topLeftY][topLeftX + 1].offsetX = 0.0f;
    g_game.animations[topLeftY][topLeftX + 1].offsetY = -1.0f;

    g_game.animations[topLeftY + 1][topLeftX + 1].offsetX = 1.0f;
    g_game.animations[topLeftY + 1][topLeftX + 1].offsetY = 0.0f;

    g_game.animations[topLeftY + 1][topLeftX].offsetX = 0.0f;
    g_game.animations[topLeftY + 1][topLeftX].offsetY = 1.0f;

    /* Reset cascade tracking for new move */
    g_game.cascadeLevel = 0;
    g_game.cascadeScore = 0;

    return true;
}

/**
 * Check if any valid rotation moves exist on the board.
 * Returns true if no valid rotations exist (game over).
 */
bool CheckTwistGameOver(void)
{
    /* Check every possible 2x2 rotation position */
    for (int y = 0; y < BOARD_HEIGHT - 1; y++) {
        for (int x = 0; x < BOARD_WIDTH - 1; x++) {
            if (IsValidRotation(x, y)) {
                return false;  /* Found a valid move */
            }
        }
    }

    /* Also check time limit */
    if (g_game.timeRemaining <= 0.0f && g_game.timeRemaining != -1.0f) {
        return true;
    }

    return true;  /* No valid rotations found */
}

/**
 * Find a valid rotation move (for hint system in Twist mode).
 */
bool GetTwistHint(int *topLeftX, int *topLeftY)
{
    for (int y = 0; y < BOARD_HEIGHT - 1; y++) {
        for (int x = 0; x < BOARD_WIDTH - 1; x++) {
            if (IsValidRotation(x, y)) {
                *topLeftX = x;
                *topLeftY = y;
                return true;
            }
        }
    }
    return false;
}

/* ============================================================================
 * SPECIAL GEM FUNCTIONS
 * ============================================================================ */

/**
 * Get pending special gems array
 */
PendingSpecialGem* GetPendingSpecialGems(void)
{
    return g_game.pendingSpecials;
}

/**
 * Get count of pending special gems
 */
int GetPendingSpecialCount(void)
{
    return g_game.pendingSpecialCount;
}

/**
 * Check if a match forms an L, T, or + shape (for Star Gem)
 * Returns the intersection point if found
 */
static bool CheckLTPlusShape(int *outX, int *outY)
{
    /* Look for positions that are part of both horizontal and vertical matches */
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            int gemType = GetGemAt(x, y);
            if (gemType == GEM_EMPTY) continue;

            int hLen = GetHorizontalMatchLength(x, y);
            int vLen = GetVerticalMatchLength(x, y);

            /* Check if this gem is at intersection of 3+ horizontal and 3+ vertical */
            if (hLen >= 3 && vLen >= 3) {
                *outX = x;
                *outY = y;
                return true;
            }
        }
    }
    return false;
}

/**
 * Analyze matches and determine which special gems should be created
 */
int AnalyzeForSpecialGems(void)
{
    g_game.pendingSpecialCount = 0;

    /* First check for L/T/+ shapes (Star Gem) - highest pattern priority */
    int intersectX, intersectY;
    if (CheckLTPlusShape(&intersectX, &intersectY)) {
        int gemColor = GetGemAt(intersectX, intersectY);
        if (gemColor != GEM_EMPTY && g_game.pendingSpecialCount < MAX_PENDING_SPECIALS) {
            PendingSpecialGem *pending = &g_game.pendingSpecials[g_game.pendingSpecialCount++];
            pending->x = intersectX;
            pending->y = intersectY;
            pending->color = gemColor;
            pending->specialType = SPECIAL_STAR;
        }
    }

    /* Check each recorded match for special gem patterns */
    for (int m = 0; m < g_game.matchCount; m++) {
        MatchInfo *match = &g_game.matches[m];
        if (match->count < 4) continue;  /* Need 4+ for special gems */

        /* Skip if this match position already has a pending special */
        bool alreadyPending = false;
        int centerIdx = match->count / 2;
        int centerX = match->positions[centerIdx].x;
        int centerY = match->positions[centerIdx].y;

        for (int p = 0; p < g_game.pendingSpecialCount; p++) {
            if (g_game.pendingSpecials[p].x == centerX &&
                g_game.pendingSpecials[p].y == centerY) {
                alreadyPending = true;
                break;
            }
        }
        if (alreadyPending) continue;

        int gemColor = GetGemAt(centerX, centerY);
        if (gemColor == GEM_EMPTY) continue;

        int specialType = SPECIAL_NONE;

        if (match->count >= 6) {
            /* 6+ in a row = Supernova */
            specialType = SPECIAL_SUPERNOVA;
        } else if (match->count == 5) {
            /* 5 in a row = Hypercube (if not already L/T/+ shape) */
            specialType = SPECIAL_HYPERCUBE;
        } else if (match->count == 4) {
            /* 4 in a row = Flame Gem */
            specialType = SPECIAL_FLAME;
        }

        if (specialType != SPECIAL_NONE && g_game.pendingSpecialCount < MAX_PENDING_SPECIALS) {
            PendingSpecialGem *pending = &g_game.pendingSpecials[g_game.pendingSpecialCount++];
            pending->x = centerX;
            pending->y = centerY;
            pending->color = gemColor;
            pending->specialType = specialType;
        }
    }

    return g_game.pendingSpecialCount;
}

/**
 * Execute a Flame Gem effect (3x3 explosion)
 */
int ExecuteFlameEffect(int x, int y)
{
    int gemsDestroyed = 0;
    int cascadeMult = (g_game.cascadeLevel < 1) ? 1 : g_game.cascadeLevel;

    /* Destroy 3x3 area around the gem */
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int gx = x + dx;
            int gy = y + dy;

            if (!IsValidPosition(gx, gy)) continue;
            if (GetGemAt(gx, gy) == GEM_EMPTY) continue;

            /* Check if this gem is also special - queue its effect */
            int specialType = GetBoardSpecial(gx, gy);
            if (specialType != SPECIAL_NONE && !(gx == x && gy == y)) {
                QueueSpecialEffect(gx, gy, specialType, GetGemAt(gx, gy));
            }

            SetGemAt(gx, gy, GEM_EMPTY);
            SetBoardSpecial(gx, gy, SPECIAL_NONE);
            g_game.animations[gy][gx].isRemoving = true;
            g_game.animations[gy][gx].scale = 1.0f;
            gemsDestroyed++;
        }
    }

    /* Add score */
    int flameScore = gemsDestroyed * SCORE_MATCH_3 * cascadeMult * 3 / 2;  /* 1.5x bonus */
    g_game.score += flameScore;
    g_game.gemsDestroyed += gemsDestroyed;

    UpdateLevel();
    return gemsDestroyed;
}

/**
 * Execute a Star Gem effect (row + column clear)
 */
int ExecuteStarEffect(int x, int y)
{
    int gemsDestroyed = 0;
    int cascadeMult = (g_game.cascadeLevel < 1) ? 1 : g_game.cascadeLevel;

    /* Clear entire row */
    for (int gx = 0; gx < BOARD_WIDTH; gx++) {
        if (GetGemAt(gx, y) == GEM_EMPTY) continue;

        /* Check if this gem is also special - queue its effect */
        int specialType = GetBoardSpecial(gx, y);
        if (specialType != SPECIAL_NONE && gx != x) {
            QueueSpecialEffect(gx, y, specialType, GetGemAt(gx, y));
        }

        SetGemAt(gx, y, GEM_EMPTY);
        SetBoardSpecial(gx, y, SPECIAL_NONE);
        g_game.animations[y][gx].isRemoving = true;
        g_game.animations[y][gx].scale = 1.0f;
        gemsDestroyed++;
    }

    /* Clear entire column */
    for (int gy = 0; gy < BOARD_HEIGHT; gy++) {
        if (gy == y) continue;  /* Already cleared in row pass */
        if (GetGemAt(x, gy) == GEM_EMPTY) continue;

        /* Check if this gem is also special - queue its effect */
        int specialType = GetBoardSpecial(x, gy);
        if (specialType != SPECIAL_NONE) {
            QueueSpecialEffect(x, gy, specialType, GetGemAt(x, gy));
        }

        SetGemAt(x, gy, GEM_EMPTY);
        SetBoardSpecial(x, gy, SPECIAL_NONE);
        g_game.animations[gy][x].isRemoving = true;
        g_game.animations[gy][x].scale = 1.0f;
        gemsDestroyed++;
    }

    /* Add score */
    int starScore = gemsDestroyed * SCORE_MATCH_3 * cascadeMult * 7 / 4;  /* 1.75x bonus */
    g_game.score += starScore;
    g_game.gemsDestroyed += gemsDestroyed;

    UpdateLevel();
    return gemsDestroyed;
}

/**
 * Execute a Hypercube effect (clear all of one color)
 */
int ExecuteHypercubeEffect(int targetColor)
{
    int gemsDestroyed = 0;
    int cascadeMult = (g_game.cascadeLevel < 1) ? 1 : g_game.cascadeLevel;

    /* Clear all gems of the target color */
    for (int gy = 0; gy < BOARD_HEIGHT; gy++) {
        for (int gx = 0; gx < BOARD_WIDTH; gx++) {
            int gem = GetGemAt(gx, gy);
            if (gem != targetColor) continue;

            /* Check if this gem is also special - queue its effect */
            int specialType = GetBoardSpecial(gx, gy);
            if (specialType != SPECIAL_NONE) {
                QueueSpecialEffect(gx, gy, specialType, gem);
            }

            SetGemAt(gx, gy, GEM_EMPTY);
            SetBoardSpecial(gx, gy, SPECIAL_NONE);
            g_game.animations[gy][gx].isRemoving = true;
            g_game.animations[gy][gx].scale = 1.0f;
            gemsDestroyed++;
        }
    }

    /* Add score */
    int hypercubeScore = gemsDestroyed * SCORE_MATCH_3 * cascadeMult * 3 / 2;  /* 1.5x bonus */
    g_game.score += hypercubeScore;
    g_game.gemsDestroyed += gemsDestroyed;

    UpdateLevel();
    return gemsDestroyed;
}

/**
 * Execute a Supernova effect (3-wide cross pattern)
 */
int ExecuteSupernovaEffect(int x, int y)
{
    int gemsDestroyed = 0;
    int cascadeMult = (g_game.cascadeLevel < 1) ? 1 : g_game.cascadeLevel;

    /* Track which positions have been cleared to avoid double-counting */
    bool cleared[BOARD_HEIGHT][BOARD_WIDTH];
    memset(cleared, false, sizeof(cleared));

    /* Clear 3 rows centered on y */
    for (int dy = -1; dy <= 1; dy++) {
        int gy = y + dy;
        if (gy < 0 || gy >= BOARD_HEIGHT) continue;

        for (int gx = 0; gx < BOARD_WIDTH; gx++) {
            if (cleared[gy][gx]) continue;
            if (GetGemAt(gx, gy) == GEM_EMPTY) continue;

            /* Check if this gem is also special - queue its effect */
            int specialType = GetBoardSpecial(gx, gy);
            if (specialType != SPECIAL_NONE && !(gx == x && gy == y)) {
                QueueSpecialEffect(gx, gy, specialType, GetGemAt(gx, gy));
            }

            SetGemAt(gx, gy, GEM_EMPTY);
            SetBoardSpecial(gx, gy, SPECIAL_NONE);
            g_game.animations[gy][gx].isRemoving = true;
            g_game.animations[gy][gx].scale = 1.0f;
            cleared[gy][gx] = true;
            gemsDestroyed++;
        }
    }

    /* Clear 3 columns centered on x */
    for (int dx = -1; dx <= 1; dx++) {
        int gx = x + dx;
        if (gx < 0 || gx >= BOARD_WIDTH) continue;

        for (int gy = 0; gy < BOARD_HEIGHT; gy++) {
            if (cleared[gy][gx]) continue;
            if (GetGemAt(gx, gy) == GEM_EMPTY) continue;

            /* Check if this gem is also special - queue its effect */
            int specialType = GetBoardSpecial(gx, gy);
            if (specialType != SPECIAL_NONE) {
                QueueSpecialEffect(gx, gy, specialType, GetGemAt(gx, gy));
            }

            SetGemAt(gx, gy, GEM_EMPTY);
            SetBoardSpecial(gx, gy, SPECIAL_NONE);
            g_game.animations[gy][gx].isRemoving = true;
            g_game.animations[gy][gx].scale = 1.0f;
            cleared[gy][gx] = true;
            gemsDestroyed++;
        }
    }

    /* Add score */
    int supernovaScore = gemsDestroyed * SCORE_MATCH_3 * cascadeMult * 2;  /* 2x bonus */
    g_game.score += supernovaScore;
    g_game.gemsDestroyed += gemsDestroyed;

    UpdateLevel();
    return gemsDestroyed;
}

/**
 * Queue a special effect for later execution
 */
void QueueSpecialEffect(int x, int y, int effectType, int targetColor)
{
    if (g_game.queuedEffectCount >= MAX_QUEUED_EFFECTS) return;

    QueuedEffect *effect = &g_game.queuedEffects[g_game.queuedEffectCount++];
    effect->x = x;
    effect->y = y;
    effect->effectType = effectType;
    effect->targetColor = targetColor;
    effect->active = true;
}

/**
 * Process queued special effects
 */
bool ProcessQueuedEffects(void)
{
    if (g_game.queuedEffectCount == 0) return false;

    /* Process one effect at a time */
    QueuedEffect effect = g_game.queuedEffects[0];

    /* Remove from queue by shifting */
    for (int i = 0; i < g_game.queuedEffectCount - 1; i++) {
        g_game.queuedEffects[i] = g_game.queuedEffects[i + 1];
    }
    g_game.queuedEffectCount--;

    if (!effect.active) return g_game.queuedEffectCount > 0;

    /* Increment cascade for chain reactions */
    IncrementCascade();

    /* Execute the effect */
    switch (effect.effectType) {
        case SPECIAL_FLAME:
            ExecuteFlameEffect(effect.x, effect.y);
            break;
        case SPECIAL_STAR:
            ExecuteStarEffect(effect.x, effect.y);
            break;
        case SPECIAL_HYPERCUBE:
            ExecuteHypercubeEffect(effect.targetColor);
            break;
        case SPECIAL_SUPERNOVA:
            ExecuteSupernovaEffect(effect.x, effect.y);
            break;
    }

    return true;
}

/**
 * Perform Hypercube swap (doesn't require valid match)
 */
bool SwapHypercube(int hcX, int hcY, int targetX, int targetY)
{
    /* Validate positions */
    if (!IsValidPosition(hcX, hcY) || !IsValidPosition(targetX, targetY)) {
        return false;
    }

    /* Check adjacency */
    int dx = abs(hcX - targetX);
    int dy = abs(hcY - targetY);
    if (!((dx == 1 && dy == 0) || (dx == 0 && dy == 1))) {
        return false;
    }

    int hcSpecial = GetBoardSpecial(hcX, hcY);
    int targetSpecial = GetBoardSpecial(targetX, targetY);

    /* Check if swapping two Hypercubes - clear entire board */
    if (hcSpecial == SPECIAL_HYPERCUBE && targetSpecial == SPECIAL_HYPERCUBE) {
        /* Clear entire board! */
        for (int gy = 0; gy < BOARD_HEIGHT; gy++) {
            for (int gx = 0; gx < BOARD_WIDTH; gx++) {
                if (GetGemAt(gx, gy) != GEM_EMPTY) {
                    SetGemAt(gx, gy, GEM_EMPTY);
                    SetBoardSpecial(gx, gy, SPECIAL_NONE);
                    g_game.animations[gy][gx].isRemoving = true;
                    g_game.animations[gy][gx].scale = 1.0f;
                }
            }
        }

        /* Massive score bonus */
        int cascadeMult = (g_game.cascadeLevel < 1) ? 1 : g_game.cascadeLevel;
        g_game.score += BOARD_WIDTH * BOARD_HEIGHT * SCORE_MATCH_3 * cascadeMult * 3;
        g_game.gemsDestroyed += BOARD_WIDTH * BOARD_HEIGHT;
        UpdateLevel();

        return true;
    }

    /* Get target gem color */
    int targetColor = GetGemAt(targetX, targetY);
    if (targetColor == GEM_EMPTY) return false;

    /* Remove the Hypercube */
    SetGemAt(hcX, hcY, GEM_EMPTY);
    SetBoardSpecial(hcX, hcY, SPECIAL_NONE);
    g_game.animations[hcY][hcX].isRemoving = true;

    /* Execute the color clear */
    ExecuteHypercubeEffect(targetColor);

    return true;
}

/* ============================================================================
 * CASCADE RUSH MODE FUNCTIONS
 * ============================================================================ */

/**
 * Initialize CASCADE RUSH mode state.
 * Called after board initialization.
 */
void InitCascadeRush(void)
{
    /* Time is already set in InitGameMode, but ensure it's correct */
    g_game.timeRemaining = CASCADE_RUSH_START_TIME;

    /* Clear rush zone state */
    g_game.rushZoneActive = false;
    g_game.rushZoneX = 0;
    g_game.rushZoneY = 0;
    g_game.rushZoneTimer = 0.0f;
    g_game.rushZoneSpawnTimer = RUSH_ZONE_SPAWN_INTERVAL;
    g_game.rushZonesCaptured = 0;
    g_game.timeAddedFromCascades = 0.0f;
}

/**
 * Spawn a new rush zone at a random position.
 * Zone is 3x3, so position range is 0 to BOARD_SIZE-3.
 */
void SpawnRushZone(void)
{
    /* Pick random position (0 to BOARD_SIZE-3 for both x and y) */
    g_game.rushZoneX = rand() % (BOARD_WIDTH - 2);
    g_game.rushZoneY = rand() % (BOARD_HEIGHT - 2);

    g_game.rushZoneActive = true;

    /* Calculate zone duration based on level: 10 seconds base, 0.9x per level */
    float baseDuration = 10.0f;
    int level = g_game.level;
    float duration = baseDuration;
    for (int i = 1; i < level; i++) {
        duration *= 0.9f;
    }
    /* Minimum duration of 3 seconds */
    if (duration < 3.0f) duration = 3.0f;
    g_game.rushZoneTimer = duration;
    g_game.rushZoneMaxDuration = duration;

    g_game.rushZoneSpawnTimer = RUSH_ZONE_SPAWN_INTERVAL;
}

/**
 * Update CASCADE RUSH mode state.
 * @param deltaTime Time since last frame
 * @return false if time expired (game over), true otherwise
 */
bool UpdateCascadeRush(float deltaTime)
{
    /* Only update if in CASCADE RUSH mode */
    if (g_game.gameMode != GAME_MODE_CASCADE_RUSH) {
        return true;
    }

    /* Decrement game time */
    if (g_game.timeRemaining > 0.0f) {
        g_game.timeRemaining -= deltaTime;
        if (g_game.timeRemaining < 0.0f) {
            g_game.timeRemaining = 0.0f;
        }
    }

    /* Update rush zone spawn timer */
    if (!g_game.rushZoneActive) {
        g_game.rushZoneSpawnTimer -= deltaTime;
        if (g_game.rushZoneSpawnTimer <= 0.0f) {
            SpawnRushZone();
        }
    } else {
        /* Zone is active, decrement zone timer */
        g_game.rushZoneTimer -= deltaTime;
        if (g_game.rushZoneTimer <= 0.0f) {
            /* Zone expired - missed */
            g_game.rushZoneActive = false;
            g_game.rushZoneTimer = 0.0f;
            /* Reset spawn timer for next zone */
            g_game.rushZoneSpawnTimer = RUSH_ZONE_SPAWN_INTERVAL;
        }
    }

    /* Return false if time expired */
    return g_game.timeRemaining > 0.0f;
}

/**
 * Check if a position is inside the current rush zone.
 * @param x, y Grid coordinates to check
 * @return true if (x,y) is within the 3x3 rush zone bounds
 */
bool IsInsideRushZone(int x, int y)
{
    if (!g_game.rushZoneActive) {
        return false;
    }

    return (x >= g_game.rushZoneX && x < g_game.rushZoneX + 3 &&
            y >= g_game.rushZoneY && y < g_game.rushZoneY + 3);
}

/**
 * Get current rush zone info.
 * @param x Output: X position of rush zone top-left
 * @param y Output: Y position of rush zone top-left
 * @param active Output: true if rush zone is active
 * @param timeRemaining Output: time remaining on rush zone
 */
void GetRushZoneInfo(int *x, int *y, bool *active, float *timeRemaining, float *maxDuration)
{
    if (x) *x = g_game.rushZoneX;
    if (y) *y = g_game.rushZoneY;
    if (active) *active = g_game.rushZoneActive;
    if (timeRemaining) *timeRemaining = g_game.rushZoneTimer;
    if (maxDuration) *maxDuration = g_game.rushZoneMaxDuration;
}

/**
 * Add time bonus from cascade.
 * @param cascadeLevel The current cascade level
 * @return The amount of time added (cascadeLevel * 2.0 seconds)
 */
float AddCascadeTimeBonus(int cascadeLevel)
{
    float timeAdded = cascadeLevel * 2.0f;
    g_game.timeRemaining += timeAdded;
    g_game.timeAddedFromCascades += timeAdded;
    return timeAdded;
}

/**
 * Capture the current rush zone (called when player swipes through zone).
 * Deactivates the zone and increments capture count.
 */
void CaptureRushZone(void)
{
    if (g_game.rushZoneActive) {
        g_game.rushZoneActive = false;
        g_game.rushZonesCaptured++;
        /* Reset spawn timer for next zone */
        g_game.rushZoneSpawnTimer = RUSH_ZONE_SPAWN_INTERVAL;
    }
}

/**
 * Get the number of rush zones captured.
 * @return Number of zones captured this game
 */
int GetRushZonesCaptured(void)
{
    return g_game.rushZonesCaptured;
}

/**
 * Get total time added from cascades.
 * @return Total time added in seconds
 */
float GetTimeAddedFromCascades(void)
{
    return g_game.timeAddedFromCascades;
}

/* ============================================================================
 * GEM SURGE MODE FUNCTIONS
 * ============================================================================ */

/**
 * Advance to the next wave in GEM SURGE mode.
 * Called when surgeWaveScore >= surgeWaveTarget.
 */
static void AdvanceToNextWave(void)
{
    g_game.surgeCurrentWave++;

    /* Add time bonus: 15 seconds, decreasing for higher waves (min 5) */
    float timeBonus = 15.0f - (g_game.surgeCurrentWave - 2) * 2.0f;
    if (timeBonus < 5.0f) {
        timeBonus = 5.0f;
    }
    g_game.timeRemaining += timeBonus;

    /* Calculate new target: previous * 1.75 */
    g_game.surgeWaveTarget = (int)(g_game.surgeWaveTarget * 1.75f);

    /* Reset wave score */
    g_game.surgeWaveScore = 0;

    /* Pick new random featured gem (1-7) */
    g_game.surgeFeaturedGem = (rand() % GEM_TYPE_COUNT) + 1;
}

/**
 * Initialize GEM SURGE mode state.
 * Called after board initialization.
 */
void InitGemSurge(void)
{
    /* Time is already set in InitGameMode, but ensure it's correct */
    g_game.timeRemaining = SURGE_INITIAL_TIME;

    /* Initialize wave state */
    g_game.surgeCurrentWave = 1;
    g_game.surgeWaveTarget = SURGE_INITIAL_TARGET;
    g_game.surgeWaveScore = 0;

    /* Pick random featured gem (1-7) */
    g_game.surgeFeaturedGem = (rand() % GEM_TYPE_COUNT) + 1;

    /* Clear all surge lines */
    for (int i = 0; i < SURGE_MAX_LINES; i++) {
        g_game.surgeLines[i] = false;
        g_game.surgeLinesHorizontal[i] = false;
        g_game.surgeLinePositions[i] = 0;
        g_game.surgeLineTimers[i] = 0.0f;
    }
    g_game.surgeActiveLineCount = 0;
    g_game.surgeLinesCleared = 0;
}

/**
 * Update GEM SURGE mode state.
 * @param deltaTime Time since last frame
 * @return false if time expired (game over), true otherwise
 */
bool UpdateGemSurge(float deltaTime)
{
    /* Only update if in GEM SURGE mode */
    if (g_game.gameMode != GAME_MODE_GEM_SURGE) {
        return true;
    }

    /* Decrement game time */
    if (g_game.timeRemaining > 0.0f) {
        g_game.timeRemaining -= deltaTime;
        if (g_game.timeRemaining < 0.0f) {
            g_game.timeRemaining = 0.0f;
        }
    }

    /* Update surge line timers (decrement, remove if expired) */
    for (int i = 0; i < SURGE_MAX_LINES; i++) {
        if (g_game.surgeLines[i]) {
            g_game.surgeLineTimers[i] -= deltaTime;
            if (g_game.surgeLineTimers[i] <= 0.0f) {
                /* Line expired - remove it */
                g_game.surgeLines[i] = false;
                g_game.surgeLineTimers[i] = 0.0f;
                g_game.surgeActiveLineCount--;
            }
        }
    }

    /* Check if wave is complete */
    if (g_game.surgeWaveScore >= g_game.surgeWaveTarget) {
        AdvanceToNextWave();
    }

    /* Return false if time expired */
    return g_game.timeRemaining > 0.0f;
}

/**
 * Spawn a new surge line at a random position.
 */
void SpawnSurgeLine(void)
{
    /* Find an empty slot in surgeLines array */
    int slot = -1;
    for (int i = 0; i < SURGE_MAX_LINES; i++) {
        if (!g_game.surgeLines[i]) {
            slot = i;
            break;
        }
    }

    /* No empty slot available */
    if (slot < 0) {
        return;
    }

    /* Randomly choose horizontal or vertical */
    bool isHorizontal = (rand() % 2) == 0;

    /* Pick random row/column */
    int position;
    if (isHorizontal) {
        position = rand() % BOARD_HEIGHT;  /* Random row */
    } else {
        position = rand() % BOARD_WIDTH;   /* Random column */
    }

    /* Set up the surge line */
    g_game.surgeLines[slot] = true;
    g_game.surgeLinesHorizontal[slot] = isHorizontal;
    g_game.surgeLinePositions[slot] = position;
    g_game.surgeLineTimers[slot] = SURGE_LINE_DURATION;
    g_game.surgeActiveLineCount++;
}

/**
 * Get the current wave number.
 * @return Current wave (1-based)
 */
int GetCurrentWave(void)
{
    return g_game.surgeCurrentWave;
}

/**
 * Get the current wave target score.
 * @return Target score for current wave
 */
int GetWaveTarget(void)
{
    return g_game.surgeWaveTarget;
}

/**
 * Get the current wave score.
 * @return Score accumulated in current wave
 */
int GetWaveScore(void)
{
    return g_game.surgeWaveScore;
}

/**
 * Get the featured gem type for GEM SURGE mode.
 * @return Featured gem type (1-7)
 */
int GetFeaturedGemType(void)
{
    return g_game.surgeFeaturedGem;
}

/**
 * Check if a surge line is active.
 * @param index Index of the surge line (0 to SURGE_MAX_LINES-1)
 * @return true if line is active
 */
bool IsSurgeLineActive(int index)
{
    if (index < 0 || index >= SURGE_MAX_LINES) {
        return false;
    }
    return g_game.surgeLines[index];
}

/**
 * Get surge line info for rendering.
 * @param index Index of the surge line
 * @param isHorizontal Output: true if horizontal, false if vertical
 * @param position Output: row (if horizontal) or column (if vertical)
 * @param timeRemaining Output: time remaining before line expires
 * @return true if line is active and info was populated
 */
bool GetSurgeLineInfo(int index, bool *isHorizontal, int *position, float *timeRemaining)
{
    if (index < 0 || index >= SURGE_MAX_LINES || !g_game.surgeLines[index]) {
        return false;
    }

    if (isHorizontal) {
        *isHorizontal = g_game.surgeLinesHorizontal[index];
    }
    if (position) {
        *position = g_game.surgeLinePositions[index];
    }
    if (timeRemaining) {
        *timeRemaining = g_game.surgeLineTimers[index];
    }

    return true;
}

/**
 * Trigger a surge line - clear all gems in that row/column.
 * @param index Index of the surge line to trigger
 * @return Points earned from triggering the line
 */
int TriggerSurgeLine(int index)
{
    if (index < 0 || index >= SURGE_MAX_LINES || !g_game.surgeLines[index]) {
        return 0;
    }

    bool isHorizontal = g_game.surgeLinesHorizontal[index];
    int position = g_game.surgeLinePositions[index];

    /* Count gems and clear them */
    int gemsCleared = 0;

    if (isHorizontal) {
        /* Clear entire row */
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (GetGemAt(x, position) != GEM_EMPTY) {
                SetGemAt(x, position, GEM_EMPTY);
                SetBoardSpecial(x, position, SPECIAL_NONE);
                g_game.animations[position][x].isRemoving = true;
                g_game.animations[position][x].scale = 1.0f;
                gemsCleared++;
            }
        }
    } else {
        /* Clear entire column */
        for (int y = 0; y < BOARD_HEIGHT; y++) {
            if (GetGemAt(position, y) != GEM_EMPTY) {
                SetGemAt(position, y, GEM_EMPTY);
                SetBoardSpecial(position, y, SPECIAL_NONE);
                g_game.animations[y][position].isRemoving = true;
                g_game.animations[y][position].scale = 1.0f;
                gemsCleared++;
            }
        }
    }

    /* Calculate points: 150 base + 25 per gem */
    int points = 150 + (gemsCleared * 25);

    /* Add to surgeWaveScore */
    g_game.surgeWaveScore += points;

    /* Also add to overall score */
    g_game.score += points;
    g_game.gemsDestroyed += gemsCleared;

    /* Set line inactive */
    g_game.surgeLines[index] = false;
    g_game.surgeLineTimers[index] = 0.0f;
    g_game.surgeActiveLineCount--;

    /* Increment lines cleared */
    g_game.surgeLinesCleared++;

    /* Update level */
    UpdateLevel();

    return points;
}

/**
 * Get the number of active surge lines.
 * @return Number of active lines
 */
int GetSurgeActiveLineCount(void)
{
    return g_game.surgeActiveLineCount;
}

/**
 * Get the total number of surge lines cleared.
 * @return Total lines cleared
 */
int GetSurgeLinesCleared(void)
{
    return g_game.surgeLinesCleared;
}

/**
 * Add score to the current wave score.
 * This should be called when matches are made in GEM SURGE mode.
 * @param score Score to add
 */
void AddToWaveScore(int score)
{
    if (g_game.gameMode == GAME_MODE_GEM_SURGE) {
        g_game.surgeWaveScore += score;
    }
}
