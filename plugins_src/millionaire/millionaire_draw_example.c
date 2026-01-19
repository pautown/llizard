/**
 * millionaire_draw_example.c
 *
 * Example integration showing how to use the millionaire_draw.h functions
 * in the flashcards plugin. This demonstrates the complete visual system.
 *
 * To integrate with flashcards_plugin.c:
 * 1. #include "millionaire_draw.h" at the top
 * 2. Call InitMillionaireGraphics() in PluginInit()
 * 3. Replace existing Draw functions with these enhanced versions
 */

#include "millionaire_draw.h"
#include <stdio.h>

// ============================================================================
// Example: Complete Millionaire Screen Drawing
// ============================================================================

/**
 * Example state structure for demonstration
 */
typedef struct {
    int currentLevel;           // 0-14
    int selectedAnswer;         // 0-3
    bool answerLocked;
    int correctAnswer;          // 0-3
    bool showingResult;
    bool wasCorrect;
    LifelineState lifelines;
    const char *question;
    const char *answers[4];
} ExampleGameState;

/**
 * DrawMillionaireGame
 *
 * Complete example of drawing the Millionaire game screen
 * using all the visual components.
 */
static void DrawMillionaireGame(ExampleGameState *state, float deltaTime, Font font) {
    // 1. Draw animated background with particles and spotlights
    DrawMillionaireBackground(deltaTime);

    // 2. Draw prize ladder on the left
    DrawPrizeLadder(state->currentLevel, font);

    // 3. Prepare answer states
    AnswerState answerStates[4];
    for (int i = 0; i < 4; i++) {
        if (state->showingResult) {
            if (i == state->correctAnswer) {
                answerStates[i] = ANSWER_STATE_CORRECT;
            } else if (i == state->selectedAnswer && !state->wasCorrect) {
                answerStates[i] = ANSWER_STATE_WRONG;
            } else {
                answerStates[i] = ANSWER_STATE_NORMAL;
            }
        } else if (state->answerLocked && i == state->selectedAnswer) {
            answerStates[i] = ANSWER_STATE_LOCKED;
        } else if (i == state->selectedAnswer) {
            answerStates[i] = ANSWER_STATE_SELECTED;
        } else {
            // Check if eliminated by 50:50
            bool eliminated = false;
            if (state->lifelines.fiftyFiftyUsed) {
                for (int j = 0; j < 2; j++) {
                    if (state->lifelines.eliminatedOptions[j] == i) {
                        eliminated = true;
                        break;
                    }
                }
            }
            answerStates[i] = eliminated ? ANSWER_STATE_ELIMINATED : ANSWER_STATE_NORMAL;
        }
    }

    // 4. Draw question panel with answers
    DrawQuestionPanel(state->question, state->answers, answerStates, state->selectedAnswer, font);

    // 5. Draw lifeline panel
    DrawLifelinePanel(state->lifelines, -1, font);  // -1 = no lifeline selected

    // 6. Draw current prize display at top
    char prizeHeader[64];
    snprintf(prizeHeader, sizeof(prizeHeader), "Playing for: %s", PRIZE_LEVELS[state->currentLevel]);
    Vector2 prizeSize = MeasureTextEx(font, prizeHeader, 24, 1);
    DrawTextEx(font, prizeHeader,
               (Vector2){(MILLIONAIRE_SCREEN_WIDTH - prizeSize.x) / 2, 12},
               24, 1, MILLIONAIRE_GOLD);

    // 7. Draw instructions at bottom
    const char *instructions;
    if (state->showingResult) {
        instructions = state->wasCorrect ? "Correct! Press SELECT to continue" : "Press SELECT to see results";
    } else if (state->answerLocked) {
        instructions = "Waiting for result...";
    } else {
        instructions = "Scroll to select, press SELECT to lock in answer";
    }

    Vector2 instSize = MeasureTextEx(font, instructions, 14, 1);
    DrawTextEx(font, instructions,
               (Vector2){MILLIONAIRE_SCREEN_WIDTH - instSize.x - 15, MILLIONAIRE_SCREEN_HEIGHT - 25},
               14, 1, ColorAlpha(MILLIONAIRE_WHITE, 0.6f));
}

// ============================================================================
// Example: Title Screen Usage
// ============================================================================

static void DrawMillionaireTitleExample(float deltaTime, Font font) {
    DrawTitleScreen(deltaTime, font);
}

// ============================================================================
// Example: Win Screen Usage
// ============================================================================

static void DrawMillionaireWinExample(float deltaTime, Font font, int finalLevel) {
    const char *prize = (finalLevel >= 15) ? "$1,000,000" : PRIZE_LEVELS[finalLevel - 1];
    bool isMillionaire = (finalLevel >= 15);
    DrawWinAnimation(deltaTime, font, prize, isMillionaire);
}

// ============================================================================
// Example: Lose Screen Usage
// ============================================================================

static void DrawMillionaireLoseExample(float deltaTime, Font font,
                                        const char *correctAnswer, int correctIdx, int levelLost) {
    // Determine walk-away prize based on safe havens
    const char *walkAway;
    if (levelLost <= 4) {
        walkAway = "$0";
    } else if (levelLost <= 9) {
        walkAway = "$1,000";
    } else {
        walkAway = "$32,000";
    }

    char letter = 'A' + correctIdx;
    DrawLoseScreen(deltaTime, font, correctAnswer, letter, walkAway);
}

// ============================================================================
// Integration Guide
// ============================================================================

/*
 * INTEGRATION INTO flashcards_plugin.c:
 *
 * 1. Add include at top:
 *    #include "millionaire_draw.h"
 *
 * 2. In PluginInit():
 *    InitMillionaireGraphics();
 *
 * 3. Replace DrawMillionaireBackground():
 *    // Old:
 *    static void DrawMillionaireBackground(void) { ... }
 *
 *    // New: (already defined in millionaire_draw.h)
 *    // Just pass deltaTime when calling:
 *    DrawMillionaireBackground(deltaTime);
 *
 * 4. Replace DrawMillionaireScreen():
 *    static void DrawMillionaireScreen(float deltaTime) {
 *        // Get current question
 *        int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
 *        const Question *q = &g_quiz.questions[qIdx];
 *
 *        // Draw background with particles
 *        DrawMillionaireBackground(deltaTime);
 *
 *        // Draw prize ladder
 *        DrawPrizeLadder(g_quiz.currentPrizeLevel, g_font);
 *
 *        // Prepare answer states
 *        AnswerState answerStates[4] = {
 *            ANSWER_STATE_NORMAL,
 *            ANSWER_STATE_NORMAL,
 *            ANSWER_STATE_NORMAL,
 *            ANSWER_STATE_NORMAL
 *        };
 *        answerStates[g_quiz.selectedOption] = ANSWER_STATE_SELECTED;
 *
 *        // Draw question panel
 *        const char *answers[4] = {
 *            q->options[0], q->options[1], q->options[2], q->options[3]
 *        };
 *        DrawQuestionPanel(q->question, answers, answerStates,
 *                          g_quiz.selectedOption, g_font);
 *
 *        // Draw lifeline panel (if you add lifelines)
 *        LifelineState lifelines = {false, false, false, {-1, -1}};
 *        DrawLifelinePanel(lifelines, -1, g_font);
 *
 *        // Prize display and instructions...
 *    }
 *
 * 5. Replace DrawMillionaireGameOverScreen():
 *    static void DrawMillionaireGameOverScreen(float deltaTime) {
 *        if (g_quiz.millionaireGameOver) {
 *            // Lost
 *            int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
 *            const Question *q = &g_quiz.questions[qIdx];
 *            const char *walkAway = GetWalkAwayPrize(g_quiz.currentPrizeLevel);
 *            DrawLoseScreen(deltaTime, g_font, q->options[q->correctIndex],
 *                          'A' + q->correctIndex, walkAway);
 *        } else {
 *            // Won
 *            const char *prize = g_prizeLevels[g_quiz.currentPrizeLevel - 1];
 *            bool isMillionaire = (g_quiz.currentPrizeLevel >= 15);
 *            DrawWinAnimation(deltaTime, g_font, prize, isMillionaire);
 *        }
 *    }
 *
 * 6. Update PluginUpdate() to pass deltaTime to draw functions:
 *    case SCREEN_MILLIONAIRE_MODE:
 *        UpdateMillionaireGraphics(deltaTime);  // Update particle/animation systems
 *        UpdateMillionaireScreen(input);
 *        break;
 *
 * 7. Add a title screen state if desired:
 *    case SCREEN_MILLIONAIRE_TITLE:
 *        DrawTitleScreen(deltaTime, g_font);
 *        if (input->selectPressed) {
 *            g_currentScreen = SCREEN_MILLIONAIRE_MODE;
 *        }
 *        break;
 */

// ============================================================================
// Quick Reference: All Functions Available
// ============================================================================

/*
 * BACKGROUND:
 *   DrawMillionaireBackground(float deltaTime)
 *     - Dark blue gradient with spotlight effects
 *     - Animated floating particles
 *     - Pulsing glow effects
 *
 * PRIZE LADDER:
 *   DrawPrizeLadder(int currentLevel, Font font)
 *     - 15 prize levels on left side
 *     - Current level with gold glow
 *     - Safe havens highlighted in orange
 *     - Passed levels in green
 *
 * QUESTION PANEL:
 *   DrawQuestionPanel(question, answers[4], answerStates[4], selectedIdx, font)
 *     - Question text with word wrap
 *     - 2x2 hexagonal answer buttons
 *     - States: NORMAL, SELECTED, LOCKED, CORRECT, WRONG, ELIMINATED
 *
 * LIFELINES:
 *   DrawLifelinePanel(LifelineState lifelines, int selectedLifeline, Font font)
 *     - 50:50 icon
 *     - Phone a Friend icon
 *     - Ask the Audience icon
 *     - Greyed out when used
 *
 * TITLE SCREEN:
 *   DrawTitleScreen(float deltaTime, Font font)
 *     - Animated "Who Wants to Be a Millionaire" logo
 *     - Particle effects
 *     - Pulsing "Press SELECT to play"
 *
 * WIN ANIMATION:
 *   DrawWinAnimation(float deltaTime, Font font, const char *prizeWon, bool isMillionaire)
 *     - Confetti celebration
 *     - Golden sparkles
 *     - Prize display
 *
 * LOSE SCREEN:
 *   DrawLoseScreen(float deltaTime, Font font, correctAnswer, correctLetter, walkAwayPrize)
 *     - Red-tinted game over display
 *     - Correct answer reveal
 *     - Walk-away prize
 *
 * INITIALIZATION:
 *   InitMillionaireGraphics()  - Call once at startup
 *   UpdateMillionaireGraphics(float deltaTime)  - Call each frame
 */

#endif // This file is for reference only
