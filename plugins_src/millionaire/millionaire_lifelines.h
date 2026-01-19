/**
 * Millionaire Lifelines System
 *
 * Implements the three classic "Who Wants to Be a Millionaire" lifelines:
 * - 50:50 (Fifty-Fifty): Removes two wrong answers
 * - Phone a Friend: Gets a hint from a "friend"
 * - Ask the Audience: Shows audience poll percentages
 *
 * Compatible with both Question (flashcards_plugin.c) and MillionaireQuestion types.
 */

#ifndef MILLIONAIRE_LIFELINES_H
#define MILLIONAIRE_LIFELINES_H

#include "raylib.h"
#include <stdbool.h>

// ============================================================================
// Constants
// ============================================================================

/** Animation durations (in seconds) */
#define LIFELINE_FIFTY_FIFTY_DURATION      2.5f
#define LIFELINE_PHONE_FRIEND_DURATION     3.5f
#define LIFELINE_PHONE_FRIEND_THINK_TIME   2.0f
#define LIFELINE_AUDIENCE_POLL_DURATION    3.0f

/** Display constants */
#define LIFELINE_ICON_SIZE                 60
#define LIFELINE_ICON_SPACING              20

// ============================================================================
// Lifeline State Enums
// ============================================================================

typedef enum {
    CONFIDENCE_LOW,
    CONFIDENCE_MEDIUM,
    CONFIDENCE_HIGH
} ConfidenceLevel;

typedef enum {
    LIFELINE_NONE,
    LIFELINE_FIFTY_FIFTY,
    LIFELINE_PHONE_FRIEND,
    LIFELINE_ASK_AUDIENCE
} LifelineType;

typedef enum {
    LIFELINE_STATE_INACTIVE,
    LIFELINE_STATE_ANIMATING,
    LIFELINE_STATE_SHOWING_RESULT,
    LIFELINE_STATE_COMPLETE
} LifelineAnimState;

// ============================================================================
// Data Structures
// ============================================================================

/** Phone a Friend result */
typedef struct {
    int suggestedAnswer;        /**< Index of the suggested answer (0-3) */
    ConfidenceLevel confidence; /**< How confident the friend sounds */
    bool isCorrect;             /**< Whether the suggestion is correct */
} PhoneFriendResult;

/** Ask the Audience result */
typedef struct {
    int percentages[4];         /**< Percentage for each answer (0-100, totals to 100) */
} AudienceResult;

/** Lifeline manager state - tracks all lifeline data for a game */
typedef struct {
    /* Availability (reset each game) */
    bool fiftyFiftyAvailable;
    bool phoneFriendAvailable;
    bool askAudienceAvailable;

    /* 50:50 state */
    bool eliminated[4];         /**< Which options have been eliminated */
    int eliminationOrder[2];    /**< Order in which options were eliminated (for animation) */

    /* Phone a Friend state */
    PhoneFriendResult phoneFriendResult;

    /* Ask the Audience state */
    AudienceResult audienceResult;

    /* Animation state */
    LifelineType activeLifeline;
    LifelineAnimState animState;
    float animTimer;            /**< Timer for animations */
    float animDuration;         /**< Total animation duration */

} LifelineManager;

// ============================================================================
// Core Lifeline Functions
// ============================================================================

/**
 * Initialize lifeline manager for a new game.
 * Resets all lifelines to available and clears any previous state.
 *
 * @param mgr Pointer to lifeline manager to initialize
 */
void LifelinesInit(LifelineManager* mgr);

/**
 * Reset lifeline state for a new question.
 * Keeps lifeline availability but clears per-question state like elimination flags.
 *
 * @param mgr Pointer to lifeline manager
 */
void LifelinesResetForQuestion(LifelineManager* mgr);

/**
 * Apply 50:50 lifeline - removes 2 incorrect answers.
 * The correct answer and one random wrong answer remain visible.
 *
 * @param mgr Lifeline manager (marks lifeline as used)
 * @param correctIndex Index of the correct answer (0-3)
 * @param optionCount Number of options (usually 4)
 * @param eliminated Output array marking which options are eliminated (true = eliminated)
 */
void ApplyFiftyFifty(LifelineManager* mgr, int correctIndex, int optionCount, bool eliminated[4]);

/**
 * Get Phone a Friend result.
 * Simulates calling a friend who gives their best guess with varying confidence.
 *
 * Accuracy varies by difficulty:
 * - Easy: ~80% chance of correct suggestion
 * - Medium: ~56% chance of correct suggestion
 * - Hard: ~40% chance of correct suggestion
 *
 * @param mgr Lifeline manager (marks lifeline as used)
 * @param correctIndex Index of the correct answer
 * @param difficulty Question difficulty ("easy", "medium", "hard")
 * @return PhoneFriendResult with suggested answer index and confidence level
 */
PhoneFriendResult GetPhoneFriendResult(LifelineManager* mgr, int correctIndex, const char* difficulty);

/**
 * Get Ask the Audience results.
 * Generates simulated audience poll percentages for each answer.
 *
 * Correct answer percentage ranges by difficulty:
 * - Easy: 40-70%
 * - Medium: 30-50%
 * - Hard: 20-40%
 *
 * @param mgr Lifeline manager (marks lifeline as used)
 * @param correctIndex Index of the correct answer
 * @param difficulty Question difficulty
 * @param eliminated Array of eliminated options (from 50:50), or NULL if not used
 * @param percentages Output array with percentage for each answer (totals to 100)
 */
void GetAudienceResults(LifelineManager* mgr, int correctIndex, const char* difficulty,
                        const bool eliminated[4], int percentages[4]);

// ============================================================================
// Animation Control
// ============================================================================

/**
 * Start a lifeline animation.
 * Initializes animation state and sets appropriate duration.
 *
 * @param mgr Lifeline manager
 * @param type Which lifeline to animate
 */
void LifelineStartAnimation(LifelineManager* mgr, LifelineType type);

/**
 * Update lifeline animation each frame.
 *
 * @param mgr Lifeline manager
 * @param deltaTime Frame delta time in seconds
 * @return true if animation is complete, false if still animating
 */
bool LifelineUpdateAnimation(LifelineManager* mgr, float deltaTime);

/**
 * Check if a lifeline is currently active/animating.
 *
 * @param mgr Lifeline manager
 * @return true if a lifeline animation is in progress or showing results
 */
bool LifelineIsActive(const LifelineManager* mgr);

/**
 * Mark lifeline animation as complete and transition to inactive state.
 *
 * @param mgr Lifeline manager
 */
void LifelineComplete(LifelineManager* mgr);

/**
 * Get animation progress as a value from 0.0 to 1.0.
 *
 * @param mgr Lifeline manager
 * @return Progress value (0.0 = start, 1.0 = complete)
 */
float LifelineGetProgress(const LifelineManager* mgr);

// ============================================================================
// Drawing Functions
// ============================================================================

/**
 * Draw 50:50 animation showing answers fading out.
 * Displays an overlay panel with letter circles, animating the elimination.
 *
 * @param font Font to use for text (use GetFontDefault() if no custom font)
 * @param progress Animation progress (0.0 to 1.0)
 * @param eliminated Which options are eliminated
 * @param eliminationOrder Order of elimination [first, second] for staged fade-out
 */
void DrawFiftyFiftyAnimation(Font font, float progress, const bool eliminated[4], const int eliminationOrder[2]);

/**
 * Draw Phone a Friend panel.
 * Shows a phone call UI with thinking animation or friend's response.
 *
 * @param font Font to use for text
 * @param suggestedAnswer Index of suggested answer (0-3)
 * @param confidence Confidence level (affects response text)
 * @param timer Animation timer (for thinking dots animation)
 * @param isThinking True if still in "thinking" phase, false to show response
 */
void DrawPhoneFriendPanel(Font font, int suggestedAnswer, ConfidenceLevel confidence, float timer, bool isThinking);

/**
 * Draw Ask the Audience bar chart.
 * Shows an animated bar chart of audience poll results.
 *
 * @param font Font to use for text
 * @param percentages Percentage for each answer (A, B, C, D)
 * @param animProgress Animation progress (0.0 to 1.0) for bar fill animation
 * @param eliminated Which options are eliminated (drawn grayed out with X)
 */
void DrawAudiencePollBars(Font font, const int percentages[4], float animProgress, const bool eliminated[4]);

/**
 * Draw lifeline icons/buttons.
 * Shows the three lifeline icons with availability and selection states.
 *
 * @param font Font to use for text (used for 50:50 label)
 * @param mgr Lifeline manager (for availability state)
 * @param x X position of first icon
 * @param y Y position of icons
 * @param selectedLifeline Currently highlighted lifeline index (0=50:50, 1=Phone, 2=Audience, -1=none)
 */
void DrawLifelineIcons(Font font, const LifelineManager* mgr, float x, float y, int selectedLifeline);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get display name for a lifeline type.
 *
 * @param type Lifeline type
 * @return Human-readable name string
 */
const char* GetLifelineName(LifelineType type);

/**
 * Check if any lifelines are still available.
 *
 * @param mgr Lifeline manager
 * @return true if at least one lifeline can still be used
 */
bool LifelinesAnyAvailable(const LifelineManager* mgr);

/**
 * Get count of remaining available lifelines.
 *
 * @param mgr Lifeline manager
 * @return Number of lifelines that haven't been used (0-3)
 */
int LifelinesAvailableCount(const LifelineManager* mgr);

#endif // MILLIONAIRE_LIFELINES_H
