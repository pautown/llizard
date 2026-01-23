/**
 * bejeweled_logic.h - Core game logic API for Bejeweled-style match-3 game
 *
 * This header exposes the public interface for the match-3 game logic.
 * Include this in the plugin file to access game functions.
 */

#ifndef BEJEWELED_LOGIC_H
#define BEJEWELED_LOGIC_H

#include <stdbool.h>

/* ============================================================================
 * GAME CONSTANTS
 * ============================================================================ */

#define BOARD_WIDTH     8       /* Number of columns */
#define BOARD_HEIGHT    8       /* Number of rows */
#define GEM_TYPE_COUNT  7       /* Number of different gem colors/types */
#define SCREEN_WIDTH    800     /* Screen width in pixels */
#define SCREEN_HEIGHT   480     /* Screen height in pixels */

/* Blitz mode timing */
#define BLITZ_TIME_LIMIT    60.0f   /* 60 seconds for Blitz mode */
#define BLITZ_SCORE_MULT    2       /* Score multiplier for Blitz mode */

/* CASCADE RUSH Mode Constants */
#define CASCADE_RUSH_START_TIME     30.0f
#define CASCADE_RUSH_TIME_PER_CASCADE  2.0f
#define CASCADE_RUSH_SCORE_MULT     3
#define RUSH_ZONE_SIZE              3
#define RUSH_ZONE_SPAWN_INTERVAL    10.0f
#define RUSH_ZONE_DURATION          5.0f
#define RUSH_ZONE_TIME_BONUS        5.0f
#define RUSH_ZONE_SCORE_BONUS       500

/* GEM SURGE Mode Constants */
#define SURGE_START_TIME            45.0f
#define SURGE_WAVE_TIME_BONUS       15.0f
#define SURGE_BASE_TARGET           1000
#define SURGE_TARGET_MULTIPLIER     1.75f
#define SURGE_LINE_DURATION         8.0f
#define SURGE_LINE_POINTS_BASE      150
#define SURGE_FEATURED_MULTIPLIER   2.0f
#define SURGE_MAX_LINES             3

/* ============================================================================
 * GAME MODE DEFINITIONS
 * ============================================================================ */

/**
 * Available game modes.
 */
typedef enum {
    GAME_MODE_CLASSIC,      /**< Classic mode - no time limit, play until no moves */
    GAME_MODE_BLITZ,        /**< Blitz mode - 60 second timed mode */
    GAME_MODE_TWIST,        /**< Twist mode - rotate 2x2 grid instead of swap */
    GAME_MODE_CASCADE_RUSH, /**< Cascade Rush mode - timed mode with cascade bonuses and rush zones */
    GAME_MODE_GEM_SURGE,    /**< Gem Surge mode - wave-based scoring with featured gems and bonus lines */
    GAME_MODE_COUNT         /**< Number of game modes */
} GameMode;

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
#define SPECIAL_NONE        0   /* Normal gem */
#define SPECIAL_FLAME       1   /* 4-in-a-row: 3x3 explosion */
#define SPECIAL_STAR        2   /* L/T/+ shape: clears row + column */
#define SPECIAL_HYPERCUBE   3   /* 5-in-a-row: clears all of swapped color */
#define SPECIAL_SUPERNOVA   4   /* 6-in-a-row: 3-wide cross pattern */
#define SPECIAL_COUNT       5

/* Scoring constants */
#define SCORE_MATCH_3   50      /* Points for 3-gem match */
#define SCORE_MATCH_4   100     /* Points for 4-gem match */
#define SCORE_MATCH_5   200     /* Points for 5+ gem match */
#define CASCADE_BONUS   25      /* Bonus per cascade level */

/* ============================================================================
 * TYPE DEFINITIONS
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

/* Pending special gem info (created after match detection) */
typedef struct {
    int x, y;                   /* Position where special gem will be created */
    int color;                  /* Color of the special gem */
    int specialType;            /* Type: SPECIAL_FLAME, SPECIAL_STAR, etc. */
} PendingSpecialGem;

/* Special effect info (queued effects to execute) */
typedef struct {
    int x, y;                   /* Position of the triggering gem */
    int effectType;             /* Type of effect */
    int targetColor;            /* For Hypercube: color to clear */
    bool active;                /* Whether this effect is pending */
} QueuedEffect;

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

/**
 * Initialize the game board with random gems, ensuring no initial matches.
 * Call this at the start of a new game. Uses Classic mode by default.
 */
void InitGame(void);

/**
 * Initialize game with a specific game mode.
 * @param mode The game mode to initialize
 */
void InitGameMode(GameMode mode);

/**
 * Initialize game with specific mode settings (legacy).
 * @param moves Number of moves allowed (-1 for unlimited)
 * @param timeLimit Time limit in seconds (-1 for unlimited)
 */
void InitGameWithMode(int moves, float timeLimit);

/**
 * Get the current game mode.
 * @return The current game mode
 */
GameMode GetCurrentGameMode(void);

/**
 * Check if game is initialized.
 * @return true if InitGame() has been called
 */
bool IsGameInitialized(void);

/* ============================================================================
 * CORE GAME FUNCTIONS
 * ============================================================================ */

/**
 * Scan board for horizontal and vertical matches of 3+.
 * Populates internal match tracking data.
 * @return Total number of matched gem positions
 */
int CheckMatches(void);

/**
 * Remove all matched gems from the board and calculate score.
 * Call after CheckMatches() returns > 0.
 * @return Score earned from this removal
 */
int RemoveMatches(void);

/**
 * Apply gravity - make gems fall down to fill empty spaces.
 * @return true if any gems moved
 */
bool ApplyGravity(void);

/**
 * Fill empty spaces at top of columns with new random gems.
 * @return Number of new gems added
 */
int FillBoard(void);

/**
 * Swap two adjacent gems if the swap creates a match.
 * @param x1, y1 First gem position
 * @param x2, y2 Second gem position (must be adjacent)
 * @return true if swap was valid and executed
 */
bool SwapGems(int x1, int y1, int x2, int y2);

/**
 * Attempt to swap gems (used for animation on invalid moves).
 * @param x1, y1 First gem position
 * @param x2, y2 Second gem position
 * @return true if swap was valid
 */
bool TrySwapGems(int x1, int y1, int x2, int y2);

/**
 * Check if swapping two gems would create a match.
 * Does not modify the board.
 * @param x1, y1 First gem position
 * @param x2, y2 Second gem position
 * @return true if swap would be valid
 */
bool IsValidSwap(int x1, int y1, int x2, int y2);

/**
 * Check if any valid moves remain on the board.
 * @return true if game is over (no valid moves)
 */
bool CheckGameOver(void);

/**
 * Count the number of valid moves available.
 * @return Number of valid swap moves
 */
int CountValidMoves(void);

/* ============================================================================
 * CASCADE HANDLING
 * ============================================================================ */

/**
 * Increment cascade level after a chain match.
 * Call when gravity/fill creates new matches.
 */
void IncrementCascade(void);

/**
 * Reset cascade tracking.
 * Call when player makes a new move.
 */
void ResetCascade(void);

/* ============================================================================
 * STATE ACCESSORS
 * ============================================================================ */

/**
 * Get current game state.
 */
GameState GetGameState(void);

/**
 * Set game state.
 */
void SetGameState(GameState state);

/**
 * Get current score.
 */
int GetScore(void);

/**
 * Get current level.
 */
int GetLevel(void);

/**
 * Get level progress information for HUD display.
 * @param currentLevelScore Output: score threshold for current level
 * @param nextLevelScore Output: score threshold for next level
 */
void GetLevelProgress(int *currentLevelScore, int *nextLevelScore);

/**
 * Get remaining moves (-1 if unlimited).
 */
int GetMovesRemaining(void);

/**
 * Get remaining time (-1 if unlimited).
 */
float GetTimeRemaining(void);

/**
 * Update time remaining (call each frame).
 * @param deltaTime Time since last frame
 */
void UpdateTime(float deltaTime);

/**
 * Get current cascade level.
 */
int GetCascadeLevel(void);

/* ============================================================================
 * SELECTION HANDLING
 * ============================================================================ */

/**
 * Get currently selected gem position.
 * @return Position with x,y = -1 if nothing selected
 */
Position GetSelectedGem(void);

/**
 * Set selected gem position.
 * @param x, y Grid coordinates
 */
void SetSelectedGem(int x, int y);

/**
 * Clear the current selection.
 */
void ClearSelection(void);

/**
 * Check if a gem is currently selected.
 */
bool HasSelection(void);

/* ============================================================================
 * BOARD ACCESS
 * ============================================================================ */

/**
 * Get the gem type at a board position.
 * @param x, y Grid coordinates
 * @return Gem type (GEM_EMPTY if position is empty or invalid)
 */
int GetBoardGem(int x, int y);

/**
 * Get the special type at a board position.
 * @param x, y Grid coordinates
 * @return Special type (SPECIAL_NONE if normal gem)
 */
int GetBoardSpecial(int x, int y);

/**
 * Set the special type at a board position.
 * @param x, y Grid coordinates
 * @param specialType The special type to set
 */
void SetBoardSpecial(int x, int y, int specialType);

/**
 * Get animation state for a gem.
 * @param x, y Grid coordinates
 * @return Pointer to GemAnimation, or NULL if invalid position
 */
GemAnimation* GetGemAnimation(int x, int y);

/* ============================================================================
 * SPECIAL GEM FUNCTIONS
 * ============================================================================ */

/**
 * Analyze matches and determine which special gems should be created.
 * Call after CheckMatches() and before RemoveMatches().
 * @return Number of special gems to be created
 */
int AnalyzeForSpecialGems(void);

/**
 * Get pending special gems array.
 * @return Pointer to pending special gems array
 */
PendingSpecialGem* GetPendingSpecialGems(void);

/**
 * Get count of pending special gems.
 * @return Number of pending special gems
 */
int GetPendingSpecialCount(void);

/**
 * Execute a Flame Gem effect (3x3 explosion).
 * @param x, y Center position
 * @return Number of gems destroyed
 */
int ExecuteFlameEffect(int x, int y);

/**
 * Execute a Star Gem effect (row + column clear).
 * @param x, y Center position
 * @return Number of gems destroyed
 */
int ExecuteStarEffect(int x, int y);

/**
 * Execute a Hypercube effect (clear all of one color).
 * @param targetColor The gem color to clear
 * @return Number of gems destroyed
 */
int ExecuteHypercubeEffect(int targetColor);

/**
 * Execute a Supernova effect (3-wide cross pattern).
 * @param x, y Center position
 * @return Number of gems destroyed
 */
int ExecuteSupernovaEffect(int x, int y);

/**
 * Queue a special effect for later execution.
 * @param x, y Position
 * @param effectType Type of effect
 * @param targetColor For Hypercube, the target color
 */
void QueueSpecialEffect(int x, int y, int effectType, int targetColor);

/**
 * Process queued special effects.
 * @return true if any effects were processed
 */
bool ProcessQueuedEffects(void);

/**
 * Check if a gem at position is special.
 * @param x, y Grid coordinates
 * @return true if gem is a special gem
 */
bool IsSpecialGem(int x, int y);

/**
 * Check if position has a Hypercube (for special swap handling).
 * @param x, y Grid coordinates
 * @return true if position has a Hypercube
 */
bool IsHypercube(int x, int y);

/**
 * Perform Hypercube swap (doesn't require valid match).
 * @param hcX, hcY Hypercube position
 * @param targetX, targetY Target gem position
 * @return true if swap was executed
 */
bool SwapHypercube(int hcX, int hcY, int targetX, int targetY);

/* ============================================================================
 * ANIMATION HELPERS
 * ============================================================================ */

/**
 * Clear all animation states.
 * Call after animations complete.
 */
void ClearAnimations(void);

/**
 * Check if any animations are currently active.
 */
bool HasActiveAnimations(void);

/* ============================================================================
 * STATISTICS
 * ============================================================================ */

/**
 * Get game statistics.
 * Pass NULL for any values you don't need.
 */
void GetGameStats(int *totalMatches, int *largestMatch, int *longestCascade, int *gemsDestroyed);

/* ============================================================================
 * UTILITIES
 * ============================================================================ */

/**
 * Find a valid move (for hint system).
 * @param x1, y1 Output: first gem position
 * @param x2, y2 Output: second gem position
 * @return true if a hint was found
 */
bool GetHint(int *x1, int *y1, int *x2, int *y2);

/**
 * Shuffle the board when no moves are available.
 * Preserves gem counts but randomizes positions.
 */
void ShuffleBoard(void);

/* ============================================================================
 * TWIST MODE FUNCTIONS
 * ============================================================================ */

/**
 * Rotate a 2x2 grid of gems clockwise.
 * Used in Twist mode instead of swapping.
 * @param topLeftX X coordinate of top-left corner of 2x2 grid
 * @param topLeftY Y coordinate of top-left corner of 2x2 grid
 * @return true if rotation created a match, false otherwise
 */
bool RotateGems(int topLeftX, int topLeftY);

/**
 * Check if rotating a 2x2 grid would create a match.
 * Does not modify the board.
 * @param topLeftX X coordinate of top-left corner
 * @param topLeftY Y coordinate of top-left corner
 * @return true if rotation would create a match
 */
bool IsValidRotation(int topLeftX, int topLeftY);

/**
 * Check if any valid rotation moves exist on the board.
 * Used for game over detection in Twist mode.
 * @return true if no valid rotations exist (game over)
 */
bool CheckTwistGameOver(void);

/**
 * Find a valid rotation move (for hint system in Twist mode).
 * @param topLeftX Output: X coordinate of valid 2x2 rotation
 * @param topLeftY Output: Y coordinate of valid 2x2 rotation
 * @return true if a hint was found
 */
bool GetTwistHint(int *topLeftX, int *topLeftY);

/* ============================================================================
 * CASCADE RUSH MODE FUNCTIONS
 * ============================================================================ */

/**
 * Initialize Cascade Rush mode state.
 */
void InitCascadeRush(void);

/**
 * Update Cascade Rush mode (call each frame).
 * @param deltaTime Time since last frame
 * @return true if game should continue, false if time expired
 */
bool UpdateCascadeRush(float deltaTime);

/**
 * Spawn a new rush zone on the board.
 */
void SpawnRushZone(void);

/**
 * Check if a board position is inside the active rush zone.
 * @param x, y Grid coordinates
 * @return true if position is inside rush zone
 */
bool IsInsideRushZone(int x, int y);

/**
 * Get information about the current rush zone.
 * @param x Output: X position of rush zone top-left
 * @param y Output: Y position of rush zone top-left
 * @param active Output: true if rush zone is active
 * @param timeRemaining Output: time remaining on rush zone
 */
void GetRushZoneInfo(int *x, int *y, bool *active, float *timeRemaining);

/**
 * Add time bonus for cascade matches.
 * @param cascadeLevel Current cascade level
 * @return Time bonus added
 */
float AddCascadeTimeBonus(int cascadeLevel);

/**
 * Capture the current rush zone (called when player swipes through zone).
 * Deactivates the zone and increments capture count.
 */
void CaptureRushZone(void);

/**
 * Get the number of rush zones captured.
 * @return Number of zones captured this game
 */
int GetRushZonesCaptured(void);

/**
 * Get total time added from cascades.
 * @return Total time added in seconds
 */
float GetTimeAddedFromCascades(void);

/* ============================================================================
 * GEM SURGE MODE FUNCTIONS
 * ============================================================================ */

/**
 * Initialize Gem Surge mode state.
 */
void InitGemSurge(void);

/**
 * Update Gem Surge mode (call each frame).
 * @param deltaTime Time since last frame
 * @return true if game should continue, false if time expired
 */
bool UpdateGemSurge(float deltaTime);

/**
 * Get current wave number (1-indexed).
 * @return Current wave number
 */
int GetCurrentWave(void);

/**
 * Get score target for current wave.
 * @return Score target to advance to next wave
 */
int GetWaveTarget(void);

/**
 * Get score earned in current wave.
 * @return Score accumulated in current wave
 */
int GetWaveScore(void);

/**
 * Get the featured gem type for current wave.
 * @return Gem type (GEM_RED, GEM_BLUE, etc.) or 0 if none
 */
int GetFeaturedGemType(void);

/**
 * Check if a surge line is active.
 * @param index Line index (0 to SURGE_MAX_LINES-1)
 * @return true if line is active
 */
bool IsSurgeLineActive(int index);

/**
 * Get information about a surge line.
 * @param index Line index (0 to SURGE_MAX_LINES-1)
 * @param horizontal Output: true if horizontal, false if vertical
 * @param position Output: row or column index
 * @param timeRemaining Output: time remaining on line
 * @return true if line is active and info was populated
 */
bool GetSurgeLineInfo(int index, bool *horizontal, int *position, float *timeRemaining);

/**
 * Spawn a new surge line at a random position.
 */
void SpawnSurgeLine(void);

/**
 * Trigger a surge line - clear all gems in that row/column.
 * @param index Index of the surge line to trigger
 * @return Points earned from triggering the line
 */
int TriggerSurgeLine(int index);

/**
 * Get the number of active surge lines.
 * @return Number of active lines
 */
int GetSurgeActiveLineCount(void);

/**
 * Get the total number of surge lines cleared.
 * @return Total lines cleared
 */
int GetSurgeLinesCleared(void);

/**
 * Add score to the current wave score.
 * @param score Score to add
 */
void AddToWaveScore(int score);

/* ============================================================================
 * LIGHTNING / SPECIAL EFFECTS
 * ============================================================================ */

/* Lightning strike info for 5-in-a-row */
typedef struct {
    bool active;            /* True if a lightning strike should occur */
    bool isHorizontal;      /* True if horizontal, false if vertical */
    int row;                /* Row for horizontal strike */
    int col;                /* Column for vertical strike */
    int centerX;            /* Center X position of the 5-match */
    int centerY;            /* Center Y position of the 5-match */
} LightningInfo;

/**
 * Get pending lightning strike info (from 5+ matches).
 * Returns info about any 5-in-a-row that triggered lightning.
 */
LightningInfo GetLightningInfo(void);

/**
 * Clear the lightning info after processing.
 */
void ClearLightningInfo(void);

/**
 * Execute a lightning strike - clears an entire row or column.
 * @param isHorizontal True to clear a row, false to clear a column
 * @param index The row or column index to clear
 * @return Number of gems destroyed
 */
int ExecuteLightningStrike(bool isHorizontal, int index);

#endif /* BEJEWELED_LOGIC_H */
