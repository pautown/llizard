#include "llz_sdk_display.h"

#include <stdlib.h>

#ifdef PLATFORM_DRM
#define DRM_NATIVE_WIDTH 480
#define DRM_NATIVE_HEIGHT 800
static RenderTexture2D g_target = {0};
static bool g_targetReady = false;
#endif

static bool g_windowReady = false;

bool LlzDisplayInit(void)
{
#ifdef PLATFORM_DRM
    const char *traceEnv = getenv("LLZ_RAYLIB_TRACE");
    if (traceEnv && traceEnv[0] != '\0' && traceEnv[0] != '0') {
        SetTraceLogLevel(LOG_TRACE);
        TraceLog(LOG_INFO, "LlzDisplay: verbose raylib tracing enabled via LLZ_RAYLIB_TRACE");
    }
#endif
#ifdef PLATFORM_DRM
    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_FULLSCREEN_MODE);
    InitWindow(DRM_NATIVE_WIDTH, DRM_NATIVE_HEIGHT, "llizardgui-host");
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(LLZ_LOGICAL_WIDTH, LLZ_LOGICAL_HEIGHT, "llizardgui-host");
#endif

    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "LlzDisplay: failed to initialize raylib window");
        return false;
    }

    g_windowReady = true;

#ifdef PLATFORM_DRM
    g_target = LoadRenderTexture(LLZ_LOGICAL_WIDTH, LLZ_LOGICAL_HEIGHT);
    if (g_target.id == 0) {
        TraceLog(LOG_ERROR, "LlzDisplay: failed to allocate render target");
        CloseWindow();
        g_windowReady = false;
        return false;
    }
    g_targetReady = true;
    SetTextureFilter(g_target.texture, TEXTURE_FILTER_BILINEAR);
#endif

    SetTargetFPS(60);
    return true;
}

void LlzDisplayBegin(void)
{
    if (!g_windowReady) return;
#ifdef PLATFORM_DRM
    if (!g_targetReady) return;
    BeginTextureMode(g_target);
    ClearBackground(BLACK);
#else
    BeginDrawing();
    ClearBackground(BLACK);
#endif
}

void LlzDisplayEnd(void)
{
    if (!g_windowReady) return;
#ifdef PLATFORM_DRM
    if (!g_targetReady) return;
    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle src = {0.0f, 0.0f, (float)g_target.texture.width, -(float)g_target.texture.height};
    Rectangle dst = {DRM_NATIVE_WIDTH / 2.0f, DRM_NATIVE_HEIGHT / 2.0f, (float)DRM_NATIVE_HEIGHT, (float)DRM_NATIVE_WIDTH};
    Vector2 origin = {dst.width / 2.0f, dst.height / 2.0f};
    DrawTexturePro(g_target.texture, src, dst, origin, 90.0f, WHITE);
    EndDrawing();
#else
    EndDrawing();
#endif
}

void LlzDisplayShutdown(void)
{
#ifdef PLATFORM_DRM
    if (g_targetReady) {
        UnloadRenderTexture(g_target);
        g_targetReady = false;
    }
#endif
    if (g_windowReady) {
        CloseWindow();
        g_windowReady = false;
    }
}
