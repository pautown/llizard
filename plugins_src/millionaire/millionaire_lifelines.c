/**
 * Millionaire Lifelines System Implementation
 *
 * Complete implementation of the three classic lifelines:
 * - 50:50: Removes two incorrect answers with fade-out animation
 * - Phone a Friend: Simulates calling a friend with thinking animation
 * - Ask the Audience: Animated bar chart showing poll results
 *
 * All drawing functions accept a Font parameter for consistent styling
 * with the host plugin.
 */

#include "millionaire_lifelines.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

// ============================================================================
// Color Palette (Millionaire Theme)
// ============================================================================

static const Color COLOR_GOLD = {255, 215, 0, 255};
static const Color COLOR_GOLD_DIM = {200, 170, 50, 255};
static const Color COLOR_DARK_BLUE = {15, 25, 50, 255};
static const Color COLOR_DARKER_BLUE = {8, 12, 35, 255};
static const Color COLOR_LIGHT_BLUE = {100, 160, 220, 255};
static const Color COLOR_HIGHLIGHT = {60, 120, 200, 255};
static const Color COLOR_TEXT_WHITE = {245, 245, 250, 255};
static const Color COLOR_TEXT_DIM = {120, 120, 140, 255};
static const Color COLOR_CORRECT_GREEN = {76, 175, 80, 255};
static const Color COLOR_WRONG_RED = {244, 67, 54, 255};
static const Color COLOR_ELIMINATED = {80, 60, 60, 255};
static const Color COLOR_BAR_BG = {30, 35, 55, 255};
static const Color COLOR_PANEL_BG = {20, 25, 45, 250};

// ============================================================================
// Friend Response Templates
// ============================================================================

static const char* FRIEND_RESPONSES_HIGH[] = {
    "I'm absolutely certain it's %c!",
    "100%% it's %c, trust me!",
    "No doubt about it - go with %c!",
    "I'd bet my house on %c!"
};

static const char* FRIEND_RESPONSES_MEDIUM[] = {
    "I'm pretty sure it's %c...",
    "I think it's %c, but don't quote me.",
    "My gut says %c.",
    "I'd go with %c if I had to choose."
};

static const char* FRIEND_RESPONSES_LOW[] = {
    "Um... maybe %c? I'm not sure...",
    "I really don't know... %c perhaps?",
    "This is a tough one... %c maybe?",
    "I'm guessing %c, but I could be wrong."
};

static const char* FRIEND_NAMES[] = {
    "Alex", "Jordan", "Sam", "Taylor", "Casey",
    "Morgan", "Riley", "Quinn", "Avery", "Drew"
};

#define NUM_RESPONSES 4
#define NUM_FRIEND_NAMES 10

// ============================================================================
// Helper Functions
// ============================================================================

/** Random integer in range [min, max] inclusive */
static int RandomRange(int min, int max) {
    if (max <= min) return min;
    return min + (rand() % (max - min + 1));
}

/** Get difficulty multiplier (affects accuracy of lifelines) */
static float GetDifficultyMultiplier(const char* difficulty) {
    if (!difficulty) return 1.0f;

    if (strcasecmp(difficulty, "easy") == 0) return 1.0f;
    if (strcasecmp(difficulty, "medium") == 0) return 0.7f;
    if (strcasecmp(difficulty, "hard") == 0) return 0.5f;

    return 0.8f; // Default for unknown difficulty
}

/** Ease-out cubic for smooth deceleration */
static float EaseOutCubic(float t) {
    t = t - 1.0f;
    return t * t * t + 1.0f;
}

/** Ease-in-out quad for smooth acceleration/deceleration */
static float EaseInOutQuad(float t) {
    if (t < 0.5f) return 2.0f * t * t;
    return 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

/** Clamp float to range */
static float Clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// ============================================================================
// Initialization Functions
// ============================================================================

void LifelinesInit(LifelineManager* mgr) {
    if (!mgr) return;

    // All lifelines available at game start
    mgr->fiftyFiftyAvailable = true;
    mgr->phoneFriendAvailable = true;
    mgr->askAudienceAvailable = true;

    // Clear elimination state
    memset(mgr->eliminated, 0, sizeof(mgr->eliminated));
    mgr->eliminationOrder[0] = -1;
    mgr->eliminationOrder[1] = -1;

    // Clear results
    memset(&mgr->phoneFriendResult, 0, sizeof(mgr->phoneFriendResult));
    mgr->phoneFriendResult.suggestedAnswer = -1;
    memset(&mgr->audienceResult, 0, sizeof(mgr->audienceResult));

    // Animation state
    mgr->activeLifeline = LIFELINE_NONE;
    mgr->animState = LIFELINE_STATE_INACTIVE;
    mgr->animTimer = 0.0f;
    mgr->animDuration = 0.0f;

    // Seed random number generator once
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    printf("Lifelines: Initialized - all lifelines available\n");
}

void LifelinesResetForQuestion(LifelineManager* mgr) {
    if (!mgr) return;

    // Keep availability, clear per-question state
    memset(mgr->eliminated, 0, sizeof(mgr->eliminated));
    mgr->eliminationOrder[0] = -1;
    mgr->eliminationOrder[1] = -1;

    memset(&mgr->phoneFriendResult, 0, sizeof(mgr->phoneFriendResult));
    mgr->phoneFriendResult.suggestedAnswer = -1;
    memset(&mgr->audienceResult, 0, sizeof(mgr->audienceResult));

    mgr->activeLifeline = LIFELINE_NONE;
    mgr->animState = LIFELINE_STATE_INACTIVE;
    mgr->animTimer = 0.0f;
    mgr->animDuration = 0.0f;
}

// ============================================================================
// 50:50 Lifeline Implementation
// ============================================================================

void ApplyFiftyFifty(LifelineManager* mgr, int correctIndex, int optionCount, bool eliminated[4]) {
    if (!mgr || !eliminated) return;
    if (!mgr->fiftyFiftyAvailable) {
        printf("Lifelines: 50:50 not available\n");
        return;
    }
    if (optionCount < 4 || correctIndex < 0 || correctIndex >= optionCount) {
        printf("Lifelines: Invalid question data for 50:50\n");
        return;
    }

    // Mark lifeline as used
    mgr->fiftyFiftyAvailable = false;

    // Initialize elimination array
    memset(eliminated, 0, sizeof(bool) * 4);

    // Collect indices of wrong answers
    int wrongIndices[3];
    int wrongCount = 0;

    for (int i = 0; i < optionCount && i < 4; i++) {
        if (i != correctIndex) {
            wrongIndices[wrongCount++] = i;
        }
    }

    // Shuffle wrong indices using Fisher-Yates
    for (int i = wrongCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = wrongIndices[i];
        wrongIndices[i] = wrongIndices[j];
        wrongIndices[j] = temp;
    }

    // Eliminate first 2 wrong answers
    if (wrongCount >= 2) {
        eliminated[wrongIndices[0]] = true;
        eliminated[wrongIndices[1]] = true;

        // Store elimination order for animation
        mgr->eliminationOrder[0] = wrongIndices[0];
        mgr->eliminationOrder[1] = wrongIndices[1];
    }

    // Copy to manager state
    memcpy(mgr->eliminated, eliminated, sizeof(bool) * 4);

    printf("Lifelines: 50:50 applied - eliminated %c and %c, keeping %c (correct) and %c\n",
           'A' + wrongIndices[0], 'A' + wrongIndices[1],
           'A' + correctIndex, 'A' + wrongIndices[2]);
}

// ============================================================================
// Phone a Friend Implementation
// ============================================================================

PhoneFriendResult GetPhoneFriendResult(LifelineManager* mgr, int correctIndex, const char* difficulty) {
    PhoneFriendResult result = {0};
    result.suggestedAnswer = -1;

    if (!mgr) return result;
    if (!mgr->phoneFriendAvailable) {
        printf("Lifelines: Phone a Friend not available\n");
        return result;
    }
    if (correctIndex < 0 || correctIndex >= 4) {
        printf("Lifelines: Invalid correct index for Phone a Friend\n");
        return result;
    }

    // Mark lifeline as used
    mgr->phoneFriendAvailable = false;

    float diffMult = GetDifficultyMultiplier(difficulty);

    // Determine if friend gives correct answer
    // Base: 80% correct for easy, scales down with difficulty
    float correctChance = 0.80f * diffMult;
    float roll = (float)rand() / (float)RAND_MAX;

    if (roll < correctChance) {
        // Correct answer
        result.suggestedAnswer = correctIndex;
        result.isCorrect = true;

        // Higher confidence more likely for correct answers
        float confRoll = (float)rand() / (float)RAND_MAX;
        if (confRoll < 0.5f * diffMult) {
            result.confidence = CONFIDENCE_HIGH;
        } else if (confRoll < 0.85f) {
            result.confidence = CONFIDENCE_MEDIUM;
        } else {
            result.confidence = CONFIDENCE_LOW;
        }
    } else {
        // Wrong answer
        // Pick a random wrong answer
        int wrongOptions[3];
        int wrongCount = 0;
        for (int i = 0; i < 4; i++) {
            if (i != correctIndex) {
                wrongOptions[wrongCount++] = i;
            }
        }

        result.suggestedAnswer = wrongOptions[rand() % wrongCount];
        result.isCorrect = false;

        // Lower confidence more likely for wrong answers
        float confRoll = (float)rand() / (float)RAND_MAX;
        if (confRoll < 0.15f) {
            result.confidence = CONFIDENCE_HIGH; // Confidently wrong - happens sometimes!
        } else if (confRoll < 0.5f) {
            result.confidence = CONFIDENCE_MEDIUM;
        } else {
            result.confidence = CONFIDENCE_LOW;
        }
    }

    // Store result
    mgr->phoneFriendResult = result;

    printf("Lifelines: Phone a Friend - suggested %c (%s), confidence %s\n",
           'A' + result.suggestedAnswer,
           result.isCorrect ? "correct" : "WRONG",
           result.confidence == CONFIDENCE_HIGH ? "high" :
           result.confidence == CONFIDENCE_MEDIUM ? "medium" : "low");

    return result;
}

// ============================================================================
// Ask the Audience Implementation
// ============================================================================

void GetAudienceResults(LifelineManager* mgr, int correctIndex, const char* difficulty,
                        const bool eliminated[4], int percentages[4]) {
    if (!mgr || !percentages) return;
    if (!mgr->askAudienceAvailable) {
        printf("Lifelines: Ask the Audience not available\n");
        return;
    }
    if (correctIndex < 0 || correctIndex >= 4) {
        printf("Lifelines: Invalid correct index for Ask the Audience\n");
        return;
    }

    // Mark lifeline as used
    mgr->askAudienceAvailable = false;

    memset(percentages, 0, sizeof(int) * 4);

    // Determine percentage range for correct answer based on difficulty
    int correctMin, correctMax;
    if (difficulty == NULL || strcasecmp(difficulty, "easy") == 0) {
        correctMin = 40;
        correctMax = 70;
    } else if (strcasecmp(difficulty, "medium") == 0) {
        correctMin = 30;
        correctMax = 50;
    } else { // hard
        correctMin = 20;
        correctMax = 40;
    }

    // Count available options (not eliminated by 50:50)
    int availableCount = 0;
    bool isEliminated[4] = {false, false, false, false};

    if (eliminated) {
        memcpy(isEliminated, eliminated, sizeof(bool) * 4);
    }

    for (int i = 0; i < 4; i++) {
        if (!isEliminated[i]) {
            availableCount++;
        }
    }

    if (availableCount == 0) {
        printf("Lifelines: Error - no available options for audience poll\n");
        return;
    }

    // Special case: If 50:50 was used, only 2 options remain
    if (availableCount == 2) {
        // Two options left - correct answer gets more
        int correctPct = RandomRange(55, 75);
        percentages[correctIndex] = correctPct;

        // Find the other remaining option
        for (int i = 0; i < 4; i++) {
            if (!isEliminated[i] && i != correctIndex) {
                percentages[i] = 100 - correctPct;
                break;
            }
        }
    } else {
        // Normal case - 4 options available
        int correctPct = RandomRange(correctMin, correctMax);
        percentages[correctIndex] = correctPct;

        int remaining = 100 - correctPct;

        // Collect wrong answer indices
        int wrongIndices[3];
        int wrongCount = 0;
        for (int i = 0; i < 4; i++) {
            if (i != correctIndex && !isEliminated[i]) {
                wrongIndices[wrongCount++] = i;
            }
        }

        // Distribute remaining percentage among wrong answers
        for (int i = 0; i < wrongCount - 1; i++) {
            // Leave at least 1% for remaining options
            int maxForThis = remaining - (wrongCount - i - 1);
            if (maxForThis < 1) maxForThis = 1;

            int pct = RandomRange(1, maxForThis);
            percentages[wrongIndices[i]] = pct;
            remaining -= pct;
        }

        // Last wrong answer gets the rest
        if (wrongCount > 0) {
            percentages[wrongIndices[wrongCount - 1]] = remaining;
        }
    }

    // Eliminated options explicitly get 0%
    for (int i = 0; i < 4; i++) {
        if (isEliminated[i]) {
            percentages[i] = 0;
        }
    }

    // Store result
    memcpy(mgr->audienceResult.percentages, percentages, sizeof(int) * 4);

    printf("Lifelines: Ask the Audience - A:%d%% B:%d%% C:%d%% D:%d%%\n",
           percentages[0], percentages[1], percentages[2], percentages[3]);
}

// ============================================================================
// Animation Control
// ============================================================================

void LifelineStartAnimation(LifelineManager* mgr, LifelineType type) {
    if (!mgr) return;

    mgr->activeLifeline = type;
    mgr->animState = LIFELINE_STATE_ANIMATING;
    mgr->animTimer = 0.0f;

    switch (type) {
        case LIFELINE_FIFTY_FIFTY:
            mgr->animDuration = LIFELINE_FIFTY_FIFTY_DURATION;
            break;
        case LIFELINE_PHONE_FRIEND:
            mgr->animDuration = LIFELINE_PHONE_FRIEND_DURATION;
            break;
        case LIFELINE_ASK_AUDIENCE:
            mgr->animDuration = LIFELINE_AUDIENCE_POLL_DURATION;
            break;
        default:
            mgr->animDuration = 2.5f;
            break;
    }

    printf("Lifelines: Starting %s animation (%.1fs)\n",
           GetLifelineName(type), mgr->animDuration);
}

bool LifelineUpdateAnimation(LifelineManager* mgr, float deltaTime) {
    if (!mgr) return true;
    if (mgr->animState == LIFELINE_STATE_INACTIVE) return true;
    if (mgr->animState == LIFELINE_STATE_COMPLETE) return true;

    mgr->animTimer += deltaTime;

    // Check if animation is complete
    if (mgr->animTimer >= mgr->animDuration) {
        mgr->animState = LIFELINE_STATE_SHOWING_RESULT;
        printf("Lifelines: Animation complete, showing result\n");
        return true;
    }

    return false;
}

bool LifelineIsActive(const LifelineManager* mgr) {
    if (!mgr) return false;
    return mgr->animState == LIFELINE_STATE_ANIMATING ||
           mgr->animState == LIFELINE_STATE_SHOWING_RESULT;
}

void LifelineComplete(LifelineManager* mgr) {
    if (!mgr) return;
    mgr->animState = LIFELINE_STATE_COMPLETE;
    mgr->activeLifeline = LIFELINE_NONE;
    printf("Lifelines: Marked as complete\n");
}

float LifelineGetProgress(const LifelineManager* mgr) {
    if (!mgr || mgr->animDuration <= 0.0f) return 1.0f;
    return Clampf(mgr->animTimer / mgr->animDuration, 0.0f, 1.0f);
}

// ============================================================================
// Drawing: 50:50 Animation
// ============================================================================

void DrawFiftyFiftyAnimation(Font font, float progress, const bool eliminated[4], const int eliminationOrder[2]) {
    // Semi-transparent overlay
    DrawRectangle(0, 0, 800, 480, ColorAlpha(COLOR_DARKER_BLUE, 0.7f));

    // Main panel
    Rectangle panel = {150, 140, 500, 200};
    DrawRectangleRounded(panel, 0.08f, 8, COLOR_PANEL_BG);
    DrawRectangleRoundedLines(panel, 0.08f, 8, COLOR_GOLD);

    // Inner glow effect
    DrawRectangleRoundedLines((Rectangle){panel.x + 2, panel.y + 2, panel.width - 4, panel.height - 4},
                              0.08f, 8, ColorAlpha(COLOR_GOLD, 0.3f));

    // Title
    const char* title = "50:50";
    Vector2 titleSize = MeasureTextEx(font, title, 42, 2);
    DrawTextEx(font, title,
               (Vector2){panel.x + (panel.width - titleSize.x) / 2, panel.y + 20},
               42, 2, COLOR_GOLD);

    // Subtitle based on progress
    const char* subtitle;
    if (progress < 0.3f) {
        subtitle = "Removing two wrong answers...";
    } else if (progress < 0.7f) {
        subtitle = "Eliminating incorrect options...";
    } else {
        subtitle = "Two options remain!";
    }
    Vector2 subSize = MeasureTextEx(font, subtitle, 18, 1);
    DrawTextEx(font, subtitle,
               (Vector2){panel.x + (panel.width - subSize.x) / 2, panel.y + 70},
               18, 1, COLOR_TEXT_WHITE);

    // Draw option letters with fade-out effect
    float optionY = panel.y + 115;
    float optionSpacing = 110;
    float startX = panel.x + 65;

    const char* letters[] = {"A", "B", "C", "D"};

    for (int i = 0; i < 4; i++) {
        float x = startX + i * optionSpacing;
        float centerX = x + 30;
        float centerY = optionY + 30;

        // Determine alpha based on elimination status and progress
        float alpha = 1.0f;
        bool beingEliminated = false;

        if (eliminated && eliminated[i]) {
            beingEliminated = true;

            // Staged elimination based on order
            float fadeStart = 0.0f;
            float fadeEnd = 0.5f;

            if (eliminationOrder && eliminationOrder[0] == i) {
                fadeStart = 0.1f;
                fadeEnd = 0.4f;
            } else if (eliminationOrder && eliminationOrder[1] == i) {
                fadeStart = 0.4f;
                fadeEnd = 0.7f;
            }

            if (progress >= fadeEnd) {
                alpha = 0.15f;
            } else if (progress >= fadeStart) {
                float fadeProgress = (progress - fadeStart) / (fadeEnd - fadeStart);
                alpha = 1.0f - (fadeProgress * 0.85f);
            }
        }

        // Circle background
        Color circleColor;
        if (beingEliminated && progress > 0.7f) {
            circleColor = ColorAlpha(COLOR_ELIMINATED, alpha);
        } else {
            circleColor = ColorAlpha(COLOR_HIGHLIGHT, alpha);
        }

        // Pulsing effect for remaining options
        float scale = 1.0f;
        if (!beingEliminated && progress > 0.8f) {
            scale = 1.0f + sinf(progress * 20.0f) * 0.05f;
        }

        DrawCircle(centerX, centerY, 35 * scale, circleColor);
        DrawCircleLines(centerX, centerY, 35 * scale, ColorAlpha(COLOR_GOLD, alpha * 0.5f));

        // Letter
        Color letterColor = ColorAlpha(COLOR_TEXT_WHITE, alpha);
        Vector2 letterSize = MeasureTextEx(font, letters[i], 36, 2);
        DrawTextEx(font, letters[i],
                   (Vector2){centerX - letterSize.x / 2, centerY - letterSize.y / 2},
                   36, 2, letterColor);

        // Draw X over eliminated options (fade in)
        if (beingEliminated && progress > 0.5f) {
            float xAlpha = Clampf((progress - 0.5f) / 0.25f, 0.0f, 1.0f);
            Color xColor = ColorAlpha(COLOR_WRONG_RED, xAlpha);
            float xSize = 22;
            float thickness = 4;

            DrawLineEx((Vector2){centerX - xSize, centerY - xSize},
                       (Vector2){centerX + xSize, centerY + xSize}, thickness, xColor);
            DrawLineEx((Vector2){centerX + xSize, centerY - xSize},
                       (Vector2){centerX - xSize, centerY + xSize}, thickness, xColor);
        }
    }

    // Progress indicator at bottom
    float barWidth = 200;
    float barHeight = 6;
    float barX = panel.x + (panel.width - barWidth) / 2;
    float barY = panel.y + panel.height - 25;

    DrawRectangleRounded((Rectangle){barX, barY, barWidth, barHeight}, 0.5f, 4,
                         ColorAlpha(COLOR_TEXT_DIM, 0.3f));
    DrawRectangleRounded((Rectangle){barX, barY, barWidth * progress, barHeight}, 0.5f, 4,
                         COLOR_GOLD);
}

// ============================================================================
// Drawing: Phone a Friend Panel
// ============================================================================

void DrawPhoneFriendPanel(Font font, int suggestedAnswer, ConfidenceLevel confidence, float timer, bool isThinking) {
    // Semi-transparent overlay
    DrawRectangle(0, 0, 800, 480, ColorAlpha(COLOR_DARKER_BLUE, 0.7f));

    // Main panel
    Rectangle panel = {100, 100, 600, 280};
    DrawRectangleRounded(panel, 0.08f, 8, COLOR_PANEL_BG);
    DrawRectangleRoundedLines(panel, 0.08f, 8, COLOR_GOLD);

    // Phone icon area
    float iconX = panel.x + 35;
    float iconY = panel.y + 35;

    // Phone body
    DrawRectangleRounded((Rectangle){iconX, iconY, 50, 80}, 0.25f, 6, COLOR_LIGHT_BLUE);
    DrawRectangleRounded((Rectangle){iconX + 5, iconY + 10, 40, 50}, 0.15f, 4, COLOR_DARK_BLUE);
    DrawCircle(iconX + 25, iconY + 70, 8, COLOR_DARK_BLUE);

    // Ringing effect when thinking
    if (isThinking) {
        for (int i = 0; i < 3; i++) {
            float ringProgress = fmodf(timer * 1.5f + i * 0.33f, 1.0f);
            float ringRadius = 35 + ringProgress * 25;
            float ringAlpha = (1.0f - ringProgress) * 0.5f;
            DrawCircleLines(iconX + 25, iconY + 40, ringRadius, ColorAlpha(COLOR_GOLD, ringAlpha));
        }
    }

    // Title
    const char* title = "Phone a Friend";
    Vector2 titleSize = MeasureTextEx(font, title, 36, 2);
    DrawTextEx(font, title,
               (Vector2){panel.x + 110, panel.y + 22},
               36, 2, COLOR_GOLD);

    // Pick a friend name (consistent for the duration of the animation)
    int friendIdx = ((int)(timer * 100)) % NUM_FRIEND_NAMES;
    if (!isThinking) friendIdx = suggestedAnswer % NUM_FRIEND_NAMES; // Consistent once revealed
    const char* friendName = FRIEND_NAMES[friendIdx];

    if (isThinking) {
        // "Calling..." text with animated dots
        int dotCount = ((int)(timer * 3)) % 4;
        char callingText[64];
        snprintf(callingText, sizeof(callingText), "Calling %s%.*s", friendName, dotCount, "...");

        DrawTextEx(font, callingText,
                   (Vector2){panel.x + 110, panel.y + 65},
                   22, 1, COLOR_TEXT_WHITE);

        // Thinking indicator
        const char* thinkText = "Your friend is thinking";
        DrawTextEx(font, thinkText,
                   (Vector2){panel.x + 110, panel.y + 100},
                   18, 1, COLOR_TEXT_DIM);

        // Animated thinking circle
        float pulseSize = 20 + sinf(timer * 6.0f) * 5.0f;
        float pulseAlpha = 0.5f + sinf(timer * 4.0f) * 0.3f;
        DrawCircle(panel.x + panel.width / 2, panel.y + 180, pulseSize,
                   ColorAlpha(COLOR_GOLD, pulseAlpha * 0.3f));
        DrawCircle(panel.x + panel.width / 2, panel.y + 180, pulseSize * 0.6f,
                   ColorAlpha(COLOR_LIGHT_BLUE, pulseAlpha * 0.5f));

        // Timer dots
        for (int i = 0; i < 3; i++) {
            float dotOffset = sinf(timer * 4.0f + i * 0.7f) * 8;
            DrawCircle(panel.x + panel.width / 2 - 30 + i * 30, panel.y + 220 + dotOffset,
                       6, COLOR_TEXT_WHITE);
        }
    } else {
        // Friend's response
        char answerLetter = 'A' + suggestedAnswer;

        // Get appropriate response template based on confidence
        const char** responses;
        const char* confidenceText;
        Color confidenceColor;

        switch (confidence) {
            case CONFIDENCE_HIGH:
                responses = FRIEND_RESPONSES_HIGH;
                confidenceText = "Very Confident";
                confidenceColor = COLOR_CORRECT_GREEN;
                break;
            case CONFIDENCE_MEDIUM:
                responses = FRIEND_RESPONSES_MEDIUM;
                confidenceText = "Somewhat Confident";
                confidenceColor = COLOR_GOLD;
                break;
            case CONFIDENCE_LOW:
            default:
                responses = FRIEND_RESPONSES_LOW;
                confidenceText = "Not Very Confident";
                confidenceColor = (Color){255, 150, 100, 255};
                break;
        }

        // Use a consistent response (based on suggested answer to avoid randomness after reveal)
        int responseIdx = suggestedAnswer % NUM_RESPONSES;
        char responseText[128];
        snprintf(responseText, sizeof(responseText), responses[responseIdx], answerLetter);

        // Speech bubble background
        Rectangle bubble = {panel.x + 100, panel.y + 65, panel.width - 120, 90};
        DrawRectangleRounded(bubble, 0.15f, 6, ColorAlpha(COLOR_HIGHLIGHT, 0.25f));
        DrawRectangleRoundedLines(bubble, 0.15f, 6, ColorAlpha(COLOR_LIGHT_BLUE, 0.4f));

        // Friend name header
        char headerText[64];
        snprintf(headerText, sizeof(headerText), "%s says:", friendName);
        DrawTextEx(font, headerText,
                   (Vector2){bubble.x + 15, bubble.y + 10},
                   16, 1, COLOR_TEXT_DIM);

        // Opening quote
        DrawTextEx(font, "\"", (Vector2){bubble.x + 12, bubble.y + 32}, 32, 1, COLOR_GOLD_DIM);

        // Response text
        DrawTextEx(font, responseText,
                   (Vector2){bubble.x + 30, bubble.y + 40},
                   24, 1, COLOR_TEXT_WHITE);

        // Closing quote
        Vector2 respSize = MeasureTextEx(font, responseText, 24, 1);
        DrawTextEx(font, "\"",
                   (Vector2){bubble.x + 35 + respSize.x, bubble.y + 32},
                   32, 1, COLOR_GOLD_DIM);

        // Confidence indicator
        float confY = panel.y + 175;
        DrawTextEx(font, "Confidence Level:",
                   (Vector2){panel.x + 110, confY},
                   18, 1, COLOR_TEXT_DIM);
        DrawTextEx(font, confidenceText,
                   (Vector2){panel.x + 270, confY},
                   20, 1, confidenceColor);

        // Confidence bar
        float barWidth = 200;
        float barHeight = 10;
        float barX = panel.x + 110;
        float barY = confY + 30;
        float fillPercent = confidence == CONFIDENCE_HIGH ? 0.9f :
                           confidence == CONFIDENCE_MEDIUM ? 0.6f : 0.3f;

        DrawRectangleRounded((Rectangle){barX, barY, barWidth, barHeight}, 0.5f, 4,
                             ColorAlpha(COLOR_TEXT_DIM, 0.3f));
        DrawRectangleRounded((Rectangle){barX, barY, barWidth * fillPercent, barHeight}, 0.5f, 4,
                             confidenceColor);

        // Highlighted answer
        char suggestedText[48];
        snprintf(suggestedText, sizeof(suggestedText), "Suggested Answer: %c", answerLetter);
        Vector2 sugSize = MeasureTextEx(font, suggestedText, 28, 1);
        DrawTextEx(font, suggestedText,
                   (Vector2){panel.x + (panel.width - sugSize.x) / 2, panel.y + 235},
                   28, 1, COLOR_GOLD);
    }

    // Instructions at bottom
    const char* instructions = isThinking ? "Please wait..." : "Press SELECT to continue";
    Vector2 instSize = MeasureTextEx(font, instructions, 14, 1);
    DrawTextEx(font, instructions,
               (Vector2){panel.x + (panel.width - instSize.x) / 2, panel.y + panel.height - 25},
               14, 1, COLOR_TEXT_DIM);
}

// ============================================================================
// Drawing: Ask the Audience Bar Chart
// ============================================================================

void DrawAudiencePollBars(Font font, const int percentages[4], float animProgress, const bool eliminated[4]) {
    // Semi-transparent overlay
    DrawRectangle(0, 0, 800, 480, ColorAlpha(COLOR_DARKER_BLUE, 0.7f));

    // Main panel
    Rectangle panel = {120, 80, 560, 320};
    DrawRectangleRounded(panel, 0.06f, 8, COLOR_PANEL_BG);
    DrawRectangleRoundedLines(panel, 0.06f, 8, COLOR_GOLD);

    // Title
    const char* title = "Ask the Audience";
    Vector2 titleSize = MeasureTextEx(font, title, 36, 2);
    DrawTextEx(font, title,
               (Vector2){panel.x + (panel.width - titleSize.x) / 2, panel.y + 15},
               36, 2, COLOR_GOLD);

    // Subtitle
    const char* subtitle = animProgress < 1.0f ? "Audience is voting..." : "Results are in!";
    Vector2 subSize = MeasureTextEx(font, subtitle, 16, 1);
    DrawTextEx(font, subtitle,
               (Vector2){panel.x + (panel.width - subSize.x) / 2, panel.y + 55},
               16, 1, COLOR_TEXT_DIM);

    // Bar chart area
    float chartX = panel.x + 70;
    float chartY = panel.y + 85;
    float chartWidth = panel.width - 140;
    float chartHeight = 180;

    float barWidth = (chartWidth - 90) / 4;
    float barSpacing = 30;
    float maxBarHeight = chartHeight - 50;

    const char* letters[] = {"A", "B", "C", "D"};

    // Draw horizontal grid lines
    for (int i = 0; i <= 4; i++) {
        float y = chartY + chartHeight - 35 - (i * maxBarHeight / 4);
        DrawLineEx((Vector2){chartX, y}, (Vector2){chartX + chartWidth - 20, y},
                   1, ColorAlpha(COLOR_TEXT_DIM, 0.2f));

        // Percentage labels
        char pctLabel[8];
        snprintf(pctLabel, sizeof(pctLabel), "%d%%", i * 25);
        DrawTextEx(font, pctLabel, (Vector2){chartX - 40, y - 8}, 12, 1, COLOR_TEXT_DIM);
    }

    // Draw bars
    float easedProgress = EaseInOutQuad(animProgress);

    for (int i = 0; i < 4; i++) {
        float barX = chartX + 15 + i * (barWidth + barSpacing);
        float barBaseY = chartY + chartHeight - 35;

        bool isEliminated = eliminated && eliminated[i];
        int pct = percentages ? percentages[i] : 0;

        // Calculate animated bar height
        float targetHeight = (pct / 100.0f) * maxBarHeight;
        float currentHeight = targetHeight * easedProgress;

        // Bar background
        Rectangle barBg = {barX, chartY + 15, barWidth, maxBarHeight};
        DrawRectangleRounded(barBg, 0.08f, 4, COLOR_BAR_BG);

        // Bar fill
        if (!isEliminated && currentHeight > 0) {
            // Color based on percentage (higher = greener)
            Color barColor;
            if (pct >= 40) {
                barColor = COLOR_CORRECT_GREEN;
            } else if (pct >= 25) {
                barColor = COLOR_GOLD;
            } else if (pct >= 10) {
                barColor = COLOR_LIGHT_BLUE;
            } else {
                barColor = ColorAlpha(COLOR_LIGHT_BLUE, 0.7f);
            }

            Rectangle bar = {barX + 3, barBaseY - currentHeight, barWidth - 6, currentHeight};
            DrawRectangleRounded(bar, 0.08f, 4, barColor);

            // Gradient overlay for depth
            DrawRectangleGradientV(bar.x, bar.y, bar.width / 2, bar.height,
                                   ColorAlpha(WHITE, 0.1f), ColorAlpha(WHITE, 0.0f));

            // Percentage label above bar (fade in at end of animation)
            if (animProgress > 0.6f) {
                char pctText[8];
                snprintf(pctText, sizeof(pctText), "%d%%", pct);
                float labelAlpha = Clampf((animProgress - 0.6f) / 0.3f, 0.0f, 1.0f);
                Vector2 pctSize = MeasureTextEx(font, pctText, 20, 1);
                DrawTextEx(font, pctText,
                           (Vector2){barX + (barWidth - pctSize.x) / 2, barBaseY - currentHeight - 25},
                           20, 1, ColorAlpha(COLOR_TEXT_WHITE, labelAlpha));
            }
        }

        // Letter label below bar
        Color letterColor = isEliminated ? COLOR_ELIMINATED : COLOR_TEXT_WHITE;
        Vector2 letterSize = MeasureTextEx(font, letters[i], 28, 2);
        DrawTextEx(font, letters[i],
                   (Vector2){barX + (barWidth - letterSize.x) / 2, barBaseY + 10},
                   28, 2, letterColor);

        // Draw X over eliminated bars
        if (isEliminated) {
            float cx = barX + barWidth / 2;
            float cy = chartY + 15 + maxBarHeight / 2;
            float xSize = 30;

            DrawLineEx((Vector2){cx - xSize, cy - xSize},
                       (Vector2){cx + xSize, cy + xSize}, 4, ColorAlpha(COLOR_WRONG_RED, 0.7f));
            DrawLineEx((Vector2){cx + xSize, cy - xSize},
                       (Vector2){cx - xSize, cy + xSize}, 4, ColorAlpha(COLOR_WRONG_RED, 0.7f));
        }
    }

    // Progress bar at bottom (only during animation)
    if (animProgress < 1.0f) {
        float progBarWidth = 300;
        float progBarHeight = 8;
        float progBarX = panel.x + (panel.width - progBarWidth) / 2;
        float progBarY = panel.y + panel.height - 35;

        DrawRectangleRounded((Rectangle){progBarX, progBarY, progBarWidth, progBarHeight},
                             0.5f, 4, ColorAlpha(COLOR_TEXT_DIM, 0.3f));
        DrawRectangleRounded((Rectangle){progBarX, progBarY, progBarWidth * animProgress, progBarHeight},
                             0.5f, 4, COLOR_GOLD);
    }

    // Instructions
    const char* instructions = animProgress < 1.0f ? "Counting votes..." : "Press SELECT to continue";
    Vector2 instSize = MeasureTextEx(font, instructions, 14, 1);
    DrawTextEx(font, instructions,
               (Vector2){panel.x + (panel.width - instSize.x) / 2, panel.y + panel.height - 25},
               14, 1, COLOR_TEXT_DIM);
}

// ============================================================================
// Drawing: Lifeline Icons
// ============================================================================

void DrawLifelineIcons(Font font, const LifelineManager* mgr, float x, float y, int selectedLifeline) {
    if (!mgr) return;

    const float iconSize = LIFELINE_ICON_SIZE;
    const float spacing = LIFELINE_ICON_SPACING;

    // ===== 50:50 Icon =====
    {
        float ix = x;
        bool available = mgr->fiftyFiftyAvailable;
        bool selected = (selectedLifeline == 0);

        Color bgColor = available ?
                       (selected ? ColorAlpha(COLOR_GOLD, 0.35f) : COLOR_DARK_BLUE) :
                       ColorAlpha(COLOR_ELIMINATED, 0.4f);
        Color borderColor = available ?
                           (selected ? COLOR_GOLD : COLOR_HIGHLIGHT) :
                           COLOR_TEXT_DIM;
        Color textColor = available ? COLOR_TEXT_WHITE : COLOR_TEXT_DIM;

        Rectangle iconRect = {ix, y, iconSize, iconSize};
        DrawRectangleRounded(iconRect, 0.2f, 6, bgColor);
        DrawRectangleRoundedLines(iconRect, 0.2f, 6, borderColor);

        // Selection glow
        if (selected && available) {
            DrawRectangleRoundedLines((Rectangle){ix - 2, y - 2, iconSize + 4, iconSize + 4},
                                      0.2f, 6, ColorAlpha(COLOR_GOLD, 0.4f));
        }

        // "50:50" text layout
        DrawTextEx(font, "50", (Vector2){ix + 6, y + 10}, 18, 1, textColor);
        DrawTextEx(font, "50", (Vector2){ix + 32, y + 32}, 18, 1, textColor);

        // Diagonal line
        DrawLineEx((Vector2){ix + 8, y + iconSize - 12},
                   (Vector2){ix + iconSize - 8, y + 12}, 2, textColor);

        // X mark if used
        if (!available) {
            DrawLineEx((Vector2){ix + 8, y + 8},
                       (Vector2){ix + iconSize - 8, y + iconSize - 8}, 3, COLOR_WRONG_RED);
            DrawLineEx((Vector2){ix + iconSize - 8, y + 8},
                       (Vector2){ix + 8, y + iconSize - 8}, 3, COLOR_WRONG_RED);
        }
    }

    // ===== Phone a Friend Icon =====
    {
        float ix = x + iconSize + spacing;
        bool available = mgr->phoneFriendAvailable;
        bool selected = (selectedLifeline == 1);

        Color bgColor = available ?
                       (selected ? ColorAlpha(COLOR_GOLD, 0.35f) : COLOR_DARK_BLUE) :
                       ColorAlpha(COLOR_ELIMINATED, 0.4f);
        Color borderColor = available ?
                           (selected ? COLOR_GOLD : COLOR_HIGHLIGHT) :
                           COLOR_TEXT_DIM;
        Color iconColor = available ? COLOR_TEXT_WHITE : COLOR_TEXT_DIM;

        Rectangle iconRect = {ix, y, iconSize, iconSize};
        DrawRectangleRounded(iconRect, 0.2f, 6, bgColor);
        DrawRectangleRoundedLines(iconRect, 0.2f, 6, borderColor);

        if (selected && available) {
            DrawRectangleRoundedLines((Rectangle){ix - 2, y - 2, iconSize + 4, iconSize + 4},
                                      0.2f, 6, ColorAlpha(COLOR_GOLD, 0.4f));
        }

        // Phone icon (simplified smartphone shape)
        float phoneX = ix + 17;
        float phoneY = y + 8;
        float phoneW = 26;
        float phoneH = 44;

        DrawRectangleRounded((Rectangle){phoneX, phoneY, phoneW, phoneH}, 0.3f, 4, iconColor);
        DrawRectangleRounded((Rectangle){phoneX + 4, phoneY + 5, phoneW - 8, phoneH - 14},
                             0.2f, 3, bgColor);
        DrawCircle(phoneX + phoneW / 2, phoneY + phoneH - 5, 4, bgColor);

        // X mark if used
        if (!available) {
            DrawLineEx((Vector2){ix + 8, y + 8},
                       (Vector2){ix + iconSize - 8, y + iconSize - 8}, 3, COLOR_WRONG_RED);
            DrawLineEx((Vector2){ix + iconSize - 8, y + 8},
                       (Vector2){ix + 8, y + iconSize - 8}, 3, COLOR_WRONG_RED);
        }
    }

    // ===== Ask the Audience Icon =====
    {
        float ix = x + (iconSize + spacing) * 2;
        bool available = mgr->askAudienceAvailable;
        bool selected = (selectedLifeline == 2);

        Color bgColor = available ?
                       (selected ? ColorAlpha(COLOR_GOLD, 0.35f) : COLOR_DARK_BLUE) :
                       ColorAlpha(COLOR_ELIMINATED, 0.4f);
        Color borderColor = available ?
                           (selected ? COLOR_GOLD : COLOR_HIGHLIGHT) :
                           COLOR_TEXT_DIM;
        Color iconColor = available ? COLOR_TEXT_WHITE : COLOR_TEXT_DIM;

        Rectangle iconRect = {ix, y, iconSize, iconSize};
        DrawRectangleRounded(iconRect, 0.2f, 6, bgColor);
        DrawRectangleRoundedLines(iconRect, 0.2f, 6, borderColor);

        if (selected && available) {
            DrawRectangleRoundedLines((Rectangle){ix - 2, y - 2, iconSize + 4, iconSize + 4},
                                      0.2f, 6, ColorAlpha(COLOR_GOLD, 0.4f));
        }

        // Bar chart icon
        float barX = ix + 10;
        float barBaseY = y + iconSize - 10;
        float barW = 9;
        float barSpacing = 3;
        float barHeights[] = {22, 32, 18, 28};

        for (int i = 0; i < 4; i++) {
            DrawRectangle(barX + i * (barW + barSpacing),
                          barBaseY - barHeights[i],
                          barW, barHeights[i], iconColor);
        }

        // X mark if used
        if (!available) {
            DrawLineEx((Vector2){ix + 8, y + 8},
                       (Vector2){ix + iconSize - 8, y + iconSize - 8}, 3, COLOR_WRONG_RED);
            DrawLineEx((Vector2){ix + iconSize - 8, y + 8},
                       (Vector2){ix + 8, y + iconSize - 8}, 3, COLOR_WRONG_RED);
        }
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* GetLifelineName(LifelineType type) {
    switch (type) {
        case LIFELINE_FIFTY_FIFTY:  return "50:50";
        case LIFELINE_PHONE_FRIEND: return "Phone a Friend";
        case LIFELINE_ASK_AUDIENCE: return "Ask the Audience";
        default:                    return "Unknown";
    }
}

bool LifelinesAnyAvailable(const LifelineManager* mgr) {
    if (!mgr) return false;
    return mgr->fiftyFiftyAvailable ||
           mgr->phoneFriendAvailable ||
           mgr->askAudienceAvailable;
}

int LifelinesAvailableCount(const LifelineManager* mgr) {
    if (!mgr) return 0;
    int count = 0;
    if (mgr->fiftyFiftyAvailable) count++;
    if (mgr->phoneFriendAvailable) count++;
    if (mgr->askAudienceAvailable) count++;
    return count;
}
