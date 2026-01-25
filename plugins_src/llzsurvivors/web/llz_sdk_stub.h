// llz_sdk_stub.h - Minimal SDK stubs for standalone/web builds
// Provides the SDK functions used by LLZ Survivors without the full SDK

#ifndef LLZ_SDK_STUB_H
#define LLZ_SDK_STUB_H

#include "raylib.h"
#include <math.h>

// =============================================================================
// Font System Stub
// =============================================================================

typedef enum {
    LLZ_FONT_UI = 0,
    LLZ_FONT_MONO,
    LLZ_FONT_COUNT
} LlzFontType;

static Font g_defaultFont = {0};
static bool g_fontInitialized = false;

static inline Font LlzFontGet(LlzFontType type, int size) {
    (void)type; (void)size;
    if (!g_fontInitialized) {
        g_defaultFont = GetFontDefault();
        g_fontInitialized = true;
    }
    return g_defaultFont;
}

// =============================================================================
// Background System Stub
// =============================================================================

typedef enum {
    LLZ_BG_STYLE_NONE = 0,
    LLZ_BG_STYLE_GRADIENT,
    LLZ_BG_STYLE_CONSTELLATION,
    LLZ_BG_STYLE_MATRIX,
    LLZ_BG_STYLE_WAVES,
    LLZ_BG_STYLE_CIRCLES,
    LLZ_BG_STYLE_PARTICLES,
    LLZ_BG_STYLE_GRID,
    LLZ_BG_STYLE_AURORA,
    LLZ_BG_STYLE_STARS,
    LLZ_BG_STYLE_COUNT
} LlzBackgroundStyle;

static Color g_bgColor1 = {20, 30, 50, 255};
static Color g_bgColor2 = {0, 150, 200, 255};
static int g_bgWidth = 800, g_bgHeight = 480;
static float g_bgTime = 0;

// Simple starfield for constellation style
#define MAX_BG_STARS 100
static Vector2 g_bgStars[MAX_BG_STARS];
static float g_bgStarBrightness[MAX_BG_STARS];
static bool g_bgInitialized = false;

static inline void LlzBackgroundInit(int width, int height) {
    g_bgWidth = width;
    g_bgHeight = height;
    // Initialize stars
    for (int i = 0; i < MAX_BG_STARS; i++) {
        g_bgStars[i] = (Vector2){(float)GetRandomValue(0, width), (float)GetRandomValue(0, height)};
        g_bgStarBrightness[i] = (float)GetRandomValue(30, 100) / 100.0f;
    }
    g_bgInitialized = true;
}

static inline void LlzBackgroundSetStyle(LlzBackgroundStyle style, bool animate) {
    (void)style; (void)animate;
}

static inline void LlzBackgroundSetColors(Color c1, Color c2) {
    g_bgColor1 = c1;
    g_bgColor2 = c2;
}

static inline void LlzBackgroundUpdate(float dt) {
    g_bgTime += dt;
    // Twinkle stars
    for (int i = 0; i < MAX_BG_STARS; i++) {
        g_bgStarBrightness[i] = 0.5f + 0.5f * sinf(g_bgTime * 2.0f + i * 0.5f);
    }
}

static inline void LlzBackgroundDraw(void) {
    // Gradient background
    DrawRectangleGradientV(0, 0, g_bgWidth, g_bgHeight, g_bgColor1,
        (Color){g_bgColor1.r/2, g_bgColor1.g/2, g_bgColor1.b/2, 255});

    // Draw stars
    for (int i = 0; i < MAX_BG_STARS; i++) {
        unsigned char alpha = (unsigned char)(g_bgStarBrightness[i] * 200);
        Color starColor = {g_bgColor2.r, g_bgColor2.g, g_bgColor2.b, alpha};
        DrawCircleV(g_bgStars[i], 1.5f, starColor);
    }
}

static inline void LlzBackgroundShutdown(void) {
    g_bgInitialized = false;
}

// =============================================================================
// Gem Color System Stub
// =============================================================================

// Gem color enum - must match sdk/include/llz_sdk_shapes.h order exactly
typedef enum {
    LLZ_GEM_RUBY,
    LLZ_GEM_AMBER,
    LLZ_GEM_TOPAZ,
    LLZ_GEM_EMERALD,
    LLZ_GEM_SAPPHIRE,
    LLZ_GEM_AMETHYST,
    LLZ_GEM_DIAMOND,
    LLZ_GEM_PINK,
    LLZ_GEM_COUNT
} LlzGemColor;

// Color arrays - must match LlzGemColor enum order
static const Color LLZ_GEM_COLORS[] = {
    {220, 50, 50, 255},    // Ruby - red
    {255, 140, 0, 255},    // Amber - orange
    {255, 220, 0, 255},    // Topaz - gold
    {50, 200, 80, 255},    // Emerald - green
    {60, 120, 230, 255},   // Sapphire - blue
    {150, 80, 200, 255},   // Amethyst - purple
    {230, 230, 250, 255},  // Diamond - white
    {255, 105, 180, 255},  // Pink - rose
};

static const Color LLZ_GEM_COLORS_LIGHT[] = {
    {255, 120, 120, 255},  // Ruby light
    {255, 190, 80, 255},   // Amber light
    {255, 255, 120, 255},  // Topaz light
    {120, 255, 150, 255},  // Emerald light
    {140, 180, 255, 255},  // Sapphire light
    {200, 150, 255, 255},  // Amethyst light
    {255, 255, 255, 255},  // Diamond light
    {255, 182, 213, 255},  // Pink light
};

static inline Color LlzGetGemColor(LlzGemColor gem) {
    if (gem >= LLZ_GEM_COUNT) return WHITE;
    return LLZ_GEM_COLORS[gem];
}

static inline Color LlzGetGemColorLight(LlzGemColor gem) {
    if (gem >= LLZ_GEM_COUNT) return WHITE;
    return LLZ_GEM_COLORS_LIGHT[gem];
}

// =============================================================================
// Gem Shape Drawing Stub
// =============================================================================

// Shape enum - must match sdk/include/llz_sdk_shapes.h order exactly
typedef enum {
    LLZ_SHAPE_CIRCLE,
    LLZ_SHAPE_SQUARE,
    LLZ_SHAPE_DIAMOND,
    LLZ_SHAPE_TALL_DIAMOND,
    LLZ_SHAPE_TRIANGLE,
    LLZ_SHAPE_HEXAGON,
    LLZ_SHAPE_OCTAGON,
    LLZ_SHAPE_KITE,
    LLZ_SHAPE_STAR,
    LLZ_SHAPE_DUTCH_CUT,
    LLZ_SHAPE_COUNT
} LlzShapeType;

// Alias for compatibility
typedef LlzShapeType LlzGemShape;

static inline void LlzDrawGemShape(LlzGemShape shape, float x, float y, float size, LlzGemColor gemColor) {
    Color color = LlzGetGemColor(gemColor);
    Color light = LlzGetGemColorLight(gemColor);

    switch (shape) {
        case LLZ_SHAPE_CIRCLE:
            DrawCircle((int)x, (int)y, size, color);
            DrawCircle((int)x - size * 0.2f, (int)y - size * 0.2f, size * 0.3f, light);
            break;
        case LLZ_SHAPE_DIAMOND: {
            Vector2 pts[4] = {
                {x, y - size},
                {x + size * 0.7f, y},
                {x, y + size},
                {x - size * 0.7f, y}
            };
            DrawTriangle(pts[0], pts[1], pts[2], color);
            DrawTriangle(pts[0], pts[2], pts[3], color);
            DrawCircle((int)x, (int)(y - size * 0.3f), size * 0.2f, light);
            break;
        }
        case LLZ_SHAPE_TRIANGLE:
            DrawTriangle(
                (Vector2){x, y - size},
                (Vector2){x + size * 0.866f, y + size * 0.5f},
                (Vector2){x - size * 0.866f, y + size * 0.5f},
                color
            );
            break;
        case LLZ_SHAPE_STAR: {
            // 5-pointed star
            for (int i = 0; i < 5; i++) {
                float angle1 = (i * 72 - 90) * DEG2RAD;
                float angle2 = ((i + 2) * 72 - 90) * DEG2RAD;
                DrawTriangle(
                    (Vector2){x, y},
                    (Vector2){x + cosf(angle1) * size, y + sinf(angle1) * size},
                    (Vector2){x + cosf(angle2) * size, y + sinf(angle2) * size},
                    color
                );
            }
            DrawCircle((int)x, (int)y, size * 0.3f, light);
            break;
        }
        case LLZ_SHAPE_HEXAGON:
            for (int i = 0; i < 6; i++) {
                float a1 = i * 60 * DEG2RAD;
                float a2 = (i + 1) * 60 * DEG2RAD;
                DrawTriangle(
                    (Vector2){x, y},
                    (Vector2){x + cosf(a1) * size, y + sinf(a1) * size},
                    (Vector2){x + cosf(a2) * size, y + sinf(a2) * size},
                    color
                );
            }
            break;
        case LLZ_SHAPE_SQUARE:
            DrawRectangle((int)(x - size), (int)(y - size), (int)(size * 2), (int)(size * 2), color);
            break;
        case LLZ_SHAPE_TALL_DIAMOND: {
            Vector2 pts[4] = {
                {x, y - size * 1.3f},
                {x + size * 0.5f, y},
                {x, y + size * 1.3f},
                {x - size * 0.5f, y}
            };
            DrawTriangle(pts[0], pts[1], pts[2], color);
            DrawTriangle(pts[0], pts[2], pts[3], color);
            DrawCircle((int)x, (int)(y - size * 0.4f), size * 0.15f, light);
            break;
        }
        case LLZ_SHAPE_OCTAGON:
            for (int i = 0; i < 8; i++) {
                float a1 = (i * 45 + 22.5f) * DEG2RAD;
                float a2 = ((i + 1) * 45 + 22.5f) * DEG2RAD;
                DrawTriangle(
                    (Vector2){x, y},
                    (Vector2){x + cosf(a1) * size, y + sinf(a1) * size},
                    (Vector2){x + cosf(a2) * size, y + sinf(a2) * size},
                    color
                );
            }
            break;
        case LLZ_SHAPE_KITE: {
            Vector2 pts[4] = {
                {x, y - size * 1.2f},
                {x + size * 0.6f, y - size * 0.2f},
                {x, y + size},
                {x - size * 0.6f, y - size * 0.2f}
            };
            DrawTriangle(pts[0], pts[1], pts[2], color);
            DrawTriangle(pts[0], pts[2], pts[3], color);
            break;
        }
        case LLZ_SHAPE_DUTCH_CUT: {
            // Rectangular emerald cut shape
            float w = size * 0.8f, h = size * 1.2f;
            float corner = size * 0.25f;
            DrawRectangle((int)(x - w), (int)(y - h + corner), (int)(w * 2), (int)(h * 2 - corner * 2), color);
            DrawTriangle(
                (Vector2){x - w, y - h + corner},
                (Vector2){x - w + corner, y - h},
                (Vector2){x + w - corner, y - h},
                color
            );
            DrawTriangle(
                (Vector2){x - w, y - h + corner},
                (Vector2){x + w - corner, y - h},
                (Vector2){x + w, y - h + corner},
                color
            );
            break;
        }
        case LLZ_SHAPE_COUNT:
        default:
            // Fallback to circle for unknown shapes
            DrawCircle((int)x, (int)y, size, color);
            break;
    }
}

// =============================================================================
// Input System Stub (for web: keyboard/mouse/touch)
// =============================================================================

typedef struct {
    // Buttons (directly from keyboard)
    bool backPressed;
    bool backReleased;
    bool selectPressed;
    bool selectReleased;
    bool upPressed;
    bool downPressed;
    bool leftPressed;
    bool rightPressed;

    // Scroll (from mouse wheel or arrow keys)
    float scrollDelta;

    // Touch/mouse gestures
    bool tap;
    bool doubleTap;
    bool hold;
    bool swipeLeft;
    bool swipeRight;
    bool swipeUp;
    bool swipeDown;

    // Mouse state (for web/desktop)
    Vector2 mousePos;
    bool mousePressed;
    bool mouseJustPressed;
    bool mouseJustReleased;

    // Drag tracking
    bool dragActive;
    Vector2 dragStart;
    Vector2 dragCurrent;
    Vector2 dragDelta;

    // Touch position (legacy compatibility)
    Vector2 touchPos;
    bool touching;
} LlzInputState;

static LlzInputState g_webInput = {0};
static float g_lastTapTime = 0;
static Vector2 g_touchStart = {0};
static Vector2 g_prevMousePos = {0};
static bool g_wasTouching = false;
static bool g_wasMousePressed = false;

static inline void LlzInputUpdate(void) {
    // Reset one-frame events
    g_webInput.tap = false;
    g_webInput.doubleTap = false;
    g_webInput.swipeLeft = false;
    g_webInput.swipeRight = false;
    g_webInput.swipeUp = false;
    g_webInput.swipeDown = false;
    g_webInput.scrollDelta = 0;
    g_webInput.backReleased = false;
    g_webInput.selectReleased = false;
    g_webInput.mouseJustPressed = false;
    g_webInput.mouseJustReleased = false;
    g_webInput.dragDelta = (Vector2){0, 0};

    // Keyboard input
    g_webInput.backPressed = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
    g_webInput.backReleased = IsKeyReleased(KEY_ESCAPE) || IsKeyReleased(KEY_BACKSPACE);
    g_webInput.selectPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    g_webInput.selectReleased = IsKeyReleased(KEY_ENTER) || IsKeyReleased(KEY_SPACE);
    g_webInput.upPressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    g_webInput.downPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    g_webInput.leftPressed = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
    g_webInput.rightPressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);

    // Scroll from mouse wheel
    g_webInput.scrollDelta = GetMouseWheelMove();

    // Mouse position (always tracked for hover/aiming)
    Vector2 currentMousePos = GetMousePosition();
    g_webInput.mousePos = currentMousePos;
    g_webInput.touchPos = currentMousePos;  // Legacy compatibility

    // Mouse button state
    bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    g_webInput.mousePressed = mouseDown;
    g_webInput.mouseJustPressed = mouseDown && !g_wasMousePressed;
    g_webInput.mouseJustReleased = !mouseDown && g_wasMousePressed;
    g_webInput.touching = mouseDown;

    // Drag tracking
    if (g_webInput.mouseJustPressed) {
        g_webInput.dragActive = true;
        g_webInput.dragStart = currentMousePos;
        g_webInput.dragCurrent = currentMousePos;
        g_touchStart = currentMousePos;

        // Double-tap detection
        float now = GetTime();
        if (now - g_lastTapTime < 0.3f) {
            g_webInput.doubleTap = true;
        }
        g_lastTapTime = now;
    }

    if (g_webInput.dragActive) {
        g_webInput.dragCurrent = currentMousePos;
        g_webInput.dragDelta = (Vector2){
            currentMousePos.x - g_prevMousePos.x,
            currentMousePos.y - g_prevMousePos.y
        };
    }

    // On mouse release: detect tap vs swipe
    if (g_webInput.mouseJustReleased && g_webInput.dragActive) {
        float dx = currentMousePos.x - g_touchStart.x;
        float dy = currentMousePos.y - g_touchStart.y;
        float dist = sqrtf(dx*dx + dy*dy);

        if (dist < 30.0f) {
            g_webInput.tap = true;
            g_webInput.selectPressed = true;  // Tap acts as select
        } else if (dist > 80.0f) {
            // Swipe detection
            if (fabsf(dx) > fabsf(dy)) {
                if (dx > 0) g_webInput.swipeRight = true;
                else g_webInput.swipeLeft = true;
            } else {
                if (dy > 0) g_webInput.swipeDown = true;
                else g_webInput.swipeUp = true;
            }
        }
        g_webInput.dragActive = false;
    }

    g_prevMousePos = currentMousePos;
    g_wasMousePressed = mouseDown;
    g_wasTouching = g_webInput.touching;
}

static inline const LlzInputState* LlzInputGet(void) {
    return &g_webInput;
}

#endif // LLZ_SDK_STUB_H
