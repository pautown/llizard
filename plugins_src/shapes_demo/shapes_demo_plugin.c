/**
 * shapes_demo_plugin.c - Shapes Demo Plugin
 *
 * Showcases all available SDK shapes and gem colors.
 */

#include "llizard_plugin.h"
#include "llz_sdk.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Plugin State
// ============================================================================

static int g_screenWidth = 800;
static int g_screenHeight = 480;
static Font g_font;

/* View mode: 0 = Shapes view, 1 = Colors view */
static int g_viewMode = 0;

/* Current selection for scrolling */
static int g_selectedShape = 0;
static int g_selectedColor = 0;

/* Animation */
static float g_animTime = 0.0f;

// ============================================================================
// Plugin Callbacks
// ============================================================================

static void plugin_init(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_font = LlzFontGet(LLZ_FONT_UI, 20);

    g_viewMode = 0;
    g_selectedShape = 0;
    g_selectedColor = 0;
    g_animTime = 0.0f;

    printf("[ShapesDemo] Initialized (%dx%d)\n", width, height);
}

static void plugin_shutdown(void) {
    printf("[ShapesDemo] Shutdown\n");
}

static void plugin_update(const LlzInputState* input, float deltaTime) {
    g_animTime += deltaTime;

    /* Switch view mode with select button */
    if (input->selectPressed) {
        g_viewMode = (g_viewMode + 1) % 2;
    }

    /* Navigate with up/down */
    if (g_viewMode == 0) {
        /* Shapes view */
        if (input->downPressed || input->scrollDelta < 0) {
            g_selectedShape = (g_selectedShape + 1) % LLZ_SHAPE_COUNT;
        }
        if (input->upPressed || input->scrollDelta > 0) {
            g_selectedShape = (g_selectedShape - 1 + LLZ_SHAPE_COUNT) % LLZ_SHAPE_COUNT;
        }
    } else {
        /* Colors view */
        if (input->downPressed || input->scrollDelta < 0) {
            g_selectedColor = (g_selectedColor + 1) % LLZ_GEM_COUNT;
        }
        if (input->upPressed || input->scrollDelta > 0) {
            g_selectedColor = (g_selectedColor - 1 + LLZ_GEM_COUNT) % LLZ_GEM_COUNT;
        }
    }
}

static bool plugin_wants_close(void) {
    return false;
}

// ============================================================================
// Drawing
// ============================================================================

static void DrawHeader(void) {
    /* Title bar */
    DrawRectangle(0, 0, g_screenWidth, 50, (Color){30, 30, 40, 255});

    /* Title */
    const char* title = (g_viewMode == 0) ? "SDK Shapes" : "Gem Colors";
    Font titleFont = LlzFontGet(LLZ_FONT_UI, 28);
    DrawTextEx(titleFont, title, (Vector2){20, 12}, 28, 1, WHITE);

    /* View mode indicator */
    const char* modeText = (g_viewMode == 0) ? "[SELECT: Colors]" : "[SELECT: Shapes]";
    Font smallFont = LlzFontGet(LLZ_FONT_UI, 16);
    int textWidth = MeasureTextEx(smallFont, modeText, 16, 1).x;
    DrawTextEx(smallFont, modeText, (Vector2){g_screenWidth - textWidth - 20, 18}, 16, 1, (Color){150, 150, 150, 255});

    /* Separator line */
    DrawRectangle(0, 50, g_screenWidth, 2, (Color){60, 60, 80, 255});
}

static void DrawShapesView(void) {
    /* Background */
    DrawRectangle(0, 52, g_screenWidth, g_screenHeight - 52, (Color){25, 25, 35, 255});

    /* Grid layout for shapes */
    int cols = 5;
    int rows = 2;
    float cellWidth = g_screenWidth / (float)cols;
    float cellHeight = (g_screenHeight - 52) / (float)rows;
    float shapeSize = 35.0f;

    for (int i = 0; i < LLZ_SHAPE_COUNT; i++) {
        int col = i % cols;
        int row = i / cols;

        float cx = col * cellWidth + cellWidth / 2;
        float cy = 52 + row * cellHeight + cellHeight / 2 - 15;

        /* Selection highlight */
        if (i == g_selectedShape) {
            Color highlight = {100, 150, 255, 60};
            DrawRectangle((int)(col * cellWidth), (int)(52 + row * cellHeight),
                         (int)cellWidth, (int)cellHeight, highlight);

            /* Pulsing border */
            float pulse = 1.0f + sinf(g_animTime * 4.0f) * 0.3f;
            Color border = {100, 150, 255, (unsigned char)(150 * pulse)};
            DrawRectangleLinesEx(
                (Rectangle){col * cellWidth + 2, 52 + row * cellHeight + 2, cellWidth - 4, cellHeight - 4},
                2, border);
        }

        /* Draw the shape with a cycling color based on index */
        LlzGemColor gemColor = (LlzGemColor)(i % LLZ_GEM_COUNT);
        LlzDrawGemShape((LlzShapeType)i, cx, cy, shapeSize, gemColor);

        /* Shape name */
        const char* name = LlzGetShapeName((LlzShapeType)i);
        Font nameFont = LlzFontGet(LLZ_FONT_UI, 14);
        int nameWidth = MeasureTextEx(nameFont, name, 14, 1).x;
        Color textColor = (i == g_selectedShape) ? WHITE : (Color){180, 180, 180, 255};
        DrawTextEx(nameFont, name, (Vector2){cx - nameWidth / 2, cy + shapeSize + 20}, 14, 1, textColor);
    }

    /* Selected shape info at bottom */
    DrawRectangle(0, g_screenHeight - 60, g_screenWidth, 60, (Color){35, 35, 50, 255});
    DrawRectangle(0, g_screenHeight - 60, g_screenWidth, 2, (Color){60, 60, 80, 255});

    const char* selectedName = LlzGetShapeName((LlzShapeType)g_selectedShape);
    char infoText[128];
    snprintf(infoText, sizeof(infoText), "Selected: %s (LLZ_SHAPE_%s)",
             selectedName, selectedName);

    /* Convert to uppercase for enum name */
    for (char* p = infoText + strlen("Selected: ") + strlen(selectedName) + strlen(" (LLZ_SHAPE_"); *p && *p != ')'; p++) {
        if (*p >= 'a' && *p <= 'z') *p -= 32;
        if (*p == ' ') *p = '_';
    }

    Font infoFont = LlzFontGet(LLZ_FONT_UI, 18);
    DrawTextEx(infoFont, infoText, (Vector2){20, g_screenHeight - 40}, 18, 1, WHITE);
}

static void DrawColorsView(void) {
    /* Background */
    DrawRectangle(0, 52, g_screenWidth, g_screenHeight - 52, (Color){25, 25, 35, 255});

    /* Grid layout for colors */
    int cols = 4;
    int rows = 2;
    float cellWidth = g_screenWidth / (float)cols;
    float cellHeight = (g_screenHeight - 112) / (float)rows;

    for (int i = 0; i < LLZ_GEM_COUNT; i++) {
        int col = i % cols;
        int row = i / cols;

        float cx = col * cellWidth + cellWidth / 2;
        float cy = 52 + row * cellHeight + cellHeight / 2 - 10;

        /* Selection highlight */
        if (i == g_selectedColor) {
            Color highlight = {100, 150, 255, 60};
            DrawRectangle((int)(col * cellWidth), (int)(52 + row * cellHeight),
                         (int)cellWidth, (int)cellHeight, highlight);

            float pulse = 1.0f + sinf(g_animTime * 4.0f) * 0.3f;
            Color border = {100, 150, 255, (unsigned char)(150 * pulse)};
            DrawRectangleLinesEx(
                (Rectangle){col * cellWidth + 2, 52 + row * cellHeight + 2, cellWidth - 4, cellHeight - 4},
                2, border);
        }

        /* Draw color swatches */
        Color baseColor = LlzGetGemColor((LlzGemColor)i);
        Color lightColor = LlzGetGemColorLight((LlzGemColor)i);
        Color darkColor = LlzGetGemColorDark((LlzGemColor)i);

        float swatchSize = 40.0f;

        /* Dark swatch */
        DrawRectangle((int)(cx - swatchSize * 1.5f - 5), (int)(cy - swatchSize / 2),
                     (int)swatchSize, (int)swatchSize, darkColor);
        /* Base swatch */
        DrawRectangle((int)(cx - swatchSize / 2), (int)(cy - swatchSize / 2),
                     (int)swatchSize, (int)swatchSize, baseColor);
        /* Light swatch */
        DrawRectangle((int)(cx + swatchSize / 2 + 5), (int)(cy - swatchSize / 2),
                     (int)swatchSize, (int)swatchSize, lightColor);

        /* Color name */
        const char* name = LlzGetGemColorName((LlzGemColor)i);
        Font nameFont = LlzFontGet(LLZ_FONT_UI, 16);
        int nameWidth = MeasureTextEx(nameFont, name, 16, 1).x;
        Color textColor = (i == g_selectedColor) ? WHITE : (Color){180, 180, 180, 255};
        DrawTextEx(nameFont, name, (Vector2){cx - nameWidth / 2, cy + swatchSize / 2 + 10}, 16, 1, textColor);

        /* Labels for swatches */
        Font labelFont = LlzFontGet(LLZ_FONT_UI, 10);
        Color labelColor = {120, 120, 120, 255};
        DrawTextEx(labelFont, "Dark", (Vector2){cx - swatchSize * 1.5f, cy + swatchSize / 2 + 2}, 10, 1, labelColor);
        DrawTextEx(labelFont, "Base", (Vector2){cx - 10, cy + swatchSize / 2 + 2}, 10, 1, labelColor);
        DrawTextEx(labelFont, "Light", (Vector2){cx + swatchSize / 2 + 8, cy + swatchSize / 2 + 2}, 10, 1, labelColor);
    }

    /* Selected color info at bottom */
    DrawRectangle(0, g_screenHeight - 60, g_screenWidth, 60, (Color){35, 35, 50, 255});
    DrawRectangle(0, g_screenHeight - 60, g_screenWidth, 2, (Color){60, 60, 80, 255});

    const char* selectedName = LlzGetGemColorName((LlzGemColor)g_selectedColor);
    Color selectedBase = LlzGetGemColor((LlzGemColor)g_selectedColor);

    char infoText[128];
    snprintf(infoText, sizeof(infoText), "%s: RGB(%d, %d, %d)  |  LLZ_COLOR_%s",
             selectedName, selectedBase.r, selectedBase.g, selectedBase.b, selectedName);

    /* Convert color name to uppercase */
    char* enumStart = strstr(infoText, "LLZ_COLOR_") + strlen("LLZ_COLOR_");
    for (char* p = enumStart; *p && *p != ' ' && *p != '\0'; p++) {
        if (*p >= 'a' && *p <= 'z') *p -= 32;
    }

    Font infoFont = LlzFontGet(LLZ_FONT_UI, 18);
    DrawTextEx(infoFont, infoText, (Vector2){20, g_screenHeight - 40}, 18, 1, WHITE);
}

static void plugin_draw(void) {
    ClearBackground((Color){20, 20, 30, 255});

    DrawHeader();

    if (g_viewMode == 0) {
        DrawShapesView();
    } else {
        DrawColorsView();
    }

    /* Navigation hint */
    Font hintFont = LlzFontGet(LLZ_FONT_UI, 12);
    DrawTextEx(hintFont, "UP/DOWN: Navigate  |  SELECT: Switch View  |  BACK: Exit",
              (Vector2){g_screenWidth / 2 - 180, g_screenHeight - 18}, 12, 1, (Color){100, 100, 100, 255});
}

// ============================================================================
// Plugin Export
// ============================================================================

static LlzPluginAPI g_pluginAPI = {
    .name = "Shapes Demo",
    .description = "Showcases SDK shapes and gem colors",
    .init = plugin_init,
    .update = plugin_update,
    .draw = plugin_draw,
    .shutdown = plugin_shutdown,
    .wants_close = plugin_wants_close,
    .category = LLZ_CATEGORY_DEBUG
};

const LlzPluginAPI* LlzGetPlugin(void) {
    return &g_pluginAPI;
}
