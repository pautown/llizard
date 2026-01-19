#include "llizard_plugin.h"
#include "llz_sdk.h"
#include "llz_notification.h"
#include "raylib.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define BOARD_SIZE 4
#define TILE_COUNT (BOARD_SIZE * BOARD_SIZE)

#define COLOR_BG            (Color){18, 18, 24, 255}
#define COLOR_PANEL         (Color){32, 34, 48, 255}
#define COLOR_PANEL_LIGHT   (Color){48, 52, 72, 255}
#define COLOR_TEXT_PRIMARY  (Color){244, 244, 244, 255}
#define COLOR_TEXT_MUTED    (Color){150, 155, 170, 255}
#define COLOR_TILE_DARK     (Color){71, 64, 57, 255}

typedef struct {
    int cells[BOARD_SIZE][BOARD_SIZE];
    int score;
    int bestScore;
    bool gameOver;
    bool gameWon;
    bool winContinued;  // Player continued playing after winning
    float statusTimer;
} Game2048State;

static Game2048State g_game;
static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_wantsClose = false;
static float g_tileAnimOffsetX[BOARD_SIZE][BOARD_SIZE];
static float g_tileAnimOffsetY[BOARD_SIZE][BOARD_SIZE];
static float g_tileSpawnTimer[BOARD_SIZE][BOARD_SIZE];
static float g_slideTimer = 0.0f;
static const float SLIDE_DURATION = 0.14f;
static const float SPAWN_DURATION = 0.24f;  // Slower fade-in at 0.9x speed

// Drag-based swipe tracking (matching llzblocks sensitivity)
static float g_dragAccumX = 0.0f;
static float g_dragAccumY = 0.0f;
static const float DRAG_THRESHOLD = 18.0f;  // Pixels per swipe action (same as llzblocks)

// Plugin config for persistent game state
static LlzPluginConfig g_config;
static bool g_configInitialized = false;

// Now Playing notification
static bool g_mediaInitialized = false;
static LlzSubscriptionId g_trackSubId = 0;

// Serialize board to comma-separated string: "2,0,4,0,..."
static void SerializeBoard(char *buffer, size_t bufferSize) {
    buffer[0] = '\0';
    size_t offset = 0;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            int len = snprintf(buffer + offset, bufferSize - offset,
                               "%d%s", g_game.cells[y][x],
                               (y == BOARD_SIZE - 1 && x == BOARD_SIZE - 1) ? "" : ",");
            if (len > 0 && offset + len < bufferSize) {
                offset += len;
            }
        }
    }
}

// Deserialize board from comma-separated string
static bool DeserializeBoard(const char *str) {
    if (!str || str[0] == '\0') return false;

    char buffer[256];
    strncpy(buffer, str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    int values[TILE_COUNT];
    int count = 0;
    char *token = strtok(buffer, ",");
    while (token && count < TILE_COUNT) {
        values[count++] = atoi(token);
        token = strtok(NULL, ",");
    }

    if (count != TILE_COUNT) return false;

    int idx = 0;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            g_game.cells[y][x] = values[idx++];
        }
    }
    return true;
}

// Save current game state to config file
static void SaveGameState(void) {
    if (!g_configInitialized) return;

    LlzPluginConfigSetInt(&g_config, "score", g_game.score);
    LlzPluginConfigSetInt(&g_config, "best_score", g_game.bestScore);
    LlzPluginConfigSetBool(&g_config, "game_over", g_game.gameOver);
    LlzPluginConfigSetBool(&g_config, "game_won", g_game.gameWon);

    char boardStr[256];
    SerializeBoard(boardStr, sizeof(boardStr));
    LlzPluginConfigSetString(&g_config, "board", boardStr);

    LlzPluginConfigSave(&g_config);
}

// Load game state from config file
static bool LoadGameState(void) {
    if (!g_configInitialized) return false;

    const char *boardStr = LlzPluginConfigGetString(&g_config, "board");
    if (!boardStr || !DeserializeBoard(boardStr)) {
        return false;
    }

    // Check if board is empty (all zeros) - means no real game saved yet
    bool hasAnyTile = false;
    for (int y = 0; y < BOARD_SIZE && !hasAnyTile; y++) {
        for (int x = 0; x < BOARD_SIZE && !hasAnyTile; x++) {
            if (g_game.cells[y][x] != 0) {
                hasAnyTile = true;
            }
        }
    }
    if (!hasAnyTile) {
        return false;  // Empty board - need to start new game
    }

    g_game.score = LlzPluginConfigGetInt(&g_config, "score", 0);
    g_game.bestScore = LlzPluginConfigGetInt(&g_config, "best_score", 0);
    g_game.gameOver = LlzPluginConfigGetBool(&g_config, "game_over", false);
    g_game.gameWon = LlzPluginConfigGetBool(&g_config, "game_won", false);

    return true;
}

static void GameReset(bool clearBest);
static bool GameMoveLeft(void);
static bool GameMoveRight(void);
static bool GameMoveUp(void);
static bool GameMoveDown(void);
static bool GameSpawnRandomTile(void);
static bool GameCanMove(void);
static void GamePostMove(bool moved);

static void DrawBoard(void);
static Rectangle ComputeBoardRect(void);
static Rectangle ComputeNewGameRect(void);
static void DrawScorePanels(void);
static void DrawStatusOverlay(Rectangle boardRect);
static Color TileColor(int value);
static int TileFontSize(float tileSize, int value);

static bool SlideLine(int line[BOARD_SIZE]);

static void GameReset(bool clearBest)
{
    memset(g_game.cells, 0, sizeof(g_game.cells));
    g_game.score = 0;
    if (clearBest) g_game.bestScore = 0;
    g_game.gameOver = false;
    g_game.gameWon = false;
    g_game.winContinued = false;
    g_game.statusTimer = 0.0f;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            g_tileAnimOffsetX[y][x] = 0.0f;
            g_tileAnimOffsetY[y][x] = 0.0f;
            g_tileSpawnTimer[y][x] = 0.0f;
        }
    }
    g_slideTimer = 0.0f;
    GameSpawnRandomTile();
    GameSpawnRandomTile();
    // Save new game state
    SaveGameState();
}

static bool GameSpawnRandomTile(void)
{
    int empty[TILE_COUNT];
    int count = 0;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (g_game.cells[y][x] == 0) {
                empty[count++] = y * BOARD_SIZE + x;
            }
        }
    }
    if (count == 0) return false;
    int choice = GetRandomValue(0, count - 1);
    int idx = empty[choice];
    int y = idx / BOARD_SIZE;
    int x = idx % BOARD_SIZE;
    int value = (GetRandomValue(0, 9) < 9) ? 2 : 4;
    g_game.cells[y][x] = value;
    g_tileAnimOffsetX[y][x] = 0.0f;
    g_tileAnimOffsetY[y][x] = 0.0f;
    g_tileSpawnTimer[y][x] = SPAWN_DURATION;
    return true;
}

static void CompressLine(const int source[BOARD_SIZE], const int sourceIndices[BOARD_SIZE],
                         int dest[BOARD_SIZE], int destOrigins[BOARD_SIZE], bool destMerged[BOARD_SIZE],
                         bool preferGreaterOrigin)
{
    for (int i = 0; i < BOARD_SIZE; i++) {
        dest[i] = 0;
        destOrigins[i] = -1;
        destMerged[i] = false;
    }

    int target = 0;
    int prevValue = 0;
    int prevOrigin = -1;

    for (int i = 0; i < BOARD_SIZE; i++) {
        int value = source[i];
        if (value == 0) continue;

        int origin = sourceIndices[i];
        if (prevValue == value) {
            dest[target - 1] = value * 2;
            if (preferGreaterOrigin) {
                destOrigins[target - 1] = (origin > prevOrigin) ? origin : prevOrigin;
            } else {
                destOrigins[target - 1] = (origin < prevOrigin) ? origin : prevOrigin;
            }
            destMerged[target - 1] = true;
            g_game.score += value * 2;
            if (dest[target - 1] >= 2048) {
                g_game.gameWon = true;
            }
            prevValue = 0;
            prevOrigin = -1;
        } else {
            dest[target] = value;
            destOrigins[target] = origin;
            prevValue = value;
            prevOrigin = origin;
            target++;
        }
    }
}

static bool GameMoveLeft(void)
{
    bool moved = false;
    for (int y = 0; y < BOARD_SIZE; y++) {
        int source[BOARD_SIZE];
        int indices[BOARD_SIZE];
        int dest[BOARD_SIZE];
        int origins[BOARD_SIZE];
        bool merged[BOARD_SIZE];
        for (int x = 0; x < BOARD_SIZE; x++) {
            source[x] = g_game.cells[y][x];
            indices[x] = x;
        }
        CompressLine(source, indices, dest, origins, merged, false);
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (g_game.cells[y][x] != dest[x]) moved = true;
            g_game.cells[y][x] = dest[x];
            if (dest[x] != 0 && origins[x] >= 0) {
                g_tileAnimOffsetX[y][x] = (float)(origins[x] - x);
                g_tileAnimOffsetY[y][x] = 0.0f;
                g_tileSpawnTimer[y][x] = merged[x] ? SPAWN_DURATION * 1.2f : 0.0f;  // Add delay for merged tiles
            } else {
                g_tileAnimOffsetX[y][x] = 0.0f;
                g_tileAnimOffsetY[y][x] = 0.0f;
                g_tileSpawnTimer[y][x] = 0.0f;
            }
        }
    }
    if (moved) g_slideTimer = SLIDE_DURATION;
    GamePostMove(moved);
    return moved;
}

static bool GameMoveRight(void)
{
    bool moved = false;
    for (int y = 0; y < BOARD_SIZE; y++) {
        int source[BOARD_SIZE];
        int indices[BOARD_SIZE];
        int dest[BOARD_SIZE];
        int origins[BOARD_SIZE];
        bool merged[BOARD_SIZE];
        for (int x = 0; x < BOARD_SIZE; x++) {
            source[x] = g_game.cells[y][BOARD_SIZE - 1 - x];
            indices[x] = BOARD_SIZE - 1 - x;
        }
        CompressLine(source, indices, dest, origins, merged, true);
        for (int x = 0; x < BOARD_SIZE; x++) {
            int destIndex = BOARD_SIZE - 1 - x;
            if (g_game.cells[y][destIndex] != dest[x]) moved = true;
            g_game.cells[y][destIndex] = dest[x];
            if (dest[x] != 0 && origins[x] >= 0) {
                g_tileAnimOffsetX[y][destIndex] = (float)(origins[x] - destIndex);
                g_tileAnimOffsetY[y][destIndex] = 0.0f;
                g_tileSpawnTimer[y][destIndex] = merged[x] ? SPAWN_DURATION * 1.2f : 0.0f;  // Add delay for merged tiles
            } else {
                g_tileAnimOffsetX[y][destIndex] = 0.0f;
                g_tileAnimOffsetY[y][destIndex] = 0.0f;
                g_tileSpawnTimer[y][destIndex] = 0.0f;
            }
        }
    }
    if (moved) g_slideTimer = SLIDE_DURATION;
    GamePostMove(moved);
    return moved;
}

static bool GameMoveUp(void)
{
    bool moved = false;
    for (int x = 0; x < BOARD_SIZE; x++) {
        int source[BOARD_SIZE];
        int indices[BOARD_SIZE];
        int dest[BOARD_SIZE];
        int origins[BOARD_SIZE];
        bool merged[BOARD_SIZE];
        for (int y = 0; y < BOARD_SIZE; y++) {
            source[y] = g_game.cells[y][x];
            indices[y] = y;
        }
        CompressLine(source, indices, dest, origins, merged, false);
        for (int y = 0; y < BOARD_SIZE; y++) {
            if (g_game.cells[y][x] != dest[y]) moved = true;
            g_game.cells[y][x] = dest[y];
            if (dest[y] != 0 && origins[y] >= 0) {
                g_tileAnimOffsetX[y][x] = 0.0f;
                g_tileAnimOffsetY[y][x] = (float)(origins[y] - y);
                g_tileSpawnTimer[y][x] = merged[y] ? SPAWN_DURATION * 1.2f : 0.0f;  // Add delay for merged tiles
            } else {
                g_tileAnimOffsetX[y][x] = 0.0f;
                g_tileAnimOffsetY[y][x] = 0.0f;
                g_tileSpawnTimer[y][x] = 0.0f;
            }
        }
    }
    if (moved) g_slideTimer = SLIDE_DURATION;
    GamePostMove(moved);
    return moved;
}

static bool GameMoveDown(void)
{
    bool moved = false;
    for (int x = 0; x < BOARD_SIZE; x++) {
        int source[BOARD_SIZE];
        int indices[BOARD_SIZE];
        int dest[BOARD_SIZE];
        int origins[BOARD_SIZE];
        bool merged[BOARD_SIZE];
        for (int y = 0; y < BOARD_SIZE; y++) {
            source[y] = g_game.cells[BOARD_SIZE - 1 - y][x];
            indices[y] = BOARD_SIZE - 1 - y;
        }
        CompressLine(source, indices, dest, origins, merged, true);
        for (int y = 0; y < BOARD_SIZE; y++) {
            int destIndex = BOARD_SIZE - 1 - y;
            if (g_game.cells[destIndex][x] != dest[y]) moved = true;
            g_game.cells[destIndex][x] = dest[y];
            if (dest[y] != 0 && origins[y] >= 0) {
                g_tileAnimOffsetX[destIndex][x] = 0.0f;
                g_tileAnimOffsetY[destIndex][x] = (float)(origins[y] - destIndex);
                g_tileSpawnTimer[destIndex][x] = merged[y] ? SPAWN_DURATION * 1.2f : 0.0f;  // Add delay for merged tiles
            } else {
                g_tileAnimOffsetX[destIndex][x] = 0.0f;
                g_tileAnimOffsetY[destIndex][x] = 0.0f;
                g_tileSpawnTimer[destIndex][x] = 0.0f;
            }
        }
    }
    if (moved) g_slideTimer = SLIDE_DURATION;
    GamePostMove(moved);
    return moved;
}

static bool GameCanMove(void)
{
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            int value = g_game.cells[y][x];
            if (value == 0) return true;
            if (x + 1 < BOARD_SIZE && g_game.cells[y][x + 1] == value) return true;
            if (y + 1 < BOARD_SIZE && g_game.cells[y + 1][x] == value) return true;
        }
    }
    return false;
}

static void GamePostMove(bool moved)
{
    if (!moved) return;
    g_game.statusTimer = 0.0f;
    // If player made a move after winning, hide the win overlay
    if (g_game.gameWon && !g_game.winContinued) {
        g_game.winContinued = true;
    }
    GameSpawnRandomTile();
    if (g_game.score > g_game.bestScore) {
        g_game.bestScore = g_game.score;
    }
    if (!GameCanMove()) {
        g_game.gameOver = true;
    }
    // Save game state after every move
    SaveGameState();
}

static Rectangle ComputeBoardRect(void)
{
    float marginX = 32.0f;
    float marginTop = 120.0f;  // Reduced from 150 to give more space
    float marginBottom = 32.0f;
    float availableWidth = g_screenWidth - marginX * 2.0f;
    float availableHeight = g_screenHeight - marginTop - marginBottom;
    float boardSize = (availableWidth < availableHeight) ? availableWidth : availableHeight;
    float offsetX = marginX + (availableWidth - boardSize) * 0.5f;
    return (Rectangle){offsetX, marginTop, boardSize, boardSize};
}

static Rectangle ComputeNewGameRect(void)
{
    float panelWidth = 120.0f;
    float spacing = 16.0f;
    float x = g_screenWidth - (panelWidth * 2.0f + spacing + 20.0f);  // 20px right margin
    Rectangle scoreRect = {x, 16.0f, panelWidth, 50.0f};  // Moved up and made smaller
    Rectangle bestRect = {scoreRect.x + panelWidth + spacing * 0.5f, 16.0f, panelWidth, 50.0f};
    Rectangle newGameRect = {scoreRect.x, scoreRect.y + scoreRect.height + spacing * 0.4f,
                             bestRect.x + bestRect.width - scoreRect.x, 40.0f};  // Smaller height
    return newGameRect;
}

static Color TileColor(int value)
{
    switch (value) {
        case 2: return (Color){238, 228, 218, 255};
        case 4: return (Color){237, 224, 200, 255};
        case 8: return (Color){242, 177, 121, 255};
        case 16: return (Color){245, 149, 99, 255};
        case 32: return (Color){246, 124, 95, 255};
        case 64: return (Color){246, 94, 59, 255};
        case 128: return (Color){237, 207, 114, 255};
        case 256: return (Color){237, 204, 97, 255};
        case 512: return (Color){237, 200, 80, 255};
        case 1024: return (Color){237, 197, 63, 255};
        case 2048: return (Color){237, 194, 46, 255};
        default:
            if (value > 2048) return (Color){60, 58, 50, 255};
            return (Color){204, 192, 179, 255};
    }
}

static int TileFontSize(float tileSize, int value)
{
    if (value < 100) return (int)(tileSize * 0.45f);
    if (value < 1000) return (int)(tileSize * 0.40f);
    return (int)(tileSize * 0.33f);
}

static void DrawScorePanel(Rectangle rect, const char *label, int value)
{
    DrawRectangleRounded(rect, 0.2f, 12, COLOR_PANEL_LIGHT);
    DrawText(label, (int)(rect.x + 10), (int)(rect.y + 6), 14, COLOR_TEXT_MUTED);  // Smaller text
    char text[32];
    snprintf(text, sizeof(text), "%d", value);
    int fontSize = 22;  // Smaller font
    int textWidth = MeasureText(text, fontSize);
    int textX = (int)(rect.x + rect.width * 0.5f - textWidth * 0.5f);
    int textY = (int)(rect.y + rect.height - fontSize - 4);
    DrawText(text, textX, textY, fontSize, COLOR_TEXT_PRIMARY);
}

static void DrawScorePanels(void)
{
    float panelWidth = 120.0f;
    float spacing = 16.0f;
    float x = g_screenWidth - (panelWidth * 2.0f + spacing + 20.0f);
    Rectangle scoreRect = {x, 16.0f, panelWidth, 50.0f};
    Rectangle bestRect = {scoreRect.x + panelWidth + spacing * 0.5f, 16.0f, panelWidth, 50.0f};
    Rectangle newGameRect = ComputeNewGameRect();

    DrawScorePanel(scoreRect, "SCORE", g_game.score);
    DrawScorePanel(bestRect, "BEST", g_game.bestScore);

    Color btnColor = g_game.gameOver ? (Color){222, 86, 92, 255} : (Color){96, 178, 255, 255};
    DrawRectangleRounded(newGameRect, 0.3f, 12, btnColor);
    const char *label = g_game.gameOver ? "TRY AGAIN" : "NEW GAME";
    int fontSize = 18;  // Smaller font for button
    int textWidth = MeasureText(label, fontSize);
    int textX = (int)(newGameRect.x + newGameRect.width * 0.5f - textWidth * 0.5f);
    int textY = (int)(newGameRect.y + newGameRect.height * 0.5f - fontSize * 0.5f);
    DrawText(label, textX, textY, fontSize, WHITE);
}

static void DrawHeader(void)
{
    // Draw title on the left side, smaller
    const char *title = "2048";
    int titleSize = 36;
    DrawText(title, 32, 20, titleSize, COLOR_TEXT_PRIMARY);

    // Draw smaller subtitle underneath
    const char *subtitle = "Swipe or use buttons";
    int subtitleSize = 16;
    DrawText(subtitle, 32, 60, subtitleSize, COLOR_TEXT_MUTED);
}

static void DrawStatusOverlay(Rectangle boardRect)
{
    // Don't show overlay if player continued after winning
    if (!g_game.gameOver && (!g_game.gameWon || g_game.winContinued)) return;

    Color overlay = ColorAlpha(BLACK, 0.55f);
    DrawRectangleRounded(boardRect, 0.04f, 20, overlay);

    const char *title = g_game.gameWon ? "YOU WIN!" : "GAME OVER";
    const char *subtitle = g_game.gameWon ? "Swipe to keep playing or start a new run"
                                          : "No more moves. Tap NEW GAME to restart";
    int titleSize = 42;
    int subtitleSize = 20;
    int titleWidth = MeasureText(title, titleSize);
    int subtitleWidth = MeasureText(subtitle, subtitleSize);
    int titleX = (int)(boardRect.x + boardRect.width * 0.5f - titleWidth * 0.5f);
    int titleY = (int)(boardRect.y + boardRect.height * 0.4f - titleSize);
    DrawText(title, titleX, titleY, titleSize, WHITE);
    DrawText(subtitle,
             (int)(boardRect.x + boardRect.width * 0.5f - subtitleWidth * 0.5f),
             titleY + titleSize + 10,
             subtitleSize,
             COLOR_TEXT_PRIMARY);
}

static void DrawBoard(void)
{
    Rectangle boardRect = ComputeBoardRect();
    float gap = boardRect.width * 0.04f / (BOARD_SIZE + 1);
    if (gap < 6.0f) gap = 6.0f;
    float tileSize = (boardRect.width - gap * (BOARD_SIZE + 1)) / BOARD_SIZE;
    float tileStep = tileSize + gap;
    float animFactor = (g_slideTimer > 0.0f && SLIDE_DURATION > 0.0f)
        ? (g_slideTimer / SLIDE_DURATION)
        : 0.0f;
    if (animFactor < 0.0f) animFactor = 0.0f;
    if (animFactor > 1.0f) animFactor = 1.0f;
    float startX = boardRect.x + gap;
    float startY = boardRect.y + gap;

    DrawRectangleRounded(boardRect, 0.04f, 18, COLOR_PANEL);

    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            float tileX = startX + x * (tileSize + gap);
            float tileY = startY + y * (tileSize + gap);
            float offsetX = g_tileAnimOffsetX[y][x] * tileStep * animFactor;
            float offsetY = g_tileAnimOffsetY[y][x] * tileStep * animFactor;
            Rectangle tileRect = {tileX + offsetX, tileY + offsetY, tileSize, tileSize};
            int value = g_game.cells[y][x];
            float spawnFactor = 1.0f;
            float spawnAlpha = 1.0f;
            if (g_tileSpawnTimer[y][x] > 0.0f && SPAWN_DURATION > 0.0f) {
                float spawnT = 1.0f - (g_tileSpawnTimer[y][x] / SPAWN_DURATION);
                if (spawnT < 0.0f) spawnT = 0.0f;
                if (spawnT > 1.0f) spawnT = 1.0f;
                spawnFactor = 0.6f + 0.4f * spawnT;
                spawnAlpha = 0.35f + 0.65f * spawnT;
            }
            float scaledSize = tileSize * spawnFactor;
            tileRect.x += (tileSize - scaledSize) * 0.5f;
            tileRect.y += (tileSize - scaledSize) * 0.5f;
            tileRect.width = scaledSize;
            tileRect.height = scaledSize;
            Color base = value ? TileColor(value) : COLOR_PANEL_LIGHT;
            DrawRectangleRounded(tileRect, 0.18f, 14, ColorAlpha(base, spawnAlpha));

            if (value > 0) {
                char text[16];
                snprintf(text, sizeof(text), "%d", value);
                int fontSize = TileFontSize(tileSize, value);
                Color textColor = (value <= 4) ? COLOR_TILE_DARK : WHITE;
                int textWidth = MeasureText(text, fontSize);
                int textX = (int)(tileRect.x + tileRect.width * 0.5f - textWidth * 0.5f);
                int textY = (int)(tileRect.y + tileRect.height * 0.5f - fontSize * 0.5f);
                DrawText(text, textX, textY, fontSize, ColorAlpha(textColor, spawnAlpha));
            }
        }
    }

    DrawStatusOverlay(boardRect);
}

// Notification tap callback - closes plugin when banner is tapped
static void OnNotificationTap(void *userData)
{
    (void)userData;
    g_wantsClose = true;
    printf("[SWIPE_2048] Notification tapped - closing to open Now Playing\n");
}

// Track change callback - called when a new song starts
static void OnTrackChanged(const char *track, const char *artist, const char *album, void *userData)
{
    (void)album;
    (void)userData;

    // Build notification message
    char message[256];
    if (artist && artist[0] && track && track[0]) {
        snprintf(message, sizeof(message), "%s - %s", artist, track);
    } else if (track && track[0]) {
        snprintf(message, sizeof(message), "%s", track);
    } else if (artist && artist[0]) {
        snprintf(message, sizeof(message), "%s", artist);
    } else {
        return;  // No content to show
    }

    // Show banner notification using the notification system
    LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_BANNER);
    strncpy(config.message, message, LLZ_NOTIFY_TEXT_MAX - 1);
    strncpy(config.iconText, "♪", LLZ_NOTIFY_ICON_MAX - 1);
    config.duration = 5.0f;
    config.position = LLZ_NOTIFY_POS_TOP;
    strncpy(config.openPluginOnTap, "Now Playing", LLZ_NOTIFY_PLUGIN_NAME_MAX - 1);
    config.onTap = OnNotificationTap;  // Close plugin when tapped

    LlzNotifyShow(&config);

    printf("[SWIPE_2048] New track notification: %s\n", message);
}

static void PluginInit(int width, int height)
{
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;

    // Initialize plugin config with defaults
    LlzPluginConfigEntry defaults[] = {
        {"score", "0"},
        {"best_score", "0"},
        {"game_over", "false"},
        {"game_won", "false"},
        {"board", "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"}
    };
    g_configInitialized = LlzPluginConfigInit(&g_config, "swipe_2048", defaults, 5);

    // Load best score first (preserve across sessions even when starting new game)
    int savedBestScore = 0;
    if (g_configInitialized) {
        savedBestScore = LlzPluginConfigGetInt(&g_config, "best_score", 0);
    }

    // Try to load saved game state
    if (g_configInitialized && LoadGameState()) {
        // Loaded successfully - restore animation states to idle
        g_game.statusTimer = 0.0f;
        for (int y = 0; y < BOARD_SIZE; y++) {
            for (int x = 0; x < BOARD_SIZE; x++) {
                g_tileAnimOffsetX[y][x] = 0.0f;
                g_tileAnimOffsetY[y][x] = 0.0f;
                g_tileSpawnTimer[y][x] = 0.0f;
            }
        }
        g_slideTimer = 0.0f;
        printf("[SWIPE_2048] Restored saved game state (score=%d, best=%d)\n",
               g_game.score, g_game.bestScore);
    } else {
        // No valid saved state - start fresh but keep best score
        GameReset(true);
        g_game.bestScore = savedBestScore;
        SaveGameState();  // Save with the preserved best score
    }

    // Initialize notification system
    LlzNotifyInit(width, height);

    // Initialize media subscription for now playing notifications
    if (LlzMediaInit(NULL)) {
        g_mediaInitialized = true;
        g_trackSubId = LlzSubscribeTrackChanged(OnTrackChanged, NULL);
        printf("[SWIPE_2048] Subscribed to track changes (id=%d)\n", g_trackSubId);
    } else {
        g_mediaInitialized = false;
        g_trackSubId = 0;
        printf("[SWIPE_2048] Media init failed - notifications disabled\n");
    }
}

static void HandleInput(const LlzInputState *input)
{
    if (!input) return;

    if (input->backReleased) {
        g_wantsClose = true;
    }

    // Notification input is handled by LlzNotifyUpdate in PluginUpdate

    bool animating = g_slideTimer > 0.0f;
    bool moved = false;
    if (!animating) {
        // Handle rotary encoder for left/right movement
        if (input->scrollDelta > 0.5f) {
            moved = GameMoveRight();
        } else if (input->scrollDelta < -0.5f) {
            moved = GameMoveLeft();
        }
        // Drag-based swipe detection (matching llzblocks flick sensitivity)
        else if (input->dragActive) {
            g_dragAccumX += input->dragDelta.x;
            g_dragAccumY += input->dragDelta.y;

            // Check dominant direction and trigger move when threshold exceeded
            // For 2048, only one move per gesture (unlike Tetris which allows continuous movement)
            if (fabsf(g_dragAccumX) >= DRAG_THRESHOLD || fabsf(g_dragAccumY) >= DRAG_THRESHOLD) {
                if (fabsf(g_dragAccumX) > fabsf(g_dragAccumY)) {
                    // Horizontal movement dominant
                    if (g_dragAccumX > 0) {
                        moved = GameMoveRight();
                    } else {
                        moved = GameMoveLeft();
                    }
                } else {
                    // Vertical movement dominant
                    if (g_dragAccumY > 0) {
                        moved = GameMoveDown();
                    } else {
                        moved = GameMoveUp();
                    }
                }
                // Reset both accumulators after a move (one move per gesture)
                g_dragAccumX = 0.0f;
                g_dragAccumY = 0.0f;
            }
        }
        // Discrete swipe events (fallback for quick flicks)
        else if (input->swipeLeft) moved = GameMoveLeft();
        else if (input->swipeRight) moved = GameMoveRight();
        else if (input->swipeUp) moved = GameMoveUp();
        else if (input->swipeDown) moved = GameMoveDown();
        // Keyboard and button controls
        else if (IsKeyPressed(KEY_LEFT)) moved = GameMoveLeft();
        else if (IsKeyPressed(KEY_RIGHT)) moved = GameMoveRight();
        else if (IsKeyPressed(KEY_UP) || input->downPressed) moved = GameMoveUp();
        else if (IsKeyPressed(KEY_DOWN) || input->upPressed) moved = GameMoveDown();
    }

    // Reset drag accumulators when not dragging
    if (!input->dragActive) {
        g_dragAccumX = 0.0f;
        g_dragAccumY = 0.0f;
    }

    Rectangle newGameRect = ComputeNewGameRect();
    bool tappedNewGame = false;
    if (input->tap && CheckCollisionPointRec(input->tapPosition, newGameRect)) {
        tappedNewGame = true;
    }
    if (input->mouseJustReleased && CheckCollisionPointRec(input->mousePos, newGameRect)) {
        tappedNewGame = true;
    }

    if (tappedNewGame) {
        GameReset(false);
        return;
    }

    if (moved) {
        g_game.statusTimer = 0.0f;
    } else {
        g_game.statusTimer += GetFrameTime();
    }
}

static void PluginUpdate(const LlzInputState *input, float deltaTime)
{
    // Poll for track change events
    if (g_mediaInitialized) {
        LlzSubscriptionPoll();
    }

    // Update notification system (handles input and animation)
    bool notifyBlocking = LlzNotifyUpdate(input, deltaTime);

    // If a dialog is blocking, skip game input
    if (notifyBlocking && LlzNotifyIsBlocking()) {
        return;
    }

    if (g_slideTimer > 0.0f) {
        g_slideTimer -= deltaTime;
        if (g_slideTimer < 0.0f) g_slideTimer = 0.0f;
    }
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (g_tileSpawnTimer[y][x] > 0.0f) {
                g_tileSpawnTimer[y][x] -= deltaTime;
                if (g_tileSpawnTimer[y][x] < 0.0f) g_tileSpawnTimer[y][x] = 0.0f;
            }
        }
    }
    HandleInput(input);
}

static void PluginDraw(void)
{
    ClearBackground(COLOR_BG);
    DrawHeader();
    DrawScorePanels();
    DrawBoard();

    // Draw notification overlay on top
    LlzNotifyDraw();
}

static void PluginShutdown(void)
{
    // Shutdown notification system
    LlzNotifyShutdown();

    // Unsubscribe and cleanup media
    if (g_trackSubId != 0) {
        LlzUnsubscribe(g_trackSubId);
        g_trackSubId = 0;
    }
    if (g_mediaInitialized) {
        LlzMediaShutdown();
        g_mediaInitialized = false;
    }

    // Save final state and cleanup config
    if (g_configInitialized) {
        SaveGameState();
        LlzPluginConfigFree(&g_config);
        g_configInitialized = false;
    }
    g_wantsClose = false;
}

static bool PluginWantsClose(void)
{
    return g_wantsClose;
}

static LlzPluginAPI g_api = {
    .name = "Swipe 2048",
    .description = "Touch-friendly 2048 clone with swipe + hardware input",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose
};

const LlzPluginAPI *LlzGetPlugin(void)
{
    return &g_api;
}
