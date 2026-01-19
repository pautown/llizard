/**
 * Millionaire Questions System - Implementation
 *
 * Handles loading and managing questions for "Who Wants to Be a Millionaire" mode.
 * Uses simple string parsing (no external JSON library).
 */

#include "millionaire_questions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// Static Question Pool
// ============================================================================

static MillionaireQuestion g_questionPool[MLQ_MAX_QUESTIONS];
static int g_questionCount = 0;
static bool g_poolLoaded = false;
static bool g_randomSeeded = false;

// ============================================================================
// JSON Parsing Helpers
// ============================================================================

/**
 * Skip whitespace characters.
 */
static const char* mlq_skipWs(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    return p;
}

/**
 * Parse a JSON string value, handling escape sequences.
 * Handles HTML entities common in OpenTDB data.
 */
static const char* mlq_parseString(const char* p, char* out, size_t maxLen) {
    if (*p != '"') return p;
    p++;

    size_t i = 0;
    while (*p != '"' && *p != '\0' && i < maxLen - 1) {
        if (*p == '\\' && *(p + 1) != '\0') {
            p++;
            switch (*p) {
                case 'n':  out[i++] = '\n'; break;
                case 'r':  out[i++] = '\r'; break;
                case 't':  out[i++] = '\t'; break;
                case '"':  out[i++] = '"';  break;
                case '\\': out[i++] = '\\'; break;
                case '/':  out[i++] = '/';  break;
                case 'u':
                    // Unicode escape \uXXXX - skip for now, just output '?'
                    if (*(p+1) && *(p+2) && *(p+3) && *(p+4)) {
                        p += 4;
                        out[i++] = '?';
                    }
                    break;
                default:
                    out[i++] = *p;
                    break;
            }
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';

    // Skip to closing quote
    while (*p != '"' && *p != '\0') p++;
    if (*p == '"') p++;

    return p;
}

/**
 * Skip a JSON value (string, number, object, array, or literal).
 */
static const char* mlq_skipValue(const char* p) {
    p = mlq_skipWs(p);

    if (*p == '"') {
        // String
        p++;
        while (*p != '"' && *p != '\0') {
            if (*p == '\\' && *(p + 1) != '\0') p++;
            p++;
        }
        if (*p == '"') p++;
    } else if (*p == '{') {
        // Object
        int depth = 1;
        p++;
        while (depth > 0 && *p != '\0') {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            else if (*p == '"') {
                p++;
                while (*p != '"' && *p != '\0') {
                    if (*p == '\\' && *(p + 1) != '\0') p++;
                    p++;
                }
            }
            p++;
        }
    } else if (*p == '[') {
        // Array
        int depth = 1;
        p++;
        while (depth > 0 && *p != '\0') {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            else if (*p == '"') {
                p++;
                while (*p != '"' && *p != '\0') {
                    if (*p == '\\' && *(p + 1) != '\0') p++;
                    p++;
                }
            }
            p++;
        }
    } else {
        // Number, boolean, or null
        while (*p != ',' && *p != '}' && *p != ']' && *p != '\0') p++;
    }

    return p;
}

/**
 * Parse an array of strings (for incorrect_answers).
 * Returns number of strings parsed.
 */
static const char* mlq_parseStringArray(const char* p, char out[][MLQ_MAX_OPTION_LEN],
                                         int maxCount, int* count) {
    *count = 0;
    p = mlq_skipWs(p);

    if (*p != '[') return p;
    p++;

    while (*count < maxCount && *p != '\0') {
        p = mlq_skipWs(p);
        if (*p == ']') break;
        if (*p == ',') {
            p++;
            continue;
        }

        if (*p == '"') {
            p = mlq_parseString(p, out[*count], MLQ_MAX_OPTION_LEN);
            (*count)++;
        } else {
            break;
        }
    }

    // Skip to closing bracket
    while (*p != '\0' && *p != ']') p++;
    if (*p == ']') p++;

    return p;
}

/**
 * Decode HTML entities common in OpenTDB data.
 */
static void mlq_decodeHtmlEntities(char* str) {
    if (!str) return;

    char* read = str;
    char* write = str;

    while (*read) {
        if (*read == '&') {
            // Check for common HTML entities
            if (strncmp(read, "&quot;", 6) == 0) {
                *write++ = '"';
                read += 6;
            } else if (strncmp(read, "&amp;", 5) == 0) {
                *write++ = '&';
                read += 5;
            } else if (strncmp(read, "&lt;", 4) == 0) {
                *write++ = '<';
                read += 4;
            } else if (strncmp(read, "&gt;", 4) == 0) {
                *write++ = '>';
                read += 4;
            } else if (strncmp(read, "&apos;", 6) == 0) {
                *write++ = '\'';
                read += 6;
            } else if (strncmp(read, "&#039;", 6) == 0) {
                *write++ = '\'';
                read += 6;
            } else if (strncmp(read, "&eacute;", 8) == 0) {
                *write++ = 'e';  // Simplified: é -> e
                read += 8;
            } else if (strncmp(read, "&ntilde;", 8) == 0) {
                *write++ = 'n';  // Simplified: ñ -> n
                read += 8;
            } else if (strncmp(read, "&ouml;", 6) == 0) {
                *write++ = 'o';  // Simplified: ö -> o
                read += 6;
            } else if (strncmp(read, "&uuml;", 6) == 0) {
                *write++ = 'u';  // Simplified: ü -> u
                read += 6;
            } else if (strncmp(read, "&auml;", 6) == 0) {
                *write++ = 'a';  // Simplified: ä -> a
                read += 6;
            } else if (strncmp(read, "&nbsp;", 6) == 0) {
                *write++ = ' ';
                read += 6;
            } else if (strncmp(read, "&ldquo;", 7) == 0 || strncmp(read, "&rdquo;", 7) == 0) {
                *write++ = '"';
                read += 7;
            } else if (strncmp(read, "&lsquo;", 7) == 0 || strncmp(read, "&rsquo;", 7) == 0) {
                *write++ = '\'';
                read += 7;
            } else if (strncmp(read, "&hellip;", 8) == 0) {
                *write++ = '.';
                *write++ = '.';
                *write++ = '.';
                read += 8;
            } else if (strncmp(read, "&mdash;", 7) == 0 || strncmp(read, "&ndash;", 7) == 0) {
                *write++ = '-';
                read += 7;
            } else if (read[1] == '#' && read[2] >= '0' && read[2] <= '9') {
                // Numeric entity &#NNN;
                char* end;
                long code = strtol(read + 2, &end, 10);
                if (*end == ';' && code > 0 && code < 128) {
                    *write++ = (char)code;
                    read = end + 1;
                } else {
                    *write++ = *read++;
                }
            } else {
                // Unknown entity, keep as-is
                *write++ = *read++;
            }
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool MlqLoadQuestionsFromJSON(const char* filepath) {
    // Clear existing pool
    MlqClearPool();

    // Seed random if not already done
    if (!g_randomSeeded) {
        srand((unsigned int)time(NULL));
        g_randomSeeded = true;
    }

    FILE* f = fopen(filepath, "r");
    if (!f) {
        printf("Millionaire: Cannot open file: %s\n", filepath);
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Limit file size to 5MB
    if (size <= 0 || size > 5 * 1024 * 1024) {
        printf("Millionaire: File too large or empty: %s (size: %ld)\n", filepath, size);
        fclose(f);
        return false;
    }

    // Read entire file
    char* content = (char*)malloc(size + 1);
    if (!content) {
        printf("Millionaire: Memory allocation failed\n");
        fclose(f);
        return false;
    }

    size_t bytesRead = fread(content, 1, size, f);
    content[bytesRead] = '\0';
    fclose(f);

    // Find "questions" array
    const char* p = strstr(content, "\"questions\"");
    if (!p) {
        printf("Millionaire: No 'questions' array found in JSON\n");
        free(content);
        return false;
    }

    p = strchr(p, '[');
    if (!p) {
        printf("Millionaire: Malformed JSON - no array start\n");
        free(content);
        return false;
    }
    p++;  // Skip '['

    // Parse questions
    int parseErrors = 0;
    while (g_questionCount < MLQ_MAX_QUESTIONS && *p != '\0') {
        p = mlq_skipWs(p);
        if (*p == ']') break;
        if (*p != '{') {
            if (*p == ',') {
                p++;
                continue;
            }
            break;
        }
        p++;  // Skip '{'

        // Temporary storage for parsing
        MillionaireQuestion q;
        memset(&q, 0, sizeof(q));
        char correctAnswer[MLQ_MAX_OPTION_LEN] = "";
        char incorrectAnswers[3][MLQ_MAX_OPTION_LEN] = {0};
        int incorrectCount = 0;

        // Parse question object fields
        while (*p != '\0' && *p != '}') {
            p = mlq_skipWs(p);
            if (*p == '}') break;
            if (*p == ',') {
                p++;
                continue;
            }
            if (*p != '"') break;

            // Parse field name
            p++;
            const char* fieldStart = p;
            while (*p != '"' && *p != '\0') p++;
            size_t fieldLen = p - fieldStart;
            if (*p == '"') p++;

            // Skip colon
            p = mlq_skipWs(p);
            if (*p == ':') p++;
            p = mlq_skipWs(p);

            // Parse value based on field name
            if (fieldLen == 2 && strncmp(fieldStart, "id", 2) == 0) {
                p = mlq_parseString(p, q.id, MLQ_MAX_ID_LEN);
            } else if (fieldLen == 8 && strncmp(fieldStart, "question", 8) == 0) {
                p = mlq_parseString(p, q.question, MLQ_MAX_QUESTION_LEN);
            } else if (fieldLen == 10 && strncmp(fieldStart, "difficulty", 10) == 0) {
                p = mlq_parseString(p, q.difficulty, MLQ_MAX_DIFFICULTY_LEN);
            } else if (fieldLen == 8 && strncmp(fieldStart, "category", 8) == 0) {
                p = mlq_parseString(p, q.category, MLQ_MAX_CATEGORY_LEN);
            } else if (fieldLen == 14 && strncmp(fieldStart, "correct_answer", 14) == 0) {
                p = mlq_parseString(p, correctAnswer, MLQ_MAX_OPTION_LEN);
            } else if (fieldLen == 17 && strncmp(fieldStart, "incorrect_answers", 17) == 0) {
                p = mlq_parseStringArray(p, incorrectAnswers, 3, &incorrectCount);
            } else {
                // Skip unknown fields (type, etc.)
                p = mlq_skipValue(p);
            }
        }

        // Skip closing brace
        if (*p == '}') p++;
        p = mlq_skipWs(p);
        if (*p == ',') p++;

        // Validate and assemble the question
        if (q.question[0] == '\0' || correctAnswer[0] == '\0' || incorrectCount < 1) {
            parseErrors++;
            continue;
        }

        // Decode HTML entities in all text fields
        mlq_decodeHtmlEntities(q.question);
        mlq_decodeHtmlEntities(q.category);
        mlq_decodeHtmlEntities(correctAnswer);
        for (int i = 0; i < incorrectCount; i++) {
            mlq_decodeHtmlEntities(incorrectAnswers[i]);
        }

        // Place correct answer first, then incorrect answers
        strncpy(q.options[0], correctAnswer, MLQ_MAX_OPTION_LEN - 1);
        for (int i = 0; i < incorrectCount && i < 3; i++) {
            strncpy(q.options[i + 1], incorrectAnswers[i], MLQ_MAX_OPTION_LEN - 1);
        }
        q.correctIndex = 0;  // Will be shuffled later

        // Default difficulty if not specified
        if (q.difficulty[0] == '\0') {
            strncpy(q.difficulty, "medium", MLQ_MAX_DIFFICULTY_LEN - 1);
        }

        // Generate ID if not present
        if (q.id[0] == '\0') {
            snprintf(q.id, MLQ_MAX_ID_LEN, "q%d", g_questionCount);
        }

        q.used = false;

        // Add to pool
        g_questionPool[g_questionCount++] = q;
    }

    free(content);

    if (g_questionCount > 0) {
        g_poolLoaded = true;
        printf("Millionaire: Loaded %d questions from %s", g_questionCount, filepath);
        if (parseErrors > 0) {
            printf(" (%d parse errors)", parseErrors);
        }
        printf("\n");

        // Print difficulty breakdown
        MillionairePoolStats stats;
        MlqGetPoolStats(&stats);
        printf("Millionaire: Easy: %d, Medium: %d, Hard: %d\n",
               stats.easyCount, stats.mediumCount, stats.hardCount);

        return true;
    }

    printf("Millionaire: No valid questions loaded from %s\n", filepath);
    return false;
}

MillionaireQuestion* MlqGetQuestionForLevel(int prizeLevel) {
    if (!g_poolLoaded || g_questionCount == 0) {
        return NULL;
    }

    // Determine required difficulty based on prize level
    const char* targetDifficulty;
    if (prizeLevel <= 4) {
        // Levels 0-4: $100, $200, $300, $500, $1,000
        targetDifficulty = "easy";
    } else if (prizeLevel <= 9) {
        // Levels 5-9: $2,000, $4,000, $8,000, $16,000, $32,000
        targetDifficulty = "medium";
    } else {
        // Levels 10-14: $64,000, $125,000, $250,000, $500,000, $1,000,000
        targetDifficulty = "hard";
    }

    // Count available questions at target difficulty
    int availableCount = 0;
    for (int i = 0; i < g_questionCount; i++) {
        if (!g_questionPool[i].used &&
            strcmp(g_questionPool[i].difficulty, targetDifficulty) == 0) {
            availableCount++;
        }
    }

    if (availableCount == 0) {
        // Fall back: try any difficulty that hasn't been used
        printf("Millionaire: No %s questions available, trying fallback\n", targetDifficulty);
        for (int i = 0; i < g_questionCount; i++) {
            if (!g_questionPool[i].used) {
                availableCount++;
            }
        }

        if (availableCount == 0) {
            printf("Millionaire: No unused questions available!\n");
            return NULL;
        }

        // Select random unused question
        int pick = rand() % availableCount;
        int count = 0;
        for (int i = 0; i < g_questionCount; i++) {
            if (!g_questionPool[i].used) {
                if (count == pick) {
                    g_questionPool[i].used = true;
                    return &g_questionPool[i];
                }
                count++;
            }
        }
        return NULL;
    }

    // Select random question from available pool
    int pick = rand() % availableCount;
    int count = 0;
    for (int i = 0; i < g_questionCount; i++) {
        if (!g_questionPool[i].used &&
            strcmp(g_questionPool[i].difficulty, targetDifficulty) == 0) {
            if (count == pick) {
                g_questionPool[i].used = true;
                return &g_questionPool[i];
            }
            count++;
        }
    }

    return NULL;
}

void MlqShuffleAnswers(MillionaireQuestion* q) {
    if (!q) return;

    // Count non-empty options
    int optionCount = 0;
    for (int i = 0; i < 4; i++) {
        if (q->options[i][0] != '\0') {
            optionCount++;
        }
    }

    if (optionCount < 2) return;

    // Seed random if needed
    if (!g_randomSeeded) {
        srand((unsigned int)time(NULL));
        g_randomSeeded = true;
    }

    // Fisher-Yates shuffle
    for (int i = optionCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        if (i != j) {
            // Swap options
            char temp[MLQ_MAX_OPTION_LEN];
            strncpy(temp, q->options[i], MLQ_MAX_OPTION_LEN - 1);
            temp[MLQ_MAX_OPTION_LEN - 1] = '\0';

            strncpy(q->options[i], q->options[j], MLQ_MAX_OPTION_LEN - 1);
            q->options[i][MLQ_MAX_OPTION_LEN - 1] = '\0';

            strncpy(q->options[j], temp, MLQ_MAX_OPTION_LEN - 1);
            q->options[j][MLQ_MAX_OPTION_LEN - 1] = '\0';

            // Track correct answer position
            if (q->correctIndex == i) {
                q->correctIndex = j;
            } else if (q->correctIndex == j) {
                q->correctIndex = i;
            }
        }
    }
}

void MlqResetQuestionPool(void) {
    for (int i = 0; i < g_questionCount; i++) {
        g_questionPool[i].used = false;
    }
    printf("Millionaire: Question pool reset (%d questions available)\n", g_questionCount);
}

void MlqGetPoolStats(MillionairePoolStats* stats) {
    if (!stats) return;

    memset(stats, 0, sizeof(MillionairePoolStats));
    stats->totalQuestions = g_questionCount;

    for (int i = 0; i < g_questionCount; i++) {
        if (strcmp(g_questionPool[i].difficulty, "easy") == 0) {
            stats->easyCount++;
        } else if (strcmp(g_questionPool[i].difficulty, "medium") == 0) {
            stats->mediumCount++;
        } else if (strcmp(g_questionPool[i].difficulty, "hard") == 0) {
            stats->hardCount++;
        }

        if (g_questionPool[i].used) {
            stats->usedCount++;
        }
    }
}

bool MlqIsPoolLoaded(void) {
    return g_poolLoaded && g_questionCount > 0;
}

void MlqClearPool(void) {
    memset(g_questionPool, 0, sizeof(g_questionPool));
    g_questionCount = 0;
    g_poolLoaded = false;
}
