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

// Menu navigation styles
typedef enum {
    MENU_STYLE_LIST = 0,      // Classic vertical list (current)
    MENU_STYLE_CAROUSEL,      // Apple Music inspired horizontal carousel
    MENU_STYLE_CARDS,         // Spotify inspired large card view
    MENU_STYLE_SPOTIFY_CT,    // True Spotify CarThing single-focus minimal
    MENU_STYLE_GRID,          // Apple Music grid layout
    MENU_STYLE_COUNT
} MenuScrollStyle;

static const char *g_styleNames[] = {
    "List",
    "Carousel",
    "Cards",
    "CarThing",
    "Grid"
};

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

// Menu style state
static MenuScrollStyle g_menuStyle = MENU_STYLE_LIST;
static float g_carouselOffset = 0.0f;       // Horizontal offset for carousel
static float g_carouselTarget = 0.0f;       // Target offset for smooth animation
static float g_styleIndicatorAlpha = 0.0f;  // Fade for style indicator
static float g_styleIndicatorTimer = 0.0f;  // Timer for indicator visibility

// Plugin refresh state
#define PLUGIN_REFRESH_INTERVAL 2.0f  // Check for changes every 2 seconds
static float g_pluginRefreshTimer = 0.0f;
static PluginDirSnapshot g_pluginSnapshot = {0};

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

    // Initialize SDK font system and use its path discovery
    LlzFontInit();

    // Try to load via SDK which searches all the correct paths
    const char *fontPath = LlzFontGetPath(LLZ_FONT_UI);
    if (fontPath) {
        Font loaded = LoadFontEx(fontPath, 48, codepoints, codepointCount);
        if (loaded.texture.id != 0) {
            g_menuFont = loaded;
            g_fontLoaded = true;
            SetTextureFilter(g_menuFont.texture, TEXTURE_FILTER_BILINEAR);
            printf("Menu: Loaded font %s\n", fontPath);
        }
    }

    // Fallback to default if SDK font not found
    if (!g_fontLoaded) {
        g_menuFont = GetFontDefault();
        printf("Menu: Using default font\n");
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

// Update carousel horizontal scroll
static void UpdateCarouselScroll(float deltaTime)
{
    float diff = g_carouselTarget - g_carouselOffset;
    float speed = 10.0f;

    g_carouselOffset += diff * speed * deltaTime;

    if (fabsf(diff) < 0.5f) {
        g_carouselOffset = g_carouselTarget;
    }
}

// Cycle to next menu style
static void CycleMenuStyle(void)
{
    g_menuStyle = (g_menuStyle + 1) % MENU_STYLE_COUNT;
    g_styleIndicatorAlpha = 1.0f;
    g_styleIndicatorTimer = 2.0f;  // Show indicator for 2 seconds
    printf("Menu style: %s\n", g_styleNames[g_menuStyle]);
}

// Update style indicator fade
static void UpdateStyleIndicator(float deltaTime)
{
    if (g_styleIndicatorTimer > 0.0f) {
        g_styleIndicatorTimer -= deltaTime;
        if (g_styleIndicatorTimer <= 0.5f) {
            // Fade out in last 0.5 seconds
            g_styleIndicatorAlpha = g_styleIndicatorTimer / 0.5f;
        }
    } else {
        g_styleIndicatorAlpha = 0.0f;
    }
}

// Draw style indicator overlay
static void DrawStyleIndicator(void)
{
    if (g_styleIndicatorAlpha <= 0.0f) return;

    const char *styleName = g_styleNames[g_menuStyle];

    // Background pill
    float fontSize = 24;
    Vector2 textSize = MeasureTextEx(g_menuFont, styleName, fontSize, 1);
    float pillWidth = textSize.x + 40;
    float pillHeight = 44;
    float pillX = (SCREEN_WIDTH - pillWidth) / 2;
    float pillY = SCREEN_HEIGHT - 70;

    Color bgColor = ColorAlpha(COLOR_BG_DARK, 0.9f * g_styleIndicatorAlpha);
    Color borderColor = ColorAlpha(COLOR_ACCENT, 0.6f * g_styleIndicatorAlpha);
    Color textColor = ColorAlpha(COLOR_TEXT_PRIMARY, g_styleIndicatorAlpha);

    Rectangle pill = {pillX, pillY, pillWidth, pillHeight};
    DrawRectangleRounded(pill, 0.5f, 8, bgColor);
    DrawRectangleRoundedLines(pill, 0.5f, 8, borderColor);

    DrawTextEx(g_menuFont, styleName,
               (Vector2){pillX + 20, pillY + (pillHeight - fontSize) / 2},
               fontSize, 1, textColor);
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

// Draw menu header (shared across styles)
static void DrawMenuHeader(const PluginRegistry *registry, int selected, Color dynamicAccent, Color complementary)
{
    // Header
    DrawTextEx(g_menuFont, "llizardOS", (Vector2){MENU_PADDING_X, 28}, 38, 2, COLOR_TEXT_PRIMARY);

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
}

// ============================================================================
// CAROUSEL STYLE - Apple Music inspired horizontal cover flow
// ============================================================================

#define CAROUSEL_ITEM_WIDTH 280
#define CAROUSEL_ITEM_HEIGHT 200
#define CAROUSEL_CENTER_Y (SCREEN_HEIGHT / 2 + 20)
#define CAROUSEL_SPACING 40

static void DrawPluginMenuCarousel(const PluginRegistry *registry, int selected, float deltaTime,
                                   Color dynamicAccent, Color dynamicAccentDim)
{
    if (!registry || registry->count == 0) {
        DrawTextEx(g_menuFont, "No plugins found", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, COLOR_TEXT_SECONDARY);
        DrawTextEx(g_menuFont, "Place .so files in ./plugins", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, COLOR_TEXT_DIM);
        return;
    }

    // Update carousel scroll to center on selected item
    float itemSpacing = CAROUSEL_ITEM_WIDTH + CAROUSEL_SPACING;
    g_carouselTarget = selected * itemSpacing;
    UpdateCarouselScroll(deltaTime);

    float centerX = SCREEN_WIDTH / 2.0f;

    // Draw items with perspective scaling
    for (int i = 0; i < registry->count; i++) {
        // Calculate horizontal position relative to center
        float itemCenterX = i * itemSpacing - g_carouselOffset + centerX;
        float distFromCenter = fabsf(itemCenterX - centerX);

        // Skip items too far off screen
        if (itemCenterX < -CAROUSEL_ITEM_WIDTH || itemCenterX > SCREEN_WIDTH + CAROUSEL_ITEM_WIDTH) continue;

        // Scale and alpha based on distance from center (Apple Music cover flow effect)
        float maxDist = SCREEN_WIDTH / 2.0f;
        float normalizedDist = fminf(distFromCenter / maxDist, 1.0f);

        // Center item is full size, others shrink
        float scale = 1.0f - normalizedDist * 0.35f;
        float alpha = 1.0f - normalizedDist * 0.6f;

        // 3D-ish perspective: items offset vertically as they move away
        float yOffset = normalizedDist * 30.0f;

        bool isSelected = (i == selected);

        // Calculate card dimensions with scale
        float cardWidth = CAROUSEL_ITEM_WIDTH * scale;
        float cardHeight = CAROUSEL_ITEM_HEIGHT * scale;
        float cardX = itemCenterX - cardWidth / 2.0f;
        float cardY = CAROUSEL_CENTER_Y - cardHeight / 2.0f + yOffset;

        Rectangle cardRect = {cardX, cardY, cardWidth, cardHeight};

        // Card background with depth shadow
        if (scale > 0.7f) {
            Color shadowColor = ColorAlpha(BLACK, 0.3f * alpha);
            Rectangle shadowRect = {cardX + 8, cardY + 8, cardWidth, cardHeight};
            DrawRectangleRounded(shadowRect, 0.12f, 8, shadowColor);
        }

        Color cardBg = isSelected ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
        Color borderColor = isSelected ? dynamicAccent : COLOR_CARD_BORDER;

        DrawRectangleRounded(cardRect, 0.12f, 8, ColorAlpha(cardBg, alpha));
        DrawRectangleRoundedLines(cardRect, 0.12f, 8, ColorAlpha(borderColor, alpha * (isSelected ? 0.8f : 0.3f)));

        // Selection glow ring for center item
        if (isSelected && normalizedDist < 0.1f) {
            Rectangle glowRect = {cardX - 4, cardY - 4, cardWidth + 8, cardHeight + 8};
            DrawRectangleRoundedLines(glowRect, 0.12f, 8, ColorAlpha(dynamicAccent, 0.4f));
        }

        // Plugin icon placeholder (large centered circle)
        float iconRadius = cardHeight * 0.25f;
        float iconY = cardY + cardHeight * 0.35f;
        DrawCircle((int)(cardX + cardWidth / 2), (int)iconY, iconRadius, ColorAlpha(dynamicAccentDim, alpha * 0.4f));
        DrawCircleLines((int)(cardX + cardWidth / 2), (int)iconY, iconRadius, ColorAlpha(dynamicAccent, alpha * 0.6f));

        // First letter as icon
        if (registry->items[i].displayName[0]) {
            char initial[2] = {registry->items[i].displayName[0], '\0'};
            float initialSize = iconRadius * 1.2f;
            Vector2 initialDim = MeasureTextEx(g_menuFont, initial, initialSize, 1);
            DrawTextEx(g_menuFont, initial,
                      (Vector2){cardX + cardWidth / 2 - initialDim.x / 2, iconY - initialDim.y / 2},
                      initialSize, 1, ColorAlpha(COLOR_TEXT_PRIMARY, alpha));
        }

        // Plugin name below icon (larger font, no description)
        float fontSize = 26 * scale;
        if (fontSize > 14) {
            Vector2 nameSize = MeasureTextEx(g_menuFont, registry->items[i].displayName, fontSize, 1);
            float nameX = cardX + (cardWidth - nameSize.x) / 2;
            float nameY = cardY + cardHeight * 0.75f;
            Color nameColor = isSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
            DrawTextEx(g_menuFont, registry->items[i].displayName,
                      (Vector2){nameX, nameY}, fontSize, 1, ColorAlpha(nameColor, alpha));
        }
    }

    // Navigation dots at bottom
    float dotY = CAROUSEL_CENTER_Y + CAROUSEL_ITEM_HEIGHT / 2 + 50;
    float totalDotsWidth = registry->count * 16;
    float dotStartX = (SCREEN_WIDTH - totalDotsWidth) / 2;

    for (int i = 0; i < registry->count; i++) {
        float dotX = dotStartX + i * 16 + 4;
        Color dotColor = (i == selected) ? dynamicAccent : ColorAlpha(COLOR_TEXT_DIM, 0.4f);
        float dotRadius = (i == selected) ? 5.0f : 3.0f;
        DrawCircle((int)dotX, (int)dotY, dotRadius, dotColor);
    }
}

// ============================================================================
// CARDS STYLE - Spotify inspired large single-card view
// ============================================================================

static void DrawPluginMenuCards(const PluginRegistry *registry, int selected,
                                Color dynamicAccent, Color complementary)
{
    if (!registry || registry->count == 0) {
        DrawTextEx(g_menuFont, "No plugins found", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, COLOR_TEXT_SECONDARY);
        DrawTextEx(g_menuFont, "Place .so files in ./plugins", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, COLOR_TEXT_DIM);
        return;
    }

    // Large card dimensions - fills most of the screen
    float cardWidth = SCREEN_WIDTH - 80;
    float cardHeight = 280;
    float cardX = 40;
    float cardY = MENU_PADDING_TOP + 20;

    // Main card background with gradient
    Rectangle cardRect = {cardX, cardY, cardWidth, cardHeight};

    // Spotify-style gradient background on card
    Color gradientTop = ColorAlpha(dynamicAccent, 0.15f);
    Color gradientBottom = COLOR_CARD_BG;
    DrawRectangleGradientV((int)cardX, (int)cardY, (int)cardWidth, (int)cardHeight, gradientTop, gradientBottom);
    DrawRectangleRoundedLines(cardRect, 0.05f, 8, ColorAlpha(dynamicAccent, 0.3f));

    // Large plugin icon/initial on the left side
    float iconSize = 160;
    float iconX = cardX + 40;
    float iconY = cardY + (cardHeight - iconSize) / 2;

    // Icon background circle
    DrawCircle((int)(iconX + iconSize / 2), (int)(iconY + iconSize / 2), iconSize / 2 + 4, ColorAlpha(dynamicAccent, 0.2f));
    DrawCircle((int)(iconX + iconSize / 2), (int)(iconY + iconSize / 2), iconSize / 2, COLOR_CARD_SELECTED);

    // Large initial letter
    if (registry->items[selected].displayName[0]) {
        char initial[2] = {registry->items[selected].displayName[0], '\0'};
        float initialSize = iconSize * 0.6f;
        Vector2 initialDim = MeasureTextEx(g_menuFont, initial, initialSize, 1);
        DrawTextEx(g_menuFont, initial,
                  (Vector2){iconX + iconSize / 2 - initialDim.x / 2, iconY + iconSize / 2 - initialDim.y / 2},
                  initialSize, 1, dynamicAccent);
    }

    // Plugin info on the right side
    float textX = iconX + iconSize + 40;

    // Large plugin name
    DrawTextEx(g_menuFont, registry->items[selected].displayName,
              (Vector2){textX, cardY + 50}, 42, 2, COLOR_TEXT_PRIMARY);

    // Description
    if (registry->items[selected].api && registry->items[selected].api->description) {
        DrawTextEx(g_menuFont, registry->items[selected].api->description,
                  (Vector2){textX, cardY + 105}, 20, 1, COLOR_TEXT_SECONDARY);
    }

    // Plugin index badge
    char indexStr[32];
    snprintf(indexStr, sizeof(indexStr), "Plugin %d of %d", selected + 1, registry->count);
    DrawTextEx(g_menuFont, indexStr, (Vector2){textX, cardY + 150}, 16, 1, COLOR_TEXT_DIM);

    // "Press select to launch" hint
    DrawTextEx(g_menuFont, "Press SELECT to launch",
              (Vector2){textX, cardY + cardHeight - 60}, 18, 1, complementary);

    // Previous/Next preview cards (smaller, on sides)
    float previewWidth = 140;
    float previewHeight = 100;
    float previewY = cardY + cardHeight + 30;

    // Previous plugin preview (if exists)
    if (selected > 0) {
        int prevIdx = selected - 1;
        Rectangle prevRect = {40, previewY, previewWidth, previewHeight};
        DrawRectangleRounded(prevRect, 0.1f, 6, ColorAlpha(COLOR_CARD_BG, 0.6f));
        DrawRectangleRoundedLines(prevRect, 0.1f, 6, ColorAlpha(COLOR_CARD_BORDER, 0.3f));

        // Arrow and name
        DrawTextEx(g_menuFont, "◀", (Vector2){50, previewY + 15}, 24, 1, COLOR_TEXT_DIM);
        DrawTextEx(g_menuFont, registry->items[prevIdx].displayName,
                  (Vector2){50, previewY + 50}, 16, 1, COLOR_TEXT_SECONDARY);
    }

    // Next plugin preview (if exists)
    if (selected < registry->count - 1) {
        int nextIdx = selected + 1;
        float nextX = SCREEN_WIDTH - 40 - previewWidth;
        Rectangle nextRect = {nextX, previewY, previewWidth, previewHeight};
        DrawRectangleRounded(nextRect, 0.1f, 6, ColorAlpha(COLOR_CARD_BG, 0.6f));
        DrawRectangleRoundedLines(nextRect, 0.1f, 6, ColorAlpha(COLOR_CARD_BORDER, 0.3f));

        // Arrow and name (right-aligned)
        Vector2 arrowSize = MeasureTextEx(g_menuFont, "▶", 24, 1);
        DrawTextEx(g_menuFont, "▶", (Vector2){nextX + previewWidth - arrowSize.x - 10, previewY + 15}, 24, 1, COLOR_TEXT_DIM);

        Vector2 nameSize = MeasureTextEx(g_menuFont, registry->items[nextIdx].displayName, 16, 1);
        float nameX = nextX + previewWidth - nameSize.x - 10;
        DrawTextEx(g_menuFont, registry->items[nextIdx].displayName,
                  (Vector2){nameX, previewY + 50}, 16, 1, COLOR_TEXT_SECONDARY);
    }

    // Progress bar at bottom showing position
    float barWidth = SCREEN_WIDTH - 160;
    float barX = 80;
    float barY = SCREEN_HEIGHT - 30;
    float barHeight = 4;

    DrawRectangleRounded((Rectangle){barX, barY, barWidth, barHeight}, 0.5f, 4, ColorAlpha(COLOR_CARD_BORDER, 0.3f));

    // Progress indicator
    float progress = (float)selected / (float)(registry->count - 1 > 0 ? registry->count - 1 : 1);
    float indicatorWidth = barWidth / registry->count;
    float indicatorX = barX + progress * (barWidth - indicatorWidth);
    DrawRectangleRounded((Rectangle){indicatorX, barY, indicatorWidth, barHeight}, 0.5f, 4, dynamicAccent);
}

// ============================================================================
// LIST STYLE - Classic vertical scrolling list (original)
// ============================================================================

static void DrawPluginMenuList(const PluginRegistry *registry, int selected, float deltaTime,
                               Color dynamicAccent, Color dynamicAccentDim)
{
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

// ============================================================================
// SPOTIFY CARTHING STYLE - Single-focus minimal like original CarThing UI
// ============================================================================

// Spotify green color for authentic CarThing look
static const Color COLOR_SPOTIFY_GREEN = {30, 215, 96, 255};

// Omicron font for CarThing style branding ("llizardOS")
static Font g_omicronFont;
static bool g_omicronFontLoaded = false;

// Tracklister font for CarThing style main text
static Font g_tracklisterFont;
static bool g_tracklisterFontLoaded = false;

static void LoadOmicronFont(void) {
    if (g_omicronFontLoaded) return;

    // Build codepoints for international support
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    // Try loading from fonts folder (check multiple locations)
    const char *fontPaths[] = {
        "./fonts/Omicron Regular.otf",
        "./fonts/Omicron Light.otf",
        "/tmp/fonts/Omicron Regular.otf",
        "/tmp/fonts/Omicron Light.otf",
        "/var/local/fonts/Omicron Regular.otf",
        "/var/local/fonts/Omicron Light.otf",
    };

    for (int i = 0; i < 6; i++) {
        if (FileExists(fontPaths[i])) {
            Font loaded = LoadFontEx(fontPaths[i], 72, codepoints, codepointCount);
            if (loaded.texture.id != 0) {
                g_omicronFont = loaded;
                g_omicronFontLoaded = true;
                SetTextureFilter(g_omicronFont.texture, TEXTURE_FILTER_BILINEAR);
                printf("CarThing: Loaded Omicron font from %s\n", fontPaths[i]);
                break;
            }
        }
    }

    // Fallback to menu font if not found
    if (!g_omicronFontLoaded) {
        g_omicronFont = g_menuFont;
        printf("CarThing: Omicron font not found, using menu font\n");
    }

    if (codepoints) free(codepoints);
}

static void LoadTracklisterFont(void) {
    if (g_tracklisterFontLoaded) return;

    // Build codepoints for international support
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    // Try loading from fonts folder (prefer Medium or Regular weight)
    const char *fontPaths[] = {
        "./fonts/Tracklister-Medium.ttf",
        "./fonts/Tracklister-Regular.ttf",
        "./fonts/Tracklister-Semibold.ttf",
        "/tmp/fonts/Tracklister-Medium.ttf",
        "/tmp/fonts/Tracklister-Regular.ttf",
        "/tmp/fonts/Tracklister-Semibold.ttf",
        "/var/local/fonts/Tracklister-Medium.ttf",
        "/var/local/fonts/Tracklister-Regular.ttf",
    };

    for (int i = 0; i < 8; i++) {
        if (FileExists(fontPaths[i])) {
            Font loaded = LoadFontEx(fontPaths[i], 72, codepoints, codepointCount);
            if (loaded.texture.id != 0) {
                g_tracklisterFont = loaded;
                g_tracklisterFontLoaded = true;
                SetTextureFilter(g_tracklisterFont.texture, TEXTURE_FILTER_BILINEAR);
                printf("CarThing: Loaded Tracklister font from %s\n", fontPaths[i]);
                break;
            }
        }
    }

    // Fallback to menu font if not found
    if (!g_tracklisterFontLoaded) {
        g_tracklisterFont = g_menuFont;
        printf("CarThing: Tracklister font not found, using menu font\n");
    }

    if (codepoints) free(codepoints);
}

static void UnloadOmicronFont(void) {
    Font defaultFont = GetFontDefault();
    if (g_omicronFontLoaded && g_omicronFont.texture.id != 0 &&
        g_omicronFont.texture.id != defaultFont.texture.id &&
        g_omicronFont.texture.id != g_menuFont.texture.id) {
        UnloadFont(g_omicronFont);
    }
    g_omicronFontLoaded = false;
}

static void UnloadTracklisterFont(void) {
    Font defaultFont = GetFontDefault();
    if (g_tracklisterFontLoaded && g_tracklisterFont.texture.id != 0 &&
        g_tracklisterFont.texture.id != defaultFont.texture.id &&
        g_tracklisterFont.texture.id != g_menuFont.texture.id) {
        UnloadFont(g_tracklisterFont);
    }
    g_tracklisterFontLoaded = false;
}

// iBrand font for Grid style
static Font g_ibrandFont;
static bool g_ibrandFontLoaded = false;

static void LoadIBrandFont(void) {
    if (g_ibrandFontLoaded) return;

    // Build codepoints for international support
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    // Try loading from fonts folder
    const char *fontPaths[] = {
        "./fonts/Ibrand.otf",
        "/tmp/fonts/Ibrand.otf",
        "/var/local/fonts/Ibrand.otf",
    };

    for (int i = 0; i < 3; i++) {
        if (FileExists(fontPaths[i])) {
            Font loaded = LoadFontEx(fontPaths[i], 72, codepoints, codepointCount);
            if (loaded.texture.id != 0) {
                g_ibrandFont = loaded;
                g_ibrandFontLoaded = true;
                SetTextureFilter(g_ibrandFont.texture, TEXTURE_FILTER_BILINEAR);
                printf("Grid: Loaded iBrand font from %s\n", fontPaths[i]);
                break;
            }
        }
    }

    // Fallback to menu font if not found
    if (!g_ibrandFontLoaded) {
        g_ibrandFont = g_menuFont;
        printf("Grid: iBrand font not found, using menu font\n");
    }

    if (codepoints) free(codepoints);
}

static void UnloadIBrandFont(void) {
    Font defaultFont = GetFontDefault();
    if (g_ibrandFontLoaded && g_ibrandFont.texture.id != 0 &&
        g_ibrandFont.texture.id != defaultFont.texture.id &&
        g_ibrandFont.texture.id != g_menuFont.texture.id) {
        UnloadFont(g_ibrandFont);
    }
    g_ibrandFontLoaded = false;
}

// Crossfade state for CarThing style
static float g_ctFadeAlpha = 1.0f;
static int g_ctLastSelected = -1;

static void DrawPluginMenuSpotifyCT(const PluginRegistry *registry, int selected, float deltaTime,
                                    Color dynamicAccent)
{
    (void)dynamicAccent;

    // Lazy load fonts on first use
    if (!g_omicronFontLoaded) {
        LoadOmicronFont();
    }
    if (!g_tracklisterFontLoaded) {
        LoadTracklisterFont();
    }

    // Tracklister for main text, Omicron for branding
    Font textFont = g_tracklisterFontLoaded ? g_tracklisterFont : g_menuFont;
    Font brandFont = g_omicronFontLoaded ? g_omicronFont : g_menuFont;

    if (!registry || registry->count == 0) {
        DrawTextEx(textFont, "No plugins", (Vector2){SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 - 20}, 32, 1, COLOR_TEXT_SECONDARY);
        return;
    }

    // Detect selection change and trigger crossfade
    if (g_ctLastSelected != selected) {
        g_ctFadeAlpha = 0.0f;  // Start fade from zero
        g_ctLastSelected = selected;
    }

    // Gentle crossfade animation
    float fadeSpeed = 5.0f;
    g_ctFadeAlpha += fadeSpeed * deltaTime;
    if (g_ctFadeAlpha > 1.0f) g_ctFadeAlpha = 1.0f;

    // Smooth easing for gentle feel
    float contentAlpha = g_ctFadeAlpha * g_ctFadeAlpha * (3.0f - 2.0f * g_ctFadeAlpha);  // smoothstep

    // Aero green glass overlay effect
    // Layer 1: Semi-transparent green tint
    Color aeroGreen1 = {20, 180, 80, 40};
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, aeroGreen1);

    // Layer 2: Subtle gradient from top (lighter) to bottom (darker green)
    for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
        float gradientAlpha = 0.02f + (float)y / SCREEN_HEIGHT * 0.06f;
        Color gradLine = ColorAlpha(COLOR_SPOTIFY_GREEN, gradientAlpha);
        DrawRectangle(0, y, SCREEN_WIDTH, 4, gradLine);
    }

    // Layer 3: Glassy highlight at top (aero reflection)
    for (int i = 0; i < 80; i++) {
        float highlightAlpha = (80 - i) / 80.0f * 0.08f;
        DrawRectangle(0, i, SCREEN_WIDTH, 1, ColorAlpha(WHITE, highlightAlpha));
    }

    // Layer 4: Subtle vignette for depth
    for (int i = 0; i < 60; i++) {
        float vignetteAlpha = i / 60.0f * 0.15f;
        DrawRectangle(0, SCREEN_HEIGHT - 60 + i, SCREEN_WIDTH, 1, ColorAlpha(BLACK, vignetteAlpha));
    }

    // Calculate layout - icon is vertically centered
    float iconRadius = 70;
    float iconCenterY = SCREEN_HEIGHT / 2;
    float iconX = SCREEN_WIDTH / 2;

    // Plugin name below icon
    const char *pluginName = registry->items[selected].displayName;
    float mainFontSize = 64;
    Vector2 mainSize = MeasureTextEx(textFont, pluginName, mainFontSize, 2);

    // If name is too long, shrink it
    while (mainSize.x > SCREEN_WIDTH - 80 && mainFontSize > 32) {
        mainFontSize -= 4;
        mainSize = MeasureTextEx(textFont, pluginName, mainFontSize, 2);
    }

    float mainX = (SCREEN_WIDTH - mainSize.x) / 2;
    float mainY = iconCenterY + iconRadius + 30;

    // Flat circle behind letter - crossfades with content
    DrawCircle((int)iconX, (int)iconCenterY, iconRadius, ColorAlpha((Color){15, 60, 35, 200}, contentAlpha));

    // Spotify green ring
    DrawCircleLines((int)iconX, (int)iconCenterY, iconRadius, ColorAlpha(COLOR_SPOTIFY_GREEN, contentAlpha));

    // Initial letter inside - crossfades (uses textFont for consistency)
    if (pluginName[0]) {
        char initial[2] = {pluginName[0], '\0'};
        float initialSize = 60;
        Vector2 initialDim = MeasureTextEx(textFont, initial, initialSize, 1);
        DrawTextEx(textFont, initial,
                  (Vector2){iconX - initialDim.x / 2, iconCenterY - initialDim.y / 2},
                  initialSize, 1, ColorAlpha(COLOR_SPOTIFY_GREEN, contentAlpha));
    }

    // Main plugin name below icon - centered, crossfades (no horizontal movement)
    DrawTextEx(textFont, pluginName, (Vector2){mainX, mainY}, mainFontSize, 2, ColorAlpha(WHITE, contentAlpha));

    // Spotify green underline - centered, crossfades (no horizontal movement)
    float underlineWidth = fminf(mainSize.x + 40, SCREEN_WIDTH - 100);
    float underlineX = (SCREEN_WIDTH - underlineWidth) / 2;
    DrawRectangle((int)underlineX, (int)(mainY + mainSize.y + 12), (int)underlineWidth, 4,
                 ColorAlpha(COLOR_SPOTIFY_GREEN, contentAlpha));

    // Plugin counter below - centered, crossfades (no horizontal movement)
    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d / %d", selected + 1, registry->count);
    Vector2 counterSize = MeasureTextEx(textFont, counterStr, 24, 1);
    DrawTextEx(textFont, counterStr,
              (Vector2){(SCREEN_WIDTH - counterSize.x) / 2, mainY + mainSize.y + 40},
              24, 1, ColorAlpha(WHITE, 0.5f * contentAlpha));

    // Minimal navigation hints at sides (fixed position, always visible)
    float sideY = SCREEN_HEIGHT / 2;

    if (selected > 0) {
        // Left arrow
        DrawTextEx(textFont, "◀", (Vector2){40, sideY - 12}, 28, 1, ColorAlpha(COLOR_SPOTIFY_GREEN, 0.4f));

        // Previous plugin name (dim)
        const char *prevName = registry->items[selected - 1].displayName;
        Vector2 prevSize = MeasureTextEx(textFont, prevName, 16, 1);
        if (prevSize.x > 120) {
            char truncName[20];
            strncpy(truncName, prevName, 15);
            truncName[15] = '.';
            truncName[16] = '.';
            truncName[17] = '.';
            truncName[18] = '\0';
            DrawTextEx(textFont, truncName, (Vector2){40, sideY + 24}, 16, 1, ColorAlpha(WHITE, 0.25f));
        } else {
            DrawTextEx(textFont, prevName, (Vector2){40, sideY + 24}, 16, 1, ColorAlpha(WHITE, 0.25f));
        }
    }

    if (selected < registry->count - 1) {
        // Right arrow
        Vector2 arrowSize = MeasureTextEx(textFont, "▶", 28, 1);
        DrawTextEx(textFont, "▶", (Vector2){SCREEN_WIDTH - 40 - arrowSize.x, sideY - 12}, 28, 1, ColorAlpha(COLOR_SPOTIFY_GREEN, 0.4f));

        // Next plugin name (dim)
        const char *nextName = registry->items[selected + 1].displayName;
        Vector2 nextSize = MeasureTextEx(textFont, nextName, 16, 1);
        if (nextSize.x > 120) {
            char truncName[20];
            strncpy(truncName, nextName, 15);
            truncName[15] = '.';
            truncName[16] = '.';
            truncName[17] = '.';
            truncName[18] = '\0';
            Vector2 truncSize = MeasureTextEx(textFont, truncName, 16, 1);
            DrawTextEx(textFont, truncName, (Vector2){SCREEN_WIDTH - 40 - truncSize.x, sideY + 24}, 16, 1, ColorAlpha(WHITE, 0.25f));
        } else {
            DrawTextEx(textFont, nextName, (Vector2){SCREEN_WIDTH - 40 - nextSize.x, sideY + 24}, 16, 1, ColorAlpha(WHITE, 0.25f));
        }
    }

    // "llizardOS" branding in top left (uses Omicron font)
    DrawTextEx(brandFont, "llizardOS", (Vector2){24, 20}, 18, 1, ColorAlpha(WHITE, 0.4f));
}

// ============================================================================
// GRID STYLE - Apple Music library grid layout
// ============================================================================

#define GRID_COLS 2
#define GRID_TILE_WIDTH 360
#define GRID_TILE_HEIGHT 180
#define GRID_SPACING 20
#define GRID_PADDING_X 40
#define GRID_PADDING_TOP 100

static void DrawPluginMenuGrid(const PluginRegistry *registry, int selected, float deltaTime,
                               Color dynamicAccent, Color dynamicAccentDim)
{
    // Lazy load iBrand font on first use
    if (!g_ibrandFontLoaded) {
        LoadIBrandFont();
    }

    Font gridFont = g_ibrandFontLoaded ? g_ibrandFont : g_menuFont;

    if (!registry || registry->count == 0) {
        DrawTextEx(gridFont, "No plugins found", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, COLOR_TEXT_SECONDARY);
        DrawTextEx(gridFont, "Place .so files in ./plugins", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, COLOR_TEXT_DIM);
        return;
    }

    // Header
    DrawTextEx(gridFont, "llizardOS", (Vector2){GRID_PADDING_X, 24}, 32, 2, COLOR_TEXT_PRIMARY);
    DrawRectangle(GRID_PADDING_X, 62, 120, 3, dynamicAccent);

    // Calculate which row the selected item is in for vertical scrolling
    int selectedRow = selected / GRID_COLS;
    float targetScrollY = 0;
    float maxVisibleRows = (SCREEN_HEIGHT - GRID_PADDING_TOP - 20) / (GRID_TILE_HEIGHT + GRID_SPACING);

    // Scroll to keep selected row visible
    if (selectedRow >= maxVisibleRows) {
        targetScrollY = (selectedRow - maxVisibleRows + 1) * (GRID_TILE_HEIGHT + GRID_SPACING);
    }

    // Smooth scroll update (using existing scroll variable)
    float diff = targetScrollY - g_scrollOffset;
    g_scrollOffset += diff * 10.0f * deltaTime;
    if (fabsf(diff) < 1.0f) g_scrollOffset = targetScrollY;

    // Draw grid of tiles
    BeginScissorMode(0, GRID_PADDING_TOP - 10, SCREEN_WIDTH, SCREEN_HEIGHT - GRID_PADDING_TOP + 10);

    for (int i = 0; i < registry->count; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;

        float tileX = GRID_PADDING_X + col * (GRID_TILE_WIDTH + GRID_SPACING);
        float tileY = GRID_PADDING_TOP + row * (GRID_TILE_HEIGHT + GRID_SPACING) - g_scrollOffset;

        // Skip tiles outside visible area
        if (tileY < GRID_PADDING_TOP - GRID_TILE_HEIGHT - 20 || tileY > SCREEN_HEIGHT + 20) continue;

        bool isSelected = (i == selected);
        Rectangle tileRect = {tileX, tileY, GRID_TILE_WIDTH, GRID_TILE_HEIGHT};

        // Tile shadow for depth
        if (isSelected) {
            Rectangle shadowRect = {tileX + 6, tileY + 6, GRID_TILE_WIDTH, GRID_TILE_HEIGHT};
            DrawRectangleRounded(shadowRect, 0.1f, 8, ColorAlpha(BLACK, 0.4f));
        }

        // Tile background - gradient from accent for selected
        Color tileTop = isSelected ? ColorAlpha(dynamicAccent, 0.25f) : COLOR_CARD_BG;
        Color tileBottom = COLOR_CARD_BG;

        // Draw gradient manually
        DrawRectangleGradientV((int)tileX, (int)tileY, (int)GRID_TILE_WIDTH, (int)GRID_TILE_HEIGHT, tileTop, tileBottom);
        DrawRectangleRounded(tileRect, 0.1f, 8, ColorAlpha(COLOR_CARD_BG, 0.0f));  // Just for shape clip

        // Border
        Color borderColor = isSelected ? dynamicAccent : ColorAlpha(COLOR_CARD_BORDER, 0.4f);
        DrawRectangleRoundedLines(tileRect, 0.1f, 8, borderColor);

        // Selection glow
        if (isSelected) {
            Rectangle glowRect = {tileX - 3, tileY - 3, GRID_TILE_WIDTH + 6, GRID_TILE_HEIGHT + 6};
            DrawRectangleRoundedLines(glowRect, 0.1f, 8, ColorAlpha(dynamicAccent, 0.4f));
        }

        // Large icon circle on left side of tile
        float iconRadius = 50;
        float iconX = tileX + 70;
        float iconY = tileY + GRID_TILE_HEIGHT / 2;

        Color iconBg = isSelected ? ColorAlpha(dynamicAccent, 0.2f) : ColorAlpha(dynamicAccentDim, 0.15f);
        DrawCircle((int)iconX, (int)iconY, iconRadius, iconBg);
        DrawCircleLines((int)iconX, (int)iconY, iconRadius, isSelected ? dynamicAccent : COLOR_CARD_BORDER);

        // Initial letter
        if (registry->items[i].displayName[0]) {
            char initial[2] = {registry->items[i].displayName[0], '\0'};
            float initialSize = 40;
            Vector2 initialDim = MeasureTextEx(gridFont, initial, initialSize, 1);
            Color initialColor = isSelected ? dynamicAccent : COLOR_TEXT_SECONDARY;
            DrawTextEx(gridFont, initial,
                      (Vector2){iconX - initialDim.x / 2, iconY - initialDim.y / 2},
                      initialSize, 1, initialColor);
        }

        // Plugin name on right side - vertically centered (no description)
        float textX = iconX + iconRadius + 30;
        float maxTextWidth = GRID_TILE_WIDTH - (textX - tileX) - 20;

        Color nameColor = isSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
        float nameSize = 28;
        Vector2 nameDim = MeasureTextEx(gridFont, registry->items[i].displayName, nameSize, 1);

        // Vertically center the name in the tile
        float nameY = tileY + (GRID_TILE_HEIGHT - nameDim.y) / 2;

        // Truncate if too long
        if (nameDim.x > maxTextWidth) {
            char truncName[32];
            int maxChars = (int)(maxTextWidth / (nameDim.x / strlen(registry->items[i].displayName)));
            if (maxChars > 28) maxChars = 28;
            if (maxChars > 3) {
                strncpy(truncName, registry->items[i].displayName, maxChars - 3);
                truncName[maxChars - 3] = '.';
                truncName[maxChars - 2] = '.';
                truncName[maxChars - 1] = '.';
                truncName[maxChars] = '\0';
                DrawTextEx(gridFont, truncName, (Vector2){textX, nameY}, nameSize, 1, nameColor);
            }
        } else {
            DrawTextEx(gridFont, registry->items[i].displayName, (Vector2){textX, nameY}, nameSize, 1, nameColor);
        }

        // Index badge in corner
        char indexStr[16];
        snprintf(indexStr, sizeof(indexStr), "%d", i + 1);
        Vector2 indexSize = MeasureTextEx(gridFont, indexStr, 14, 1);
        float indexX = tileX + GRID_TILE_WIDTH - indexSize.x - 12;
        float indexY = tileY + GRID_TILE_HEIGHT - 24;
        DrawTextEx(gridFont, indexStr, (Vector2){indexX, indexY}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.5f));
    }

    EndScissorMode();

    // Page indicator at bottom
    char pageStr[32];
    snprintf(pageStr, sizeof(pageStr), "%d of %d", selected + 1, registry->count);
    Vector2 pageSize = MeasureTextEx(gridFont, pageStr, 16, 1);
    DrawTextEx(gridFont, pageStr, (Vector2){(SCREEN_WIDTH - pageSize.x) / 2, SCREEN_HEIGHT - 30}, 16, 1, COLOR_TEXT_DIM);
}

// ============================================================================
// MAIN MENU DISPATCHER
// ============================================================================

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

    // Update style indicator animation
    UpdateStyleIndicator(deltaTime);

    // Draw header (shared across list and carousel styles only)
    // Cards, SpotifyCT, and Grid styles handle their own headers
    if (g_menuStyle == MENU_STYLE_LIST || g_menuStyle == MENU_STYLE_CAROUSEL) {
        DrawMenuHeader(registry, selected, dynamicAccent, complementary);
    }

    // Dispatch to appropriate style renderer
    switch (g_menuStyle) {
        case MENU_STYLE_LIST:
            DrawPluginMenuList(registry, selected, deltaTime, dynamicAccent, dynamicAccentDim);
            break;

        case MENU_STYLE_CAROUSEL:
            DrawPluginMenuCarousel(registry, selected, deltaTime, dynamicAccent, dynamicAccentDim);
            break;

        case MENU_STYLE_CARDS:
            // Cards style has its own header in the card design
            DrawMenuHeader(registry, selected, dynamicAccent, complementary);
            DrawPluginMenuCards(registry, selected, dynamicAccent, complementary);
            break;

        case MENU_STYLE_SPOTIFY_CT:
            // Spotify CarThing style has completely custom layout - no shared header
            DrawPluginMenuSpotifyCT(registry, selected, deltaTime, dynamicAccent);
            break;

        case MENU_STYLE_GRID:
            // Grid style has its own header
            DrawPluginMenuGrid(registry, selected, deltaTime, dynamicAccent, dynamicAccentDim);
            break;

        default:
            DrawPluginMenuList(registry, selected, deltaTime, dynamicAccent, dynamicAccentDim);
            break;
    }

    // Draw style indicator overlay (shows when style changes)
    DrawStyleIndicator();
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

    // Create initial snapshot for change detection
    g_pluginSnapshot = CreatePluginSnapshot(pluginDir);

    int selectedIndex = 0;
    if (selectedIndex >= registry.count) selectedIndex = registry.count - 1;
    bool runningPlugin = false;
    LoadedPlugin *active = NULL;
    int lastPluginIndex = -1;  // Track last opened plugin for quick reopen

    // Check for startup plugin configuration
    if (LlzConfigHasStartupPlugin() && registry.count > 0) {
        const char *startupName = LlzConfigGetStartupPlugin();
        int startupIndex = -1;

        // Find the plugin by name (check displayName, api->name, and filename)
        for (int i = 0; i < registry.count; i++) {
            // Check exact match on displayName or api->name
            if (strcmp(registry.items[i].displayName, startupName) == 0 ||
                (registry.items[i].api && registry.items[i].api->name &&
                 strcmp(registry.items[i].api->name, startupName) == 0)) {
                startupIndex = i;
                break;
            }

            // Also check case-insensitive match (settings plugin may capitalize differently)
            if (strcasecmp(registry.items[i].displayName, startupName) == 0 ||
                (registry.items[i].api && registry.items[i].api->name &&
                 strcasecmp(registry.items[i].api->name, startupName) == 0)) {
                startupIndex = i;
                break;
            }

            // Check against filename (e.g., "nowplaying.so" -> "nowplaying")
            // Settings plugin generates names from filenames, so this helps match
            const char *path = registry.items[i].path;
            const char *filename = strrchr(path, '/');
            filename = filename ? filename + 1 : path;
            char baseName[64] = {0};
            const char *dot = strrchr(filename, '.');
            size_t baseLen = dot ? (size_t)(dot - filename) : strlen(filename);
            if (baseLen < sizeof(baseName)) {
                strncpy(baseName, filename, baseLen);
                // Capitalize first letter for comparison
                if (baseName[0] >= 'a' && baseName[0] <= 'z') {
                    baseName[0] -= 32;
                }
                if (strcasecmp(baseName, startupName) == 0) {
                    startupIndex = i;
                    break;
                }
            }
        }

        if (startupIndex >= 0) {
            printf("Launching startup plugin: %s\n", startupName);
            selectedIndex = startupIndex;
            lastPluginIndex = startupIndex;
            active = &registry.items[startupIndex];
            if (active->api && active->api->init) {
                active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
            }
            runningPlugin = true;
        } else {
            printf("Startup plugin '%s' not found, showing menu\n", startupName);
        }
    }

    LlzInputState inputState;

    while (!WindowShouldClose()) {
        float delta = GetFrameTime();
        LlzInputUpdate(&inputState);

        if (!runningPlugin) {
            // Update SDK background animations
            LlzBackgroundUpdate(delta);

            // Periodic plugin directory check
            g_pluginRefreshTimer += delta;
            if (g_pluginRefreshTimer >= PLUGIN_REFRESH_INTERVAL) {
                g_pluginRefreshTimer = 0.0f;

                if (HasPluginDirectoryChanged(pluginDir, &g_pluginSnapshot)) {
                    // Remember current selection name to restore after refresh
                    char selectedName[128] = {0};
                    if (selectedIndex >= 0 && selectedIndex < registry.count) {
                        strncpy(selectedName, registry.items[selectedIndex].displayName,
                                sizeof(selectedName) - 1);
                    }

                    // Refresh plugins
                    int changes = RefreshPlugins(pluginDir, &registry);
                    if (changes > 0) {
                        printf("Plugins refreshed: %d change(s)\n", changes);

                        // Update snapshot
                        FreePluginSnapshot(&g_pluginSnapshot);
                        g_pluginSnapshot = CreatePluginSnapshot(pluginDir);

                        // Try to restore selection by name
                        int newIndex = -1;
                        if (selectedName[0]) {
                            for (int i = 0; i < registry.count; i++) {
                                if (strcmp(registry.items[i].displayName, selectedName) == 0) {
                                    newIndex = i;
                                    break;
                                }
                            }
                        }

                        // Update selection
                        if (newIndex >= 0) {
                            selectedIndex = newIndex;
                        } else if (registry.count > 0) {
                            // Clamp to valid range
                            if (selectedIndex >= registry.count) {
                                selectedIndex = registry.count - 1;
                            }
                        } else {
                            selectedIndex = 0;
                        }

                        // Reset last plugin index if that plugin was removed
                        if (lastPluginIndex >= registry.count) {
                            lastPluginIndex = -1;
                        }
                    }
                }
            }

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

            // Cycle menu navigation style with button3 release (display mode button)
            if (inputState.button3Pressed) {
                CycleMenuStyle();
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
    FreePluginSnapshot(&g_pluginSnapshot);
    UnloadPlugins(&registry);
    UnloadIBrandFont();
    UnloadTracklisterFont();
    UnloadOmicronFont();
    UnloadMenuFont();
    LlzBackgroundShutdown();
    LlzInputShutdown();
    LlzDisplayShutdown();
    LlzConfigShutdown();
    return 0;
}
