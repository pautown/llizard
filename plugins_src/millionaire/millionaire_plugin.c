/**
 * millionaire_plugin.c
 *
 * "Who Wants to Be a Millionaire" game plugin for llizardgui-host
 * A fully-featured trivia game with lifelines, animations, and 1165 questions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "raylib.h"
#include "llizard_plugin.h"
#include "llz_sdk.h"

#include "millionaire_types.h"
#include "millionaire_questions.h"
#include "millionaire_lifelines.h"

// ============================================================================
// Color Definitions
// ============================================================================

#define MILLIONAIRE_BLUE       (Color){0, 20, 80, 255}
#define MILLIONAIRE_DARK_BLUE  (Color){0, 10, 40, 255}
#define MILLIONAIRE_GOLD       (Color){255, 215, 0, 255}
#define MILLIONAIRE_ORANGE     (Color){255, 165, 0, 255}
#define MILLIONAIRE_PURPLE     (Color){100, 0, 150, 255}
#define MILLIONAIRE_GREEN      (Color){0, 200, 80, 255}
#define MILLIONAIRE_RED        (Color){220, 50, 50, 255}
#define ANSWER_BOX_BG          (Color){20, 40, 100, 255}
#define ANSWER_BOX_HIGHLIGHT   (Color){40, 80, 180, 255}
#define SAFE_HAVEN_COLOR       (Color){255, 180, 0, 255}

// ============================================================================
// Game State
// ============================================================================

static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;
static Font g_font = {0};
static bool g_fontLoaded = false;

// Plugin configuration for saving stats
static LlzPluginConfig g_config = {0};
static bool g_configLoaded = false;

// Prize levels
static const int g_prizeAmounts[15] = {
    100, 200, 300, 500, 1000,
    2000, 4000, 8000, 16000, 32000,
    64000, 125000, 250000, 500000, 1000000
};

static const char* g_prizeStrings[15] = {
    "$100", "$200", "$300", "$500", "$1,000",
    "$2,000", "$4,000", "$8,000", "$16,000", "$32,000",
    "$64,000", "$125,000", "$250,000", "$500,000", "$1,000,000"
};

// Timer settings
#define TIMER_EASY_SECONDS    90.0f   // Questions 1-5
#define TIMER_MEDIUM_SECONDS  75.0f   // Questions 6-10
#define TIMER_HARD_SECONDS    60.0f   // Questions 11-15

// Lifeline settings
#define PHONE_CALL_DURATION   30.0f   // 30 seconds to ask a friend
#define FIFTY_FIFTY_DURATION   1.5f   // Animation duration for 50:50
#define AUDIENCE_POLL_DURATION 3.0f   // Animation duration for audience poll

// Additional game states beyond millionaire_types.h
#define MIL_STATE_QUIT_CONFIRM 100     // Quit game confirmation dialog
#define MIL_STATE_LIFELINE_CONFIRM 101 // Lifeline usage confirmation dialog

// Lifeline constants
#define LIFELINE_RESET_TIME 30.0f  // Time given after using a lifeline

// Game state structure
typedef struct {
    MilGameState state;
    int prizeLevel;              // 0-14
    int cursorIndex;             // 0-3 for answer selection (loops A->B->C->D->A)
    int selectedAnswer;          // -1 or 0-3
    bool eliminated[4];          // 50:50 eliminations
    bool lifelinesUsed[3];       // 50:50, Phone, Audience
    int audiencePercentages[4];
    int phoneSuggestion;
    int phoneConfidence;
    float stateTimer;
    float animTimer;
    float pulseTimer;
    float questionTimer;         // Countdown timer for current question
    float questionTimeLimit;     // Time limit based on difficulty
    float phoneCallTimer;        // 30-second phone call countdown
    float audiencePollProgress;  // Animation progress for audience bars (0-1)
    int pendingLifeline;         // Which lifeline is pending confirmation (0=50:50, 1=Phone, 2=Audience, -1=none)
    float lifelineConfirmTimer;  // Timer for lifeline confirmation (same as remaining question time)
    float lifelineConfirmTimeLimit; // Time limit for confirmation (snapshot of remaining time)
    int selectedLifelineIdx;     // Currently selected lifeline icon index (-1 = none)
    int gamesPlayed;
    int totalWinnings;
    int highScore;               // Highest single-game winnings
    bool questionsLoaded;
    bool gameInProgress;         // True when actively playing a game
    MillionaireQuestion* currentQuestion;
} GameData;

static GameData g_game = {0};
static LifelineManager g_lifelines = {0};

// Forward declarations
static void EndGame(int winnings);
static void SaveStats(void);
static void RequestLifeline(int lifelineIdx);

// Background particles
#define MAX_PARTICLES 50
typedef struct {
    float x, y;
    float vx, vy;
    float size;
    float alpha;
    Color color;
    bool active;
} Particle;

static Particle g_particles[MAX_PARTICLES];

// ============================================================================
// Helper Functions
// ============================================================================

static bool IsSafeHaven(int level) {
    return level == 4 || level == 9;
}

static int GetGuaranteedPrize(int level) {
    if (level >= 9) return g_prizeAmounts[9];  // $32,000
    if (level >= 4) return g_prizeAmounts[4];  // $1,000
    return 0;
}

static const char* GetGuaranteedPrizeString(int level) {
    if (level >= 9) return g_prizeStrings[9];
    if (level >= 4) return g_prizeStrings[4];
    return "$0";
}

static int GetCursorIndex(void) {
    return g_game.cursorIndex;
}

static float GetTimerForLevel(int level) {
    if (level < 5) return TIMER_EASY_SECONDS;
    if (level < 10) return TIMER_MEDIUM_SECONDS;
    return TIMER_HARD_SECONDS;
}

// Font loading - uses SDK font loader module
static void LoadPluginFont(void) {
    if (g_fontLoaded) return;

    // Initialize SDK font system
    LlzFontInit();

    // Get font at high resolution for quality text rendering
    g_font = LlzFontGet(LLZ_FONT_UI, 64);

    if (g_font.texture.id != 0 && g_font.texture.id != GetFontDefault().texture.id) {
        g_fontLoaded = true;
        printf("Millionaire: Loaded font via SDK from: %s\n", LlzFontGetPath(LLZ_FONT_UI));
    } else {
        g_font = GetFontDefault();
        printf("Millionaire: Using default font\n");
    }
}

static void UnloadPluginFont(void) {
    // SDK manages font lifecycle - just mark as unloaded
    // The font will be freed when SDK shuts down
    g_fontLoaded = false;
}

// Convert string to uppercase (Copperplate is all-caps)
static void ToUpperCase(const char* src, char* dest, int maxLen) {
    int i = 0;
    while (src[i] && i < maxLen - 1) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') {
            dest[i] = c - 32;
        } else {
            dest[i] = c;
        }
        i++;
    }
    dest[i] = '\0';
}

// Copperplate Gothic style text rendering
// - All caps with wide letter spacing
// - Engraved/embossed look with shadow and highlight
// - Wedge-serif simulation through layered drawing
static void DrawTextCopperplate(const char* text, int x, int y, int fontSize, Color color) {
    char upper[512];
    ToUpperCase(text, upper, sizeof(upper));

    // Letter spacing multiplier (Copperplate has wide tracking)
    float spacing = fontSize * 0.15f;

    // Engraved effect colors
    Color shadowColor = (Color){0, 0, 0, 180};
    Color highlightColor = (Color){255, 255, 255, 60};
    Color midColor = (Color){color.r / 2, color.g / 2, color.b / 2, color.a};

    if (g_fontLoaded) {
        // Layer 1: Dark shadow (engraved depth)
        DrawTextEx(g_font, upper, (Vector2){x + 1, y + 2}, fontSize, spacing, shadowColor);

        // Layer 2: Subtle highlight (raised edge simulation)
        DrawTextEx(g_font, upper, (Vector2){x - 1, y - 1}, fontSize, spacing, highlightColor);

        // Layer 3: Main text
        DrawTextEx(g_font, upper, (Vector2){x, y}, fontSize, spacing, color);
    } else {
        // Fallback with basic shadow
        DrawText(upper, x + 1, y + 1, fontSize, shadowColor);
        DrawText(upper, x, y, fontSize, color);
    }
}

// Copperplate style with outline (for important text like prize amounts)
static void DrawTextCopperplateOutlined(const char* text, int x, int y, int fontSize, Color color, Color outlineColor) {
    char upper[512];
    ToUpperCase(text, upper, sizeof(upper));

    float spacing = fontSize * 0.15f;

    if (g_fontLoaded) {
        // Outline (draw in 8 directions)
        for (int ox = -1; ox <= 1; ox++) {
            for (int oy = -1; oy <= 1; oy++) {
                if (ox != 0 || oy != 0) {
                    DrawTextEx(g_font, upper, (Vector2){x + ox, y + oy}, fontSize, spacing, outlineColor);
                }
            }
        }
        // Main text
        DrawTextEx(g_font, upper, (Vector2){x, y}, fontSize, spacing, color);
    } else {
        // Simple outline fallback
        DrawText(upper, x + 1, y, fontSize, outlineColor);
        DrawText(upper, x - 1, y, fontSize, outlineColor);
        DrawText(upper, x, y + 1, fontSize, outlineColor);
        DrawText(upper, x, y - 1, fontSize, outlineColor);
        DrawText(upper, x, y, fontSize, color);
    }
}

// Small caps style (larger first letter, smaller rest) - authentic Copperplate
static void DrawTextSmallCaps(const char* text, int x, int y, int fontSize, Color color) {
    char upper[512];
    ToUpperCase(text, upper, sizeof(upper));

    float spacing = fontSize * 0.12f;
    int smallSize = (int)(fontSize * 0.75f);

    Color shadowColor = (Color){0, 0, 0, 150};

    if (g_fontLoaded) {
        int currentX = x;
        bool isFirstLetter = true;

        for (int i = 0; upper[i]; i++) {
            char ch[2] = {upper[i], '\0'};
            int size = isFirstLetter ? fontSize : smallSize;
            int yOffset = isFirstLetter ? 0 : (fontSize - smallSize) / 2;

            // Shadow
            DrawTextEx(g_font, ch, (Vector2){currentX + 1, y + yOffset + 1}, size, 0, shadowColor);
            // Main
            DrawTextEx(g_font, ch, (Vector2){currentX, y + yOffset}, size, 0, color);

            Vector2 charSize = MeasureTextEx(g_font, ch, size, 0);
            currentX += (int)(charSize.x + spacing);

            if (upper[i] == ' ') {
                isFirstLetter = true;
            } else {
                isFirstLetter = false;
            }
        }
    } else {
        DrawText(upper, x + 1, y + 1, fontSize, shadowColor);
        DrawText(upper, x, y, fontSize, color);
    }
}

// Standard styled text (converts to uppercase with spacing)
static void DrawTextStyled(const char* text, int x, int y, int fontSize, Color color) {
    DrawTextCopperplate(text, x, y, fontSize, color);
}

static int MeasureTextStyled(const char* text, int fontSize) {
    char upper[512];
    ToUpperCase(text, upper, sizeof(upper));
    float spacing = fontSize * 0.15f;

    if (g_fontLoaded) {
        Vector2 size = MeasureTextEx(g_font, upper, fontSize, spacing);
        return (int)size.x;
    }
    return MeasureText(upper, fontSize);
}

static void InitParticles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        g_particles[i].active = false;
    }
}

static void SpawnParticle(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g_particles[i].active) {
            g_particles[i].x = GetRandomValue(0, g_screenWidth);
            g_particles[i].y = GetRandomValue(0, g_screenHeight);
            g_particles[i].vx = (GetRandomValue(-100, 100) / 100.0f);
            g_particles[i].vy = (GetRandomValue(-50, -10) / 100.0f);
            g_particles[i].size = GetRandomValue(2, 6);
            g_particles[i].alpha = GetRandomValue(30, 80) / 100.0f;
            g_particles[i].color = (GetRandomValue(0, 1) == 0) ? MILLIONAIRE_GOLD : WHITE;
            g_particles[i].active = true;
            break;
        }
    }
}

static void UpdateParticles(float dt) {
    static float spawnTimer = 0;
    spawnTimer += dt;
    if (spawnTimer > 0.1f) {
        SpawnParticle();
        spawnTimer = 0;
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_particles[i].active) {
            g_particles[i].x += g_particles[i].vx * dt * 30;
            g_particles[i].y += g_particles[i].vy * dt * 30;
            g_particles[i].alpha -= dt * 0.1f;

            if (g_particles[i].alpha <= 0 || g_particles[i].y < -10) {
                g_particles[i].active = false;
            }
        }
    }
}

static void DrawParticles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_particles[i].active) {
            Color c = g_particles[i].color;
            c.a = (unsigned char)(g_particles[i].alpha * 255);
            DrawCircle((int)g_particles[i].x, (int)g_particles[i].y, g_particles[i].size, c);
        }
    }
}

// ============================================================================
// Drawing Functions
// ============================================================================

static void DrawBackground(void) {
    // Gradient background
    for (int y = 0; y < g_screenHeight; y++) {
        float t = (float)y / g_screenHeight;
        Color c = {
            (unsigned char)(0 + t * 20),
            (unsigned char)(10 + t * 30),
            (unsigned char)(40 + t * 60),
            255
        };
        DrawRectangle(0, y, g_screenWidth, 1, c);
    }

    // Spotlight effect
    float pulse = sinf(g_game.pulseTimer * 0.5f) * 0.3f + 0.7f;
    DrawCircleGradient(g_screenWidth / 2, g_screenHeight / 3, 300 * pulse,
                       (Color){60, 80, 150, 40}, (Color){0, 0, 0, 0});

    // Draw particles
    DrawParticles();
}

static void DrawPrizeLadder(void) {
    int ladderX = 620;
    int ladderY = 30;
    int rowHeight = 28;
    int ladderWidth = 170;

    // Draw ladder background
    DrawRectangle(ladderX - 5, ladderY - 5, ladderWidth + 10, rowHeight * 15 + 15, (Color){0, 0, 0, 150});
    DrawRectangleLines(ladderX - 5, ladderY - 5, ladderWidth + 10, rowHeight * 15 + 15, MILLIONAIRE_GOLD);

    for (int i = 14; i >= 0; i--) {
        int y = ladderY + (14 - i) * rowHeight;
        bool isCurrent = (i == g_game.prizeLevel);
        bool isPassed = (i < g_game.prizeLevel);
        bool isSafe = IsSafeHaven(i);

        Color bgColor = (Color){20, 30, 60, 200};
        Color textColor = (Color){150, 150, 180, 255};

        if (isCurrent) {
            // Pulsing highlight for current level
            float pulse = sinf(g_game.pulseTimer * 3.0f) * 0.3f + 0.7f;
            bgColor = (Color){(unsigned char)(255 * pulse), (unsigned char)(180 * pulse), 0, 255};
            textColor = BLACK;
            DrawRectangle(ladderX - 2, y - 1, ladderWidth + 4, rowHeight, MILLIONAIRE_GOLD);
        } else if (isPassed) {
            bgColor = (Color){0, 80, 40, 200};
            textColor = MILLIONAIRE_GREEN;
        } else if (isSafe) {
            bgColor = (Color){80, 60, 0, 200};
            textColor = SAFE_HAVEN_COLOR;
        }

        DrawRectangle(ladderX, y, ladderWidth, rowHeight - 2, bgColor);

        // Draw prize amount
        const char* prizeText = g_prizeStrings[i];
        int textWidth = MeasureText(prizeText, 16);
        DrawText(prizeText, ladderX + ladderWidth - textWidth - 10, y + 5, 16, textColor);

        // Draw level number
        char levelNum[4];
        snprintf(levelNum, sizeof(levelNum), "%d", i + 1);
        DrawText(levelNum, ladderX + 8, y + 5, 16, textColor);
    }
}

static void DrawQuestionBox(void) {
    if (!g_game.currentQuestion) return;

    int boxX = 20;
    int boxY = 30;
    int boxWidth = 580;
    int boxHeight = 100;

    // Question box background
    DrawRectangle(boxX, boxY, boxWidth, boxHeight, (Color){10, 20, 60, 230});
    DrawRectangleLines(boxX, boxY, boxWidth, boxHeight, MILLIONAIRE_GOLD);

    // Draw decorative corners
    DrawTriangle(
        (Vector2){boxX, boxY},
        (Vector2){boxX + 20, boxY},
        (Vector2){boxX, boxY + 20},
        MILLIONAIRE_GOLD
    );
    DrawTriangle(
        (Vector2){boxX + boxWidth, boxY},
        (Vector2){boxX + boxWidth - 20, boxY},
        (Vector2){boxX + boxWidth, boxY + 20},
        MILLIONAIRE_GOLD
    );

    // Category display - styled
    char categoryText[80];
    snprintf(categoryText, sizeof(categoryText), "Category: %s", g_game.currentQuestion->category);
    DrawTextStyled(categoryText, boxX + 10, boxY + 5, 12, (Color){180, 180, 200, 255});

    // Question text with word wrap - using styled font
    const char* question = g_game.currentQuestion->question;
    int fontSize = 18;
    int maxWidth = boxWidth - 20;

    // Simple word wrap
    char wrapped[512];
    strncpy(wrapped, question, sizeof(wrapped) - 1);
    wrapped[sizeof(wrapped) - 1] = '\0';

    int textY = boxY + 25;
    int lineStart = 0;
    int lastSpace = 0;
    int lineCount = 0;

    for (int i = 0; wrapped[i] && lineCount < 4; i++) {
        if (wrapped[i] == ' ') lastSpace = i;

        char temp[512];
        strncpy(temp, wrapped + lineStart, i - lineStart + 1);
        temp[i - lineStart + 1] = '\0';

        if (MeasureTextStyled(temp, fontSize) > maxWidth && lastSpace > lineStart) {
            wrapped[lastSpace] = '\0';
            DrawTextStyled(wrapped + lineStart, boxX + 10, textY, fontSize, WHITE);
            textY += fontSize + 4;
            lineStart = lastSpace + 1;
            lineCount++;
        }
    }
    if (wrapped[lineStart]) {
        DrawTextStyled(wrapped + lineStart, boxX + 10, textY, fontSize, WHITE);
    }
}

static void DrawAnswerGrid(void) {
    if (!g_game.currentQuestion) return;

    const char* letters[] = {"A", "B", "C", "D"};
    int startX = 30;
    int startY = 145;
    int boxWidth = 280;
    int boxHeight = 60;
    int gapX = 20;
    int gapY = 12;

    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            int idx = row * 2 + col;
            int x = startX + col * (boxWidth + gapX);
            int y = startY + row * (boxHeight + gapY);

            bool isHighlighted = (g_game.cursorIndex == idx);
            bool isSelected = (g_game.selectedAnswer == idx);
            bool isEliminated = g_game.eliminated[idx];
            bool isCorrect = (idx == g_game.currentQuestion->correctIndex);

            Color bgColor = ANSWER_BOX_BG;
            Color borderColor = MILLIONAIRE_BLUE;
            Color textColor = WHITE;

            if (isEliminated) {
                bgColor = (Color){30, 30, 40, 200};
                textColor = (Color){80, 80, 80, 255};
            } else if (g_game.state == MIL_STATE_CORRECT_ANSWER && isCorrect) {
                bgColor = MILLIONAIRE_GREEN;
                borderColor = WHITE;
            } else if (g_game.state == MIL_STATE_WRONG_ANSWER) {
                if (isCorrect) {
                    bgColor = MILLIONAIRE_GREEN;
                    borderColor = WHITE;
                } else if (isSelected) {
                    bgColor = MILLIONAIRE_RED;
                    borderColor = WHITE;
                }
            } else if (isSelected || (g_game.state == MIL_STATE_ANSWER_LOCKED && isHighlighted)) {
                float pulse = sinf(g_game.pulseTimer * 5.0f) * 0.3f + 0.7f;
                bgColor = (Color){(unsigned char)(255 * pulse), (unsigned char)(140 * pulse), 0, 255};
                borderColor = MILLIONAIRE_GOLD;
                textColor = BLACK;
            } else if (isHighlighted && g_game.state == MIL_STATE_GAME_PLAYING) {
                bgColor = ANSWER_BOX_HIGHLIGHT;
                borderColor = MILLIONAIRE_GOLD;
            }

            // Draw hexagonal-ish answer box
            DrawRectangle(x + 15, y, boxWidth - 30, boxHeight, bgColor);
            DrawTriangle(
                (Vector2){x + 15, y},
                (Vector2){x, y + boxHeight / 2},
                (Vector2){x + 15, y + boxHeight},
                bgColor
            );
            DrawTriangle(
                (Vector2){x + boxWidth - 15, y},
                (Vector2){x + boxWidth, y + boxHeight / 2},
                (Vector2){x + boxWidth - 15, y + boxHeight},
                bgColor
            );

            // Border (thicker for highlighted)
            if (isHighlighted && !isEliminated) {
                DrawRectangleLinesEx((Rectangle){x + 15, y, boxWidth - 30, boxHeight}, 3, borderColor);
            } else {
                DrawRectangleLines(x + 15, y, boxWidth - 30, boxHeight, borderColor);
            }

            // Letter label - larger and clearer
            DrawTextStyled(letters[idx], x + 22, y + 16, 28, MILLIONAIRE_GOLD);

            // Answer text - larger and clearer
            if (!isEliminated) {
                const char* answerText = g_game.currentQuestion->options[idx];
                int fontSize = 20;
                int textWidth = MeasureTextStyled(answerText, fontSize);
                if (textWidth > boxWidth - 80) fontSize = 16;
                if (MeasureTextStyled(answerText, fontSize) > boxWidth - 80) fontSize = 14;
                DrawTextStyled(answerText, x + 58, y + 18, fontSize, textColor);
            } else {
                DrawLine(x + 20, y + boxHeight / 2, x + boxWidth - 20, y + boxHeight / 2, (Color){100, 100, 100, 255});
            }
        }
    }
}

// Lifeline icon dimensions and positions (local versions, not conflicting with header)
#define LLZ_LIFELINE_BAR_X 30
#define LLZ_LIFELINE_BAR_Y 410
#define LLZ_LIFELINE_ICON_SZ 45
#define LLZ_LIFELINE_ICON_GAP 15

// Helper to get lifeline icon rectangle
static Rectangle GetLifelineIconRect(int index) {
    return (Rectangle){
        LLZ_LIFELINE_BAR_X + index * (LLZ_LIFELINE_ICON_SZ + LLZ_LIFELINE_ICON_GAP),
        LLZ_LIFELINE_BAR_Y,
        LLZ_LIFELINE_ICON_SZ,
        LLZ_LIFELINE_ICON_SZ
    };
}

// Check if a point is inside a lifeline icon, returns index (0-2) or -1
static int GetLifelineAtPoint(Vector2 point) {
    for (int i = 0; i < 3; i++) {
        Rectangle rect = GetLifelineIconRect(i);
        if (CheckCollisionPointRec(point, rect)) {
            return i;
        }
    }
    return -1;
}

static void DrawLifelineBar(void) {
    const char* labels[] = {"50:50", "PHONE", "AUDIENCE"};

    for (int i = 0; i < 3; i++) {
        Rectangle rect = GetLifelineIconRect(i);
        int x = (int)rect.x;
        int y = (int)rect.y;
        int iconSize = LLZ_LIFELINE_ICON_SZ;

        bool used = g_game.lifelinesUsed[i];
        bool isSelected = (g_game.selectedLifelineIdx == i && !used);
        bool isPending = (g_game.state == MIL_STATE_LIFELINE_CONFIRM && g_game.pendingLifeline == i);

        Color bgColor = used ? (Color){40, 40, 40, 200} : MILLIONAIRE_PURPLE;
        Color borderColor = used ? (Color){80, 80, 80, 255} : MILLIONAIRE_GOLD;

        // Highlight if selected or pending
        if (isSelected && !used) {
            float pulse = sinf(g_game.pulseTimer * 4.0f) * 0.3f + 0.7f;
            bgColor = (Color){(unsigned char)(150 * pulse), 0, (unsigned char)(180 * pulse), 255};
            borderColor = WHITE;
        }
        if (isPending) {
            float pulse = sinf(g_game.pulseTimer * 6.0f) * 0.5f + 0.5f;
            bgColor = (Color){(unsigned char)(80 + 80 * pulse), (unsigned char)(100 * pulse), 0, 255};
            borderColor = MILLIONAIRE_GOLD;
        }

        // Draw box with thicker border if selected
        DrawRectangle(x, y, iconSize, iconSize, bgColor);
        if (isSelected || isPending) {
            DrawRectangleLinesEx(rect, 3, borderColor);
        } else {
            DrawRectangleLines(x, y, iconSize, iconSize, borderColor);
        }

        // Draw icon or X
        if (used) {
            DrawLine(x + 8, y + 8, x + iconSize - 8, y + iconSize - 8, MILLIONAIRE_RED);
            DrawLine(x + iconSize - 8, y + 8, x + 8, y + iconSize - 8, MILLIONAIRE_RED);
        } else {
            Color iconColor = (isSelected || isPending) ? WHITE : MILLIONAIRE_GOLD;
            if (i == 0) {
                DrawText("50", x + 6, y + 6, 14, iconColor);
                DrawText("50", x + 16, y + 22, 14, iconColor);
            } else if (i == 1) {
                // Phone icon
                DrawRectangle(x + 15, y + 8, 12, 26, iconColor);
                DrawCircle(x + 21, y + 6, 4, iconColor);
            } else {
                // Bar chart icon
                DrawRectangle(x + 8, y + 30, 7, 10, iconColor);
                DrawRectangle(x + 18, y + 20, 7, 20, iconColor);
                DrawRectangle(x + 28, y + 10, 7, 30, iconColor);
            }
        }

        DrawText(labels[i], x + 2, y + iconSize + 3, 9, (Color){150, 150, 150, 255});
    }
}

static void DrawTitleScreen(void) {
    DrawBackground();

    // Title text with Copperplate Gothic style
    const char* title1 = "Who Wants To Be A";
    const char* title2 = "Millionaire?";

    float pulse = sinf(g_game.pulseTimer * 2.0f) * 0.2f + 0.8f;

    int t1Width = MeasureTextStyled(title1, 26);
    int t2Width = MeasureTextStyled(title2, 52);

    // Glow effect layers
    for (int i = 4; i > 0; i--) {
        Color glow = MILLIONAIRE_GOLD;
        glow.a = (unsigned char)(25 * (5 - i) * pulse);
        DrawTextCopperplate(title1, (g_screenWidth - t1Width) / 2 - i, 140, 26, glow);
        DrawTextCopperplate(title2, (g_screenWidth - t2Width) / 2 - i, 185, 52, glow);
    }

    // Main title with outline for prominence
    Color outlineColor = (Color){80, 50, 0, 255};
    DrawTextCopperplateOutlined(title1, (g_screenWidth - t1Width) / 2, 140, 26, MILLIONAIRE_GOLD, outlineColor);
    DrawTextCopperplateOutlined(title2, (g_screenWidth - t2Width) / 2, 185, 52, MILLIONAIRE_GOLD, outlineColor);

    // Decorative diamond
    int diamondY = 270;
    float diamondPulse = sinf(g_game.pulseTimer * 3.0f) * 10 + 30;
    DrawPoly((Vector2){g_screenWidth / 2, diamondY}, 4, diamondPulse, 45, MILLIONAIRE_GOLD);
    DrawPoly((Vector2){g_screenWidth / 2, diamondY}, 4, diamondPulse - 5, 45, MILLIONAIRE_PURPLE);

    // Decorative lines (Victorian style)
    int lineY = 265;
    DrawLineEx((Vector2){150, lineY}, (Vector2){350, lineY}, 2, MILLIONAIRE_GOLD);
    DrawLineEx((Vector2){450, lineY}, (Vector2){650, lineY}, 2, MILLIONAIRE_GOLD);

    // Press to play with Copperplate style
    if (((int)(g_game.pulseTimer * 2) % 2) == 0) {
        const char* pressText = "Press Select to Play";
        int pWidth = MeasureTextStyled(pressText, 22);
        DrawTextCopperplate(pressText, (g_screenWidth - pWidth) / 2, 360, 22, WHITE);
    }

    // Stats with small caps style
    if (g_game.gamesPlayed > 0) {
        char stats[64];
        snprintf(stats, sizeof(stats), "Games: %d  Total Winnings: $%d",
                 g_game.gamesPlayed, g_game.totalWinnings);
        int sWidth = MeasureTextStyled(stats, 14);
        DrawTextCopperplate(stats, (g_screenWidth - sWidth) / 2, 420, 14, (Color){180, 170, 140, 255});
    }

    // Question count
    if (g_game.questionsLoaded) {
        MillionairePoolStats stats;
        MlqGetPoolStats(&stats);
        char qText[64];
        snprintf(qText, sizeof(qText), "%d Questions Loaded", stats.totalQuestions);
        DrawText(qText, 20, g_screenHeight - 30, 14, (Color){100, 100, 100, 255});
    }
}

static void DrawTimer(void) {
    // Only show timer during active gameplay states (not during Phone a Friend which has its own timer)
    if (g_game.state != MIL_STATE_GAME_PLAYING &&
        g_game.state != MIL_STATE_LIFELINE_5050 &&
        g_game.state != MIL_STATE_LIFELINE_AUDIENCE &&
        g_game.state != MIL_STATE_ANSWER_LOCKED) return;

    // Position to the right of lifelines at bottom of screen
    // Lifelines: barX=30, 3 icons * 45 + 2 gaps * 15 = 165, so start at 30+165+20=215
    int timerX = 220;
    int timerY = 415;
    int timerWidth = 180;
    int timerHeight = 45;

    // Calculate remaining time
    float remaining = g_game.questionTimeLimit - g_game.questionTimer;
    if (remaining < 0) remaining = 0;
    int seconds = (int)remaining;
    float progress = remaining / g_game.questionTimeLimit;

    // Timer background
    DrawRectangle(timerX, timerY, timerWidth, timerHeight, (Color){10, 20, 50, 220});
    DrawRectangleLines(timerX, timerY, timerWidth, timerHeight, MILLIONAIRE_GOLD);

    // Progress bar
    Color barColor = MILLIONAIRE_GREEN;
    if (remaining < 15) barColor = MILLIONAIRE_RED;
    else if (remaining < 30) barColor = MILLIONAIRE_ORANGE;

    int barWidth = (int)((timerWidth - 10) * progress);
    DrawRectangle(timerX + 5, timerY + 28, barWidth, 12, barColor);
    DrawRectangleLines(timerX + 5, timerY + 28, timerWidth - 10, 12, (Color){100, 100, 120, 255});

    // Timer text
    char timerText[16];
    snprintf(timerText, sizeof(timerText), "%d", seconds);

    // Pulse effect when low on time
    Color timerColor = WHITE;
    int fontSize = 24;
    if (remaining < 10) {
        float pulse = sinf(g_game.pulseTimer * 8.0f) * 0.5f + 0.5f;
        timerColor = (Color){255, (unsigned char)(255 * (1.0f - pulse)), (unsigned char)(255 * (1.0f - pulse)), 255};
        fontSize = 26 + (int)(pulse * 4);
    }

    DrawTextStyled(timerText, timerX + 10, timerY + 2, fontSize, timerColor);
    DrawTextStyled("sec", timerX + 50, timerY + 6, 16, (Color){180, 180, 180, 255});
}

static void DrawGameScreen(void) {
    DrawBackground();
    DrawPrizeLadder();
    DrawQuestionBox();
    DrawAnswerGrid();
    DrawLifelineBar();
    DrawTimer();

    // Draw current prize level indicator
    char levelText[64];
    snprintf(levelText, sizeof(levelText), "Question %d for %s",
             g_game.prizeLevel + 1, g_prizeStrings[g_game.prizeLevel]);
    DrawTextStyled(levelText, 30, 5, 18, MILLIONAIRE_GOLD);
}

// ============================================================================
// Lifeline Overlay Screens
// ============================================================================

static void Draw5050Overlay(void) {
    DrawGameScreen();

    // Darken overlay
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 150});

    // 50:50 banner
    int bannerY = 180;
    DrawRectangle(100, bannerY, 400, 120, (Color){20, 0, 60, 240});
    DrawRectangleLines(100, bannerY, 400, 120, MILLIONAIRE_GOLD);

    // Title
    const char* title = "50:50";
    int tWidth = MeasureTextStyled(title, 48);
    DrawTextCopperplateOutlined(title, 100 + (400 - tWidth) / 2, bannerY + 15, 48, MILLIONAIRE_GOLD, (Color){80, 60, 0, 255});

    // Subtitle
    const char* subtitle = "Two Wrong Answers Removed!";
    int sWidth = MeasureTextStyled(subtitle, 18);
    DrawTextCopperplate(subtitle, 100 + (400 - sWidth) / 2, bannerY + 75, 18, WHITE);
}

static void DrawPhoneFriendOverlay(void) {
    DrawBackground();

    // Phone a Friend - Real life mode!
    // Show question clearly so user can read it to their friend

    int boxX = 30;
    int boxY = 30;
    int boxW = 740;
    int boxH = 380;

    // Main panel
    DrawRectangle(boxX, boxY, boxW, boxH, (Color){10, 20, 60, 245});
    DrawRectangleLines(boxX, boxY, boxW, boxH, MILLIONAIRE_GOLD);

    // Corner decorations
    DrawTriangle((Vector2){boxX, boxY}, (Vector2){boxX + 20, boxY}, (Vector2){boxX, boxY + 20}, MILLIONAIRE_GOLD);
    DrawTriangle((Vector2){boxX + boxW, boxY}, (Vector2){boxX + boxW - 20, boxY}, (Vector2){boxX + boxW, boxY + 20}, MILLIONAIRE_GOLD);

    // Title
    const char* title = "Phone A Friend";
    int tWidth = MeasureTextStyled(title, 32);
    DrawTextCopperplateOutlined(title, boxX + (boxW - tWidth) / 2, boxY + 15, 32, MILLIONAIRE_GOLD, (Color){80, 60, 0, 255});

    // Subtitle - instruction
    const char* instruction = "Read This Question to Your Friend!";
    int iWidth = MeasureTextStyled(instruction, 16);
    DrawTextCopperplate(instruction, boxX + (boxW - iWidth) / 2, boxY + 55, 16, (Color){200, 200, 255, 255});

    // Timer display - large and prominent
    float remaining = PHONE_CALL_DURATION - g_game.phoneCallTimer;
    if (remaining < 0) remaining = 0;
    int seconds = (int)remaining;

    // Timer background
    int timerX = boxX + boxW - 120;
    int timerY = boxY + 10;
    DrawRectangle(timerX, timerY, 100, 60, (Color){0, 0, 0, 200});
    DrawRectangleLines(timerX, timerY, 100, 60, remaining < 10 ? MILLIONAIRE_RED : MILLIONAIRE_GOLD);

    // Timer text
    char timerText[8];
    snprintf(timerText, sizeof(timerText), "%d", seconds);
    Color timerColor = WHITE;
    if (remaining < 10) {
        float pulse = sinf(g_game.pulseTimer * 8.0f) * 0.5f + 0.5f;
        timerColor = (Color){255, (unsigned char)(255 * (1 - pulse)), (unsigned char)(255 * (1 - pulse)), 255};
    }
    int tmWidth = MeasureTextStyled(timerText, 36);
    DrawTextStyled(timerText, timerX + (100 - tmWidth) / 2, timerY + 5, 36, timerColor);
    DrawTextStyled("sec", timerX + 35, timerY + 40, 14, (Color){150, 150, 150, 255});

    // Question text - large and clear
    if (g_game.currentQuestion) {
        // Category
        char catText[80];
        snprintf(catText, sizeof(catText), "Category: %s", g_game.currentQuestion->category);
        DrawText(catText, boxX + 20, boxY + 85, 14, (Color){180, 180, 200, 255});

        // Question
        const char* question = g_game.currentQuestion->question;
        int qY = boxY + 110;

        // Word wrap the question
        char wrapped[512];
        strncpy(wrapped, question, sizeof(wrapped) - 1);
        wrapped[sizeof(wrapped) - 1] = '\0';

        int fontSize = 22;
        int maxWidth = boxW - 40;
        int lineStart = 0;
        int lastSpace = 0;
        int lineCount = 0;

        for (int i = 0; wrapped[i] && lineCount < 4; i++) {
            if (wrapped[i] == ' ') lastSpace = i;

            char temp[512];
            strncpy(temp, wrapped + lineStart, i - lineStart + 1);
            temp[i - lineStart + 1] = '\0';

            if (MeasureText(temp, fontSize) > maxWidth && lastSpace > lineStart) {
                wrapped[lastSpace] = '\0';
                DrawTextStyled(wrapped + lineStart, boxX + 20, qY, fontSize, WHITE);
                qY += fontSize + 6;
                lineStart = lastSpace + 1;
                lineCount++;
            }
        }
        if (wrapped[lineStart]) {
            DrawTextStyled(wrapped + lineStart, boxX + 20, qY, fontSize, WHITE);
            qY += fontSize + 10;
        }

        // Answer options - clearly labeled
        const char* letters[] = {"A:", "B:", "C:", "D:"};
        int ansY = boxY + 200;
        int ansX1 = boxX + 40;
        int ansX2 = boxX + 380;

        for (int i = 0; i < 4; i++) {
            int x = (i % 2 == 0) ? ansX1 : ansX2;
            int y = ansY + (i / 2) * 50;

            bool isEliminated = g_game.eliminated[i];
            Color letterColor = MILLIONAIRE_GOLD;
            Color ansColor = WHITE;

            if (isEliminated) {
                letterColor = (Color){80, 80, 80, 255};
                ansColor = (Color){80, 80, 80, 255};
            }

            // Letter
            DrawTextStyled(letters[i], x, y, 24, letterColor);

            // Answer text
            if (!isEliminated) {
                const char* ansText = g_game.currentQuestion->options[i];
                int ansFont = 20;
                if (MeasureText(ansText, ansFont) > 300) ansFont = 16;
                DrawTextStyled(ansText, x + 40, y + 2, ansFont, ansColor);
            } else {
                DrawLine(x + 40, y + 12, x + 200, y + 12, (Color){100, 100, 100, 255});
            }
        }
    }

    // Bottom instruction
    const char* backText = "Press Back When Done";
    int bWidth = MeasureTextStyled(backText, 18);
    DrawTextCopperplate(backText, boxX + (boxW - bWidth) / 2, boxY + boxH - 35, 18, (Color){150, 200, 150, 255});

    // Prize at stake
    char prizeText[64];
    snprintf(prizeText, sizeof(prizeText), "Playing for %s", g_prizeStrings[g_game.prizeLevel]);
    int pWidth = MeasureTextStyled(prizeText, 14);
    DrawTextCopperplate(prizeText, boxX + (boxW - pWidth) / 2, boxY + boxH - 60, 14, MILLIONAIRE_GOLD);
}

static void DrawAudiencePollOverlay(void) {
    DrawGameScreen();

    // Darken overlay
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 180});

    // Poll results panel
    int panelX = 80;
    int panelY = 100;
    int panelW = 440;
    int panelH = 280;

    DrawRectangle(panelX, panelY, panelW, panelH, (Color){10, 20, 60, 245});
    DrawRectangleLines(panelX, panelY, panelW, panelH, MILLIONAIRE_GOLD);

    // Title
    const char* title = "Ask The Audience";
    int tWidth = MeasureTextStyled(title, 28);
    DrawTextCopperplateOutlined(title, panelX + (panelW - tWidth) / 2, panelY + 15, 28, MILLIONAIRE_GOLD, (Color){80, 60, 0, 255});

    // Bar chart
    int chartX = panelX + 50;
    int chartY = panelY + 200;
    int barWidth = 70;
    int barGap = 20;
    int maxBarHeight = 120;

    const char* letters[] = {"A", "B", "C", "D"};

    for (int i = 0; i < 4; i++) {
        int x = chartX + i * (barWidth + barGap);
        bool isEliminated = g_game.eliminated[i];

        // Get percentage (animated)
        int pct = g_game.audiencePercentages[i];
        float animatedPct = pct * g_game.audiencePollProgress;
        int barHeight = (int)(maxBarHeight * animatedPct / 100.0f);

        // Bar background
        DrawRectangle(x, chartY - maxBarHeight, barWidth, maxBarHeight, (Color){30, 30, 50, 200});

        // Bar fill
        Color barColor = isEliminated ? (Color){60, 60, 60, 255} : MILLIONAIRE_PURPLE;
        if (!isEliminated && pct >= 40) barColor = MILLIONAIRE_GREEN;
        else if (!isEliminated && pct >= 25) barColor = MILLIONAIRE_ORANGE;

        DrawRectangle(x, chartY - barHeight, barWidth, barHeight, barColor);
        DrawRectangleLines(x, chartY - maxBarHeight, barWidth, maxBarHeight, (Color){100, 100, 120, 255});

        // Letter label
        Color letterColor = isEliminated ? (Color){80, 80, 80, 255} : MILLIONAIRE_GOLD;
        int lWidth = MeasureTextStyled(letters[i], 24);
        DrawTextStyled(letters[i], x + (barWidth - lWidth) / 2, chartY + 10, 24, letterColor);

        // Percentage label
        if (g_game.audiencePollProgress > 0.5f) {
            char pctText[8];
            snprintf(pctText, sizeof(pctText), "%d%%", (int)animatedPct);
            int pctWidth = MeasureText(pctText, 16);
            Color pctColor = isEliminated ? (Color){80, 80, 80, 255} : WHITE;
            DrawText(pctText, x + (barWidth - pctWidth) / 2, chartY - barHeight - 20, 16, pctColor);
        }
    }

    // Instruction
    if (g_game.audiencePollProgress >= 1.0f) {
        const char* backText = "Press Back to Continue";
        int bWidth = MeasureTextStyled(backText, 16);
        DrawTextCopperplate(backText, panelX + (panelW - bWidth) / 2, panelY + panelH - 30, 16, (Color){150, 200, 150, 255});
    }
}

static void DrawLifelineConfirmOverlay(void) {
    DrawGameScreen();

    // Darken overlay
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 180});

    // Confirmation panel
    int panelW = 480;
    int panelH = 240;
    int panelX = (g_screenWidth - panelW) / 2;
    int panelY = (g_screenHeight - panelH) / 2 - 30;

    // Panel background with Victorian border
    DrawRectangle(panelX - 3, panelY - 3, panelW + 6, panelH + 6, (Color){80, 60, 0, 255});
    DrawRectangle(panelX, panelY, panelW, panelH, (Color){10, 20, 60, 245});
    DrawRectangleLines(panelX, panelY, panelW, panelH, MILLIONAIRE_GOLD);

    // Corner decorations
    DrawTriangle((Vector2){panelX, panelY}, (Vector2){panelX + 15, panelY}, (Vector2){panelX, panelY + 15}, MILLIONAIRE_GOLD);
    DrawTriangle((Vector2){panelX + panelW, panelY}, (Vector2){panelX + panelW - 15, panelY}, (Vector2){panelX + panelW, panelY + 15}, MILLIONAIRE_GOLD);

    // Lifeline name
    const char* lifelineNames[] = {"50:50", "Phone A Friend", "Ask The Audience"};
    const char* lifelineDesc[] = {
        "Remove two wrong answers",
        "30 seconds to ask a real friend",
        "See what the audience thinks"
    };

    int lifelineIdx = g_game.pendingLifeline;
    if (lifelineIdx < 0 || lifelineIdx > 2) lifelineIdx = 0;

    const char* title = lifelineNames[lifelineIdx];
    int tWidth = MeasureTextStyled(title, 36);
    DrawTextCopperplateOutlined(title, panelX + (panelW - tWidth) / 2, panelY + 20, 36, MILLIONAIRE_GOLD, (Color){80, 60, 0, 255});

    // Description
    const char* desc = lifelineDesc[lifelineIdx];
    int dWidth = MeasureTextStyled(desc, 18);
    DrawTextCopperplate(desc, panelX + (panelW - dWidth) / 2, panelY + 70, 18, WHITE);

    // Confirmation timer
    float remaining = g_game.lifelineConfirmTimeLimit - g_game.lifelineConfirmTimer;
    if (remaining < 0) remaining = 0;
    int seconds = (int)remaining;

    // Timer box
    int timerBoxX = panelX + panelW - 100;
    int timerBoxY = panelY + 15;
    DrawRectangle(timerBoxX, timerBoxY, 80, 50, (Color){0, 0, 0, 200});

    Color timerBorderColor = MILLIONAIRE_GOLD;
    Color timerColor = WHITE;
    if (remaining < 10) {
        float pulse = sinf(g_game.pulseTimer * 8.0f) * 0.5f + 0.5f;
        timerBorderColor = (Color){255, (unsigned char)(100 * pulse), 0, 255};
        timerColor = (Color){255, (unsigned char)(255 * (1 - pulse)), (unsigned char)(255 * (1 - pulse)), 255};
    }
    DrawRectangleLines(timerBoxX, timerBoxY, 80, 50, timerBorderColor);

    char timerText[8];
    snprintf(timerText, sizeof(timerText), "%d", seconds);
    int tmWidth = MeasureTextStyled(timerText, 28);
    DrawTextStyled(timerText, timerBoxX + (80 - tmWidth) / 2, timerBoxY + 5, 28, timerColor);
    DrawTextStyled("sec", timerBoxX + 25, timerBoxY + 33, 12, (Color){150, 150, 150, 255});

    // "Use this lifeline?" prompt
    const char* prompt = "Use This Lifeline?";
    int pWidth = MeasureTextStyled(prompt, 22);
    DrawTextCopperplate(prompt, panelX + (panelW - pWidth) / 2, panelY + 105, 22, MILLIONAIRE_ORANGE);

    // Instructions
    const char* confirmText = "Tap Lifeline or Press Select to Confirm";
    const char* cancelText = "Press Back to Cancel";
    const char* autoText = "Auto-confirms when timer expires";

    int cfWidth = MeasureTextStyled(confirmText, 16);
    int cnWidth = MeasureTextStyled(cancelText, 16);
    int atWidth = MeasureTextStyled(autoText, 12);

    DrawTextCopperplate(confirmText, panelX + (panelW - cfWidth) / 2, panelY + 150, 16, MILLIONAIRE_GREEN);
    DrawTextCopperplate(cancelText, panelX + (panelW - cnWidth) / 2, panelY + 180, 16, (Color){180, 180, 180, 255});
    DrawTextCopperplate(autoText, panelX + (panelW - atWidth) / 2, panelY + 210, 12, (Color){120, 120, 150, 255});
}

static void DrawCorrectScreen(void) {
    DrawBackground();
    DrawPrizeLadder();

    // Celebration effect
    float pulse = sinf(g_game.stateTimer * 10.0f) * 0.3f + 0.7f;

    const char* correctText = "Correct!";
    int cWidth = MeasureTextStyled(correctText, 56);

    // Glow layers
    for (int i = 5; i > 0; i--) {
        Color glow = MILLIONAIRE_GREEN;
        glow.a = (unsigned char)(35 * (6 - i) * pulse);
        DrawTextCopperplate(correctText, (450 - cWidth) / 2, 150 - i, 56, glow);
    }
    DrawTextCopperplateOutlined(correctText, (450 - cWidth) / 2, 150, 56, MILLIONAIRE_GREEN, (Color){0, 80, 30, 255});

    // Prize won
    char prizeText[64];
    snprintf(prizeText, sizeof(prizeText), "You've Won %s!", g_prizeStrings[g_game.prizeLevel]);
    int pWidth = MeasureTextStyled(prizeText, 26);
    DrawTextCopperplate(prizeText, (450 - pWidth) / 2, 230, 26, MILLIONAIRE_GOLD);

    // Next question hint
    if (g_game.prizeLevel < 14) {
        const char* nextText = "Get Ready for the Next Question...";
        int nWidth = MeasureTextStyled(nextText, 18);
        DrawTextCopperplate(nextText, (450 - nWidth) / 2, 290, 18, (Color){200, 200, 200, 255});
    }
}

static void DrawWrongScreen(void) {
    DrawBackground();

    // Red tint overlay
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){100, 0, 0, 100});

    // Game Over title with dramatic Copperplate styling
    const char* wrongText = "Game Over";
    int wWidth = MeasureTextStyled(wrongText, 54);

    // Red glow effect
    float pulse = sinf(g_game.stateTimer * 3.0f) * 0.2f + 0.8f;
    for (int i = 4; i > 0; i--) {
        Color glow = MILLIONAIRE_RED;
        glow.a = (unsigned char)(30 * (5 - i) * pulse);
        DrawTextCopperplate(wrongText, (g_screenWidth - wWidth) / 2 - i, 100, 54, glow);
    }
    DrawTextCopperplateOutlined(wrongText, (g_screenWidth - wWidth) / 2, 100, 54, MILLIONAIRE_RED, (Color){80, 0, 0, 255});

    // Show correct answer
    if (g_game.currentQuestion) {
        const char* correctLabel = "The Correct Answer Was:";
        int clWidth = MeasureTextStyled(correctLabel, 18);
        DrawTextCopperplate(correctLabel, (g_screenWidth - clWidth) / 2, 180, 18, WHITE);

        char correctAnswer[300];
        char letter = 'A' + g_game.currentQuestion->correctIndex;
        snprintf(correctAnswer, sizeof(correctAnswer), "%c: %s",
                 letter, g_game.currentQuestion->options[g_game.currentQuestion->correctIndex]);
        int caWidth = MeasureTextStyled(correctAnswer, 22);
        DrawTextCopperplateOutlined(correctAnswer, (g_screenWidth - caWidth) / 2, 210, 22, MILLIONAIRE_GREEN, (Color){0, 60, 20, 255});
    }

    // Prize walked away with
    const char* guaranteedPrize = GetGuaranteedPrizeString(g_game.prizeLevel);
    char prizeText[64];
    snprintf(prizeText, sizeof(prizeText), "You Walk Away With: %s", guaranteedPrize);
    int pWidth = MeasureTextStyled(prizeText, 26);
    DrawTextCopperplate(prizeText, (g_screenWidth - pWidth) / 2, 290, 26, MILLIONAIRE_GOLD);

    // Continue prompt
    if (((int)(g_game.pulseTimer * 2) % 2) == 0) {
        const char* continueText = "Press Select to Continue";
        int cWidth = MeasureTextStyled(continueText, 18);
        DrawTextCopperplate(continueText, (g_screenWidth - cWidth) / 2, 380, 18, (Color){180, 180, 180, 255});
    }
}

static void DrawWinScreen(void) {
    DrawBackground();

    // Massive celebration
    float pulse = sinf(g_game.stateTimer * 8.0f) * 0.5f + 0.5f;

    // Gold rays - Victorian sunburst effect
    for (int i = 0; i < 16; i++) {
        float angle = (i * 22.5f + g_game.stateTimer * 50) * DEG2RAD;
        float len = 400 + sinf(g_game.stateTimer * 3 + i) * 50;
        Vector2 end = {
            g_screenWidth / 2 + cosf(angle) * len,
            g_screenHeight / 2 + sinf(angle) * len
        };
        DrawLineEx((Vector2){g_screenWidth / 2, g_screenHeight / 2}, end, 3,
                   (Color){255, 215, 0, (unsigned char)(50 + pulse * 50)});
    }

    // Decorative corner flourishes
    Color flourishColor = (Color){255, 215, 0, (unsigned char)(100 + pulse * 100)};
    DrawLineEx((Vector2){50, 50}, (Vector2){150, 50}, 2, flourishColor);
    DrawLineEx((Vector2){50, 50}, (Vector2){50, 100}, 2, flourishColor);
    DrawLineEx((Vector2){750, 50}, (Vector2){650, 50}, 2, flourishColor);
    DrawLineEx((Vector2){750, 50}, (Vector2){750, 100}, 2, flourishColor);
    DrawLineEx((Vector2){50, 430}, (Vector2){150, 430}, 2, flourishColor);
    DrawLineEx((Vector2){50, 430}, (Vector2){50, 380}, 2, flourishColor);
    DrawLineEx((Vector2){750, 430}, (Vector2){650, 430}, 2, flourishColor);
    DrawLineEx((Vector2){750, 430}, (Vector2){750, 380}, 2, flourishColor);

    // Title with massive Copperplate styling
    const char* winText = "Millionaire!";
    int wWidth = MeasureTextStyled(winText, 64);

    // Multi-layer golden glow effect
    for (int i = 8; i > 0; i--) {
        Color glow = MILLIONAIRE_GOLD;
        glow.a = (unsigned char)(35 * (9 - i) * pulse);
        DrawTextCopperplate(winText, (g_screenWidth - wWidth) / 2 - i, 130, 64, glow);
    }
    DrawTextCopperplateOutlined(winText, (g_screenWidth - wWidth) / 2, 130, 64, MILLIONAIRE_GOLD, (Color){100, 70, 0, 255});

    // Prize amount with grand styling
    const char* prizeText = "You've Won $1,000,000!";
    int pWidth = MeasureTextStyled(prizeText, 32);
    DrawTextCopperplateOutlined(prizeText, (g_screenWidth - pWidth) / 2, 230, 32, WHITE, (Color){40, 40, 60, 255});

    // Congratulations with green accent
    const char* congrats = "Congratulations!";
    int cWidth = MeasureTextStyled(congrats, 28);
    DrawTextCopperplate(congrats, (g_screenWidth - cWidth) / 2, 300, 28, MILLIONAIRE_GREEN);

    // Decorative diamond centerpiece
    float diamondPulse = sinf(g_game.stateTimer * 4.0f) * 8 + 20;
    DrawPoly((Vector2){g_screenWidth / 2, 380}, 4, diamondPulse, 45, MILLIONAIRE_GOLD);
    DrawPoly((Vector2){g_screenWidth / 2, 380}, 4, diamondPulse - 4, 45, MILLIONAIRE_PURPLE);
}

static void DrawWalkawayConfirm(void) {
    DrawGameScreen();

    // Overlay
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 180});

    // Confirmation box with Victorian border
    int boxW = 420;
    int boxH = 210;
    int boxX = (g_screenWidth - boxW) / 2;
    int boxY = (g_screenHeight - boxH) / 2;

    // Double border effect
    DrawRectangle(boxX - 3, boxY - 3, boxW + 6, boxH + 6, (Color){80, 60, 0, 255});
    DrawRectangle(boxX, boxY, boxW, boxH, MILLIONAIRE_DARK_BLUE);
    DrawRectangleLines(boxX, boxY, boxW, boxH, MILLIONAIRE_GOLD);

    // Decorative corner accents
    int cornerSize = 15;
    DrawTriangle(
        (Vector2){boxX, boxY},
        (Vector2){boxX + cornerSize, boxY},
        (Vector2){boxX, boxY + cornerSize},
        MILLIONAIRE_GOLD
    );
    DrawTriangle(
        (Vector2){boxX + boxW, boxY},
        (Vector2){boxX + boxW - cornerSize, boxY},
        (Vector2){boxX + boxW, boxY + cornerSize},
        MILLIONAIRE_GOLD
    );

    // Title with Copperplate styling
    const char* title = "Walk Away?";
    int tWidth = MeasureTextStyled(title, 26);
    DrawTextCopperplateOutlined(title, boxX + (boxW - tWidth) / 2, boxY + 20, 26, MILLIONAIRE_GOLD, (Color){80, 60, 0, 255});

    // Prize text
    char prizeText[64];
    snprintf(prizeText, sizeof(prizeText), "You Will Leave With %s", g_prizeStrings[g_game.prizeLevel]);
    int pWidth = MeasureTextStyled(prizeText, 18);
    DrawTextCopperplate(prizeText, boxX + (boxW - pWidth) / 2, boxY + 70, 18, WHITE);

    // Instructions with Copperplate styling
    const char* confirm = "Press Select to Confirm";
    const char* cancel = "Press Back to Continue";
    int cfWidth = MeasureTextStyled(confirm, 14);
    int cnWidth = MeasureTextStyled(cancel, 14);
    DrawTextCopperplate(confirm, boxX + (boxW - cfWidth) / 2, boxY + 125, 14, MILLIONAIRE_GREEN);
    DrawTextCopperplate(cancel, boxX + (boxW - cnWidth) / 2, boxY + 160, 14, (Color){180, 180, 180, 255});
}

static void DrawFinalResults(void) {
    DrawBackground();

    // Title with Victorian Copperplate styling
    const char* title = "Final Results";
    int tWidth = MeasureTextStyled(title, 34);

    // Subtle glow for title
    for (int i = 3; i > 0; i--) {
        Color glow = MILLIONAIRE_GOLD;
        glow.a = (unsigned char)(40 * (4 - i));
        DrawTextCopperplate(title, (g_screenWidth - tWidth) / 2 - i, 70, 34, glow);
    }
    DrawTextCopperplateOutlined(title, (g_screenWidth - tWidth) / 2, 70, 34, MILLIONAIRE_GOLD, (Color){80, 60, 0, 255});

    // Decorative lines under title
    int lineY = 115;
    DrawLineEx((Vector2){200, lineY}, (Vector2){350, lineY}, 2, MILLIONAIRE_GOLD);
    DrawPoly((Vector2){400, lineY}, 4, 6, 45, MILLIONAIRE_GOLD);
    DrawLineEx((Vector2){450, lineY}, (Vector2){600, lineY}, 2, MILLIONAIRE_GOLD);

    // Results box with Victorian border
    int boxW = 500;
    int boxH = 200;
    int boxX = (g_screenWidth - boxW) / 2;
    int boxY = 140;

    // Double border effect
    DrawRectangle(boxX - 3, boxY - 3, boxW + 6, boxH + 6, (Color){80, 60, 0, 255});
    DrawRectangle(boxX, boxY, boxW, boxH, (Color){10, 20, 50, 230});
    DrawRectangleLines(boxX, boxY, boxW, boxH, MILLIONAIRE_GOLD);

    // Corner accents
    DrawTriangle((Vector2){boxX, boxY}, (Vector2){boxX + 15, boxY}, (Vector2){boxX, boxY + 15}, MILLIONAIRE_GOLD);
    DrawTriangle((Vector2){boxX + boxW, boxY}, (Vector2){boxX + boxW - 15, boxY}, (Vector2){boxX + boxW, boxY + 15}, MILLIONAIRE_GOLD);
    DrawTriangle((Vector2){boxX, boxY + boxH}, (Vector2){boxX + 15, boxY + boxH}, (Vector2){boxX, boxY + boxH - 15}, MILLIONAIRE_GOLD);
    DrawTriangle((Vector2){boxX + boxW, boxY + boxH}, (Vector2){boxX + boxW - 15, boxY + boxH}, (Vector2){boxX + boxW, boxY + boxH - 15}, MILLIONAIRE_GOLD);

    // Prize won - largest and most prominent
    char wonText[64];
    snprintf(wonText, sizeof(wonText), "Prize Won: %s",
             g_game.prizeLevel > 0 ? g_prizeStrings[g_game.prizeLevel - 1] : "$0");
    DrawTextCopperplateOutlined(wonText, boxX + 30, boxY + 35, 26, MILLIONAIRE_GOLD, (Color){80, 60, 0, 255});

    // Questions answered
    char questionsText[64];
    snprintf(questionsText, sizeof(questionsText), "Questions Answered: %d", g_game.prizeLevel);
    DrawTextCopperplate(questionsText, boxX + 30, boxY + 90, 20, WHITE);

    // Career winnings
    char totalText[64];
    snprintf(totalText, sizeof(totalText), "Career Winnings: $%d", g_game.totalWinnings);
    DrawTextCopperplate(totalText, boxX + 30, boxY + 130, 20, WHITE);

    // Games played
    char gamesText[64];
    snprintf(gamesText, sizeof(gamesText), "Total Games: %d", g_game.gamesPlayed);
    DrawTextCopperplate(gamesText, boxX + 30, boxY + 165, 16, (Color){150, 150, 180, 255});

    // Play again prompt with blinking effect
    if (((int)(g_game.pulseTimer * 2) % 2) == 0) {
        const char* playAgain = "Press Select to Play Again";
        int paWidth = MeasureTextStyled(playAgain, 20);
        DrawTextCopperplate(playAgain, (g_screenWidth - paWidth) / 2, 400, 20, (Color){180, 180, 180, 255});
    }
}

static void DrawQuitConfirm(void) {
    DrawGameScreen();

    // Overlay
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 200});

    // Quit confirmation box with Victorian border
    int boxW = 450;
    int boxH = 220;
    int boxX = (g_screenWidth - boxW) / 2;
    int boxY = (g_screenHeight - boxH) / 2;

    // Double border effect
    DrawRectangle(boxX - 3, boxY - 3, boxW + 6, boxH + 6, (Color){120, 40, 40, 255});
    DrawRectangle(boxX, boxY, boxW, boxH, MILLIONAIRE_DARK_BLUE);
    DrawRectangleLines(boxX, boxY, boxW, boxH, MILLIONAIRE_RED);

    // Corner decorations
    DrawTriangle((Vector2){boxX, boxY}, (Vector2){boxX + 15, boxY}, (Vector2){boxX, boxY + 15}, MILLIONAIRE_RED);
    DrawTriangle((Vector2){boxX + boxW, boxY}, (Vector2){boxX + boxW - 15, boxY}, (Vector2){boxX + boxW, boxY + 15}, MILLIONAIRE_RED);

    // Title
    const char* title = "Quit Game?";
    int tWidth = MeasureTextStyled(title, 30);
    DrawTextCopperplateOutlined(title, boxX + (boxW - tWidth) / 2, boxY + 20, 30, MILLIONAIRE_RED, (Color){80, 0, 0, 255});

    // Warning message
    const char* warning = "Your Progress Will Be Lost!";
    int wWidth = MeasureTextStyled(warning, 18);
    DrawTextCopperplate(warning, boxX + (boxW - wWidth) / 2, boxY + 65, 18, MILLIONAIRE_ORANGE);

    // Current progress info
    char progressText[64];
    if (g_game.prizeLevel > 0) {
        snprintf(progressText, sizeof(progressText), "Current Progress: Question %d", g_game.prizeLevel + 1);
    } else {
        snprintf(progressText, sizeof(progressText), "You Haven't Won Anything Yet");
    }
    int pWidth = MeasureTextStyled(progressText, 16);
    DrawTextCopperplate(progressText, boxX + (boxW - pWidth) / 2, boxY + 100, 16, WHITE);

    // Instructions
    const char* quitText = "Press Select to Quit";
    const char* stayText = "Press Back to Continue Playing";
    int qWidth = MeasureTextStyled(quitText, 16);
    int sWidth = MeasureTextStyled(stayText, 16);
    DrawTextCopperplate(quitText, boxX + (boxW - qWidth) / 2, boxY + 145, 16, MILLIONAIRE_RED);
    DrawTextCopperplate(stayText, boxX + (boxW - sWidth) / 2, boxY + 175, 16, MILLIONAIRE_GREEN);
}

// ============================================================================
// Game Logic
// ============================================================================

static void StartNewGame(void) {
    g_game.state = MIL_STATE_GAME_PLAYING;
    g_game.prizeLevel = 0;
    g_game.cursorIndex = 0;
    g_game.selectedAnswer = -1;
    g_game.gameInProgress = true;  // Mark game as active
    g_game.pendingLifeline = -1;
    g_game.selectedLifelineIdx = -1;

    for (int i = 0; i < 4; i++) g_game.eliminated[i] = false;
    for (int i = 0; i < 3; i++) g_game.lifelinesUsed[i] = false;

    g_game.stateTimer = 0;
    g_game.questionTimer = 0;
    g_game.questionTimeLimit = GetTimerForLevel(0);
    g_game.lifelineConfirmTimer = 0;
    g_game.lifelineConfirmTimeLimit = 0;

    MlqResetQuestionPool();
    LifelinesInit(&g_lifelines);

    // Get first question
    g_game.currentQuestion = MlqGetQuestionForLevel(g_game.prizeLevel);
    if (g_game.currentQuestion) {
        MlqShuffleAnswers(g_game.currentQuestion);
    }

    printf("Started new game, question: %s\n",
           g_game.currentQuestion ? g_game.currentQuestion->question : "NULL");
}

static void AdvanceToNextQuestion(void) {
    g_game.prizeLevel++;
    g_game.cursorIndex = 0;
    g_game.selectedAnswer = -1;

    for (int i = 0; i < 4; i++) g_game.eliminated[i] = false;

    if (g_game.prizeLevel >= 15) {
        g_game.state = MIL_STATE_GAME_WON;
        EndGame(1000000);  // Million dollar winner!
    } else {
        g_game.state = MIL_STATE_GAME_PLAYING;
        g_game.currentQuestion = MlqGetQuestionForLevel(g_game.prizeLevel);
        if (g_game.currentQuestion) {
            MlqShuffleAnswers(g_game.currentQuestion);
        }
        // Reset timer for next question
        g_game.questionTimer = 0;
        g_game.questionTimeLimit = GetTimerForLevel(g_game.prizeLevel);
    }

    g_game.stateTimer = 0;
}

static void AnswerWrong(void) {
    g_game.state = MIL_STATE_WRONG_ANSWER;
    EndGame(GetGuaranteedPrize(g_game.prizeLevel));
    g_game.stateTimer = 0;
}

static void WalkAway(void) {
    g_game.state = MIL_STATE_FINAL_RESULTS;
    int winnings = (g_game.prizeLevel > 0) ? g_prizeAmounts[g_game.prizeLevel - 1] : 0;
    EndGame(winnings);
    g_game.stateTimer = 0;
}

static void QuitGame(void) {
    // Quit without saving - forfeit current game
    g_game.state = MIL_STATE_TITLE_SCREEN;
    g_game.gameInProgress = false;
    g_game.stateTimer = 0;
}

static void LockInAnswer(void) {
    int idx = GetCursorIndex();
    if (g_game.eliminated[idx]) return;

    g_game.selectedAnswer = idx;
    g_game.state = MIL_STATE_ANSWER_LOCKED;
    g_game.stateTimer = 0;
}

static void RevealAnswer(void) {
    if (!g_game.currentQuestion) return;

    if (g_game.selectedAnswer == g_game.currentQuestion->correctIndex) {
        g_game.state = MIL_STATE_CORRECT_ANSWER;
    } else {
        AnswerWrong();
    }
    g_game.stateTimer = 0;
}

// ============================================================================
// Update Functions
// ============================================================================

static void UpdateTitleScreen(const LlzInputState* input) {
    // Press select button to start game
    if (input->selectPressed) {
        if (g_game.questionsLoaded) {
            StartNewGame();
        }
    }
    // Back button exits the plugin
    if (input->backReleased) {
        g_wantsClose = true;
    }
}

static void UpdateGamePlaying(const LlzInputState* input, float deltaTime) {
    // Update timer
    g_game.questionTimer += deltaTime;
    if (g_game.questionTimer >= g_game.questionTimeLimit) {
        // Time's up - wrong answer
        g_game.selectedAnswer = g_game.cursorIndex;
        AnswerWrong();
        return;
    }

    // Track mouse position for lifeline hover highlighting
    g_game.selectedLifelineIdx = GetLifelineAtPoint(input->mousePos);

    // Check for tap on lifelines - triggers confirmation
    if (input->tap) {
        int tappedLifeline = GetLifelineAtPoint(input->tapPosition);
        if (tappedLifeline >= 0 && !g_game.lifelinesUsed[tappedLifeline] && g_game.currentQuestion) {
            RequestLifeline(tappedLifeline);
            return;
        }
    }

    // Navigation: scroll wheel loops through A->B->C->D->A
    static float scrollCooldown = 0;
    scrollCooldown -= deltaTime;

    if (scrollCooldown <= 0) {
        int direction = 0;

        // Scroll wheel navigation (primary method - loops through all 4)
        if (input->scrollDelta > 0.1f) {
            direction = 1;   // Scroll up = next answer
            scrollCooldown = 0.15f;
        } else if (input->scrollDelta < -0.1f) {
            direction = -1;  // Scroll down = previous answer
            scrollCooldown = 0.15f;
        }
        // D-pad up/down as alternative
        else if (input->upPressed) {
            direction = -1;
            scrollCooldown = 0.2f;
        } else if (input->downPressed) {
            direction = 1;
            scrollCooldown = 0.2f;
        }
        // Swipe gestures
        else if (input->swipeUp) {
            direction = -1;
            scrollCooldown = 0.3f;
        } else if (input->swipeDown) {
            direction = 1;
            scrollCooldown = 0.3f;
        }

        if (direction != 0) {
            // Find next non-eliminated answer (looping)
            int startIdx = g_game.cursorIndex;
            int attempts = 0;
            do {
                g_game.cursorIndex = (g_game.cursorIndex + direction + 4) % 4;
                attempts++;
            } while (g_game.eliminated[g_game.cursorIndex] && attempts < 4);

            // If all answers eliminated (shouldn't happen), reset to start
            if (attempts >= 4) {
                g_game.cursorIndex = startIdx;
            }
        }
    }

    // Lock in answer - ONLY with rotary wheel select button (not screen tap)
    if (input->selectPressed) {
        if (!g_game.eliminated[g_game.cursorIndex]) {
            LockInAnswer();
        }
    }

    // Walk away
    if (input->backReleased) {
        g_game.state = MIL_STATE_WALKAWAY_CONFIRM;
    }

    // Lifelines via buttons (all go through confirmation now)
    // 50:50 - button1 or swipe left
    if ((input->button1Pressed || input->swipeLeft) && !g_game.lifelinesUsed[0] && g_game.currentQuestion) {
        RequestLifeline(0);
    }
    // Phone a Friend - button2 or swipe right
    if ((input->button2Pressed || input->swipeRight) && !g_game.lifelinesUsed[1] && g_game.currentQuestion) {
        RequestLifeline(1);
    }
    // Ask the Audience - button3
    if (input->button3Pressed && !g_game.lifelinesUsed[2] && g_game.currentQuestion) {
        RequestLifeline(2);
    }
}

// ============================================================================
// Lifeline Update Functions
// ============================================================================

static void Update5050Lifeline(const LlzInputState* input, float dt) {
    g_game.stateTimer += dt;

    // Brief overlay then return to game - select or back to dismiss
    if (g_game.stateTimer >= FIFTY_FIFTY_DURATION || input->selectPressed || input->backReleased) {
        g_game.state = MIL_STATE_GAME_PLAYING;
    }
}

static void UpdatePhoneFriend(const LlzInputState* input, float dt) {
    g_game.phoneCallTimer += dt;

    // 30 second call - user reads question to real friend
    // Press select or back to return to game, or auto-return when time runs out
    if (input->backReleased || input->selectPressed || g_game.phoneCallTimer >= PHONE_CALL_DURATION) {
        g_game.state = MIL_STATE_GAME_PLAYING;
    }
}

static void UpdateAudiencePoll(const LlzInputState* input, float dt) {
    g_game.stateTimer += dt;

    // Animate the bars filling up
    if (g_game.audiencePollProgress < 1.0f) {
        g_game.audiencePollProgress += dt / AUDIENCE_POLL_DURATION;
        if (g_game.audiencePollProgress > 1.0f) {
            g_game.audiencePollProgress = 1.0f;
        }
    }

    // Can dismiss after animation completes - select or back
    if (g_game.audiencePollProgress >= 1.0f && (input->backReleased || input->selectPressed)) {
        g_game.state = MIL_STATE_GAME_PLAYING;
    }
}

// Activate the pending lifeline and reset timer to 30 seconds
static void ActivateLifeline(int lifelineIdx) {
    if (lifelineIdx < 0 || lifelineIdx > 2) return;
    if (g_game.lifelinesUsed[lifelineIdx]) return;
    if (!g_game.currentQuestion) return;

    g_game.lifelinesUsed[lifelineIdx] = true;
    g_game.pendingLifeline = -1;

    // Reset question timer to 30 seconds after using lifeline
    g_game.questionTimer = 0;
    g_game.questionTimeLimit = LIFELINE_RESET_TIME;

    switch (lifelineIdx) {
        case 0:  // 50:50
            ApplyFiftyFifty(&g_lifelines, g_game.currentQuestion->correctIndex, 4, g_game.eliminated);
            g_game.state = MIL_STATE_LIFELINE_5050;
            g_game.stateTimer = 0;
            break;

        case 1:  // Phone a Friend
            g_game.phoneCallTimer = 0;  // Start 30-second phone call
            g_game.state = MIL_STATE_LIFELINE_PHONE;
            g_game.stateTimer = 0;
            break;

        case 2:  // Ask the Audience
            GetAudienceResults(&g_lifelines,
                g_game.currentQuestion->correctIndex,
                g_game.currentQuestion->difficulty,
                g_game.eliminated,
                g_game.audiencePercentages);
            g_game.audiencePollProgress = 0;  // Start bar animation
            g_game.state = MIL_STATE_LIFELINE_AUDIENCE;
            g_game.stateTimer = 0;
            break;
    }
}

// Request a lifeline - shows confirmation dialog
static void RequestLifeline(int lifelineIdx) {
    if (lifelineIdx < 0 || lifelineIdx > 2) return;
    if (g_game.lifelinesUsed[lifelineIdx]) return;

    g_game.pendingLifeline = lifelineIdx;

    // Confirmation timer = remaining question time
    float remaining = g_game.questionTimeLimit - g_game.questionTimer;
    if (remaining < 5.0f) remaining = 5.0f;  // Minimum 5 seconds to decide

    g_game.lifelineConfirmTimer = 0;
    g_game.lifelineConfirmTimeLimit = remaining;
    g_game.state = MIL_STATE_LIFELINE_CONFIRM;
}

static void UpdateLifelineConfirm(const LlzInputState* input, float dt) {
    g_game.lifelineConfirmTimer += dt;

    // Check for timer expiration - auto-confirm
    if (g_game.lifelineConfirmTimer >= g_game.lifelineConfirmTimeLimit) {
        ActivateLifeline(g_game.pendingLifeline);
        return;
    }

    // Check for tap on the pending lifeline icon to confirm
    if (input->tap) {
        int tappedLifeline = GetLifelineAtPoint(input->tapPosition);
        if (tappedLifeline == g_game.pendingLifeline) {
            ActivateLifeline(g_game.pendingLifeline);
            return;
        }
    }

    // Select button confirms
    if (input->selectPressed) {
        ActivateLifeline(g_game.pendingLifeline);
        return;
    }

    // Back button cancels - return to gameplay with adjusted timer
    if (input->backReleased) {
        // Time spent in confirmation counts against question time
        g_game.questionTimer += g_game.lifelineConfirmTimer;
        g_game.pendingLifeline = -1;
        g_game.state = MIL_STATE_GAME_PLAYING;
    }
}

static void UpdateAnswerLocked(const LlzInputState* input, float dt) {
    (void)input;
    g_game.stateTimer += dt;

    if (g_game.stateTimer >= 2.0f) {
        RevealAnswer();
    }
}

static void UpdateCorrectAnswer(const LlzInputState* input, float dt) {
    g_game.stateTimer += dt;

    // Auto-advance after delay, or press select to continue
    if (g_game.stateTimer >= 2.5f || input->selectPressed) {
        AdvanceToNextQuestion();
    }
}

static void UpdateWrongAnswer(const LlzInputState* input, float dt) {
    g_game.stateTimer += dt;

    // Press select to continue after brief delay
    if (g_game.stateTimer >= 1.0f && input->selectPressed) {
        g_game.state = MIL_STATE_FINAL_RESULTS;
    }
}

static void UpdateWalkawayConfirm(const LlzInputState* input) {
    // Select button confirms walkaway
    if (input->selectPressed) {
        WalkAway();
    }
    // Back button cancels
    if (input->backReleased) {
        g_game.state = MIL_STATE_GAME_PLAYING;
    }
}

static void UpdateFinalResults(const LlzInputState* input) {
    // Select to play again
    if (input->selectPressed) {
        g_game.state = MIL_STATE_TITLE_SCREEN;
    }
    // Back to exit plugin
    if (input->backReleased) {
        g_wantsClose = true;
    }
}

static void UpdateQuitConfirm(const LlzInputState* input) {
    // Select button confirms quit - forfeit game and return to title
    if (input->selectPressed) {
        QuitGame();
    }
    // Back button cancels - return to gameplay
    if (input->backReleased) {
        g_game.state = MIL_STATE_GAME_PLAYING;
    }
}

static void UpdateGameWon(const LlzInputState* input, float dt) {
    g_game.stateTimer += dt;

    // Press select to continue after celebration
    if (g_game.stateTimer >= 3.0f && input->selectPressed) {
        g_game.state = MIL_STATE_FINAL_RESULTS;
    }
}

// ============================================================================
// Config/Save Functions
// ============================================================================

static void LoadSavedStats(void) {
    // Define default config entries
    LlzPluginConfigEntry defaults[] = {
        {"games_played", "0"},
        {"total_winnings", "0"},
        {"high_score", "0"}
    };

    if (LlzPluginConfigInit(&g_config, "millionaire", defaults, 3)) {
        g_configLoaded = true;
        g_game.gamesPlayed = LlzPluginConfigGetInt(&g_config, "games_played", 0);
        g_game.totalWinnings = LlzPluginConfigGetInt(&g_config, "total_winnings", 0);
        g_game.highScore = LlzPluginConfigGetInt(&g_config, "high_score", 0);
        printf("Loaded Millionaire stats: %d games, $%d total, $%d high score\n",
               g_game.gamesPlayed, g_game.totalWinnings, g_game.highScore);
    } else {
        printf("Could not load Millionaire config, using defaults\n");
    }
}

static void SaveStats(void) {
    if (!g_configLoaded) return;

    LlzPluginConfigSetInt(&g_config, "games_played", g_game.gamesPlayed);
    LlzPluginConfigSetInt(&g_config, "total_winnings", g_game.totalWinnings);
    LlzPluginConfigSetInt(&g_config, "high_score", g_game.highScore);
    LlzPluginConfigSave(&g_config);
    printf("Saved Millionaire stats\n");
}

static void EndGame(int winnings) {
    g_game.gameInProgress = false;
    g_game.totalWinnings += winnings;
    g_game.gamesPlayed++;

    // Update high score if this game was better
    if (winnings > g_game.highScore) {
        g_game.highScore = winnings;
    }

    // Save stats
    SaveStats();
}

// ============================================================================
// Plugin API
// ============================================================================

static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;

    srand((unsigned int)time(NULL));

    // Initialize game state
    memset(&g_game, 0, sizeof(g_game));
    g_game.state = MIL_STATE_TITLE_SCREEN;
    g_game.selectedAnswer = -1;
    g_game.gameInProgress = false;
    g_game.pendingLifeline = -1;
    g_game.selectedLifelineIdx = -1;

    // Load saved stats from config
    LoadSavedStats();

    // Initialize lifelines
    LifelinesInit(&g_lifelines);

    // Load font for better text rendering
    LoadPluginFont();

    // Initialize particles
    InitParticles();

    // Load questions
    const char* questionPaths[] = {
        "plugins/millionaire/questions/all_questions.json",
        "./questions/all_questions.json",
        "questions/all_questions.json",
        "/tmp/millionaire/questions/all_questions.json"
    };

    for (int i = 0; i < 4; i++) {
        if (MlqLoadQuestionsFromJSON(questionPaths[i])) {
            g_game.questionsLoaded = true;
            printf("Loaded questions from: %s\n", questionPaths[i]);
            break;
        }
    }

    if (!g_game.questionsLoaded) {
        printf("WARNING: Could not load questions!\n");
    }

    printf("Millionaire plugin initialized (%dx%d)\n", width, height);
}

static void PluginUpdate(const LlzInputState* input, float deltaTime) {
    g_game.pulseTimer += deltaTime;
    g_game.animTimer += deltaTime;

    UpdateParticles(deltaTime);

    switch (g_game.state) {
        case MIL_STATE_TITLE_SCREEN:
            UpdateTitleScreen(input);
            break;
        case MIL_STATE_GAME_PLAYING:
            UpdateGamePlaying(input, deltaTime);
            break;
        case MIL_STATE_LIFELINE_5050:
            Update5050Lifeline(input, deltaTime);
            break;
        case MIL_STATE_LIFELINE_PHONE:
            UpdatePhoneFriend(input, deltaTime);
            break;
        case MIL_STATE_LIFELINE_AUDIENCE:
            UpdateAudiencePoll(input, deltaTime);
            break;
        case MIL_STATE_LIFELINE_CONFIRM:
            UpdateLifelineConfirm(input, deltaTime);
            break;
        case MIL_STATE_ANSWER_LOCKED:
            UpdateAnswerLocked(input, deltaTime);
            break;
        case MIL_STATE_CORRECT_ANSWER:
            UpdateCorrectAnswer(input, deltaTime);
            break;
        case MIL_STATE_WRONG_ANSWER:
            UpdateWrongAnswer(input, deltaTime);
            break;
        case MIL_STATE_GAME_WON:
            UpdateGameWon(input, deltaTime);
            break;
        case MIL_STATE_WALKAWAY_CONFIRM:
            UpdateWalkawayConfirm(input);
            break;
        case MIL_STATE_FINAL_RESULTS:
            UpdateFinalResults(input);
            break;
        case MIL_STATE_QUIT_CONFIRM:
            UpdateQuitConfirm(input);
            break;
        default:
            break;
    }
}

static void PluginDraw(void) {
    switch (g_game.state) {
        case MIL_STATE_TITLE_SCREEN:
            DrawTitleScreen();
            break;
        case MIL_STATE_GAME_PLAYING:
            DrawGameScreen();
            break;
        case MIL_STATE_LIFELINE_5050:
            Draw5050Overlay();
            break;
        case MIL_STATE_LIFELINE_PHONE:
            DrawPhoneFriendOverlay();
            break;
        case MIL_STATE_LIFELINE_AUDIENCE:
            DrawAudiencePollOverlay();
            break;
        case MIL_STATE_LIFELINE_CONFIRM:
            DrawLifelineConfirmOverlay();
            break;
        case MIL_STATE_ANSWER_LOCKED:
            DrawGameScreen();
            break;
        case MIL_STATE_CORRECT_ANSWER:
            DrawCorrectScreen();
            break;
        case MIL_STATE_WRONG_ANSWER:
            DrawWrongScreen();
            break;
        case MIL_STATE_GAME_WON:
            DrawWinScreen();
            break;
        case MIL_STATE_WALKAWAY_CONFIRM:
            DrawWalkawayConfirm();
            break;
        case MIL_STATE_FINAL_RESULTS:
            DrawFinalResults();
            break;
        case MIL_STATE_QUIT_CONFIRM:
            DrawQuitConfirm();
            break;
        default:
            DrawBackground();
            break;
    }
}

static void PluginShutdown(void) {
    // Save any pending stats and free config resources
    if (g_configLoaded) {
        SaveStats();
        LlzPluginConfigFree(&g_config);
        g_configLoaded = false;
    }

    UnloadPluginFont();
    MlqClearPool();
    printf("Millionaire plugin shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin Export
// ============================================================================

static LlzPluginAPI g_api = {
    .name = "Millionaire",
    .description = "Who Wants to Be a Millionaire trivia game",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .handles_back_button = true,
    .category = LLZ_CATEGORY_GAMES
};

const LlzPluginAPI* LlzGetPlugin(void) {
    return &g_api;
}
