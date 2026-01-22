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

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

/**
 * Initialize the game board with random gems, ensuring no initial matches.
 * Call this at the start of a new game.
 */
void InitGame(void);

/**
 * Initialize game with specific mode settings.
 * @param moves Number of moves allowed (-1 for unlimited)
 * @param timeLimit Time limit in seconds (-1 for unlimited)
 */
void InitGameWithMode(int moves, float timeLimit);

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
 * Get animation state for a gem.
 * @param x, y Grid coordinates
 * @return Pointer to GemAnimation, or NULL if invalid position
 */
GemAnimation* GetGemAnimation(int x, int y);

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
