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

/* Scoring constants */
#define SCORE_MATCH_3   50      /* Points for 3-gem match */
#define SCORE_MATCH_4   100     /* Points for 4-gem match */
#define SCORE_MATCH_5   200     /* Points for 5+ gem match */
#define CASCADE_BONUS   25      /* Bonus per cascade level */

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

/* Main game state structure */
typedef struct {
    /* Board state - gem types at each position (0 = empty) */
    int board[BOARD_HEIGHT][BOARD_WIDTH];

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
static int CalculateMatchScore(int matchLength, int cascadeLevel);

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
 * Score = baseScore * matchLength * comboMultiplier
 */
static int CalculateMatchScore(int matchLength, int cascadeLevel)
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

    /* Score scales with match length and combo */
    return baseScore * matchLength * comboMultiplier / 3;
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

    /* Process each match */
    for (int m = 0; m < g_game.matchCount; m++) {
        MatchInfo *match = &g_game.matches[m];

        /* Calculate score for this match */
        int matchScore = CalculateMatchScore(match->count, g_game.cascadeLevel);
        scoreEarned += matchScore;

        /* Track statistics */
        if (match->count > g_game.largestMatch) {
            g_game.largestMatch = match->count;
        }

        /* Remove gems and set animation state */
        for (int i = 0; i < match->count; i++) {
            int x = match->positions[i].x;
            int y = match->positions[i].y;

            if (GetGemAt(x, y) != GEM_EMPTY) {
                SetGemAt(x, y, GEM_EMPTY);
                g_game.animations[y][x].isRemoving = true;
                g_game.animations[y][x].scale = 1.0f;
                gemsRemoved++;
            }
        }
    }

    /* Update game state */
    g_game.score += scoreEarned;
    g_game.cascadeScore += scoreEarned;
    g_game.gemsDestroyed += gemsRemoved;
    g_game.totalMatches += g_game.matchCount;

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
 * Initialize the game board with random gems, ensuring no initial matches
 */
void InitGame(void)
{
    /* Seed random number generator if not already done */
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    /* Clear entire game state */
    memset(&g_game, 0, sizeof(g_game));

    /* Initialize selection to none */
    g_game.selectedGem.x = -1;
    g_game.selectedGem.y = -1;
    g_game.swapGem.x = -1;
    g_game.swapGem.y = -1;

    /* Set default game mode (unlimited moves/time) */
    g_game.movesRemaining = -1;    /* -1 = unlimited */
    g_game.timeRemaining = -1.0f;  /* -1 = unlimited */

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

    /* Ensure at least one valid move exists */
    if (CheckGameOver()) {
        /* Rare case: regenerate entire board */
        InitGame();
        return;
    }

    /* Set initial state */
    g_game.state = GAME_STATE_IDLE;
    g_game.level = 1;

    g_initialized = true;
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
