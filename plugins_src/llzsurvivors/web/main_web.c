// main_web.c - Standalone web entry point for LLZ Survivors
// Compiles with Emscripten to run in browser

#include "raylib.h"

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

// Include the SDK stub (provides all Llz* functions)
#include "llz_sdk_stub.h"

// Forward declarations from the game
void GameInit(int width, int height);
void GameUpdate(const LlzInputState *input, float dt);
void GameDraw(void);
void GameShutdown(void);
bool GameWantsClose(void);

// Screen dimensions (CarThing resolution, works well for web too)
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Main loop function for Emscripten
static void UpdateDrawFrame(void) {
    // Update input
    LlzInputUpdate();
    const LlzInputState *input = LlzInputGet();

    // Update game
    float dt = GetFrameTime();
    GameUpdate(input, dt);

    // Draw
    BeginDrawing();
    ClearBackground(BLACK);
    GameDraw();

    // Draw FPS in debug builds
    #ifndef NDEBUG
    DrawFPS(10, 10);
    #endif

    EndDrawing();
}

int main(void) {
    // Initialize window
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "LLZ Survivors");

    #ifdef PLATFORM_WEB
    // Web: Run at display refresh rate
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
    #else
    // Desktop: 60 FPS target
    SetTargetFPS(60);

    // Initialize game
    GameInit(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Main game loop
    while (!WindowShouldClose() && !GameWantsClose()) {
        UpdateDrawFrame();
    }

    // Cleanup
    GameShutdown();
    CloseWindow();
    #endif

    return 0;
}

#ifdef PLATFORM_WEB
// Called by Emscripten before main loop starts
void EMSCRIPTEN_KEEPALIVE WebInit(void) {
    GameInit(SCREEN_WIDTH, SCREEN_HEIGHT);
}
#endif
