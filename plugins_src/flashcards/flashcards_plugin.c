/**
 * Flashcards Plugin for llizardgui-host
 *
 * A multiple choice question tester with hierarchical folder navigation:
 * Category Selection -> Subfolder/File List -> Quiz Mode -> Results
 *
 * Questions are loaded from JSON files in the questions/ folder.
 * Supports nested subfolders for organization.
 * Tracks correct/incorrect statistics per question set.
 */

#include "llz_sdk.h"
#include "llizard_plugin.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

// ============================================================================
// Screen States
// ============================================================================

typedef enum {
    SCREEN_CATEGORY_SELECT,   // Root: Choose category folder
    SCREEN_SUBFOLDER_LIST,    // List subfolders and question files
    SCREEN_MODE_SELECT,       // Choose quiz mode (multiple choice or flashcard)
    SCREEN_QUIZ_MODE,         // Active quiz - showing multiple choice question
    SCREEN_ANSWER_FEEDBACK,   // Show correct/incorrect after answer (multiple choice)
    SCREEN_FLASHCARD_MODE,    // Flashcard mode - show question, flip to reveal
    SCREEN_FLASHCARD_RESULT,  // Flashcard mode - user marks correct/incorrect
    SCREEN_RESULTS,           // Show results after quiz completion
    SCREEN_STATS,             // View overall statistics
    SCREEN_MILLIONAIRE_MODE,  // Who Wants to Be a Millionaire mode
    SCREEN_MILLIONAIRE_GAMEOVER // Millionaire game over screen
} FlashcardScreen;

typedef enum {
    QUIZ_MODE_MULTIPLE_CHOICE,
    QUIZ_MODE_FLASHCARD
} QuizModeType;

// ============================================================================
// Data Structures
// ============================================================================

#define MAX_CATEGORIES 32
#define MAX_ITEMS_PER_FOLDER 64
#define MAX_QUESTIONS 500
#define MAX_OPTIONS 4
#define MAX_PATH_LEN 512
#define MAX_NAME_LEN 128
#define MAX_QUESTION_LEN 512
#define MAX_OPTION_LEN 256

// Folder/file entry for navigation
typedef struct {
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    bool isDirectory;
    int questionCount;  // Only valid for .json files
} FolderEntry;

// A single question
typedef struct {
    char question[MAX_QUESTION_LEN];
    char options[MAX_OPTIONS][MAX_OPTION_LEN];
    int correctIndex;  // Index of correct answer (0-3)
    int optionCount;
    char difficulty[32];  // Difficulty level for Millionaire mode
} Question;

// Statistics for a question set
typedef struct {
    char setName[MAX_NAME_LEN];
    int totalAttempts;
    int correctAnswers;
    int incorrectAnswers;
    time_t lastAttempted;
} QuestionSetStats;

// Current quiz state
typedef struct {
    Question questions[MAX_QUESTIONS];
    int questionCount;
    int currentQuestionIndex;
    int correctCount;
    int incorrectCount;
    int selectedOption;
    bool answered;
    bool wasCorrect;
    char setName[MAX_NAME_LEN];
    char setPath[MAX_PATH_LEN];
    int shuffledIndices[MAX_QUESTIONS];  // For randomizing question order
    QuizModeType mode;                   // Multiple choice or flashcard
    bool cardFlipped;                    // For flashcard mode - is answer visible?
    bool isFlipping;                     // Flip animation in progress
    float flipProgress;                  // 0.0 = question side, 1.0 = answer side
    bool isMillionaireMode;              // True if this question set supports millionaire mode
    bool isMillionaireEnabled;           // True if question set has millionaire_mode: true in JSON
    int currentPrizeLevel;               // Current prize level (0-14 for 15 questions)
    bool millionaireGameOver;            // True if player got a question wrong
    float celebrationTimer;              // Timer for celebration animation
} QuizState;

// ============================================================================
// Plugin State
// ============================================================================

static FlashcardScreen g_currentScreen = SCREEN_CATEGORY_SELECT;
static bool g_wantsClose = false;
static float g_highlightPulse = 0.0f;

// Navigation state
static FolderEntry g_categories[MAX_CATEGORIES];
static int g_categoryCount = 0;
static FolderEntry g_currentFolderItems[MAX_ITEMS_PER_FOLDER];
static int g_currentFolderItemCount = 0;
static char g_currentPath[MAX_PATH_LEN] = "";
static char g_currentCategoryName[MAX_NAME_LEN] = "";

// Navigation stack for deep folder navigation
#define MAX_NAV_DEPTH 8
static char g_navStack[MAX_NAV_DEPTH][MAX_PATH_LEN];
static char g_navNames[MAX_NAV_DEPTH][MAX_NAME_LEN];
static int g_navDepth = 0;

// Selection state
static int g_highlightedItem = 0;
static int g_listScrollOffset = 0;

// Quiz state
static QuizState g_quiz;

// Statistics
#define MAX_STATS 100
static QuestionSetStats g_stats[MAX_STATS];
static int g_statsCount = 0;

// Display constants
static const int SCREEN_WIDTH = 800;
static const int SCREEN_HEIGHT = 480;
static const int HEADER_HEIGHT = 80;
static const int ITEM_HEIGHT = 72;
static const int ITEM_SPACING = 8;
static const int ITEMS_PER_PAGE = 5;
static const int PADDING = 32;
static const int LIST_TOP = 100;

// Modern color palette (matching podcast plugin)
static const Color COLOR_BG_DARK = {18, 18, 22, 255};
static const Color COLOR_BG_GRADIENT = {28, 24, 38, 255};
static const Color COLOR_ACCENT = {138, 106, 210, 255};
static const Color COLOR_ACCENT_DIM = {90, 70, 140, 255};
static const Color COLOR_TEXT_PRIMARY = {245, 245, 250, 255};
static const Color COLOR_TEXT_SECONDARY = {160, 160, 175, 255};
static const Color COLOR_TEXT_DIM = {100, 100, 115, 255};
static const Color COLOR_CARD_BG = {32, 30, 42, 255};
static const Color COLOR_CARD_SELECTED = {48, 42, 68, 255};
static const Color COLOR_CARD_BORDER = {60, 55, 80, 255};
static const Color COLOR_CORRECT = {76, 175, 80, 255};
static const Color COLOR_INCORRECT = {244, 67, 54, 255};
static const Color COLOR_OPTION_BG = {38, 35, 52, 255};

// Font
static Font g_font;
static bool g_fontLoaded = false;

// Smooth scroll state
static float g_smoothScrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;

// Questions base path
static char g_questionsBasePath[MAX_PATH_LEN] = "";

// ============================================================================
// Forward Declarations
// ============================================================================

static void LoadCategories(void);
static void LoadFolderContents(const char *path);
static bool LoadQuestionsFromFile(const char *filepath);
static void ShuffleQuestions(void);
static void SaveStats(void);
static void LoadStats(void);
static void UpdateStatsForCurrentQuiz(void);

// ============================================================================
// Font Loading (uses SDK font functions)
// ============================================================================

static int *BuildUnicodeCodepoints(int *outCount) {
    static const int ranges[][2] = {
        {0x0020, 0x007E},   // ASCII
        {0x00A0, 0x00FF},   // Latin-1 Supplement
        {0x0100, 0x017F},   // Latin Extended-A
    };

    const int numRanges = sizeof(ranges) / sizeof(ranges[0]);
    int total = 0;
    for (int i = 0; i < numRanges; i++) {
        total += (ranges[i][1] - ranges[i][0] + 1);
    }

    int *codepoints = (int *)malloc(total * sizeof(int));
    if (!codepoints) {
        *outCount = 0;
        return NULL;
    }

    int idx = 0;
    for (int i = 0; i < numRanges; i++) {
        for (int cp = ranges[i][0]; cp <= ranges[i][1]; cp++) {
            codepoints[idx++] = cp;
        }
    }

    *outCount = total;
    return codepoints;
}

static void LoadPluginFont(void) {
    // Use SDK font loading with custom codepoints for extended Unicode support
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    g_font = LlzFontLoadCustom(LLZ_FONT_UI, 48, codepoints, codepointCount);
    if (g_font.texture.id != 0) {
        g_fontLoaded = true;
        SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
        printf("Flashcards: Loaded font via SDK\n");
    } else {
        g_font = GetFontDefault();
        g_fontLoaded = false;
        printf("Flashcards: Using default font\n");
    }

    if (codepoints) free(codepoints);
}

static void UnloadPluginFont(void) {
    // Font loaded via LlzFontLoadCustom must be unloaded by caller
    Font defaultFont = GetFontDefault();
    if (g_fontLoaded && g_font.texture.id != 0 && g_font.texture.id != defaultFont.texture.id) {
        UnloadFont(g_font);
    }
    g_fontLoaded = false;
}

// ============================================================================
// Smooth Scroll
// ============================================================================

static void UpdateSmoothScroll(float deltaTime) {
    float diff = g_targetScrollOffset - g_smoothScrollOffset;
    float speed = 12.0f;
    g_smoothScrollOffset += diff * speed * deltaTime;
    if (fabsf(diff) < 0.5f) {
        g_smoothScrollOffset = g_targetScrollOffset;
    }
}

static float CalculateTargetScroll(int selected, int totalItems, int visibleItems) {
    if (totalItems <= visibleItems) return 0.0f;

    float itemTotalHeight = ITEM_HEIGHT + ITEM_SPACING;
    float totalListHeight = totalItems * itemTotalHeight;
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 40;
    float maxScroll = totalListHeight - visibleArea;
    if (maxScroll < 0) maxScroll = 0;

    float selectedTop = selected * itemTotalHeight;
    float selectedBottom = selectedTop + ITEM_HEIGHT;

    float visibleTop = g_targetScrollOffset;
    float visibleBottom = g_targetScrollOffset + visibleArea;

    float topMargin = ITEM_HEIGHT * 0.5f;
    float bottomMargin = ITEM_HEIGHT * 1.2f;

    float newTarget = g_targetScrollOffset;

    if (selectedTop < visibleTop + topMargin) {
        newTarget = selectedTop - topMargin;
    } else if (selectedBottom > visibleBottom - bottomMargin) {
        newTarget = selectedBottom - visibleArea + bottomMargin;
    }

    if (newTarget < 0) newTarget = 0;
    if (newTarget > maxScroll) newTarget = maxScroll;

    return newTarget;
}

// ============================================================================
// JSON Parsing Helpers
// ============================================================================

static const char *skipWs(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char *parseString(const char *p, char *out, size_t maxLen) {
    if (*p != '"') return p;
    p++;

    size_t i = 0;
    while (*p != '"' && *p != '\0' && i < maxLen - 1) {
        if (*p == '\\' && *(p+1) != '\0') {
            p++;
            switch (*p) {
                case 'n': out[i++] = '\n'; break;
                case 'r': out[i++] = '\r'; break;
                case 't': out[i++] = '\t'; break;
                case '"': out[i++] = '"'; break;
                case '\\': out[i++] = '\\'; break;
                default: out[i++] = *p; break;
            }
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';

    if (*p == '"') p++;
    return p;
}

static const char *skipValue(const char *p) {
    p = skipWs(p);
    if (*p == '"') {
        p++;
        while (*p != '"' && *p != '\0') {
            if (*p == '\\' && *(p+1) != '\0') p++;
            p++;
        }
        if (*p == '"') p++;
    } else if (*p == '{') {
        int depth = 1;
        p++;
        while (depth > 0 && *p != '\0') {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            else if (*p == '"') {
                p++;
                while (*p != '"' && *p != '\0') {
                    if (*p == '\\' && *(p+1) != '\0') p++;
                    p++;
                }
            }
            p++;
        }
    } else if (*p == '[') {
        int depth = 1;
        p++;
        while (depth > 0 && *p != '\0') {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            else if (*p == '"') {
                p++;
                while (*p != '"' && *p != '\0') {
                    if (*p == '\\' && *(p+1) != '\0') p++;
                    p++;
                }
            }
            p++;
        }
    } else {
        while (*p != ',' && *p != '}' && *p != ']' && *p != '\0') p++;
    }
    return p;
}

// ============================================================================
// File System Helpers
// ============================================================================

static bool IsDirectory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

static bool LocalFileExists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static bool HasJsonExtension(const char *filename) {
    size_t len = strlen(filename);
    if (len < 5) return false;
    return strcasecmp(filename + len - 5, ".json") == 0;
}

// Count questions in a JSON file (quick scan)
static int CountQuestionsInFile(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {  // Max 1MB
        fclose(f);
        return 0;
    }

    char *content = (char *)malloc(size + 1);
    if (!content) {
        fclose(f);
        return 0;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    // Count "question": occurrences
    int count = 0;
    const char *p = content;
    while ((p = strstr(p, "\"question\"")) != NULL) {
        count++;
        p++;
    }

    free(content);
    return count;
}

// ============================================================================
// Questions Base Path Discovery
// ============================================================================

static void FindQuestionsBasePath(void) {
    const char *searchPaths[] = {
#ifdef PLATFORM_DRM
        "/var/local/flashcards/questions",
        "/tmp/flashcards/questions",
#endif
        "plugins/flashcards/questions",
        "./flashcards/questions",
        "flashcards/questions",
        "../flashcards/questions",
        "questions"
    };

    for (int i = 0; i < (int)(sizeof(searchPaths)/sizeof(searchPaths[0])); i++) {
        if (IsDirectory(searchPaths[i])) {
            strncpy(g_questionsBasePath, searchPaths[i], MAX_PATH_LEN - 1);
            g_questionsBasePath[MAX_PATH_LEN - 1] = '\0';
            printf("Flashcards: Found questions folder at: %s\n", g_questionsBasePath);
            return;
        }
    }

    // Default to plugins/flashcards/questions if none found
    strncpy(g_questionsBasePath, "plugins/flashcards/questions", MAX_PATH_LEN - 1);
    printf("Flashcards: Using default questions path: %s\n", g_questionsBasePath);
}

// ============================================================================
// Category/Folder Loading
// ============================================================================

static int CompareEntries(const void *a, const void *b) {
    const FolderEntry *ea = (const FolderEntry *)a;
    const FolderEntry *eb = (const FolderEntry *)b;

    // Directories first
    if (ea->isDirectory && !eb->isDirectory) return -1;
    if (!ea->isDirectory && eb->isDirectory) return 1;

    // Then alphabetically
    return strcasecmp(ea->name, eb->name);
}

static void LoadCategories(void) {
    g_categoryCount = 0;

    DIR *dir = opendir(g_questionsBasePath);
    if (!dir) {
        printf("Flashcards: Cannot open questions directory: %s\n", g_questionsBasePath);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && g_categoryCount < MAX_CATEGORIES) {
        // Skip hidden files and . / ..
        if (entry->d_name[0] == '.') continue;

        char fullPath[MAX_PATH_LEN];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", g_questionsBasePath, entry->d_name);

        FolderEntry *cat = &g_categories[g_categoryCount];
        strncpy(cat->name, entry->d_name, MAX_NAME_LEN - 1);
        cat->name[MAX_NAME_LEN - 1] = '\0';
        strncpy(cat->path, fullPath, MAX_PATH_LEN - 1);
        cat->path[MAX_PATH_LEN - 1] = '\0';
        cat->isDirectory = IsDirectory(fullPath);
        cat->questionCount = 0;

        // For JSON files at root level, count questions
        if (!cat->isDirectory && HasJsonExtension(entry->d_name)) {
            cat->questionCount = CountQuestionsInFile(fullPath);
        }

        g_categoryCount++;
    }

    closedir(dir);

    // Sort entries
    qsort(g_categories, g_categoryCount, sizeof(FolderEntry), CompareEntries);

    printf("Flashcards: Loaded %d categories from %s\n", g_categoryCount, g_questionsBasePath);
}

static void LoadFolderContents(const char *path) {
    g_currentFolderItemCount = 0;
    strncpy(g_currentPath, path, MAX_PATH_LEN - 1);
    g_currentPath[MAX_PATH_LEN - 1] = '\0';

    DIR *dir = opendir(path);
    if (!dir) {
        printf("Flashcards: Cannot open directory: %s\n", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && g_currentFolderItemCount < MAX_ITEMS_PER_FOLDER) {
        // Skip hidden files and . / ..
        if (entry->d_name[0] == '.') continue;

        char fullPath[MAX_PATH_LEN];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        FolderEntry *item = &g_currentFolderItems[g_currentFolderItemCount];
        strncpy(item->name, entry->d_name, MAX_NAME_LEN - 1);
        item->name[MAX_NAME_LEN - 1] = '\0';
        strncpy(item->path, fullPath, MAX_PATH_LEN - 1);
        item->path[MAX_PATH_LEN - 1] = '\0';
        item->isDirectory = IsDirectory(fullPath);
        item->questionCount = 0;

        // For JSON files, count questions
        if (!item->isDirectory && HasJsonExtension(entry->d_name)) {
            item->questionCount = CountQuestionsInFile(fullPath);
        } else if (!item->isDirectory) {
            // Skip non-JSON files
            continue;
        }

        g_currentFolderItemCount++;
    }

    closedir(dir);

    // Sort entries
    qsort(g_currentFolderItems, g_currentFolderItemCount, sizeof(FolderEntry), CompareEntries);

    printf("Flashcards: Loaded %d items from %s\n", g_currentFolderItemCount, path);
}

// ============================================================================
// Question Loading
// ============================================================================

static bool LoadQuestionsFromFile(const char *filepath) {
    memset(&g_quiz, 0, sizeof(g_quiz));

    FILE *f = fopen(filepath, "r");
    if (!f) {
        printf("Flashcards: Cannot open file: %s\n", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 2 * 1024 * 1024) {  // Max 2MB
        fclose(f);
        return false;
    }

    char *content = (char *)malloc(size + 1);
    if (!content) {
        fclose(f);
        return false;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    // Check for "millionaire_mode": true at root level
    g_quiz.isMillionaireEnabled = false;
    const char *millionaireCheck = strstr(content, "\"millionaire_mode\"");
    if (millionaireCheck) {
        const char *valueStart = strchr(millionaireCheck, ':');
        if (valueStart) {
            valueStart = skipWs(valueStart + 1);
            if (strncmp(valueStart, "true", 4) == 0) {
                g_quiz.isMillionaireEnabled = true;
                printf("Flashcards: Millionaire mode enabled for this question set\n");
            }
        }
    }

    // Find "questions" array
    const char *p = strstr(content, "\"questions\"");
    if (!p) {
        free(content);
        return false;
    }

    p = strchr(p, '[');
    if (!p) {
        free(content);
        return false;
    }
    p++;

    // Parse questions
    while (g_quiz.questionCount < MAX_QUESTIONS && *p != '\0') {
        p = skipWs(p);
        if (*p == ']') break;
        if (*p != '{') break;
        p++;

        Question *q = &g_quiz.questions[g_quiz.questionCount];
        memset(q, 0, sizeof(Question));
        char answerText[MAX_OPTION_LEN] = "";

        while (*p != '\0' && *p != '}') {
            p = skipWs(p);
            if (*p == '}') break;
            if (*p == ',') { p++; continue; }
            if (*p != '"') break;

            p++;
            const char *fieldStart = p;
            while (*p != '"' && *p != '\0') p++;
            size_t fieldLen = p - fieldStart;
            if (*p == '"') p++;

            p = skipWs(p);
            if (*p == ':') p++;
            p = skipWs(p);

            if (fieldLen == 8 && strncmp(fieldStart, "question", 8) == 0) {
                p = parseString(p, q->question, sizeof(q->question));
            } else if (fieldLen == 7 && strncmp(fieldStart, "options", 7) == 0) {
                // Parse options array
                if (*p != '[') {
                    p = skipValue(p);
                    continue;
                }
                p++;

                while (q->optionCount < MAX_OPTIONS && *p != '\0') {
                    p = skipWs(p);
                    if (*p == ']') break;
                    if (*p == ',') { p++; continue; }

                    p = parseString(p, q->options[q->optionCount], MAX_OPTION_LEN);
                    q->optionCount++;
                }

                while (*p != '\0' && *p != ']') p++;
                if (*p == ']') p++;
            } else if (fieldLen == 6 && strncmp(fieldStart, "answer", 6) == 0) {
                p = parseString(p, answerText, sizeof(answerText));
            } else if (fieldLen == 10 && strncmp(fieldStart, "difficulty", 10) == 0) {
                p = parseString(p, q->difficulty, sizeof(q->difficulty));
            } else {
                p = skipValue(p);
            }
        }

        // Find correct answer index
        q->correctIndex = -1;
        for (int i = 0; i < q->optionCount; i++) {
            if (strcmp(q->options[i], answerText) == 0) {
                q->correctIndex = i;
                break;
            }
        }

        // Only add valid questions (has question text, options, and valid answer)
        if (q->question[0] != '\0' && q->optionCount >= 2 && q->correctIndex >= 0) {
            g_quiz.questionCount++;
        }

        if (*p == '}') p++;
        p = skipWs(p);
        if (*p == ',') p++;
    }

    free(content);

    // Store set info
    strncpy(g_quiz.setPath, filepath, MAX_PATH_LEN - 1);

    // Extract set name from filename
    const char *lastSlash = strrchr(filepath, '/');
    const char *filename = lastSlash ? lastSlash + 1 : filepath;
    strncpy(g_quiz.setName, filename, MAX_NAME_LEN - 1);

    // Remove .json extension from name
    char *ext = strstr(g_quiz.setName, ".json");
    if (ext) *ext = '\0';

    printf("Flashcards: Loaded %d questions from %s\n", g_quiz.questionCount, filepath);
    return g_quiz.questionCount > 0;
}

static void ShuffleQuestions(void) {
    // Initialize indices
    for (int i = 0; i < g_quiz.questionCount; i++) {
        g_quiz.shuffledIndices[i] = i;
    }

    // Fisher-Yates shuffle
    srand((unsigned int)time(NULL));
    for (int i = g_quiz.questionCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = g_quiz.shuffledIndices[i];
        g_quiz.shuffledIndices[i] = g_quiz.shuffledIndices[j];
        g_quiz.shuffledIndices[j] = temp;
    }
}

// ============================================================================
// Statistics
// ============================================================================

static const char *GetStatsFilePath(void) {
    static char path[MAX_PATH_LEN];
#ifdef PLATFORM_DRM
    snprintf(path, sizeof(path), "/var/local/flashcards/stats.dat");
#else
    snprintf(path, sizeof(path), "plugins/flashcards/stats.dat");
#endif
    return path;
}

static void LoadStats(void) {
    g_statsCount = 0;

    FILE *f = fopen(GetStatsFilePath(), "rb");
    if (!f) return;

    // Read count
    if (fread(&g_statsCount, sizeof(int), 1, f) != 1) {
        g_statsCount = 0;
        fclose(f);
        return;
    }

    if (g_statsCount > MAX_STATS) g_statsCount = MAX_STATS;

    // Read stats entries
    fread(g_stats, sizeof(QuestionSetStats), g_statsCount, f);
    fclose(f);

    printf("Flashcards: Loaded stats for %d question sets\n", g_statsCount);
}

static void SaveStats(void) {
    // Ensure directory exists
#ifdef PLATFORM_DRM
    mkdir("/var/local/flashcards", 0755);
#else
    mkdir("plugins/flashcards", 0755);
#endif

    FILE *f = fopen(GetStatsFilePath(), "wb");
    if (!f) {
        printf("Flashcards: Cannot save stats to %s\n", GetStatsFilePath());
        return;
    }

    fwrite(&g_statsCount, sizeof(int), 1, f);
    fwrite(g_stats, sizeof(QuestionSetStats), g_statsCount, f);
    fclose(f);

    printf("Flashcards: Saved stats for %d question sets\n", g_statsCount);
}

static QuestionSetStats *FindOrCreateStats(const char *setName) {
    // Find existing
    for (int i = 0; i < g_statsCount; i++) {
        if (strcmp(g_stats[i].setName, setName) == 0) {
            return &g_stats[i];
        }
    }

    // Create new if space available
    if (g_statsCount < MAX_STATS) {
        QuestionSetStats *stats = &g_stats[g_statsCount++];
        memset(stats, 0, sizeof(QuestionSetStats));
        strncpy(stats->setName, setName, MAX_NAME_LEN - 1);
        return stats;
    }

    return NULL;
}

static void UpdateStatsForCurrentQuiz(void) {
    QuestionSetStats *stats = FindOrCreateStats(g_quiz.setName);
    if (stats) {
        stats->totalAttempts++;
        stats->correctAnswers += g_quiz.correctCount;
        stats->incorrectAnswers += g_quiz.incorrectCount;
        stats->lastAttempted = time(NULL);
        SaveStats();
    }
}

// ============================================================================
// Drawing Helpers
// ============================================================================

static void DrawBackground(void) {
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG_DARK, COLOR_BG_GRADIENT);

    for (int i = 0; i < 3; i++) {
        float alpha = 0.03f - i * 0.01f;
        Color glow = ColorAlpha(COLOR_ACCENT, alpha);
        DrawCircleGradient(SCREEN_WIDTH / 2, -100 + i * 50, 400 - i * 80, glow, ColorAlpha(glow, 0.0f));
    }
}

static void DrawHeader(const char *title, bool showBack) {
    float fontSize = 32;
    float textX = PADDING;

    if (showBack) {
        DrawTextEx(g_font, "<", (Vector2){textX, 24}, 28, 1, COLOR_ACCENT);
        textX += 36;
    }

    DrawTextEx(g_font, title, (Vector2){textX, 24}, fontSize, 2, COLOR_TEXT_PRIMARY);
    DrawRectangle(PADDING, 66, 160, 3, COLOR_ACCENT);

    const char *instructions = showBack ? "back to return" : "scroll to navigate";
    DrawTextEx(g_font, instructions, (Vector2){PADDING, 76}, 14, 1, COLOR_TEXT_DIM);
}

static void DrawListItem(Rectangle bounds, const char *title, const char *subtitle,
                         bool isHighlighted, bool isFolder) {
    Color cardBg = isHighlighted ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
    Color borderColor = isHighlighted ? COLOR_ACCENT : COLOR_CARD_BORDER;

    DrawRectangleRounded(bounds, 0.15f, 8, cardBg);

    if (isHighlighted) {
        Rectangle accentBar = {bounds.x, bounds.y + 8, 4, bounds.height - 16};
        DrawRectangleRounded(accentBar, 0.5f, 4, COLOR_ACCENT);
    }

    DrawRectangleRoundedLines(bounds, 0.15f, 8, ColorAlpha(borderColor, isHighlighted ? 0.6f : 0.2f));

    float textX = bounds.x + 20;
    float titleY = bounds.y + 16;
    float subtitleY = bounds.y + 44;

    // Folder icon
    if (isFolder) {
        DrawTextEx(g_font, "[+]", (Vector2){textX, titleY}, 18, 1, COLOR_ACCENT_DIM);
        textX += 40;
    }

    Color titleColor = isHighlighted ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
    DrawTextEx(g_font, title, (Vector2){textX, titleY}, 22, 1.5f, titleColor);

    if (subtitle) {
        Color subColor = isHighlighted ? COLOR_TEXT_SECONDARY : COLOR_TEXT_DIM;
        DrawTextEx(g_font, subtitle, (Vector2){textX, subtitleY}, 15, 1, subColor);
    }

    if (isHighlighted) {
        DrawTextEx(g_font, ">", (Vector2){bounds.x + bounds.width - 30, bounds.y + (bounds.height - 20) / 2}, 20, 1, COLOR_ACCENT_DIM);
    }
}

static void DrawScrollIndicator(int currentOffset, int totalItems, int visibleItems) {
    if (totalItems <= visibleItems) return;

    int scrollAreaHeight = SCREEN_HEIGHT - LIST_TOP - 40;
    float scrollRatio = (float)currentOffset / (float)(totalItems - visibleItems);
    float handleHeight = (float)visibleItems / (float)totalItems * scrollAreaHeight;
    if (handleHeight < 40) handleHeight = 40;

    int handleY = LIST_TOP + (int)(scrollRatio * (scrollAreaHeight - handleHeight));

    Rectangle track = {SCREEN_WIDTH - 10, LIST_TOP, 4, scrollAreaHeight};
    DrawRectangleRounded(track, 0.5f, 4, ColorAlpha(COLOR_CARD_BORDER, 0.3f));

    Rectangle handle = {SCREEN_WIDTH - 10, handleY, 4, handleHeight};
    DrawRectangleRounded(handle, 0.5f, 4, COLOR_ACCENT_DIM);
}

static void DrawScrollFades(bool canScrollUp, bool canScrollDown) {
    if (canScrollUp) {
        for (int i = 0; i < 30; i++) {
            float alpha = (30 - i) / 30.0f * 0.8f;
            Color fade = ColorAlpha(COLOR_BG_DARK, alpha);
            DrawRectangle(0, LIST_TOP + i, SCREEN_WIDTH - 16, 1, fade);
        }
        DrawTextEx(g_font, "^", (Vector2){SCREEN_WIDTH / 2 - 6, LIST_TOP + 4}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.6f));
    }

    if (canScrollDown) {
        int bottomY = SCREEN_HEIGHT - 40;
        for (int i = 0; i < 30; i++) {
            float alpha = i / 30.0f * 0.8f;
            Color fade = ColorAlpha(COLOR_BG_DARK, alpha);
            DrawRectangle(0, bottomY - 30 + i, SCREEN_WIDTH - 16, 1, fade);
        }
        DrawTextEx(g_font, "v", (Vector2){SCREEN_WIDTH / 2 - 6, bottomY - 18}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.6f));
    }
}

// ============================================================================
// Text Wrapping Helper
// ============================================================================

static void DrawTextWrapped(const char *text, float x, float y, float maxWidth, float fontSize, float spacing, Color color) {
    char line[512];
    const char *p = text;
    float lineY = y;
    float lineHeight = fontSize * 1.3f;

    while (*p != '\0') {
        // Build line word by word
        int lineLen = 0;
        line[0] = '\0';

        while (*p != '\0' && *p != '\n') {
            // Find next word
            const char *wordStart = p;
            while (*p != '\0' && *p != ' ' && *p != '\n') p++;
            int wordLen = p - wordStart;

            // Try adding word to line
            char testLine[512];
            if (lineLen > 0) {
                snprintf(testLine, sizeof(testLine), "%s %.*s", line, wordLen, wordStart);
            } else {
                snprintf(testLine, sizeof(testLine), "%.*s", wordLen, wordStart);
            }

            Vector2 testSize = MeasureTextEx(g_font, testLine, fontSize, spacing);
            if (testSize.x > maxWidth && lineLen > 0) {
                // Line full, draw it and start new line with this word
                DrawTextEx(g_font, line, (Vector2){x, lineY}, fontSize, spacing, color);
                lineY += lineHeight;
                snprintf(line, sizeof(line), "%.*s", wordLen, wordStart);
                lineLen = wordLen;
            } else {
                strcpy(line, testLine);
                lineLen = strlen(line);
            }

            // Skip spaces
            while (*p == ' ') p++;
        }

        // Draw remaining line
        if (lineLen > 0) {
            DrawTextEx(g_font, line, (Vector2){x, lineY}, fontSize, spacing, color);
            lineY += lineHeight;
        }

        // Skip newlines
        while (*p == '\n') {
            p++;
            lineY += lineHeight * 0.5f;  // Extra space for explicit newlines
        }
    }
}

// ============================================================================
// Screen: Category Select
// ============================================================================

static void DrawCategorySelectScreen(void) {
    DrawBackground();
    DrawHeader("Flashcards", false);

    if (g_categoryCount == 0) {
        Rectangle bounds = {PADDING, LIST_TOP + 10, SCREEN_WIDTH - (PADDING * 2) - 16, ITEM_HEIGHT};
        DrawListItem(bounds, "No categories found", "Add folders to questions/", true, false);

        char pathMsg[256];
        snprintf(pathMsg, sizeof(pathMsg), "Looking in: %s", g_questionsBasePath);
        DrawTextEx(g_font, pathMsg, (Vector2){PADDING, SCREEN_HEIGHT - 32}, 14, 1, COLOR_TEXT_DIM);
        return;
    }

    float itemTotalHeight = ITEM_HEIGHT + ITEM_SPACING;
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 40;
    float totalListHeight = g_categoryCount * itemTotalHeight;
    float maxScroll = totalListHeight - visibleArea;
    if (maxScroll < 0) maxScroll = 0;

    bool canScrollUp = g_smoothScrollOffset > 1.0f;
    bool canScrollDown = g_smoothScrollOffset < maxScroll - 1.0f;

    BeginScissorMode(0, LIST_TOP, SCREEN_WIDTH, (int)visibleArea);

    for (int i = 0; i < g_categoryCount; i++) {
        float itemY = LIST_TOP + i * itemTotalHeight - g_smoothScrollOffset;
        if (itemY < LIST_TOP - ITEM_HEIGHT || itemY > SCREEN_HEIGHT) continue;

        const FolderEntry *cat = &g_categories[i];

        Rectangle bounds = {PADDING, itemY, SCREEN_WIDTH - (PADDING * 2) - 16, ITEM_HEIGHT};
        bool isHighlighted = (g_highlightedItem == i);

        char subtitle[80];
        if (cat->isDirectory) {
            snprintf(subtitle, sizeof(subtitle), "Folder");
        } else {
            snprintf(subtitle, sizeof(subtitle), "%d questions", cat->questionCount);
        }

        DrawListItem(bounds, cat->name, subtitle, isHighlighted, cat->isDirectory);
    }

    EndScissorMode();

    DrawScrollFades(canScrollUp, canScrollDown);
    DrawScrollIndicator(g_listScrollOffset, g_categoryCount, ITEMS_PER_PAGE);

    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d of %d", g_highlightedItem + 1, g_categoryCount);
    Vector2 counterSize = MeasureTextEx(g_font, counterStr, 16, 1);
    DrawTextEx(g_font, counterStr, (Vector2){SCREEN_WIDTH - counterSize.x - PADDING, SCREEN_HEIGHT - 28}, 16, 1, COLOR_TEXT_DIM);
}

static void UpdateCategorySelectScreen(const LlzInputState *input) {
    if (g_categoryCount == 0) return;

    int delta = 0;
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;
    if (input->scrollDelta != 0) delta = (input->scrollDelta > 0) ? 1 : -1;

    if (delta != 0) {
        g_highlightedItem += delta;
        if (g_highlightedItem < 0) g_highlightedItem = 0;
        if (g_highlightedItem >= g_categoryCount) g_highlightedItem = g_categoryCount - 1;
    }

    if (input->selectPressed) {
        const FolderEntry *cat = &g_categories[g_highlightedItem];

        if (cat->isDirectory) {
            // Navigate into folder
            strncpy(g_navStack[g_navDepth], cat->path, MAX_PATH_LEN - 1);
            strncpy(g_navNames[g_navDepth], cat->name, MAX_NAME_LEN - 1);
            g_navDepth++;
            strncpy(g_currentCategoryName, cat->name, MAX_NAME_LEN - 1);

            LoadFolderContents(cat->path);
            g_highlightedItem = 0;
            g_listScrollOffset = 0;
            g_smoothScrollOffset = 0.0f;
            g_targetScrollOffset = 0.0f;

            g_currentScreen = SCREEN_SUBFOLDER_LIST;
        } else if (HasJsonExtension(cat->name)) {
            // Load questions and go to mode selection
            if (LoadQuestionsFromFile(cat->path)) {
                ShuffleQuestions();
                g_quiz.currentQuestionIndex = 0;
                g_quiz.correctCount = 0;
                g_quiz.incorrectCount = 0;
                g_quiz.selectedOption = 0;
                g_quiz.answered = false;
                g_quiz.cardFlipped = false;
                g_highlightedItem = 0;
                g_currentScreen = SCREEN_MODE_SELECT;
            }
        }
    }
}

// ============================================================================
// Screen: Subfolder List
// ============================================================================

static void DrawSubfolderListScreen(void) {
    DrawBackground();
    DrawHeader(g_currentCategoryName, true);

    if (g_currentFolderItemCount == 0) {
        Rectangle bounds = {PADDING, LIST_TOP + 10, SCREEN_WIDTH - (PADDING * 2) - 16, ITEM_HEIGHT};
        DrawListItem(bounds, "Empty folder", "No question files found", true, false);
        return;
    }

    float itemTotalHeight = ITEM_HEIGHT + ITEM_SPACING;
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 40;
    float totalListHeight = g_currentFolderItemCount * itemTotalHeight;
    float maxScroll = totalListHeight - visibleArea;
    if (maxScroll < 0) maxScroll = 0;

    bool canScrollUp = g_smoothScrollOffset > 1.0f;
    bool canScrollDown = g_smoothScrollOffset < maxScroll - 1.0f;

    BeginScissorMode(0, LIST_TOP, SCREEN_WIDTH, (int)visibleArea);

    for (int i = 0; i < g_currentFolderItemCount; i++) {
        float itemY = LIST_TOP + i * itemTotalHeight - g_smoothScrollOffset;
        if (itemY < LIST_TOP - ITEM_HEIGHT || itemY > SCREEN_HEIGHT) continue;

        const FolderEntry *item = &g_currentFolderItems[i];

        Rectangle bounds = {PADDING, itemY, SCREEN_WIDTH - (PADDING * 2) - 16, ITEM_HEIGHT};
        bool isHighlighted = (g_highlightedItem == i);

        char subtitle[80];
        if (item->isDirectory) {
            snprintf(subtitle, sizeof(subtitle), "Subfolder");
        } else {
            snprintf(subtitle, sizeof(subtitle), "%d questions", item->questionCount);
        }

        DrawListItem(bounds, item->name, subtitle, isHighlighted, item->isDirectory);
    }

    EndScissorMode();

    DrawScrollFades(canScrollUp, canScrollDown);
    DrawScrollIndicator(g_listScrollOffset, g_currentFolderItemCount, ITEMS_PER_PAGE);

    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d of %d", g_highlightedItem + 1, g_currentFolderItemCount);
    Vector2 counterSize = MeasureTextEx(g_font, counterStr, 16, 1);
    DrawTextEx(g_font, counterStr, (Vector2){SCREEN_WIDTH - counterSize.x - PADDING, SCREEN_HEIGHT - 28}, 16, 1, COLOR_TEXT_DIM);
}

static void UpdateSubfolderListScreen(const LlzInputState *input) {
    if (g_currentFolderItemCount == 0) return;

    int delta = 0;
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;
    if (input->scrollDelta != 0) delta = (input->scrollDelta > 0) ? 1 : -1;

    if (delta != 0) {
        g_highlightedItem += delta;
        if (g_highlightedItem < 0) g_highlightedItem = 0;
        if (g_highlightedItem >= g_currentFolderItemCount) g_highlightedItem = g_currentFolderItemCount - 1;
    }

    if (input->selectPressed) {
        const FolderEntry *item = &g_currentFolderItems[g_highlightedItem];

        if (item->isDirectory) {
            // Navigate into subfolder
            if (g_navDepth < MAX_NAV_DEPTH) {
                strncpy(g_navStack[g_navDepth], item->path, MAX_PATH_LEN - 1);
                strncpy(g_navNames[g_navDepth], item->name, MAX_NAME_LEN - 1);
                g_navDepth++;
                strncpy(g_currentCategoryName, item->name, MAX_NAME_LEN - 1);

                LoadFolderContents(item->path);
                g_highlightedItem = 0;
                g_listScrollOffset = 0;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;
            }
        } else if (HasJsonExtension(item->name)) {
            // Load questions and go to mode selection
            if (LoadQuestionsFromFile(item->path)) {
                ShuffleQuestions();
                g_quiz.currentQuestionIndex = 0;
                g_quiz.correctCount = 0;
                g_quiz.incorrectCount = 0;
                g_quiz.selectedOption = 0;
                g_quiz.answered = false;
                g_quiz.cardFlipped = false;
                g_highlightedItem = 0;
                g_currentScreen = SCREEN_MODE_SELECT;
            }
        }
    }
}

// ============================================================================
// Screen: Mode Select
// ============================================================================

static void DrawModeSelectScreen(void) {
    DrawBackground();

    // Header
    DrawHeader(g_quiz.setName, true);

    // Title
    const char *title = "Choose Study Mode";
    Vector2 titleSize = MeasureTextEx(g_font, title, 28, 1);
    DrawTextEx(g_font, title, (Vector2){(SCREEN_WIDTH - titleSize.x) / 2, 100}, 28, 1, COLOR_TEXT_PRIMARY);

    // Question count info
    char infoText[64];
    snprintf(infoText, sizeof(infoText), "%d questions available", g_quiz.questionCount);
    Vector2 infoSize = MeasureTextEx(g_font, infoText, 18, 1);
    DrawTextEx(g_font, infoText, (Vector2){(SCREEN_WIDTH - infoSize.x) / 2, 135}, 18, 1, COLOR_TEXT_SECONDARY);

    // Mode options
    float optionY = 180;
    float optionHeight = 100;
    float optionSpacing = 20;
    float optionWidth = SCREEN_WIDTH - PADDING * 2;

    // Multiple Choice option
    Rectangle mcBounds = {PADDING, optionY, optionWidth, optionHeight};
    bool mcSelected = (g_highlightedItem == 0);
    Color mcBg = mcSelected ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
    Color mcBorder = mcSelected ? COLOR_ACCENT : COLOR_CARD_BORDER;

    DrawRectangleRounded(mcBounds, 0.1f, 8, mcBg);
    DrawRectangleRoundedLines(mcBounds, 0.1f, 8, ColorAlpha(mcBorder, mcSelected ? 0.8f : 0.3f));

    if (mcSelected) {
        Rectangle selectBar = {mcBounds.x, mcBounds.y + 12, 4, mcBounds.height - 24};
        DrawRectangleRounded(selectBar, 0.5f, 4, COLOR_ACCENT);
    }

    DrawTextEx(g_font, "Multiple Choice", (Vector2){mcBounds.x + 24, mcBounds.y + 20}, 26, 1,
               mcSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY);
    DrawTextEx(g_font, "Answer questions by selecting from 4 options", (Vector2){mcBounds.x + 24, mcBounds.y + 55}, 18, 1, COLOR_TEXT_DIM);

    // Flashcard option
    optionY += optionHeight + optionSpacing;
    Rectangle fcBounds = {PADDING, optionY, optionWidth, optionHeight};
    bool fcSelected = (g_highlightedItem == 1);
    Color fcBg = fcSelected ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
    Color fcBorder = fcSelected ? COLOR_ACCENT : COLOR_CARD_BORDER;

    DrawRectangleRounded(fcBounds, 0.1f, 8, fcBg);
    DrawRectangleRoundedLines(fcBounds, 0.1f, 8, ColorAlpha(fcBorder, fcSelected ? 0.8f : 0.3f));

    if (fcSelected) {
        Rectangle selectBar = {fcBounds.x, fcBounds.y + 12, 4, fcBounds.height - 24};
        DrawRectangleRounded(selectBar, 0.5f, 4, COLOR_ACCENT);
    }

    DrawTextEx(g_font, "Flashcard Flip", (Vector2){fcBounds.x + 24, fcBounds.y + 20}, 26, 1,
               fcSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY);
    DrawTextEx(g_font, "See question, flip to reveal answer, self-grade", (Vector2){fcBounds.x + 24, fcBounds.y + 55}, 18, 1, COLOR_TEXT_DIM);

    // Millionaire mode option (only if enabled for this question set)
    if (g_quiz.isMillionaireEnabled) {
        optionY += optionHeight + optionSpacing;
        Rectangle mlBounds = {PADDING, optionY, optionWidth, optionHeight};
        bool mlSelected = (g_highlightedItem == 2);

        // Gold/premium colors for Millionaire mode
        Color mlBg = mlSelected ? (Color){60, 50, 20, 255} : (Color){35, 30, 15, 255};
        Color mlBorder = mlSelected ? (Color){255, 215, 0, 255} : (Color){180, 150, 50, 255};

        DrawRectangleRounded(mlBounds, 0.1f, 8, mlBg);
        DrawRectangleRoundedLines(mlBounds, 0.1f, 8, ColorAlpha(mlBorder, mlSelected ? 0.9f : 0.5f));

        if (mlSelected) {
            Rectangle selectBar = {mlBounds.x, mlBounds.y + 12, 4, mlBounds.height - 24};
            DrawRectangleRounded(selectBar, 0.5f, 4, (Color){255, 215, 0, 255});
        }

        Color goldText = (Color){255, 215, 0, 255};
        DrawTextEx(g_font, "Millionaire Mode", (Vector2){mlBounds.x + 24, mlBounds.y + 20}, 26, 1,
                   mlSelected ? goldText : (Color){200, 170, 50, 255});
        DrawTextEx(g_font, "Play for virtual millions! One wrong = game over", (Vector2){mlBounds.x + 24, mlBounds.y + 55}, 18, 1,
                   mlSelected ? COLOR_TEXT_SECONDARY : COLOR_TEXT_DIM);
    }

    // Instructions
    DrawTextEx(g_font, "Scroll to select, press to start",
               (Vector2){PADDING, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
}

static void UpdateModeSelectScreen(const LlzInputState *input) {
    int maxOption = g_quiz.isMillionaireEnabled ? 2 : 1;

    int delta = 0;
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;
    if (input->scrollDelta != 0) delta = (input->scrollDelta > 0) ? 1 : -1;

    if (delta != 0) {
        g_highlightedItem += delta;
        if (g_highlightedItem < 0) g_highlightedItem = 0;
        if (g_highlightedItem > maxOption) g_highlightedItem = maxOption;
    }

    if (input->selectPressed) {
        if (g_highlightedItem == 0) {
            // Multiple choice mode
            g_quiz.mode = QUIZ_MODE_MULTIPLE_CHOICE;
            g_quiz.selectedOption = 0;
            g_currentScreen = SCREEN_QUIZ_MODE;
        } else if (g_highlightedItem == 1) {
            // Flashcard mode
            g_quiz.mode = QUIZ_MODE_FLASHCARD;
            g_quiz.cardFlipped = false;
            g_currentScreen = SCREEN_FLASHCARD_MODE;
        } else if (g_highlightedItem == 2 && g_quiz.isMillionaireEnabled) {
            // Millionaire mode - don't shuffle, questions are in order
            g_quiz.isMillionaireMode = true;
            g_quiz.currentQuestionIndex = 0;
            g_quiz.currentPrizeLevel = 0;
            g_quiz.correctCount = 0;
            g_quiz.incorrectCount = 0;
            g_quiz.selectedOption = 0;
            g_quiz.millionaireGameOver = false;
            g_quiz.celebrationTimer = 0.0f;
            // Use sequential indices for Millionaire mode
            for (int i = 0; i < g_quiz.questionCount; i++) {
                g_quiz.shuffledIndices[i] = i;
            }
            g_currentScreen = SCREEN_MILLIONAIRE_MODE;
        }
    }
}

// ============================================================================
// Screen: Quiz Mode (Multiple Choice)
// ============================================================================

static void DrawQuizScreen(void) {
    DrawBackground();

    // Header with progress
    char headerText[64];
    snprintf(headerText, sizeof(headerText), "Question %d of %d",
             g_quiz.currentQuestionIndex + 1, g_quiz.questionCount);
    DrawHeader(headerText, true);

    // Progress bar
    float progress = (float)(g_quiz.currentQuestionIndex) / (float)g_quiz.questionCount;
    Rectangle progressBg = {PADDING, 90, SCREEN_WIDTH - PADDING * 2, 6};
    DrawRectangleRounded(progressBg, 0.5f, 4, COLOR_CARD_BORDER);
    Rectangle progressFill = {PADDING, 90, (SCREEN_WIDTH - PADDING * 2) * progress, 6};
    DrawRectangleRounded(progressFill, 0.5f, 4, COLOR_ACCENT);

    // Score display
    char scoreText[64];
    snprintf(scoreText, sizeof(scoreText), "Correct: %d  |  Incorrect: %d",
             g_quiz.correctCount, g_quiz.incorrectCount);
    Vector2 scoreSize = MeasureTextEx(g_font, scoreText, 14, 1);
    DrawTextEx(g_font, scoreText, (Vector2){SCREEN_WIDTH - scoreSize.x - PADDING, 76}, 14, 1, COLOR_TEXT_DIM);

    // Get current question
    int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
    const Question *q = &g_quiz.questions[qIdx];

    // Question text area
    float questionY = 105;
    float questionMaxWidth = SCREEN_WIDTH - PADDING * 2;
    DrawTextWrapped(q->question, PADDING, questionY, questionMaxWidth, 26, 1.2f, COLOR_TEXT_PRIMARY);

    // Options area - calculate based on option count
    float optionStartY = 210;
    float optionHeight = 58;
    float optionSpacing = 6;
    float optionWidth = SCREEN_WIDTH - PADDING * 2;

    for (int i = 0; i < q->optionCount; i++) {
        Rectangle optBounds = {
            PADDING,
            optionStartY + i * (optionHeight + optionSpacing),
            optionWidth,
            optionHeight
        };

        bool isSelected = (g_quiz.selectedOption == i);
        Color optBg = isSelected ? COLOR_CARD_SELECTED : COLOR_OPTION_BG;
        Color borderCol = isSelected ? COLOR_ACCENT : COLOR_CARD_BORDER;

        DrawRectangleRounded(optBounds, 0.12f, 6, optBg);
        DrawRectangleRoundedLines(optBounds, 0.12f, 6, ColorAlpha(borderCol, isSelected ? 0.8f : 0.3f));

        // Option letter
        char letterBuf[4];
        snprintf(letterBuf, sizeof(letterBuf), "%c.", 'A' + i);
        DrawTextEx(g_font, letterBuf, (Vector2){optBounds.x + 16, optBounds.y + 16}, 24, 1, COLOR_ACCENT);

        // Option text
        Color textCol = isSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
        DrawTextEx(g_font, q->options[i], (Vector2){optBounds.x + 52, optBounds.y + 16}, 22, 1, textCol);

        // Selection indicator
        if (isSelected) {
            Rectangle selectBar = {optBounds.x, optBounds.y + 8, 3, optBounds.height - 16};
            DrawRectangleRounded(selectBar, 0.5f, 4, COLOR_ACCENT);
        }
    }

    // Instructions
    DrawTextEx(g_font, "Scroll to select, press to confirm",
               (Vector2){PADDING, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
}

static void UpdateQuizScreen(const LlzInputState *input) {
    int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
    const Question *q = &g_quiz.questions[qIdx];

    int delta = 0;
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;
    if (input->scrollDelta != 0) delta = (input->scrollDelta > 0) ? 1 : -1;

    if (delta != 0) {
        g_quiz.selectedOption += delta;
        if (g_quiz.selectedOption < 0) g_quiz.selectedOption = 0;
        if (g_quiz.selectedOption >= q->optionCount) g_quiz.selectedOption = q->optionCount - 1;
    }

    if (input->selectPressed) {
        // Check answer
        g_quiz.wasCorrect = (g_quiz.selectedOption == q->correctIndex);
        if (g_quiz.wasCorrect) {
            g_quiz.correctCount++;
        } else {
            g_quiz.incorrectCount++;
        }
        g_quiz.answered = true;
        g_currentScreen = SCREEN_ANSWER_FEEDBACK;
    }
}

// ============================================================================
// Screen: Answer Feedback
// ============================================================================

static void DrawAnswerFeedbackScreen(void) {
    DrawBackground();

    int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
    const Question *q = &g_quiz.questions[qIdx];

    // Result header
    const char *resultText = g_quiz.wasCorrect ? "Correct!" : "Incorrect";
    Color resultColor = g_quiz.wasCorrect ? COLOR_CORRECT : COLOR_INCORRECT;

    Vector2 resultSize = MeasureTextEx(g_font, resultText, 48, 2);
    DrawTextEx(g_font, resultText, (Vector2){(SCREEN_WIDTH - resultSize.x) / 2, 30}, 48, 2, resultColor);

    // Question
    DrawTextWrapped(q->question, PADDING, 95, SCREEN_WIDTH - PADDING * 2, 22, 1.2f, COLOR_TEXT_SECONDARY);

    // Show correct answer
    float answerY = 195;

    DrawTextEx(g_font, "Correct answer:", (Vector2){PADDING, answerY}, 18, 1, COLOR_TEXT_DIM);
    answerY += 28;

    Rectangle correctBounds = {PADDING, answerY, SCREEN_WIDTH - PADDING * 2, 60};
    DrawRectangleRounded(correctBounds, 0.12f, 6, ColorAlpha(COLOR_CORRECT, 0.2f));
    DrawRectangleRoundedLines(correctBounds, 0.12f, 6, ColorAlpha(COLOR_CORRECT, 0.6f));

    char correctLetter[4];
    snprintf(correctLetter, sizeof(correctLetter), "%c.", 'A' + q->correctIndex);
    DrawTextEx(g_font, correctLetter, (Vector2){correctBounds.x + 16, correctBounds.y + 16}, 24, 1, COLOR_CORRECT);
    DrawTextEx(g_font, q->options[q->correctIndex], (Vector2){correctBounds.x + 52, correctBounds.y + 16}, 22, 1, COLOR_TEXT_PRIMARY);

    // If wrong, show what was selected
    if (!g_quiz.wasCorrect) {
        answerY += 78;
        DrawTextEx(g_font, "Your answer:", (Vector2){PADDING, answerY}, 18, 1, COLOR_TEXT_DIM);
        answerY += 28;

        Rectangle wrongBounds = {PADDING, answerY, SCREEN_WIDTH - PADDING * 2, 60};
        DrawRectangleRounded(wrongBounds, 0.12f, 6, ColorAlpha(COLOR_INCORRECT, 0.2f));
        DrawRectangleRoundedLines(wrongBounds, 0.12f, 6, ColorAlpha(COLOR_INCORRECT, 0.6f));

        char wrongLetter[4];
        snprintf(wrongLetter, sizeof(wrongLetter), "%c.", 'A' + g_quiz.selectedOption);
        DrawTextEx(g_font, wrongLetter, (Vector2){wrongBounds.x + 16, wrongBounds.y + 16}, 24, 1, COLOR_INCORRECT);
        DrawTextEx(g_font, q->options[g_quiz.selectedOption], (Vector2){wrongBounds.x + 52, wrongBounds.y + 16}, 22, 1, COLOR_TEXT_PRIMARY);
    }

    // Progress
    char progressText[64];
    snprintf(progressText, sizeof(progressText), "Question %d of %d  |  Score: %d/%d",
             g_quiz.currentQuestionIndex + 1, g_quiz.questionCount,
             g_quiz.correctCount, g_quiz.currentQuestionIndex + 1);
    Vector2 progSize = MeasureTextEx(g_font, progressText, 16, 1);
    DrawTextEx(g_font, progressText, (Vector2){(SCREEN_WIDTH - progSize.x) / 2, SCREEN_HEIGHT - 60}, 16, 1, COLOR_TEXT_SECONDARY);

    // Continue instruction
    const char *continueText = "Press select to continue";
    Vector2 contSize = MeasureTextEx(g_font, continueText, 14, 1);
    DrawTextEx(g_font, continueText, (Vector2){(SCREEN_WIDTH - contSize.x) / 2, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
}

static void UpdateAnswerFeedbackScreen(const LlzInputState *input) {
    if (input->selectPressed || input->tap) {
        // Move to next question or results
        g_quiz.currentQuestionIndex++;

        if (g_quiz.currentQuestionIndex >= g_quiz.questionCount) {
            // Quiz complete - update stats and show results
            UpdateStatsForCurrentQuiz();
            g_currentScreen = SCREEN_RESULTS;
        } else {
            // Next question
            g_quiz.selectedOption = 0;
            g_quiz.answered = false;
            g_currentScreen = SCREEN_QUIZ_MODE;
        }
    }
}

// ============================================================================
// Screen: Flashcard Mode
// ============================================================================

static void DrawFlashcardScreen(void) {
    DrawBackground();

    // Header with progress
    char headerText[64];
    snprintf(headerText, sizeof(headerText), "Card %d of %d",
             g_quiz.currentQuestionIndex + 1, g_quiz.questionCount);
    DrawHeader(headerText, true);

    // Progress bar
    float progress = (float)(g_quiz.currentQuestionIndex) / (float)g_quiz.questionCount;
    Rectangle progressBg = {PADDING, 90, SCREEN_WIDTH - PADDING * 2, 6};
    DrawRectangleRounded(progressBg, 0.5f, 4, COLOR_CARD_BORDER);
    Rectangle progressFill = {PADDING, 90, (SCREEN_WIDTH - PADDING * 2) * progress, 6};
    DrawRectangleRounded(progressFill, 0.5f, 4, COLOR_ACCENT);

    // Score display
    char scoreText[64];
    snprintf(scoreText, sizeof(scoreText), "Correct: %d  |  Incorrect: %d",
             g_quiz.correctCount, g_quiz.incorrectCount);
    Vector2 scoreSize = MeasureTextEx(g_font, scoreText, 14, 1);
    DrawTextEx(g_font, scoreText, (Vector2){SCREEN_WIDTH - scoreSize.x - PADDING, 76}, 14, 1, COLOR_TEXT_DIM);

    // Get current question
    int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
    const Question *q = &g_quiz.questions[qIdx];

    // Card dimensions
    float cardY = 115;
    float cardHeight = 300;
    float fullCardWidth = SCREEN_WIDTH - PADDING * 2;
    float cardCenterX = SCREEN_WIDTH / 2;

    // Calculate flip animation scale (using cosine for smooth easing)
    // flipProgress: 0.0 = question side fully visible, 1.0 = answer side fully visible
    // At 0.5, the card is edge-on (scale = 0)
    float scaleX;
    bool showAnswerSide;

    if (g_quiz.isFlipping) {
        // During animation: use cosine curve for smooth flip
        // Map progress 0->1 to angle 0->PI, then cos gives us 1->-1
        float angle = g_quiz.flipProgress * PI;
        scaleX = fabsf(cosf(angle));

        // Show answer side when past the midpoint
        showAnswerSide = (g_quiz.flipProgress > 0.5f);

        // Minimum scale to prevent disappearing completely
        if (scaleX < 0.02f) scaleX = 0.02f;
    } else {
        // Not animating
        scaleX = 1.0f;
        showAnswerSide = g_quiz.cardFlipped;
    }

    // Calculate scaled card bounds (centered)
    float scaledWidth = fullCardWidth * scaleX;
    float cardX = cardCenterX - scaledWidth / 2;
    Rectangle cardBounds = {cardX, cardY, scaledWidth, cardHeight};

    // Card colors based on which side is showing
    Color cardBg = showAnswerSide ? ColorAlpha(COLOR_ACCENT, 0.15f) : COLOR_CARD_BG;
    Color cardBorder = showAnswerSide ? COLOR_ACCENT : COLOR_CARD_BORDER;

    // Draw card shadow for 3D effect during flip
    if (g_quiz.isFlipping && scaleX < 0.95f) {
        float shadowOffset = (1.0f - scaleX) * 8;
        Rectangle shadowBounds = {cardX + shadowOffset, cardY + shadowOffset, scaledWidth, cardHeight};
        DrawRectangleRounded(shadowBounds, 0.08f, 8, ColorAlpha(BLACK, 0.2f * (1.0f - scaleX)));
    }

    // Draw the card
    DrawRectangleRounded(cardBounds, 0.08f, 8, cardBg);
    DrawRectangleRoundedLines(cardBounds, 0.08f, 8, ColorAlpha(cardBorder, 0.6f));

    // Only draw content if card is wide enough to be readable
    if (scaleX > 0.3f) {
        // Scale text positions relative to card bounds
        float contentAlpha = (scaleX - 0.3f) / 0.7f;  // Fade in text as card opens
        if (contentAlpha > 1.0f) contentAlpha = 1.0f;

        if (!showAnswerSide) {
            // Show question side
            const char *sideLabel = "QUESTION";
            Color labelColor = ColorAlpha(COLOR_ACCENT, contentAlpha);
            DrawTextEx(g_font, sideLabel, (Vector2){cardBounds.x + 20 * scaleX, cardBounds.y + 15}, 14, 1, labelColor);

            // Question text
            float textY = cardBounds.y + 60;
            float textMaxWidth = cardBounds.width - 40 * scaleX;
            if (textMaxWidth > 50) {
                Color textColor = ColorAlpha(COLOR_TEXT_PRIMARY, contentAlpha);
                DrawTextWrapped(q->question, cardBounds.x + 20 * scaleX, textY, textMaxWidth, 26, 1.2f, textColor);
            }

            // Flip instruction at bottom of card
            if (!g_quiz.isFlipping) {
                const char *flipText = "Press select to flip";
                Vector2 flipSize = MeasureTextEx(g_font, flipText, 16, 1);
                DrawTextEx(g_font, flipText,
                           (Vector2){cardBounds.x + (cardBounds.width - flipSize.x) / 2, cardBounds.y + cardBounds.height - 35},
                           16, 1, COLOR_TEXT_DIM);
            }
        } else {
            // Show answer side
            const char *sideLabel = "ANSWER";
            Color labelColor = ColorAlpha(COLOR_CORRECT, contentAlpha);
            DrawTextEx(g_font, sideLabel, (Vector2){cardBounds.x + 20 * scaleX, cardBounds.y + 15}, 14, 1, labelColor);

            // Answer text
            float textY = cardBounds.y + 60;
            float textMaxWidth = cardBounds.width - 40 * scaleX;
            if (textMaxWidth > 50) {
                Color textColor = ColorAlpha(COLOR_TEXT_PRIMARY, contentAlpha);
                DrawTextWrapped(q->options[q->correctIndex], cardBounds.x + 20 * scaleX, textY, textMaxWidth, 28, 1.2f, textColor);
            }

            // Grade instruction at bottom of card
            if (!g_quiz.isFlipping) {
                const char *gradeText = "Press select to grade yourself";
                Vector2 gradeSize = MeasureTextEx(g_font, gradeText, 16, 1);
                DrawTextEx(g_font, gradeText,
                           (Vector2){cardBounds.x + (cardBounds.width - gradeSize.x) / 2, cardBounds.y + cardBounds.height - 35},
                           16, 1, COLOR_TEXT_DIM);
            }
        }
    }

    // Instructions at bottom
    const char *instructions;
    if (g_quiz.isFlipping) {
        instructions = "Flipping...";
    } else if (g_quiz.cardFlipped) {
        instructions = "Select to grade";
    } else {
        instructions = "Select to flip card";
    }
    DrawTextEx(g_font, instructions, (Vector2){PADDING, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
}

static void UpdateFlashcardScreen(const LlzInputState *input) {
    // Update flip animation
    if (g_quiz.isFlipping) {
        g_quiz.flipProgress += GetFrameTime() * 3.0f;  // Animation speed

        if (g_quiz.flipProgress >= 1.0f) {
            // Animation complete
            g_quiz.flipProgress = 1.0f;
            g_quiz.isFlipping = false;
            g_quiz.cardFlipped = true;
        }
        return;  // Don't process input during animation
    }

    if (input->selectPressed || input->tap) {
        if (!g_quiz.cardFlipped) {
            // Start flip animation
            g_quiz.isFlipping = true;
            g_quiz.flipProgress = 0.0f;
        } else {
            // Go to self-grading screen
            g_highlightedItem = 0;  // Default to "I got it right"
            g_currentScreen = SCREEN_FLASHCARD_RESULT;
        }
    }
}

// ============================================================================
// Screen: Flashcard Self-Grade
// ============================================================================

static void DrawFlashcardResultScreen(void) {
    DrawBackground();

    // Get current question for reference
    int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
    const Question *q = &g_quiz.questions[qIdx];

    // Header
    DrawHeader("Did you get it right?", true);

    // Show the answer for reference
    DrawTextEx(g_font, "The correct answer was:", (Vector2){PADDING, 100}, 18, 1, COLOR_TEXT_DIM);

    Rectangle answerBox = {PADDING, 128, SCREEN_WIDTH - PADDING * 2, 70};
    DrawRectangleRounded(answerBox, 0.1f, 6, ColorAlpha(COLOR_ACCENT, 0.15f));
    DrawRectangleRoundedLines(answerBox, 0.1f, 6, ColorAlpha(COLOR_ACCENT, 0.4f));
    DrawTextWrapped(q->options[q->correctIndex], answerBox.x + 16, answerBox.y + 12, answerBox.width - 32, 22, 1.2f, COLOR_TEXT_PRIMARY);

    // Grade buttons
    float buttonY = 230;
    float buttonHeight = 80;
    float buttonSpacing = 20;
    float buttonWidth = SCREEN_WIDTH - PADDING * 2;

    // "I got it right" button
    Rectangle correctBtn = {PADDING, buttonY, buttonWidth, buttonHeight};
    bool correctSelected = (g_highlightedItem == 0);
    Color correctBg = correctSelected ? ColorAlpha(COLOR_CORRECT, 0.3f) : COLOR_CARD_BG;
    Color correctBorder = correctSelected ? COLOR_CORRECT : COLOR_CARD_BORDER;

    DrawRectangleRounded(correctBtn, 0.1f, 8, correctBg);
    DrawRectangleRoundedLines(correctBtn, 0.1f, 8, ColorAlpha(correctBorder, correctSelected ? 0.8f : 0.3f));

    if (correctSelected) {
        Rectangle bar = {correctBtn.x, correctBtn.y + 10, 4, correctBtn.height - 20};
        DrawRectangleRounded(bar, 0.5f, 4, COLOR_CORRECT);
    }

    DrawTextEx(g_font, "I got it right", (Vector2){correctBtn.x + 24, correctBtn.y + 16}, 26, 1,
               correctSelected ? COLOR_CORRECT : COLOR_TEXT_SECONDARY);
    DrawTextEx(g_font, "Mark as correct and continue", (Vector2){correctBtn.x + 24, correctBtn.y + 48}, 16, 1, COLOR_TEXT_DIM);

    // "I got it wrong" button
    buttonY += buttonHeight + buttonSpacing;
    Rectangle wrongBtn = {PADDING, buttonY, buttonWidth, buttonHeight};
    bool wrongSelected = (g_highlightedItem == 1);
    Color wrongBg = wrongSelected ? ColorAlpha(COLOR_INCORRECT, 0.3f) : COLOR_CARD_BG;
    Color wrongBorder = wrongSelected ? COLOR_INCORRECT : COLOR_CARD_BORDER;

    DrawRectangleRounded(wrongBtn, 0.1f, 8, wrongBg);
    DrawRectangleRoundedLines(wrongBtn, 0.1f, 8, ColorAlpha(wrongBorder, wrongSelected ? 0.8f : 0.3f));

    if (wrongSelected) {
        Rectangle bar = {wrongBtn.x, wrongBtn.y + 10, 4, wrongBtn.height - 20};
        DrawRectangleRounded(bar, 0.5f, 4, COLOR_INCORRECT);
    }

    DrawTextEx(g_font, "I got it wrong", (Vector2){wrongBtn.x + 24, wrongBtn.y + 16}, 26, 1,
               wrongSelected ? COLOR_INCORRECT : COLOR_TEXT_SECONDARY);
    DrawTextEx(g_font, "Mark as incorrect and continue", (Vector2){wrongBtn.x + 24, wrongBtn.y + 48}, 16, 1, COLOR_TEXT_DIM);

    // Progress
    char progressText[64];
    snprintf(progressText, sizeof(progressText), "Card %d of %d  |  Score: %d correct",
             g_quiz.currentQuestionIndex + 1, g_quiz.questionCount, g_quiz.correctCount);
    Vector2 progSize = MeasureTextEx(g_font, progressText, 14, 1);
    DrawTextEx(g_font, progressText, (Vector2){(SCREEN_WIDTH - progSize.x) / 2, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
}

static void UpdateFlashcardResultScreen(const LlzInputState *input) {
    int delta = 0;
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;
    if (input->scrollDelta != 0) delta = (input->scrollDelta > 0) ? 1 : -1;

    if (delta != 0) {
        g_highlightedItem += delta;
        if (g_highlightedItem < 0) g_highlightedItem = 0;
        if (g_highlightedItem > 1) g_highlightedItem = 1;
    }

    if (input->selectPressed || input->tap) {
        // Record result
        if (g_highlightedItem == 0) {
            g_quiz.correctCount++;
        } else {
            g_quiz.incorrectCount++;
        }

        // Move to next card or results
        g_quiz.currentQuestionIndex++;

        if (g_quiz.currentQuestionIndex >= g_quiz.questionCount) {
            // Quiz complete
            UpdateStatsForCurrentQuiz();
            g_currentScreen = SCREEN_RESULTS;
        } else {
            // Next card - reset flip state
            g_quiz.cardFlipped = false;
            g_quiz.isFlipping = false;
            g_quiz.flipProgress = 0.0f;
            g_currentScreen = SCREEN_FLASHCARD_MODE;
        }
    }
}

// ============================================================================
// Screen: Millionaire Mode
// ============================================================================

// Prize amounts for each level
static const char *g_prizeLevels[] = {
    "$100", "$200", "$300", "$500", "$1,000",
    "$2,000", "$4,000", "$8,000", "$16,000", "$32,000",
    "$64,000", "$125,000", "$250,000", "$500,000", "$1,000,000"
};

// Safe haven levels (indices 4 and 9 for $1,000 and $32,000)
static bool IsSafeHaven(int level) {
    return level == 4 || level == 9;
}

// Get prize for walking away at current level
static const char *GetWalkAwayPrize(int currentLevel) {
    if (currentLevel <= 0) return "$0";
    if (currentLevel <= 4) return g_prizeLevels[currentLevel - 1];
    if (currentLevel <= 9) return "$1,000";  // Safe haven
    return "$32,000";  // Second safe haven
}

// Millionaire-style dark blue background
static void DrawMillionaireBackground(void) {
    // Dark blue gradient
    Color topColor = {8, 12, 35, 255};
    Color bottomColor = {20, 30, 60, 255};
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, topColor, bottomColor);

    // Subtle spotlight effect
    for (int i = 0; i < 3; i++) {
        float alpha = 0.04f - i * 0.012f;
        Color glow = ColorAlpha((Color){100, 150, 255, 255}, alpha);
        DrawCircleGradient(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 350 - i * 60, glow, ColorAlpha(glow, 0.0f));
    }
}

static void DrawMillionaireScreen(void) {
    DrawMillionaireBackground();

    // Get current question
    int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
    const Question *q = &g_quiz.questions[qIdx];

    // Colors
    Color goldColor = {255, 215, 0, 255};
    Color blueHighlight = {60, 120, 200, 255};
    Color darkBlue = {15, 25, 50, 255};
    Color lightBlue = {100, 160, 220, 255};

    // Current prize display at top
    const char *currentPrize = g_prizeLevels[g_quiz.currentPrizeLevel];
    char prizeHeader[64];
    snprintf(prizeHeader, sizeof(prizeHeader), "Playing for: %s", currentPrize);
    Vector2 prizeSize = MeasureTextEx(g_font, prizeHeader, 28, 1);
    DrawTextEx(g_font, prizeHeader, (Vector2){(SCREEN_WIDTH - prizeSize.x) / 2, 15}, 28, 1, goldColor);

    // Prize ladder on the right side
    float ladderX = SCREEN_WIDTH - 140;
    float ladderY = 60;
    float ladderItemHeight = 24;

    for (int i = 14; i >= 0; i--) {
        float itemY = ladderY + (14 - i) * ladderItemHeight;
        bool isCurrent = (i == g_quiz.currentPrizeLevel);
        bool isPassed = (i < g_quiz.currentPrizeLevel);
        bool isSafe = IsSafeHaven(i);

        Color textColor;
        if (isCurrent) {
            textColor = goldColor;
            // Highlight box for current level
            Rectangle highlight = {ladderX - 5, itemY - 2, 130, ladderItemHeight};
            DrawRectangleRounded(highlight, 0.3f, 4, ColorAlpha(goldColor, 0.2f));
        } else if (isPassed) {
            textColor = COLOR_CORRECT;
        } else if (isSafe) {
            textColor = (Color){255, 180, 100, 255};  // Orange for safe havens
        } else {
            textColor = ColorAlpha(COLOR_TEXT_SECONDARY, 0.6f);
        }

        DrawTextEx(g_font, g_prizeLevels[i], (Vector2){ladderX, itemY}, 16, 1, textColor);
    }

    // Question area
    float questionAreaWidth = SCREEN_WIDTH - 180;  // Leave room for prize ladder

    // Question box
    Rectangle questionBox = {20, 55, questionAreaWidth - 10, 90};
    DrawRectangleRounded(questionBox, 0.1f, 6, darkBlue);
    DrawRectangleRoundedLines(questionBox, 0.1f, 6, ColorAlpha(blueHighlight, 0.5f));

    // Question number indicator
    char qNumText[32];
    snprintf(qNumText, sizeof(qNumText), "Question %d", g_quiz.currentPrizeLevel + 1);
    DrawTextEx(g_font, qNumText, (Vector2){questionBox.x + 15, questionBox.y + 8}, 14, 1, lightBlue);

    // Question text
    DrawTextWrapped(q->question, questionBox.x + 15, questionBox.y + 30, questionBox.width - 30, 22, 1.2f, COLOR_TEXT_PRIMARY);

    // Answer options in 2x2 grid (classic Millionaire style)
    float optionWidth = (questionAreaWidth - 50) / 2;
    float optionHeight = 55;
    float optionStartY = 165;
    float optionSpacing = 10;

    const char *optionLetters[] = {"A:", "B:", "C:", "D:"};

    for (int i = 0; i < q->optionCount && i < 4; i++) {
        int col = i % 2;
        int row = i / 2;

        Rectangle optBounds = {
            20 + col * (optionWidth + optionSpacing),
            optionStartY + row * (optionHeight + optionSpacing),
            optionWidth,
            optionHeight
        };

        bool isSelected = (g_quiz.selectedOption == i);

        // Option styling
        Color optBg = isSelected ? ColorAlpha(goldColor, 0.25f) : darkBlue;
        Color optBorder = isSelected ? goldColor : ColorAlpha(blueHighlight, 0.4f);

        // Draw hexagonal-ish option box (classic WWTBAM style approximation)
        DrawRectangleRounded(optBounds, 0.2f, 6, optBg);
        DrawRectangleRoundedLines(optBounds, 0.2f, 6, optBorder);

        // Option letter
        Color letterColor = isSelected ? goldColor : (Color){255, 180, 50, 255};
        DrawTextEx(g_font, optionLetters[i], (Vector2){optBounds.x + 12, optBounds.y + 16}, 22, 1, letterColor);

        // Option text
        Color textColor = isSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
        DrawTextEx(g_font, q->options[i], (Vector2){optBounds.x + 45, optBounds.y + 16}, 20, 1, textColor);

        // Selection indicator
        if (isSelected) {
            Rectangle selectBar = {optBounds.x + 2, optBounds.y + 8, 3, optBounds.height - 16};
            DrawRectangleRounded(selectBar, 0.5f, 4, goldColor);
        }
    }

    // Safe haven indicator
    if (g_quiz.currentPrizeLevel > 0) {
        const char *safeText;
        if (g_quiz.currentPrizeLevel < 5) {
            safeText = "Guaranteed: $0";
        } else if (g_quiz.currentPrizeLevel < 10) {
            safeText = "Guaranteed: $1,000";
        } else {
            safeText = "Guaranteed: $32,000";
        }
        DrawTextEx(g_font, safeText, (Vector2){20, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
    }

    // Instructions
    DrawTextEx(g_font, "Scroll to select, press to lock in answer",
               (Vector2){SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
}

static void UpdateMillionaireScreen(const LlzInputState *input) {
    int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
    const Question *q = &g_quiz.questions[qIdx];

    int delta = 0;
    // 2x2 grid navigation
    if (input->downPressed) {
        if (g_quiz.selectedOption < 2 && q->optionCount > 2) delta = 2;
    }
    if (input->upPressed) {
        if (g_quiz.selectedOption >= 2) delta = -2;
    }
    if (input->scrollDelta > 0) delta = 1;
    if (input->scrollDelta < 0) delta = -1;

    if (delta != 0) {
        g_quiz.selectedOption += delta;
        if (g_quiz.selectedOption < 0) g_quiz.selectedOption = 0;
        if (g_quiz.selectedOption >= q->optionCount) g_quiz.selectedOption = q->optionCount - 1;
    }

    if (input->selectPressed) {
        // Check answer
        if (g_quiz.selectedOption == q->correctIndex) {
            // Correct!
            g_quiz.correctCount++;
            g_quiz.currentPrizeLevel++;
            g_quiz.currentQuestionIndex++;

            if (g_quiz.currentQuestionIndex >= g_quiz.questionCount || g_quiz.currentPrizeLevel >= 15) {
                // Won the million (or completed all questions)!
                g_quiz.celebrationTimer = 0.0f;
                UpdateStatsForCurrentQuiz();
                g_currentScreen = SCREEN_MILLIONAIRE_GAMEOVER;
            } else {
                // Next question
                g_quiz.selectedOption = 0;
                g_quiz.celebrationTimer = 1.0f;  // Brief celebration
            }
        } else {
            // Wrong answer - game over
            g_quiz.incorrectCount++;
            g_quiz.millionaireGameOver = true;
            UpdateStatsForCurrentQuiz();
            g_currentScreen = SCREEN_MILLIONAIRE_GAMEOVER;
        }
    }

    // Update celebration timer
    if (g_quiz.celebrationTimer > 0) {
        g_quiz.celebrationTimer -= GetFrameTime();
    }
}

// ============================================================================
// Screen: Millionaire Game Over
// ============================================================================

static void DrawMillionaireGameOverScreen(void) {
    DrawMillionaireBackground();

    Color goldColor = {255, 215, 0, 255};
    bool won = !g_quiz.millionaireGameOver && g_quiz.currentPrizeLevel >= 15;

    if (won) {
        // Won the million!
        const char *title = "MILLIONAIRE!";
        Vector2 titleSize = MeasureTextEx(g_font, title, 52, 2);
        DrawTextEx(g_font, title, (Vector2){(SCREEN_WIDTH - titleSize.x) / 2, 60}, 52, 2, goldColor);

        const char *subtitle = "You've won $1,000,000!";
        Vector2 subSize = MeasureTextEx(g_font, subtitle, 28, 1);
        DrawTextEx(g_font, subtitle, (Vector2){(SCREEN_WIDTH - subSize.x) / 2, 130}, 28, 1, COLOR_CORRECT);

        // Celebration sparkles
        float time = GetTime();
        for (int i = 0; i < 20; i++) {
            float x = (SCREEN_WIDTH / 2) + sinf(time * 2 + i) * (150 + i * 10);
            float y = 200 + cosf(time * 3 + i * 0.5f) * 50;
            float size = 3 + sinf(time * 5 + i) * 2;
            DrawCircle(x, y, size, ColorAlpha(goldColor, 0.5f + sinf(time * 4 + i) * 0.3f));
        }
    } else if (g_quiz.millionaireGameOver) {
        // Lost
        const char *title = "GAME OVER";
        Vector2 titleSize = MeasureTextEx(g_font, title, 48, 2);
        DrawTextEx(g_font, title, (Vector2){(SCREEN_WIDTH - titleSize.x) / 2, 60}, 48, 2, COLOR_INCORRECT);

        // Show correct answer
        if (g_quiz.currentQuestionIndex < g_quiz.questionCount) {
            int qIdx = g_quiz.shuffledIndices[g_quiz.currentQuestionIndex];
            const Question *q = &g_quiz.questions[qIdx];

            DrawTextEx(g_font, "The correct answer was:", (Vector2){PADDING, 130}, 18, 1, COLOR_TEXT_DIM);

            char correctAnswer[300];
            snprintf(correctAnswer, sizeof(correctAnswer), "%c: %s", 'A' + q->correctIndex, q->options[q->correctIndex]);
            DrawTextEx(g_font, correctAnswer, (Vector2){PADDING, 155}, 22, 1, COLOR_CORRECT);
        }

        // Prize won
        const char *wonPrize = GetWalkAwayPrize(g_quiz.currentPrizeLevel);
        char prizeText[64];
        snprintf(prizeText, sizeof(prizeText), "You walk away with: %s", wonPrize);
        Vector2 prizeSize = MeasureTextEx(g_font, prizeText, 28, 1);
        DrawTextEx(g_font, prizeText, (Vector2){(SCREEN_WIDTH - prizeSize.x) / 2, 220}, 28, 1, goldColor);
    } else {
        // Completed all questions but didn't reach million (shouldn't happen with 15 questions)
        const char *title = "CONGRATULATIONS!";
        Vector2 titleSize = MeasureTextEx(g_font, title, 42, 2);
        DrawTextEx(g_font, title, (Vector2){(SCREEN_WIDTH - titleSize.x) / 2, 60}, 42, 2, goldColor);

        char prizeText[64];
        snprintf(prizeText, sizeof(prizeText), "You won: %s", g_prizeLevels[g_quiz.currentPrizeLevel - 1]);
        Vector2 prizeSize = MeasureTextEx(g_font, prizeText, 32, 1);
        DrawTextEx(g_font, prizeText, (Vector2){(SCREEN_WIDTH - prizeSize.x) / 2, 130}, 32, 1, COLOR_CORRECT);
    }

    // Stats
    char statsText[128];
    snprintf(statsText, sizeof(statsText), "Questions answered correctly: %d of %d",
             g_quiz.correctCount, g_quiz.currentQuestionIndex + (g_quiz.millionaireGameOver ? 1 : 0));
    Vector2 statsSize = MeasureTextEx(g_font, statsText, 18, 1);
    DrawTextEx(g_font, statsText, (Vector2){(SCREEN_WIDTH - statsSize.x) / 2, 300}, 18, 1, COLOR_TEXT_SECONDARY);

    // Prize ladder summary
    DrawTextEx(g_font, "Final Position:", (Vector2){PADDING, 350}, 16, 1, COLOR_TEXT_DIM);
    for (int i = 0; i < g_quiz.currentPrizeLevel && i < 15; i++) {
        Color checkColor = (i == 4 || i == 9) ? (Color){255, 180, 100, 255} : COLOR_CORRECT;
        DrawTextEx(g_font, g_prizeLevels[i], (Vector2){PADDING + (i % 5) * 100, 375 + (i / 5) * 25}, 16, 1, checkColor);
    }

    // Instructions
    DrawTextEx(g_font, "Press select to continue",
               (Vector2){PADDING, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
}

static void UpdateMillionaireGameOverScreen(const LlzInputState *input) {
    if (input->selectPressed || input->tap) {
        // Reset millionaire state
        g_quiz.isMillionaireMode = false;
        g_quiz.millionaireGameOver = false;
        g_quiz.currentPrizeLevel = 0;
        g_highlightedItem = 0;

        // Return to mode select
        g_currentScreen = SCREEN_MODE_SELECT;
    }
}

// ============================================================================
// Screen: Results
// ============================================================================

static void DrawResultsScreen(void) {
    DrawBackground();

    // Title
    const char *title = "Quiz Complete!";
    Vector2 titleSize = MeasureTextEx(g_font, title, 42, 2);
    DrawTextEx(g_font, title, (Vector2){(SCREEN_WIDTH - titleSize.x) / 2, 40}, 42, 2, COLOR_ACCENT);

    // Set name
    Vector2 nameSize = MeasureTextEx(g_font, g_quiz.setName, 18, 1);
    DrawTextEx(g_font, g_quiz.setName, (Vector2){(SCREEN_WIDTH - nameSize.x) / 2, 90}, 18, 1, COLOR_TEXT_SECONDARY);

    // Score circle
    float centerX = SCREEN_WIDTH / 2;
    float centerY = 200;
    float radius = 70;

    float percentage = (float)g_quiz.correctCount / (float)g_quiz.questionCount;
    Color scoreColor = percentage >= 0.7f ? COLOR_CORRECT : (percentage >= 0.5f ? COLOR_ACCENT : COLOR_INCORRECT);

    // Background circle
    DrawCircle(centerX, centerY, radius, COLOR_CARD_BG);
    DrawCircleLines(centerX, centerY, radius, COLOR_CARD_BORDER);

    // Progress arc
    for (int i = 0; i < (int)(360 * percentage); i++) {
        float angle = (i - 90) * DEG2RAD;
        float x = centerX + cosf(angle) * radius;
        float y = centerY + sinf(angle) * radius;
        DrawCircle(x, y, 4, scoreColor);
    }

    // Score percentage
    char percentText[16];
    snprintf(percentText, sizeof(percentText), "%d%%", (int)(percentage * 100));
    Vector2 percentSize = MeasureTextEx(g_font, percentText, 36, 2);
    DrawTextEx(g_font, percentText, (Vector2){centerX - percentSize.x / 2, centerY - 20}, 36, 2, COLOR_TEXT_PRIMARY);

    // Fraction
    char fractionText[32];
    snprintf(fractionText, sizeof(fractionText), "%d / %d", g_quiz.correctCount, g_quiz.questionCount);
    Vector2 fracSize = MeasureTextEx(g_font, fractionText, 16, 1);
    DrawTextEx(g_font, fractionText, (Vector2){centerX - fracSize.x / 2, centerY + 20}, 16, 1, COLOR_TEXT_SECONDARY);

    // Stats boxes
    float boxY = 300;
    float boxWidth = 200;
    float boxHeight = 60;
    float boxSpacing = 40;

    // Correct box
    Rectangle correctBox = {centerX - boxWidth - boxSpacing / 2, boxY, boxWidth, boxHeight};
    DrawRectangleRounded(correctBox, 0.15f, 6, ColorAlpha(COLOR_CORRECT, 0.2f));
    DrawRectangleRoundedLines(correctBox, 0.15f, 6, ColorAlpha(COLOR_CORRECT, 0.5f));

    char correctText[32];
    snprintf(correctText, sizeof(correctText), "%d Correct", g_quiz.correctCount);
    Vector2 corrSize = MeasureTextEx(g_font, correctText, 22, 1);
    DrawTextEx(g_font, correctText, (Vector2){correctBox.x + (boxWidth - corrSize.x) / 2, correctBox.y + 18}, 22, 1, COLOR_CORRECT);

    // Incorrect box
    Rectangle incorrectBox = {centerX + boxSpacing / 2, boxY, boxWidth, boxHeight};
    DrawRectangleRounded(incorrectBox, 0.15f, 6, ColorAlpha(COLOR_INCORRECT, 0.2f));
    DrawRectangleRoundedLines(incorrectBox, 0.15f, 6, ColorAlpha(COLOR_INCORRECT, 0.5f));

    char incorrectText[32];
    snprintf(incorrectText, sizeof(incorrectText), "%d Incorrect", g_quiz.incorrectCount);
    Vector2 incSize = MeasureTextEx(g_font, incorrectText, 22, 1);
    DrawTextEx(g_font, incorrectText, (Vector2){incorrectBox.x + (boxWidth - incSize.x) / 2, incorrectBox.y + 18}, 22, 1, COLOR_INCORRECT);

    // Rating
    const char *rating;
    if (percentage >= 0.9f) rating = "Excellent!";
    else if (percentage >= 0.8f) rating = "Great job!";
    else if (percentage >= 0.7f) rating = "Good work!";
    else if (percentage >= 0.6f) rating = "Not bad!";
    else if (percentage >= 0.5f) rating = "Keep practicing!";
    else rating = "More study needed";

    Vector2 ratingSize = MeasureTextEx(g_font, rating, 24, 1);
    DrawTextEx(g_font, rating, (Vector2){(SCREEN_WIDTH - ratingSize.x) / 2, 390}, 24, 1, scoreColor);

    // Instructions
    DrawTextEx(g_font, "Press select to return  |  Press back to exit",
               (Vector2){PADDING, SCREEN_HEIGHT - 28}, 14, 1, COLOR_TEXT_DIM);
}

static void UpdateResultsScreen(const LlzInputState *input) {
    if (input->selectPressed || input->tap) {
        // Return to folder view
        g_highlightedItem = 0;
        g_listScrollOffset = 0;
        g_smoothScrollOffset = 0.0f;
        g_targetScrollOffset = 0.0f;

        if (g_navDepth > 0) {
            g_currentScreen = SCREEN_SUBFOLDER_LIST;
        } else {
            g_currentScreen = SCREEN_CATEGORY_SELECT;
        }
    }
}

// ============================================================================
// Plugin Lifecycle
// ============================================================================

static void PluginInit(int width, int height) {
    (void)width;
    (void)height;

    printf("Flashcards plugin initialized\n");

    LoadPluginFont();

    // Find questions folder
    FindQuestionsBasePath();

    // Load categories
    LoadCategories();

    // Load statistics
    LoadStats();

    // Reset state
    g_currentScreen = SCREEN_CATEGORY_SELECT;
    g_highlightedItem = 0;
    g_listScrollOffset = 0;
    g_wantsClose = false;
    g_highlightPulse = 0.0f;
    g_smoothScrollOffset = 0.0f;
    g_targetScrollOffset = 0.0f;
    g_navDepth = 0;
}

static void PluginUpdate(const LlzInputState *input, float deltaTime) {
    g_highlightPulse += deltaTime;
    UpdateSmoothScroll(deltaTime);

    // Calculate target scroll based on screen
    switch (g_currentScreen) {
        case SCREEN_CATEGORY_SELECT:
            g_targetScrollOffset = CalculateTargetScroll(g_highlightedItem, g_categoryCount, ITEMS_PER_PAGE);
            break;
        case SCREEN_SUBFOLDER_LIST:
            g_targetScrollOffset = CalculateTargetScroll(g_highlightedItem, g_currentFolderItemCount, ITEMS_PER_PAGE);
            break;
        default:
            g_targetScrollOffset = 0.0f;
            break;
    }

    // Handle back button
    bool backPressed = input->backReleased;

    if (backPressed) {
        switch (g_currentScreen) {
            case SCREEN_CATEGORY_SELECT:
                g_wantsClose = true;
                return;

            case SCREEN_SUBFOLDER_LIST:
                if (g_navDepth > 1) {
                    // Go up one level
                    g_navDepth--;
                    strncpy(g_currentCategoryName, g_navNames[g_navDepth - 1], MAX_NAME_LEN - 1);
                    LoadFolderContents(g_navStack[g_navDepth - 1]);
                    g_highlightedItem = 0;
                    g_listScrollOffset = 0;
                    g_smoothScrollOffset = 0.0f;
                    g_targetScrollOffset = 0.0f;
                } else {
                    // Back to category select
                    g_navDepth = 0;
                    g_highlightedItem = 0;
                    g_listScrollOffset = 0;
                    g_smoothScrollOffset = 0.0f;
                    g_targetScrollOffset = 0.0f;
                    g_currentScreen = SCREEN_CATEGORY_SELECT;
                }
                return;

            case SCREEN_MODE_SELECT:
                // Go back to folder view
                g_highlightedItem = 0;
                g_listScrollOffset = 0;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;
                if (g_navDepth > 0) {
                    g_currentScreen = SCREEN_SUBFOLDER_LIST;
                } else {
                    g_currentScreen = SCREEN_CATEGORY_SELECT;
                }
                return;

            case SCREEN_QUIZ_MODE:
            case SCREEN_ANSWER_FEEDBACK:
            case SCREEN_FLASHCARD_MODE:
            case SCREEN_FLASHCARD_RESULT:
                // Exit quiz/flashcard mode - go back to mode select
                g_highlightedItem = 0;
                g_listScrollOffset = 0;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;
                g_currentScreen = SCREEN_MODE_SELECT;
                return;

            case SCREEN_RESULTS:
                g_highlightedItem = 0;
                g_listScrollOffset = 0;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;
                if (g_navDepth > 0) {
                    g_currentScreen = SCREEN_SUBFOLDER_LIST;
                } else {
                    g_currentScreen = SCREEN_CATEGORY_SELECT;
                }
                return;

            case SCREEN_STATS:
                g_currentScreen = SCREEN_CATEGORY_SELECT;
                return;

            case SCREEN_MILLIONAIRE_MODE:
                // Exit millionaire mode - go back to mode select
                g_highlightedItem = 0;
                g_listScrollOffset = 0;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;
                g_currentScreen = SCREEN_MODE_SELECT;
                return;

            case SCREEN_MILLIONAIRE_GAMEOVER:
                // Go back to folder view
                g_highlightedItem = 0;
                g_listScrollOffset = 0;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;
                if (g_navDepth > 0) {
                    g_currentScreen = SCREEN_SUBFOLDER_LIST;
                } else {
                    g_currentScreen = SCREEN_CATEGORY_SELECT;
                }
                return;
        }
    }

    // Screen-specific updates
    switch (g_currentScreen) {
        case SCREEN_CATEGORY_SELECT:
            UpdateCategorySelectScreen(input);
            break;
        case SCREEN_SUBFOLDER_LIST:
            UpdateSubfolderListScreen(input);
            break;
        case SCREEN_MODE_SELECT:
            UpdateModeSelectScreen(input);
            break;
        case SCREEN_QUIZ_MODE:
            UpdateQuizScreen(input);
            break;
        case SCREEN_ANSWER_FEEDBACK:
            UpdateAnswerFeedbackScreen(input);
            break;
        case SCREEN_FLASHCARD_MODE:
            UpdateFlashcardScreen(input);
            break;
        case SCREEN_FLASHCARD_RESULT:
            UpdateFlashcardResultScreen(input);
            break;
        case SCREEN_RESULTS:
            UpdateResultsScreen(input);
            break;
        case SCREEN_STATS:
            // Stats screen not implemented yet
            break;
        case SCREEN_MILLIONAIRE_MODE:
            UpdateMillionaireScreen(input);
            break;
        case SCREEN_MILLIONAIRE_GAMEOVER:
            UpdateMillionaireGameOverScreen(input);
            break;
    }
}

static void PluginDraw(void) {
    switch (g_currentScreen) {
        case SCREEN_CATEGORY_SELECT:
            DrawCategorySelectScreen();
            break;
        case SCREEN_SUBFOLDER_LIST:
            DrawSubfolderListScreen();
            break;
        case SCREEN_MODE_SELECT:
            DrawModeSelectScreen();
            break;
        case SCREEN_QUIZ_MODE:
            DrawQuizScreen();
            break;
        case SCREEN_ANSWER_FEEDBACK:
            DrawAnswerFeedbackScreen();
            break;
        case SCREEN_FLASHCARD_MODE:
            DrawFlashcardScreen();
            break;
        case SCREEN_FLASHCARD_RESULT:
            DrawFlashcardResultScreen();
            break;
        case SCREEN_RESULTS:
            DrawResultsScreen();
            break;
        case SCREEN_STATS:
            // Stats screen not implemented yet
            DrawBackground();
            DrawHeader("Statistics", true);
            break;
        case SCREEN_MILLIONAIRE_MODE:
            DrawMillionaireScreen();
            break;
        case SCREEN_MILLIONAIRE_GAMEOVER:
            DrawMillionaireGameOverScreen();
            break;
    }
}

static void PluginShutdown(void) {
    UnloadPluginFont();
    printf("Flashcards plugin shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin API Export
// ============================================================================

static LlzPluginAPI g_api = {
    .name = "Flashcards",
    .description = "Multiple choice quiz tester",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .handles_back_button = true,
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
