#include "raylib.h"
#include "plugin_loader.h"
#include "llz_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT

// Menu styling constants
#define MENU_ITEM_HEIGHT 72
#define MENU_ITEM_SPACING 8
#define MENU_PADDING_X 32
#define MENU_PADDING_TOP 120
#define MENU_VISIBLE_AREA (SCREEN_HEIGHT - MENU_PADDING_TOP)

// Color palette
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

// Smooth scroll state
static float g_scrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;

// Font
static Font g_menuFont;
static bool g_fontLoaded = false;

// Build Unicode codepoints for international character support
static int *BuildUnicodeCodepoints(int *outCount) {
    static const int ranges[][2] = {
        {0x0020, 0x007E},   // ASCII
        {0x00A0, 0x00FF},   // Latin-1 Supplement
        {0x0100, 0x017F},   // Latin Extended-A
        {0x0180, 0x024F},   // Latin Extended-B
        {0x0400, 0x04FF},   // Cyrillic
        {0x0500, 0x052F},   // Cyrillic Supplement
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

static void LoadMenuFont(void) {
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    const char *fontPaths[] = {
#ifdef PLATFORM_DRM
        "/var/local/fonts/ZegoeUI-U.ttf",
#endif
        "fonts/ZegoeUI-U.ttf",
        "../fonts/ZegoeUI-U.ttf"
    };

    g_menuFont = GetFontDefault();
    for (int i = 0; i < (int)(sizeof(fontPaths)/sizeof(fontPaths[0])); i++) {
        Font loaded = LoadFontEx(fontPaths[i], 48, codepoints, codepointCount);
        if (loaded.texture.id != 0) {
            g_menuFont = loaded;
            g_fontLoaded = true;
            SetTextureFilter(g_menuFont.texture, TEXTURE_FILTER_BILINEAR);
            printf("Menu: Loaded font %s\n", fontPaths[i]);
            break;
        }
    }

    if (codepoints) free(codepoints);
}

static void UnloadMenuFont(void) {
    Font defaultFont = GetFontDefault();
    if (g_fontLoaded && g_menuFont.texture.id != 0 && g_menuFont.texture.id != defaultFont.texture.id) {
        UnloadFont(g_menuFont);
    }
    g_fontLoaded = false;
}

// Calculate scroll offset to keep selection visible
static float CalculateTargetScroll(int selected, int count)
{
    if (count == 0) return 0.0f;

    float itemTotalHeight = MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
    float totalListHeight = count * itemTotalHeight;
    float maxScroll = totalListHeight - MENU_VISIBLE_AREA;
    if (maxScroll < 0) maxScroll = 0;

    // Calculate where the selected item is
    float selectedTop = selected * itemTotalHeight;
    float selectedBottom = selectedTop + MENU_ITEM_HEIGHT;

    // Visible window based on current target scroll
    float visibleTop = g_targetScrollOffset;
    float visibleBottom = g_targetScrollOffset + MENU_VISIBLE_AREA;

    // Margins to keep selection away from edges
    float topMargin = MENU_ITEM_HEIGHT * 0.5f;
    float bottomMargin = MENU_ITEM_HEIGHT * 1.2f;

    float newTarget = g_targetScrollOffset;

    // If selected item is above visible area, scroll up
    if (selectedTop < visibleTop + topMargin) {
        newTarget = selectedTop - topMargin;
    }
    // If selected item is below visible area, scroll down
    else if (selectedBottom > visibleBottom - bottomMargin) {
        newTarget = selectedBottom - MENU_VISIBLE_AREA + bottomMargin;
    }

    // Clamp to valid range
    if (newTarget < 0) newTarget = 0;
    if (newTarget > maxScroll) newTarget = maxScroll;

    return newTarget;
}

// Update smooth scroll
static void UpdateScroll(float deltaTime)
{
    float diff = g_targetScrollOffset - g_scrollOffset;
    float speed = 12.0f;

    // Smooth interpolation
    g_scrollOffset += diff * speed * deltaTime;

    // Snap when very close
    if (fabsf(diff) < 0.5f) {
        g_scrollOffset = g_targetScrollOffset;
    }
}

static void DrawMenuBackground(void)
{
    if (LlzBackgroundIsEnabled()) {
        // Use SDK animated background when enabled
        LlzBackgroundDraw();
    } else {
        // Subtle gradient background
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG_DARK, COLOR_BG_GRADIENT);

        // Subtle accent glow at top
        for (int i = 0; i < 3; i++) {
            float alpha = 0.03f - i * 0.01f;
            Color glow = ColorAlpha(COLOR_ACCENT, alpha);
            DrawCircleGradient(SCREEN_WIDTH / 2, -100 + i * 50, 400 - i * 80, glow, ColorAlpha(glow, 0.0f));
        }
    }
}

static void DrawPluginMenu(const PluginRegistry *registry, int selected, float deltaTime)
{
    DrawMenuBackground();

    // Get dynamic accent color from background palette
    const LlzBackgroundPalette *palette = LlzBackgroundGetPalette();
    Color dynamicAccent = palette ? palette->colors[1] : COLOR_ACCENT;
    Color dynamicAccentDim = ColorAlpha(dynamicAccent, 0.6f);

    // Calculate complementary color (opposite hue) like nowplaying volume bar
    Color primaryColor = palette ? palette->colors[0] : COLOR_ACCENT;
    Vector3 hsv = ColorToHSV(primaryColor);
    float compHue = fmodf(hsv.x + 180.0f, 360.0f);
    Color complementary = ColorFromHSV(compHue, fminf(hsv.y * 0.8f, 0.7f), fminf(hsv.z + 0.2f, 0.9f));

    // Header
    DrawTextEx(g_menuFont, "llizardgui", (Vector2){MENU_PADDING_X, 28}, 38, 2, COLOR_TEXT_PRIMARY);

    // Selected plugin name in top right (if plugins exist) - uses complementary color
    if (registry && registry->count > 0 && selected >= 0 && selected < registry->count) {
        const char *pluginName = registry->items[selected].displayName;
        float fontSize = 36;
        float spacing = 2;
        Vector2 textSize = MeasureTextEx(g_menuFont, pluginName, fontSize, spacing);
        float textX = SCREEN_WIDTH - textSize.x - MENU_PADDING_X;
        float textY = 28;
        DrawTextEx(g_menuFont, pluginName, (Vector2){textX, textY}, fontSize, spacing, complementary);
    }

    // Subtle accent underline
    DrawRectangle(MENU_PADDING_X, 74, 160, 3, dynamicAccent);

    // Instruction text
    const char *instructions = "scroll to navigate • select to launch";
    DrawTextEx(g_menuFont, instructions, (Vector2){MENU_PADDING_X, 88}, 16, 1, COLOR_TEXT_DIM);

    if (!registry || registry->count == 0) {
        DrawTextEx(g_menuFont, "No plugins found", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, COLOR_TEXT_SECONDARY);
        DrawTextEx(g_menuFont, "Place .so files in ./plugins", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, COLOR_TEXT_DIM);
        return;
    }

    // Update scroll animation
    g_targetScrollOffset = CalculateTargetScroll(selected, registry->count);
    UpdateScroll(deltaTime);

    // Calculate scroll indicators
    float itemTotalHeight = MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
    float totalListHeight = registry->count * itemTotalHeight;
    float maxScroll = totalListHeight - MENU_VISIBLE_AREA;
    if (maxScroll < 0) maxScroll = 0;

    bool canScrollUp = g_scrollOffset > 1.0f;
    bool canScrollDown = g_scrollOffset < maxScroll - 1.0f;

    // Clipping region for list
    BeginScissorMode(0, MENU_PADDING_TOP, SCREEN_WIDTH, (int)MENU_VISIBLE_AREA);

    // Draw plugin items
    for (int i = 0; i < registry->count; ++i) {
        float itemY = MENU_PADDING_TOP + i * itemTotalHeight - g_scrollOffset;

        // Skip items outside visible area (with margin for partial visibility)
        if (itemY < MENU_PADDING_TOP - MENU_ITEM_HEIGHT || itemY > SCREEN_HEIGHT) continue;

        bool isSelected = (i == selected);

        // Card background
        Rectangle cardRect = {
            MENU_PADDING_X - 12,
            itemY,
            SCREEN_WIDTH - (MENU_PADDING_X - 12) * 2,
            MENU_ITEM_HEIGHT
        };

        Color cardBg = isSelected ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
        Color borderColor = isSelected ? dynamicAccent : COLOR_CARD_BORDER;

        // Draw card with rounded corners
        DrawRectangleRounded(cardRect, 0.15f, 8, cardBg);

        // Selection accent bar on left
        if (isSelected) {
            Rectangle accentBar = {cardRect.x, cardRect.y + 8, 4, cardRect.height - 16};
            DrawRectangleRounded(accentBar, 0.5f, 4, dynamicAccent);
        }

        // Subtle border
        DrawRectangleRoundedLines(cardRect, 0.15f, 8, ColorAlpha(borderColor, isSelected ? 0.6f : 0.2f));

        // Plugin name
        Color nameColor = isSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
        float nameY = itemY + 16;
        DrawTextEx(g_menuFont, registry->items[i].displayName, (Vector2){MENU_PADDING_X + 8, nameY}, 24, 1.5f, nameColor);

        // Plugin description
        if (registry->items[i].api && registry->items[i].api->description) {
            Color descColor = isSelected ? COLOR_TEXT_SECONDARY : COLOR_TEXT_DIM;
            DrawTextEx(g_menuFont, registry->items[i].api->description,
                      (Vector2){MENU_PADDING_X + 8, nameY + 30}, 16, 1, descColor);
        }

        // Index indicator on right
        char indexStr[16];
        snprintf(indexStr, sizeof(indexStr), "%d", i + 1);
        Vector2 indexSize = MeasureTextEx(g_menuFont, indexStr, 16, 1);
        Color indexColor = isSelected ? dynamicAccentDim : ColorAlpha(COLOR_TEXT_DIM, 0.5f);
        DrawTextEx(g_menuFont, indexStr,
                  (Vector2){cardRect.x + cardRect.width - indexSize.x - 16, itemY + (MENU_ITEM_HEIGHT - 16) / 2},
                  16, 1, indexColor);
    }

    EndScissorMode();

    // Scroll indicators (fade gradients at top/bottom)
    if (canScrollUp) {
        for (int i = 0; i < 30; i++) {
            float alpha = (30 - i) / 30.0f * 0.8f;
            Color fade = ColorAlpha(COLOR_BG_DARK, alpha);
            DrawRectangle(0, MENU_PADDING_TOP + i, SCREEN_WIDTH, 1, fade);
        }
        // Up arrow hint
        DrawTextEx(g_menuFont, "▲", (Vector2){SCREEN_WIDTH / 2 - 6, MENU_PADDING_TOP + 4}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.6f));
    }

    if (canScrollDown) {
        int bottomY = MENU_PADDING_TOP + (int)MENU_VISIBLE_AREA;
        for (int i = 0; i < 30; i++) {
            float alpha = i / 30.0f * 0.8f;
            Color fade = ColorAlpha(COLOR_BG_DARK, alpha);
            DrawRectangle(0, bottomY - 30 + i, SCREEN_WIDTH, 1, fade);
        }
        // Down arrow hint
        DrawTextEx(g_menuFont, "▼", (Vector2){SCREEN_WIDTH / 2 - 6, bottomY - 18}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.6f));
    }

    // Selection counter at bottom right
    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d of %d", selected + 1, registry->count);
    Vector2 counterSize = MeasureTextEx(g_menuFont, counterStr, 16, 1);
    DrawTextEx(g_menuFont, counterStr,
              (Vector2){SCREEN_WIDTH - counterSize.x - MENU_PADDING_X, SCREEN_HEIGHT - 28},
              16, 1, COLOR_TEXT_DIM);
}

int main(void)
{
    // Initialize config system first (before display for brightness)
    LlzConfigInit();

    if (!LlzDisplayInit()) {
        fprintf(stderr, "Failed to initialize display. Check DRM permissions and cabling.\n");
        return 1;
    }
    LlzInputInit();
    LoadMenuFont();

    // Initialize SDK background system for animated menu backgrounds
    LlzBackgroundInit(SCREEN_WIDTH, SCREEN_HEIGHT);
    LlzBackgroundSetColors(COLOR_ACCENT, COLOR_ACCENT_DIM);

    char pluginDir[512];
    const char *working = GetWorkingDirectory();
    snprintf(pluginDir, sizeof(pluginDir), "%s/plugins", working ? working : ".");

    PluginRegistry registry = {0};
    LoadPlugins(pluginDir, &registry);

    int selectedIndex = 0;
    if (selectedIndex >= registry.count) selectedIndex = registry.count - 1;
    bool runningPlugin = false;
    LoadedPlugin *active = NULL;
    int lastPluginIndex = -1;  // Track last opened plugin for quick reopen

    LlzInputState inputState;

    while (!WindowShouldClose()) {
        float delta = GetFrameTime();
        LlzInputUpdate(&inputState);

        if (!runningPlugin) {
            // Update SDK background animations
            LlzBackgroundUpdate(delta);

            bool downKey = IsKeyPressed(KEY_DOWN) || inputState.downPressed || (inputState.scrollDelta > 0.0f);
            bool upKey = IsKeyPressed(KEY_UP) || inputState.upPressed || (inputState.scrollDelta < 0.0f);

            if (downKey && registry.count > 0) {
                selectedIndex = (selectedIndex + 1) % registry.count;
            }
            if (upKey && registry.count > 0) {
                selectedIndex = (selectedIndex - 1 + registry.count) % registry.count;
            }

            // Cycle background style with screenshot button (or button4)
            if (inputState.screenshotPressed || inputState.button4Pressed) {
                LlzBackgroundCycleNext();
            }

            // Back button release reopens last plugin
            if (inputState.backReleased && lastPluginIndex >= 0 && lastPluginIndex < registry.count) {
                selectedIndex = lastPluginIndex;
                active = &registry.items[selectedIndex];
                if (active && active->api && active->api->init) {
                    active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
                }
                runningPlugin = true;
                continue;
            }

            bool selectPressed = IsKeyPressed(KEY_ENTER) || inputState.selectPressed;
            if (selectPressed && registry.count > 0) {
                lastPluginIndex = selectedIndex;  // Remember which plugin we're launching
                active = &registry.items[selectedIndex];
                if (active && active->api && active->api->init) {
                    active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
                }
                runningPlugin = true;
                continue;
            }

            LlzDisplayBegin();
            DrawPluginMenu(&registry, selectedIndex, delta);
            LlzBackgroundDrawIndicator();
            LlzDisplayEnd();
        } else if (active && active->api) {
            if (active->api->update) active->api->update(&inputState, delta);

            LlzDisplayBegin();
            if (active->api->draw) active->api->draw();
            LlzDisplayEnd();

            // Check if plugin wants to close
            // Default: host handles back button to exit. Plugins can set
            // handles_back_button=true to handle back navigation themselves.
            bool exitRequest = IsKeyReleased(KEY_ESCAPE);
            if (!exitRequest && !active->api->handles_back_button) {
                // Host handles back button - exit on release
                exitRequest = inputState.backReleased;
            }
            if (!exitRequest && active->api->wants_close) {
                exitRequest = active->api->wants_close();
            }

            if (exitRequest) {
                if (active->api->shutdown) active->api->shutdown();

                // Check if the plugin requested opening another plugin
                if (LlzHasRequestedPlugin()) {
                    const char *requestedName = LlzGetRequestedPlugin();
                    if (requestedName) {
                        // Find the requested plugin by name
                        int foundIndex = -1;
                        for (int i = 0; i < registry.count; i++) {
                            if (strcmp(registry.items[i].displayName, requestedName) == 0 ||
                                (registry.items[i].api && registry.items[i].api->name &&
                                 strcmp(registry.items[i].api->name, requestedName) == 0)) {
                                foundIndex = i;
                                break;
                            }
                        }

                        LlzClearRequestedPlugin();

                        if (foundIndex >= 0) {
                            // Open the requested plugin directly
                            selectedIndex = foundIndex;
                            lastPluginIndex = foundIndex;
                            active = &registry.items[foundIndex];
                            if (active->api && active->api->init) {
                                active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
                            }
                            // Stay in running mode
                            continue;
                        }
                    }
                }

                runningPlugin = false;
                active = NULL;
            }
        }
    }

    if (active && active->api && active->api->shutdown) {
        active->api->shutdown();
    }
    UnloadPlugins(&registry);
    UnloadMenuFont();
    LlzBackgroundShutdown();
    LlzInputShutdown();
    LlzDisplayShutdown();
    LlzConfigShutdown();
    return 0;
}
