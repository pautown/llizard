/**
 * millionaire_types.h
 *
 * "Who Wants to Be a Millionaire" game plugin for llizardgui-host
 * Complete game state machine and data structures
 *
 * Target: 800x480 display (Spotify CarThing)
 * Uses: raylib, LlzPluginAPI, LlzInputState
 */

#ifndef MILLIONAIRE_TYPES_H
#define MILLIONAIRE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Total number of questions per game (classic format) */
#define MIL_QUESTION_COUNT          15

/** Number of answer options per question */
#define MIL_ANSWER_COUNT            4

/** Maximum length for question text */
#define MIL_QUESTION_TEXT_MAX       256

/** Maximum length for answer option text */
#define MIL_ANSWER_TEXT_MAX         64

/** Maximum length for category name */
#define MIL_CATEGORY_TEXT_MAX       32

/** Number of lifelines available */
#define MIL_LIFELINE_COUNT          3

/** Safe haven levels (indices into prize array) */
#define MIL_SAFE_HAVEN_1            4   /* $1,000 - question 5 */
#define MIL_SAFE_HAVEN_2            9   /* $32,000 - question 10 */

/** Animation timing constants (seconds) */
#define MIL_TITLE_PULSE_SPEED       1.5f
#define MIL_ANSWER_LOCK_DURATION    2.0f
#define MIL_CORRECT_CELEBRATE_TIME  3.0f
#define MIL_WRONG_REVEAL_TIME       4.0f
#define MIL_LIFELINE_ANIM_DURATION  2.5f
#define MIL_AUDIENCE_POLL_DURATION  3.0f
#define MIL_PHONE_FRIEND_DURATION   30.0f

/** Input timing */
#define MIL_INPUT_DEBOUNCE_TIME     0.15f
#define MIL_HOLD_THRESHOLD          0.5f

/** Display layout constants (800x480 screen) */
#define MIL_SCREEN_WIDTH            800
#define MIL_SCREEN_HEIGHT           480

#define MIL_QUESTION_BOX_Y          50
#define MIL_QUESTION_BOX_HEIGHT     120
#define MIL_ANSWER_GRID_Y           200
#define MIL_ANSWER_BOX_WIDTH        360
#define MIL_ANSWER_BOX_HEIGHT       70
#define MIL_ANSWER_GRID_GAP_X       40
#define MIL_ANSWER_GRID_GAP_Y       20
#define MIL_PRIZE_LADDER_X          650
#define MIL_PRIZE_LADDER_WIDTH      140
#define MIL_LIFELINE_BAR_Y          420
#define MIL_LIFELINE_ICON_SIZE      48

/* ============================================================================
 * GAME STATE MACHINE
 * ============================================================================ */

/**
 * Main game states - controls which screen/mode is active
 */
typedef enum MilGameState {
    MIL_STATE_TITLE_SCREEN = 0,     /**< Animated title with "Press to Play" */
    MIL_STATE_GAME_PLAYING,         /**< Main gameplay - question display */
    MIL_STATE_LIFELINE_5050,        /**< 50:50 lifeline animation (removing 2 wrong answers) */
    MIL_STATE_LIFELINE_PHONE,       /**< Phone a friend - shows "friend's" answer/confidence */
    MIL_STATE_LIFELINE_AUDIENCE,    /**< Audience poll - animated bar chart */
    MIL_STATE_ANSWER_LOCKED,        /**< Brief pause after selecting - "Final answer?" moment */
    MIL_STATE_CORRECT_ANSWER,       /**< Celebration animation - answer was correct */
    MIL_STATE_WRONG_ANSWER,         /**< Reveal correct answer, show guaranteed winnings */
    MIL_STATE_GAME_WON,             /**< Million dollar winner celebration */
    MIL_STATE_WALKAWAY_CONFIRM,     /**< "Are you sure you want to walk away?" prompt */
    MIL_STATE_FINAL_RESULTS,        /**< Final score display - option to play again */

    MIL_STATE_COUNT                 /**< Number of states (for validation) */
} MilGameState;

/**
 * Answer selection states for visual feedback
 */
typedef enum MilAnswerState {
    MIL_ANS_NORMAL = 0,             /**< Default unselected state */
    MIL_ANS_HIGHLIGHTED,            /**< Currently highlighted by cursor */
    MIL_ANS_SELECTED,               /**< Selected by player (orange flash) */
    MIL_ANS_LOCKED,                 /**< Locked in as final answer */
    MIL_ANS_CORRECT,                /**< Revealed as correct (green) */
    MIL_ANS_WRONG,                  /**< Revealed as wrong (red) */
    MIL_ANS_ELIMINATED              /**< Eliminated by 50:50 lifeline (grayed out) */
} MilAnswerState;

/**
 * Lifeline types
 */
typedef enum MilLifelineType {
    MIL_LIFELINE_50_50 = 0,         /**< Remove two wrong answers */
    MIL_LIFELINE_PHONE_FRIEND,      /**< Phone a friend for help */
    MIL_LIFELINE_ASK_AUDIENCE,      /**< Ask the audience poll */

    MIL_LIFELINE_TYPE_COUNT
} MilLifelineType;

/**
 * Question difficulty levels
 */
typedef enum MilDifficulty {
    MIL_DIFF_EASY = 0,              /**< Questions 1-5 ($100 - $1,000) */
    MIL_DIFF_MEDIUM,                /**< Questions 6-10 ($2,000 - $32,000) */
    MIL_DIFF_HARD,                  /**< Questions 11-14 ($64,000 - $500,000) */
    MIL_DIFF_EXTREME,               /**< Question 15 ($1,000,000) */

    MIL_DIFF_COUNT
} MilDifficulty;

/**
 * Navigation direction for 2x2 answer grid
 */
typedef enum MilNavDirection {
    MIL_NAV_UP = 0,
    MIL_NAV_DOWN,
    MIL_NAV_LEFT,
    MIL_NAV_RIGHT
} MilNavDirection;

/* ============================================================================
 * PRIZE LEVEL DEFINITIONS
 * ============================================================================ */

/**
 * Prize amounts for each of the 15 levels (in dollars)
 * Index 0 = Question 1 ($100)
 * Index 14 = Question 15 ($1,000,000)
 */
static const int MIL_PRIZE_AMOUNTS[MIL_QUESTION_COUNT] = {
    100,        /* Level 1 */
    200,        /* Level 2 */
    300,        /* Level 3 */
    500,        /* Level 4 */
    1000,       /* Level 5 - SAFE HAVEN 1 */
    2000,       /* Level 6 */
    4000,       /* Level 7 */
    8000,       /* Level 8 */
    16000,      /* Level 9 */
    32000,      /* Level 10 - SAFE HAVEN 2 */
    64000,      /* Level 11 */
    125000,     /* Level 12 */
    250000,     /* Level 13 */
    500000,     /* Level 14 */
    1000000     /* Level 15 - MILLION DOLLARS! */
};

/**
 * Prize amount display strings (formatted with commas and $)
 */
static const char* MIL_PRIZE_STRINGS[MIL_QUESTION_COUNT] = {
    "$100",
    "$200",
    "$300",
    "$500",
    "$1,000",
    "$2,000",
    "$4,000",
    "$8,000",
    "$16,000",
    "$32,000",
    "$64,000",
    "$125,000",
    "$250,000",
    "$500,000",
    "$1,000,000"
};

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * Single question with 4 answer options
 */
typedef struct MilQuestion {
    int id;                                         /**< Unique question identifier */
    char text[MIL_QUESTION_TEXT_MAX];               /**< Question text */
    char answers[MIL_ANSWER_COUNT][MIL_ANSWER_TEXT_MAX]; /**< A, B, C, D answer options */
    int correctIndex;                               /**< Correct answer (0-3) */
    MilDifficulty difficulty;                       /**< Difficulty tier */
    char category[MIL_CATEGORY_TEXT_MAX];           /**< Category/topic */
    bool used;                                      /**< Already used in current session */
} MilQuestion;

/**
 * Lifeline state tracking
 */
typedef struct MilLifelineState {
    bool available[MIL_LIFELINE_TYPE_COUNT];        /**< Which lifelines can still be used */
    bool eliminatedAnswers[MIL_ANSWER_COUNT];       /**< For 50:50 - which answers are eliminated */

    /* Phone a friend result */
    struct {
        int suggestedAnswer;                        /**< Friend's suggested answer (0-3, or -1 if unsure) */
        int confidencePercent;                      /**< Friend's confidence (0-100) */
        char friendName[32];                        /**< Name of the "friend" */
    } phoneResult;

    /* Audience poll result */
    struct {
        int pollPercentages[MIL_ANSWER_COUNT];      /**< Percentage for each answer (should sum to 100) */
        bool pollComplete;                          /**< Animation finished */
    } audienceResult;
} MilLifelineState;

/**
 * Visual/animation state for current screen
 */
typedef struct MilAnimationState {
    float stateTimer;                               /**< Time elapsed in current state */
    float pulsePhase;                               /**< For pulsing animations (0.0 - 2*PI) */
    float slideOffset;                              /**< For sliding animations */
    float fadeAlpha;                                /**< For fade transitions (0.0 - 1.0) */

    /* Answer reveal animation */
    struct {
        int revealStep;                             /**< Current step of reveal sequence */
        float revealTimer;                          /**< Timer for current reveal step */
        bool flashOn;                               /**< For flashing effect */
    } answerReveal;

    /* Prize ladder animation */
    struct {
        float highlightOffset;                      /**< Animated highlight position */
        bool climbing;                              /**< Currently climbing ladder */
    } prizeLadder;

    /* Title screen */
    struct {
        float logoScale;                            /**< Pulsing logo scale */
        float promptAlpha;                          /**< Blinking "Press to Play" alpha */
    } title;

    /* Lifeline animations */
    struct {
        float barFillProgress;                      /**< For audience poll bars (0.0 - 1.0) */
        float phoneTimer;                           /**< Phone call timer countdown */
        int eliminationStep;                        /**< 50:50 animation step */
    } lifeline;
} MilAnimationState;

/**
 * 2x2 grid cursor for answer selection
 * Layout:
 *   [A=0] [B=1]
 *   [C=2] [D=3]
 */
typedef struct MilCursorState {
    int selectedIndex;                              /**< Currently highlighted answer (0-3) */
    int lockedIndex;                                /**< Locked-in answer (-1 if none) */
    float inputCooldown;                            /**< Debounce timer */
} MilCursorState;

/**
 * Sound effect identifiers
 */
typedef enum MilSoundEffect {
    MIL_SFX_NONE = 0,
    MIL_SFX_CURSOR_MOVE,            /**< D-pad navigation */
    MIL_SFX_ANSWER_SELECT,          /**< Answer selected */
    MIL_SFX_ANSWER_LOCK,            /**< "Final answer" lock-in */
    MIL_SFX_CORRECT,                /**< Correct answer sting */
    MIL_SFX_WRONG,                  /**< Wrong answer buzzer */
    MIL_SFX_LIFELINE_ACTIVATE,      /**< Lifeline used */
    MIL_SFX_TENSION_LOOP,           /**< Background tension music */
    MIL_SFX_CLOCK_TICK,             /**< Phone timer tick */
    MIL_SFX_MILLION_WIN,            /**< Million dollar celebration */
    MIL_SFX_COUNT
} MilSoundEffect;

/**
 * Complete game state - all data needed to save/restore a game
 */
typedef struct MilGameData {
    /* Core game progress */
    MilGameState currentState;                      /**< Current state machine state */
    MilGameState previousState;                     /**< For returning from confirmation dialogs */
    int currentQuestionIndex;                       /**< Which question we're on (0-14) */
    int currentPrizeLevel;                          /**< Corresponds to question index */
    int guaranteedPrize;                            /**< Safe haven amount if player loses */
    bool gameInProgress;                            /**< Game has started */
    bool gameComplete;                              /**< Game has ended (win/lose/walkaway) */

    /* Question management */
    MilQuestion* currentQuestion;                   /**< Pointer to current question */
    MilQuestion questionPool[MIL_QUESTION_COUNT * 3]; /**< Pool of available questions */
    int questionPoolSize;                           /**< Number of questions in pool */
    int questionsUsed[MIL_QUESTION_COUNT];          /**< IDs of questions used this game */

    /* Lifelines */
    MilLifelineState lifelines;                     /**< Lifeline availability and results */
    MilLifelineType activeLifeline;                 /**< Currently active lifeline (if any) */

    /* Selection state */
    MilCursorState cursor;                          /**< Answer grid navigation */
    MilAnswerState answerStates[MIL_ANSWER_COUNT];  /**< Visual state per answer */

    /* Animation */
    MilAnimationState animation;                    /**< Current animation state */

    /* Statistics (for this session) */
    struct {
        int gamesPlayed;
        int questionsAnswered;
        int totalWinnings;
        int highestLevel;
        int lifelinesUsed;
    } stats;

    /* Audio */
    MilSoundEffect pendingSound;                    /**< Sound to play this frame */
    bool musicPlaying;                              /**< Background music active */
} MilGameData;

/**
 * Input actions interpreted from LlzInputState
 * Abstracts the raw input into game-specific actions
 */
typedef struct MilInputActions {
    /* Navigation */
    bool navUp;                                     /**< D-pad up or swipe up */
    bool navDown;                                   /**< D-pad down or swipe down */
    bool navLeft;                                   /**< D-pad left or swipe left */
    bool navRight;                                  /**< D-pad right or swipe right */

    /* Confirm/Cancel */
    bool confirm;                                   /**< Select button or tap */
    bool cancel;                                    /**< Back button */

    /* Lifeline activation (quick access) */
    bool lifeline5050;                              /**< Button 1 - 50:50 */
    bool lifelinePhone;                             /**< Button 2 - Phone */
    bool lifelineAudience;                          /**< Button 3 - Audience */

    /* Touch/tap */
    bool tap;                                       /**< Screen tap detected */
    Vector2 tapPosition;                            /**< Position of tap */

    /* Special */
    bool walkaway;                                  /**< Long press back for walkaway */
} MilInputActions;

/**
 * Callback for question database loading
 */
typedef bool (*MilQuestionLoader)(MilQuestion* outQuestions, int* outCount, int maxCount);

/**
 * Plugin configuration
 */
typedef struct MilPluginConfig {
    bool soundEnabled;                              /**< Play sound effects */
    bool musicEnabled;                              /**< Play background music */
    bool showTimer;                                 /**< Show optional timer */
    int timerSeconds;                               /**< Time limit per question (0 = unlimited) */
    bool hardcoreMode;                              /**< No safe havens */
    MilQuestionLoader questionLoader;               /**< Custom question source */
} MilPluginConfig;

/* ============================================================================
 * HELPER MACROS
 * ============================================================================ */

/** Check if a state is a gameplay state (not menu/results) */
#define MIL_IS_GAMEPLAY_STATE(s) \
    ((s) == MIL_STATE_GAME_PLAYING || \
     (s) == MIL_STATE_LIFELINE_5050 || \
     (s) == MIL_STATE_LIFELINE_PHONE || \
     (s) == MIL_STATE_LIFELINE_AUDIENCE || \
     (s) == MIL_STATE_ANSWER_LOCKED || \
     (s) == MIL_STATE_CORRECT_ANSWER || \
     (s) == MIL_STATE_WRONG_ANSWER)

/** Check if player has reached a safe haven */
#define MIL_IS_SAFE_HAVEN(level) \
    ((level) == MIL_SAFE_HAVEN_1 || (level) == MIL_SAFE_HAVEN_2)

/** Get guaranteed winnings based on current level */
#define MIL_GET_GUARANTEED_PRIZE(level) \
    ((level) >= MIL_SAFE_HAVEN_2 ? MIL_PRIZE_AMOUNTS[MIL_SAFE_HAVEN_2] : \
     (level) >= MIL_SAFE_HAVEN_1 ? MIL_PRIZE_AMOUNTS[MIL_SAFE_HAVEN_1] : 0)

/** Convert 2x2 grid index to row (0=top, 1=bottom) */
#define MIL_GRID_ROW(idx) ((idx) / 2)

/** Convert 2x2 grid index to column (0=left, 1=right) */
#define MIL_GRID_COL(idx) ((idx) % 2)

/** Convert row,col to grid index */
#define MIL_GRID_INDEX(row, col) ((row) * 2 + (col))

/** Answer letter from index (A, B, C, D) */
#define MIL_ANSWER_LETTER(idx) ('A' + (idx))

/* ============================================================================
 * FUNCTION PROTOTYPES
 * ============================================================================ */

/**
 * Initialize game data to default state
 */
void MilGameDataInit(MilGameData* data);

/**
 * Reset game for new round (keeps stats)
 */
void MilGameDataReset(MilGameData* data);

/**
 * Process raw input state into game actions
 * @param input Raw input from host
 * @param actions Output action struct
 * @param cursor Cursor state for debounce tracking
 * @param deltaTime Frame time for cooldown
 */
void MilProcessInput(const LlzInputState* input, MilInputActions* actions,
                     MilCursorState* cursor, float deltaTime);

/**
 * Navigate cursor in 2x2 grid
 * @param cursor Current cursor state
 * @param direction Navigation direction
 * @param eliminatedAnswers Which answers are eliminated (for 50:50)
 * @return true if cursor moved
 */
bool MilNavigateCursor(MilCursorState* cursor, MilNavDirection direction,
                       const bool eliminatedAnswers[MIL_ANSWER_COUNT]);

/**
 * Get difficulty for question level
 */
MilDifficulty MilGetDifficultyForLevel(int level);

/**
 * Select a random question for the given difficulty
 */
MilQuestion* MilSelectQuestion(MilGameData* data, MilDifficulty difficulty);

/**
 * Apply 50:50 lifeline - eliminates 2 wrong answers
 * @param data Game data
 * @return true if lifeline was available and applied
 */
bool MilApply5050Lifeline(MilGameData* data);

/**
 * Apply Phone a Friend lifeline - generates friend's response
 * @param data Game data
 * @return true if lifeline was available and applied
 */
bool MilApplyPhoneLifeline(MilGameData* data);

/**
 * Apply Ask the Audience lifeline - generates poll results
 * @param data Game data
 * @return true if lifeline was available and applied
 */
bool MilApplyAudienceLifeline(MilGameData* data);

/**
 * Transition to new game state
 * @param data Game data
 * @param newState State to transition to
 */
void MilTransitionState(MilGameData* data, MilGameState newState);

/**
 * Update animation state for current frame
 * @param data Game data
 * @param deltaTime Frame time
 */
void MilUpdateAnimations(MilGameData* data, float deltaTime);

/**
 * Check answer and update game state
 * @param data Game data
 * @param answerIndex Selected answer (0-3)
 * @return true if answer was correct
 */
bool MilCheckAnswer(MilGameData* data, int answerIndex);

/**
 * Format prize amount with commas and $
 * @param amount Dollar amount
 * @param buffer Output buffer
 * @param bufferSize Size of output buffer
 */
void MilFormatPrize(int amount, char* buffer, int bufferSize);

#ifdef __cplusplus
}
#endif

#endif /* MILLIONAIRE_TYPES_H */
