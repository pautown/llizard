/**
 * Millionaire Questions System
 *
 * Handles loading and managing questions for "Who Wants to Be a Millionaire" mode.
 * Questions are loaded from JSON files with the OpenTDB format.
 */

#ifndef MILLIONAIRE_QUESTIONS_H
#define MILLIONAIRE_QUESTIONS_H

#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Constants
// ============================================================================

#define MLQ_MAX_QUESTIONS 2000      // Maximum total questions in pool
#define MLQ_MAX_ID_LEN 16
#define MLQ_MAX_QUESTION_LEN 512
#define MLQ_MAX_OPTION_LEN 256
#define MLQ_MAX_DIFFICULTY_LEN 16
#define MLQ_MAX_CATEGORY_LEN 64

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Represents a single Millionaire question.
 */
typedef struct {
    char id[MLQ_MAX_ID_LEN];                    // Unique question ID
    char question[MLQ_MAX_QUESTION_LEN];        // Question text
    char options[4][MLQ_MAX_OPTION_LEN];        // Answer options (shuffled)
    int correctIndex;                           // Index of correct answer (0-3) after shuffle
    char difficulty[MLQ_MAX_DIFFICULTY_LEN];    // "easy", "medium", or "hard"
    char category[MLQ_MAX_CATEGORY_LEN];        // Question category
    bool used;                                  // True if used in current game
} MillionaireQuestion;

/**
 * Statistics about the loaded question pool.
 */
typedef struct {
    int totalQuestions;
    int easyCount;
    int mediumCount;
    int hardCount;
    int usedCount;
} MillionairePoolStats;

// ============================================================================
// Public API
// ============================================================================

/**
 * Load questions from a JSON file in OpenTDB format.
 *
 * Expected JSON format:
 * {
 *   "total_questions": 1165,
 *   "questions": [
 *     {
 *       "id": "28857ac60b2b",
 *       "type": "multiple",
 *       "difficulty": "easy",
 *       "category": "General Knowledge",
 *       "question": "What type of animal was Harambe?",
 *       "correct_answer": "Gorilla",
 *       "incorrect_answers": ["Tiger", "Panda", "Crocodile"]
 *     }
 *   ]
 * }
 *
 * @param filepath Path to the JSON file
 * @return true if questions were loaded successfully, false otherwise
 */
bool MlqLoadQuestionsFromJSON(const char* filepath);

/**
 * Get a question appropriate for the given prize level.
 *
 * Difficulty mapping:
 *   Levels 0-4   ($100 - $1,000):    "easy" questions
 *   Levels 5-9   ($2,000 - $32,000): "medium" questions
 *   Levels 10-14 ($64,000 - $1M):    "hard" questions
 *
 * The selected question is marked as "used" to avoid repeats in the same game.
 *
 * @param prizeLevel Current prize level (0-14)
 * @return Pointer to the selected question, or NULL if no suitable question available
 */
MillionaireQuestion* MlqGetQuestionForLevel(int prizeLevel);

/**
 * Shuffle the answer options for a question.
 * Updates the correctIndex to reflect the new position of the correct answer.
 *
 * @param q Pointer to the question to shuffle
 */
void MlqShuffleAnswers(MillionaireQuestion* q);

/**
 * Reset all "used" flags for a new game.
 * Call this at the start of each new Millionaire game session.
 */
void MlqResetQuestionPool(void);

/**
 * Get statistics about the question pool.
 *
 * @param stats Pointer to struct to fill with statistics
 */
void MlqGetPoolStats(MillionairePoolStats* stats);

/**
 * Check if the question pool is loaded and has questions.
 *
 * @return true if questions are available, false otherwise
 */
bool MlqIsPoolLoaded(void);

/**
 * Clear all loaded questions and free resources.
 */
void MlqClearPool(void);

#endif // MILLIONAIRE_QUESTIONS_H
