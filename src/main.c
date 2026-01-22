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
static const Color COLOR_FOLDER = {100, 180, 255, 255};

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

// Folder-based menu state
static MenuItemList g_menuItems = {0};
static bool g_insideFolder = false;
static LlzPluginCategory g_currentFolder = LLZ_CATEGORY_MEDIA;
static int *g_folderPlugins = NULL;
static int g_folderPluginCount = 0;

// Global registry (needed by menu item helpers)
static PluginRegistry g_registry = {0};

// ============================================================================
// Menu Item Helpers - unified access for all view types
// ============================================================================

// Get total item count for current menu context
static int GetMenuItemCount(void) {
    return g_insideFolder ? g_folderPluginCount : g_menuItems.count;
}

// Get display name for item at index
static const char* GetMenuItemName(int index) {
    if (g_insideFolder) {
        if (index < 0 || index >= g_folderPluginCount) return "";
        int pluginIdx = g_folderPlugins[index];
        if (pluginIdx < 0 || pluginIdx >= g_registry.count) return "";
        return g_registry.items[pluginIdx].displayName;
    } else {
        if (index < 0 || index >= g_menuItems.count) return "";
        return g_menuItems.items[index].displayName;
    }
}

// Get description for item at index (folders return NULL)
static const char* GetMenuItemDescription(int index) {
    if (g_insideFolder) {
        if (index < 0 || index >= g_folderPluginCount) return NULL;
        int pluginIdx = g_folderPlugins[index];
        if (pluginIdx < 0 || pluginIdx >= g_registry.count) return NULL;
        if (g_registry.items[pluginIdx].api) return g_registry.items[pluginIdx].api->description;
        return NULL;
    } else {
        if (index < 0 || index >= g_menuItems.count) return NULL;
        MenuItem *item = &g_menuItems.items[index];
        if (item->type == MENU_ITEM_FOLDER) return NULL;
        int pluginIdx = item->plugin.pluginIndex;
        if (pluginIdx < 0 || pluginIdx >= g_registry.count) return NULL;
        if (g_registry.items[pluginIdx].api) return g_registry.items[pluginIdx].api->description;
        return NULL;
    }
}

// Check if item at index is a folder
static bool IsMenuItemFolder(int index) {
    if (g_insideFolder) return false;  // Inside folder = all items are plugins
    if (index < 0 || index >= g_menuItems.count) return false;
    return g_menuItems.items[index].type == MENU_ITEM_FOLDER;
}

// Get folder plugin count (for folder items)
static int GetMenuItemFolderCount(int index) {
    if (g_insideFolder) return 0;
    if (index < 0 || index >= g_menuItems.count) return 0;
    MenuItem *item = &g_menuItems.items[index];
    if (item->type != MENU_ITEM_FOLDER) return 0;
    return item->folder.pluginCount;
}

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

    // Save to config for persistence across reboots
    LlzConfigSetMenuStyle((LlzMenuStyle)g_menuStyle);
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
    (void)registry;  // Used by other menu styles, kept for API consistency
    // Header - show folder name with back hint if inside folder
    if (g_insideFolder) {
        // Back arrow and folder name
        DrawTextEx(g_menuFont, "◀", (Vector2){MENU_PADDING_X, 32}, 24, 1, COLOR_TEXT_DIM);
        const char *folderName = LLZ_CATEGORY_NAMES[g_currentFolder];
        DrawTextEx(g_menuFont, folderName, (Vector2){MENU_PADDING_X + 34, 28}, 38, 2, COLOR_TEXT_PRIMARY);

        // Subtle accent underline
        Vector2 folderSize = MeasureTextEx(g_menuFont, folderName, 38, 2);
        DrawRectangle(MENU_PADDING_X + 34, 74, (int)folderSize.x, 3, COLOR_FOLDER);

        // Back hint
        const char *instructions = "back to return • select to launch";
        DrawTextEx(g_menuFont, instructions, (Vector2){MENU_PADDING_X, 88}, 16, 1, COLOR_TEXT_DIM);
    } else {
        // Normal header
        DrawTextEx(g_menuFont, "llizardOS", (Vector2){MENU_PADDING_X, 28}, 38, 2, COLOR_TEXT_PRIMARY);

        // Selected item name in top right - uses complementary color
        if (g_menuItems.count > 0 && selected >= 0 && selected < g_menuItems.count) {
            MenuItem *item = &g_menuItems.items[selected];
            const char *itemName = item->displayName;
            float fontSize = 36;
            float spacing = 2;
            Vector2 textSize = MeasureTextEx(g_menuFont, itemName, fontSize, spacing);
            float textX = SCREEN_WIDTH - textSize.x - MENU_PADDING_X;
            float textY = 28;
            DrawTextEx(g_menuFont, itemName, (Vector2){textX, textY}, fontSize, spacing, complementary);
        }

        // Subtle accent underline
        DrawRectangle(MENU_PADDING_X, 74, 160, 3, dynamicAccent);

        // Instruction text
        const char *instructions = "scroll to navigate • select to launch";
        DrawTextEx(g_menuFont, instructions, (Vector2){MENU_PADDING_X, 88}, 16, 1, COLOR_TEXT_DIM);
    }
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
    (void)registry;  // Now uses global helpers for menu items

    int itemCount = GetMenuItemCount();
    if (itemCount == 0) {
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
    for (int i = 0; i < itemCount; i++) {
        const char *itemName = GetMenuItemName(i);
        bool isFolder = IsMenuItemFolder(i);

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

        // Use folder color for folders
        Color itemAccent = isFolder ? COLOR_FOLDER : dynamicAccent;
        Color itemAccentDim = isFolder ? ColorAlpha(COLOR_FOLDER, 0.5f) : dynamicAccentDim;

        Color cardBg = isSelected ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
        Color borderColor = isSelected ? itemAccent : COLOR_CARD_BORDER;

        DrawRectangleRounded(cardRect, 0.12f, 8, ColorAlpha(cardBg, alpha));
        DrawRectangleRoundedLines(cardRect, 0.12f, 8, ColorAlpha(borderColor, alpha * (isSelected ? 0.8f : 0.3f)));

        // Selection glow ring for center item
        if (isSelected && normalizedDist < 0.1f) {
            Rectangle glowRect = {cardX - 4, cardY - 4, cardWidth + 8, cardHeight + 8};
            DrawRectangleRoundedLines(glowRect, 0.12f, 8, ColorAlpha(itemAccent, 0.4f));
        }

        // Plugin/folder icon placeholder (large centered circle)
        float iconRadius = cardHeight * 0.25f;
        float iconY = cardY + cardHeight * 0.35f;
        DrawCircle((int)(cardX + cardWidth / 2), (int)iconY, iconRadius, ColorAlpha(itemAccentDim, alpha * 0.4f));
        DrawCircleLines((int)(cardX + cardWidth / 2), (int)iconY, iconRadius, ColorAlpha(itemAccent, alpha * 0.6f));

        // First letter as icon (or folder icon)
        if (itemName && itemName[0]) {
            const char *iconChar = isFolder ? "F" : NULL;
            char initial[2] = {0};
            if (!iconChar) {
                initial[0] = itemName[0];
                initial[1] = '\0';
                iconChar = initial;
            }
            float initialSize = iconRadius * 1.2f;
            Vector2 initialDim = MeasureTextEx(g_menuFont, iconChar, initialSize, 1);
            DrawTextEx(g_menuFont, iconChar,
                      (Vector2){cardX + cardWidth / 2 - initialDim.x / 2, iconY - initialDim.y / 2},
                      initialSize, 1, ColorAlpha(isFolder ? COLOR_FOLDER : COLOR_TEXT_PRIMARY, alpha));
        }

        // Item name below icon (larger font, no description)
        float fontSize = 26 * scale;
        if (fontSize > 14 && itemName) {
            Vector2 nameSize = MeasureTextEx(g_menuFont, itemName, fontSize, 1);
            float nameX = cardX + (cardWidth - nameSize.x) / 2;
            float nameY = cardY + cardHeight * 0.75f;
            Color nameColor = isSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
            DrawTextEx(g_menuFont, itemName,
                      (Vector2){nameX, nameY}, fontSize, 1, ColorAlpha(nameColor, alpha));
        }
    }

    // Navigation dots at bottom
    float dotY = CAROUSEL_CENTER_Y + CAROUSEL_ITEM_HEIGHT / 2 + 50;
    float totalDotsWidth = itemCount * 16;
    float dotStartX = (SCREEN_WIDTH - totalDotsWidth) / 2;

    for (int i = 0; i < itemCount; i++) {
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
    (void)registry;  // Now uses global helpers for menu items

    int itemCount = GetMenuItemCount();
    if (itemCount == 0) {
        DrawTextEx(g_menuFont, "No plugins found", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, COLOR_TEXT_SECONDARY);
        DrawTextEx(g_menuFont, "Place .so files in ./plugins", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, COLOR_TEXT_DIM);
        return;
    }

    const char *selectedName = GetMenuItemName(selected);
    const char *selectedDesc = GetMenuItemDescription(selected);
    bool isFolder = IsMenuItemFolder(selected);
    int folderCount = GetMenuItemFolderCount(selected);

    // Use folder color for folders
    Color itemAccent = isFolder ? COLOR_FOLDER : dynamicAccent;

    // Large card dimensions - fills most of the screen
    float cardWidth = SCREEN_WIDTH - 80;
    float cardHeight = 280;
    float cardX = 40;
    float cardY = MENU_PADDING_TOP + 20;

    // Main card background with gradient
    Rectangle cardRect = {cardX, cardY, cardWidth, cardHeight};

    // Spotify-style gradient background on card
    Color gradientTop = ColorAlpha(itemAccent, 0.15f);
    Color gradientBottom = COLOR_CARD_BG;
    DrawRectangleGradientV((int)cardX, (int)cardY, (int)cardWidth, (int)cardHeight, gradientTop, gradientBottom);
    DrawRectangleRoundedLines(cardRect, 0.05f, 8, ColorAlpha(itemAccent, 0.3f));

    // Large plugin icon/initial on the left side
    float iconSize = 160;
    float iconX = cardX + 40;
    float iconY = cardY + (cardHeight - iconSize) / 2;

    // Icon background circle
    DrawCircle((int)(iconX + iconSize / 2), (int)(iconY + iconSize / 2), iconSize / 2 + 4, ColorAlpha(itemAccent, 0.2f));
    DrawCircle((int)(iconX + iconSize / 2), (int)(iconY + iconSize / 2), iconSize / 2, COLOR_CARD_SELECTED);

    // Large initial letter (or folder icon)
    if (selectedName && selectedName[0]) {
        const char *iconChar = isFolder ? "F" : NULL;
        char initial[2] = {0};
        if (!iconChar) {
            initial[0] = selectedName[0];
            initial[1] = '\0';
            iconChar = initial;
        }
        float initialSize = iconSize * 0.6f;
        Vector2 initialDim = MeasureTextEx(g_menuFont, iconChar, initialSize, 1);
        DrawTextEx(g_menuFont, iconChar,
                  (Vector2){iconX + iconSize / 2 - initialDim.x / 2, iconY + iconSize / 2 - initialDim.y / 2},
                  initialSize, 1, itemAccent);
    }

    // Plugin info on the right side
    float textX = iconX + iconSize + 40;

    // Large plugin/folder name
    if (selectedName) {
        DrawTextEx(g_menuFont, selectedName, (Vector2){textX, cardY + 50}, 42, 2, COLOR_TEXT_PRIMARY);
    }

    // Description (or folder plugin count)
    if (isFolder) {
        char folderDesc[64];
        snprintf(folderDesc, sizeof(folderDesc), "%d plugin%s", folderCount, folderCount == 1 ? "" : "s");
        DrawTextEx(g_menuFont, folderDesc, (Vector2){textX, cardY + 105}, 20, 1, COLOR_TEXT_SECONDARY);
    } else if (selectedDesc) {
        DrawTextEx(g_menuFont, selectedDesc, (Vector2){textX, cardY + 105}, 20, 1, COLOR_TEXT_SECONDARY);
    }

    // Item index badge
    char indexStr[32];
    snprintf(indexStr, sizeof(indexStr), "%s %d of %d", isFolder ? "Folder" : "Plugin", selected + 1, itemCount);
    DrawTextEx(g_menuFont, indexStr, (Vector2){textX, cardY + 150}, 16, 1, COLOR_TEXT_DIM);

    // "Press select to launch/open" hint
    const char *actionHint = isFolder ? "Press SELECT to open" : "Press SELECT to launch";
    DrawTextEx(g_menuFont, actionHint, (Vector2){textX, cardY + cardHeight - 60}, 18, 1, complementary);

    // Previous/Next preview cards (smaller, on sides)
    float previewWidth = 140;
    float previewHeight = 100;
    float previewY = cardY + cardHeight + 30;

    // Previous item preview (if exists)
    if (selected > 0) {
        const char *prevName = GetMenuItemName(selected - 1);
        Rectangle prevRect = {40, previewY, previewWidth, previewHeight};
        DrawRectangleRounded(prevRect, 0.1f, 6, ColorAlpha(COLOR_CARD_BG, 0.6f));
        DrawRectangleRoundedLines(prevRect, 0.1f, 6, ColorAlpha(COLOR_CARD_BORDER, 0.3f));

        // Arrow and name
        DrawTextEx(g_menuFont, "◀", (Vector2){50, previewY + 15}, 24, 1, COLOR_TEXT_DIM);
        if (prevName) {
            DrawTextEx(g_menuFont, prevName, (Vector2){50, previewY + 50}, 16, 1, COLOR_TEXT_SECONDARY);
        }
    }

    // Next item preview (if exists)
    if (selected < itemCount - 1) {
        const char *nextName = GetMenuItemName(selected + 1);
        float nextX = SCREEN_WIDTH - 40 - previewWidth;
        Rectangle nextRect = {nextX, previewY, previewWidth, previewHeight};
        DrawRectangleRounded(nextRect, 0.1f, 6, ColorAlpha(COLOR_CARD_BG, 0.6f));
        DrawRectangleRoundedLines(nextRect, 0.1f, 6, ColorAlpha(COLOR_CARD_BORDER, 0.3f));

        // Arrow and name (right-aligned)
        Vector2 arrowSize = MeasureTextEx(g_menuFont, "▶", 24, 1);
        DrawTextEx(g_menuFont, "▶", (Vector2){nextX + previewWidth - arrowSize.x - 10, previewY + 15}, 24, 1, COLOR_TEXT_DIM);

        if (nextName) {
            Vector2 nameSize = MeasureTextEx(g_menuFont, nextName, 16, 1);
            float nameX = nextX + previewWidth - nameSize.x - 10;
            DrawTextEx(g_menuFont, nextName, (Vector2){nameX, previewY + 50}, 16, 1, COLOR_TEXT_SECONDARY);
        }
    }

    // Progress bar at bottom showing position
    float barWidth = SCREEN_WIDTH - 160;
    float barX = 80;
    float barY = SCREEN_HEIGHT - 30;
    float barHeight = 4;

    DrawRectangleRounded((Rectangle){barX, barY, barWidth, barHeight}, 0.5f, 4, ColorAlpha(COLOR_CARD_BORDER, 0.3f));

    // Progress indicator
    float progress = (float)selected / (float)(itemCount - 1 > 0 ? itemCount - 1 : 1);
    float indicatorWidth = barWidth / itemCount;
    float indicatorX = barX + progress * (barWidth - indicatorWidth);
    DrawRectangleRounded((Rectangle){indicatorX, barY, indicatorWidth, barHeight}, 0.5f, 4, itemAccent);
}

// ============================================================================
// LIST STYLE - Folder-based menu with categories
// ============================================================================

// Draw a single menu item (folder or plugin)
static void DrawMenuItem(float x, float y, float width, float height,
                         const char *name, const char *description,
                         bool isFolder, bool isSelected, int itemCount,
                         Color dynamicAccent, Color dynamicAccentDim)
{
    (void)dynamicAccentDim;  // Reserved for future use
    Rectangle cardRect = {x, y, width, height};

    Color cardBg = isSelected ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
    Color borderColor = isSelected ? dynamicAccent : COLOR_CARD_BORDER;

    // Draw card with rounded corners
    DrawRectangleRounded(cardRect, 0.15f, 8, cardBg);

    // Selection accent bar on left
    if (isSelected) {
        Rectangle accentBar = {cardRect.x, cardRect.y + 8, 4, cardRect.height - 16};
        Color barColor = isFolder ? COLOR_FOLDER : dynamicAccent;
        DrawRectangleRounded(accentBar, 0.5f, 4, barColor);
    }

    // Subtle border
    DrawRectangleRoundedLines(cardRect, 0.15f, 8, ColorAlpha(borderColor, isSelected ? 0.6f : 0.2f));

    // Icon for folders
    float textStartX = MENU_PADDING_X + 8;
    if (isFolder) {
        // Draw folder icon
        Color iconColor = isSelected ? COLOR_FOLDER : ColorAlpha(COLOR_FOLDER, 0.6f);
        DrawTextEx(g_menuFont, "📁", (Vector2){textStartX, y + 20}, 24, 1, iconColor);
        textStartX += 36;
    }

    // Name
    Color nameColor = isSelected ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
    DrawTextEx(g_menuFont, name, (Vector2){textStartX, y + 16}, 24, 1.5f, nameColor);

    // Description or plugin count for folders
    if (isFolder && itemCount > 0) {
        char countStr[32];
        snprintf(countStr, sizeof(countStr), "%d plugin%s", itemCount, itemCount == 1 ? "" : "s");
        Color descColor = isSelected ? COLOR_TEXT_SECONDARY : COLOR_TEXT_DIM;
        DrawTextEx(g_menuFont, countStr, (Vector2){textStartX, y + 46}, 16, 1, descColor);

        // Arrow indicator on right
        Vector2 arrowSize = MeasureTextEx(g_menuFont, "▶", 18, 1);
        Color arrowColor = isSelected ? dynamicAccent : COLOR_TEXT_DIM;
        DrawTextEx(g_menuFont, "▶",
                  (Vector2){cardRect.x + cardRect.width - arrowSize.x - 16, y + (height - 18) / 2},
                  18, 1, arrowColor);
    } else if (description) {
        Color descColor = isSelected ? COLOR_TEXT_SECONDARY : COLOR_TEXT_DIM;
        DrawTextEx(g_menuFont, description, (Vector2){textStartX, y + 46}, 16, 1, descColor);
    }
}

static void DrawPluginMenuList(const PluginRegistry *registry, int selected, float deltaTime,
                               Color dynamicAccent, Color dynamicAccentDim)
{
    int itemCount = 0;

    // Determine what we're showing
    if (g_insideFolder) {
        itemCount = g_folderPluginCount;
    } else {
        itemCount = g_menuItems.count;
    }

    if (itemCount == 0) {
        if (g_insideFolder) {
            DrawTextEx(g_menuFont, "Folder is empty", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, COLOR_TEXT_SECONDARY);
        } else {
            DrawTextEx(g_menuFont, "No plugins found", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, COLOR_TEXT_SECONDARY);
            DrawTextEx(g_menuFont, "Place .so files in ./plugins", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, COLOR_TEXT_DIM);
        }
        return;
    }

    // Update scroll animation
    g_targetScrollOffset = CalculateTargetScroll(selected, itemCount);
    UpdateScroll(deltaTime);

    // Calculate scroll indicators
    float itemTotalHeight = MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
    float totalListHeight = itemCount * itemTotalHeight;
    float maxScroll = totalListHeight - MENU_VISIBLE_AREA;
    if (maxScroll < 0) maxScroll = 0;

    bool canScrollUp = g_scrollOffset > 1.0f;
    bool canScrollDown = g_scrollOffset < maxScroll - 1.0f;

    // Clipping region for list
    BeginScissorMode(0, MENU_PADDING_TOP, SCREEN_WIDTH, (int)MENU_VISIBLE_AREA);

    // Draw items
    for (int i = 0; i < itemCount; ++i) {
        float itemY = MENU_PADDING_TOP + i * itemTotalHeight - g_scrollOffset;

        // Skip items outside visible area
        if (itemY < MENU_PADDING_TOP - MENU_ITEM_HEIGHT || itemY > SCREEN_HEIGHT) continue;

        bool isSelected = (i == selected);
        float cardX = MENU_PADDING_X - 12;
        float cardWidth = SCREEN_WIDTH - (MENU_PADDING_X - 12) * 2;

        if (g_insideFolder) {
            // Inside folder: show plugins
            int pluginIdx = g_folderPlugins[i];
            const LoadedPlugin *plugin = &registry->items[pluginIdx];
            const char *desc = (plugin->api && plugin->api->description) ? plugin->api->description : "";
            DrawMenuItem(cardX, itemY, cardWidth, MENU_ITEM_HEIGHT,
                         plugin->displayName, desc,
                         false, isSelected, 0,
                         dynamicAccent, dynamicAccentDim);
        } else {
            // Main menu: show folders and home plugins
            MenuItem *item = &g_menuItems.items[i];
            if (item->type == MENU_ITEM_FOLDER) {
                DrawMenuItem(cardX, itemY, cardWidth, MENU_ITEM_HEIGHT,
                             item->displayName, NULL,
                             true, isSelected, item->folder.pluginCount,
                             dynamicAccent, dynamicAccentDim);
            } else {
                int pluginIdx = item->plugin.pluginIndex;
                const LoadedPlugin *plugin = &registry->items[pluginIdx];
                const char *desc = (plugin->api && plugin->api->description) ? plugin->api->description : "";
                DrawMenuItem(cardX, itemY, cardWidth, MENU_ITEM_HEIGHT,
                             plugin->displayName, desc,
                             false, isSelected, 0,
                             dynamicAccent, dynamicAccentDim);
            }
        }
    }

    EndScissorMode();

    // Scroll indicators
    if (canScrollUp) {
        for (int i = 0; i < 30; i++) {
            float alpha = (30 - i) / 30.0f * 0.8f;
            Color fade = ColorAlpha(COLOR_BG_DARK, alpha);
            DrawRectangle(0, MENU_PADDING_TOP + i, SCREEN_WIDTH, 1, fade);
        }
        DrawTextEx(g_menuFont, "▲", (Vector2){SCREEN_WIDTH / 2 - 6, MENU_PADDING_TOP + 4}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.6f));
    }

    if (canScrollDown) {
        int bottomY = MENU_PADDING_TOP + (int)MENU_VISIBLE_AREA;
        for (int i = 0; i < 30; i++) {
            float alpha = i / 30.0f * 0.8f;
            Color fade = ColorAlpha(COLOR_BG_DARK, alpha);
            DrawRectangle(0, bottomY - 30 + i, SCREEN_WIDTH, 1, fade);
        }
        DrawTextEx(g_menuFont, "▼", (Vector2){SCREEN_WIDTH / 2 - 6, bottomY - 18}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.6f));
    }

    // Selection counter and folder indicator at bottom
    char counterStr[64];
    if (g_insideFolder) {
        snprintf(counterStr, sizeof(counterStr), "%s: %d of %d",
                 LLZ_CATEGORY_NAMES[g_currentFolder], selected + 1, itemCount);
    } else {
        snprintf(counterStr, sizeof(counterStr), "%d of %d", selected + 1, itemCount);
    }
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
    (void)registry;  // Now uses global helpers for menu items
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

    int itemCount = GetMenuItemCount();
    if (itemCount == 0) {
        DrawTextEx(textFont, "No plugins", (Vector2){SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 - 20}, 32, 1, COLOR_TEXT_SECONDARY);
        return;
    }

    const char *itemName = GetMenuItemName(selected);
    bool isFolder = IsMenuItemFolder(selected);

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

    // Use folder color or Spotify green
    Color accentColor = isFolder ? COLOR_FOLDER : COLOR_SPOTIFY_GREEN;

    // Aero glass overlay effect
    // Layer 1: Semi-transparent tint
    Color aeroTint = isFolder ? (Color){40, 100, 180, 40} : (Color){20, 180, 80, 40};
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, aeroTint);

    // Layer 2: Subtle gradient from top (lighter) to bottom (darker)
    for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
        float gradientAlpha = 0.02f + (float)y / SCREEN_HEIGHT * 0.06f;
        Color gradLine = ColorAlpha(accentColor, gradientAlpha);
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

    // Item name below icon
    float mainFontSize = 64;
    Vector2 mainSize = itemName ? MeasureTextEx(textFont, itemName, mainFontSize, 2) : (Vector2){0, 0};

    // If name is too long, shrink it
    while (itemName && mainSize.x > SCREEN_WIDTH - 80 && mainFontSize > 32) {
        mainFontSize -= 4;
        mainSize = MeasureTextEx(textFont, itemName, mainFontSize, 2);
    }

    float mainX = (SCREEN_WIDTH - mainSize.x) / 2;
    float mainY = iconCenterY + iconRadius + 30;

    // Flat circle behind letter - crossfades with content
    Color circleBg = isFolder ? (Color){20, 50, 100, 200} : (Color){15, 60, 35, 200};
    DrawCircle((int)iconX, (int)iconCenterY, iconRadius, ColorAlpha(circleBg, contentAlpha));

    // Accent ring
    DrawCircleLines((int)iconX, (int)iconCenterY, iconRadius, ColorAlpha(accentColor, contentAlpha));

    // Initial letter inside - crossfades (uses textFont for consistency)
    if (itemName && itemName[0]) {
        const char *iconChar = isFolder ? "F" : NULL;
        char initial[2] = {0};
        if (!iconChar) {
            initial[0] = itemName[0];
            initial[1] = '\0';
            iconChar = initial;
        }
        float initialSize = 60;
        Vector2 initialDim = MeasureTextEx(textFont, iconChar, initialSize, 1);
        DrawTextEx(textFont, iconChar,
                  (Vector2){iconX - initialDim.x / 2, iconCenterY - initialDim.y / 2},
                  initialSize, 1, ColorAlpha(accentColor, contentAlpha));
    }

    // Main item name below icon - centered, crossfades (no horizontal movement)
    if (itemName) {
        DrawTextEx(textFont, itemName, (Vector2){mainX, mainY}, mainFontSize, 2, ColorAlpha(WHITE, contentAlpha));
    }

    // Accent underline - centered, crossfades (no horizontal movement)
    float underlineWidth = fminf(mainSize.x + 40, SCREEN_WIDTH - 100);
    float underlineX = (SCREEN_WIDTH - underlineWidth) / 2;
    DrawRectangle((int)underlineX, (int)(mainY + mainSize.y + 12), (int)underlineWidth, 4,
                 ColorAlpha(accentColor, contentAlpha));

    // Item counter below - centered, crossfades (no horizontal movement)
    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d / %d", selected + 1, itemCount);
    Vector2 counterSize = MeasureTextEx(textFont, counterStr, 24, 1);
    DrawTextEx(textFont, counterStr,
              (Vector2){(SCREEN_WIDTH - counterSize.x) / 2, mainY + mainSize.y + 40},
              24, 1, ColorAlpha(WHITE, 0.5f * contentAlpha));

    // Minimal navigation hints at sides (fixed position, always visible)
    float sideY = SCREEN_HEIGHT / 2;

    if (selected > 0) {
        // Left arrow
        DrawTextEx(textFont, "◀", (Vector2){40, sideY - 12}, 28, 1, ColorAlpha(accentColor, 0.4f));

        // Previous item name (dim)
        const char *prevName = GetMenuItemName(selected - 1);
        if (prevName) {
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
    }

    if (selected < itemCount - 1) {
        // Right arrow
        Vector2 arrowSize = MeasureTextEx(textFont, "▶", 28, 1);
        DrawTextEx(textFont, "▶", (Vector2){SCREEN_WIDTH - 40 - arrowSize.x, sideY - 12}, 28, 1, ColorAlpha(accentColor, 0.4f));

        // Next item name (dim)
        const char *nextName = GetMenuItemName(selected + 1);
        if (nextName) {
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
    (void)registry;          // Now uses global helpers for menu items
    (void)dynamicAccent;     // Grid uses its own Apple color scheme
    (void)dynamicAccentDim;

    // Apple-inspired white theme colors
    static const Color GRID_BG_WHITE = {250, 250, 252, 255};
    static const Color GRID_TILE_BG = {255, 255, 255, 255};
    static const Color GRID_TILE_HOVER = {248, 248, 250, 255};
    static const Color GRID_TEXT_PRIMARY = {30, 30, 32, 255};
    static const Color GRID_TEXT_SECONDARY = {100, 100, 105, 255};
    static const Color GRID_TEXT_DIM = {160, 160, 165, 255};
    static const Color GRID_BORDER = {220, 220, 225, 255};

    // Apple traffic light colors
    static const Color APPLE_RED = {255, 95, 86, 255};
    static const Color APPLE_YELLOW = {255, 189, 46, 255};
    static const Color APPLE_GREEN = {39, 201, 63, 255};
    static const Color APPLE_BLUE = {0, 122, 255, 255};  // For folders

    // Draw white background (override animated background)
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GRID_BG_WHITE);

    // Lazy load iBrand font on first use
    if (!g_ibrandFontLoaded) {
        LoadIBrandFont();
    }

    Font gridFont = g_ibrandFontLoaded ? g_ibrandFont : g_menuFont;

    int itemCount = GetMenuItemCount();
    if (itemCount == 0) {
        DrawTextEx(gridFont, "No plugins found", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 40}, 24, 1, GRID_TEXT_SECONDARY);
        DrawTextEx(gridFont, "Place .so files in ./plugins", (Vector2){MENU_PADDING_X, MENU_PADDING_TOP + 70}, 18, 1, GRID_TEXT_DIM);
        return;
    }

    // Header with traffic light dots
    float dotY = 36;
    float dotSpacing = 24;
    float dotRadius = 8;
    DrawCircle(GRID_PADDING_X + 8, dotY, dotRadius, APPLE_RED);
    DrawCircle(GRID_PADDING_X + 8 + dotSpacing, dotY, dotRadius, APPLE_YELLOW);
    DrawCircle(GRID_PADDING_X + 8 + dotSpacing * 2, dotY, dotRadius, APPLE_GREEN);

    // Show folder context if inside folder
    const char *headerText = g_insideFolder ? GetMenuItemName(0) : "llizardOS";
    if (g_insideFolder) {
        // Get the folder name from g_currentFolder
        headerText = LLZ_CATEGORY_NAMES[g_currentFolder];
    }
    DrawTextEx(gridFont, headerText, (Vector2){GRID_PADDING_X + dotSpacing * 3 + 20, 24}, 32, 2, GRID_TEXT_PRIMARY);

    // Subtle divider line
    DrawRectangle(GRID_PADDING_X, 68, SCREEN_WIDTH - GRID_PADDING_X * 2, 1, GRID_BORDER);

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

    for (int i = 0; i < itemCount; i++) {
        const char *itemName = GetMenuItemName(i);
        bool isFolder = IsMenuItemFolder(i);

        int col = i % GRID_COLS;
        int row = i / GRID_COLS;

        float tileX = GRID_PADDING_X + col * (GRID_TILE_WIDTH + GRID_SPACING);
        float tileY = GRID_PADDING_TOP + row * (GRID_TILE_HEIGHT + GRID_SPACING) - g_scrollOffset;

        // Skip tiles outside visible area
        if (tileY < GRID_PADDING_TOP - GRID_TILE_HEIGHT - 20 || tileY > SCREEN_HEIGHT + 20) continue;

        bool isSelected = (i == selected);
        Rectangle tileRect = {tileX, tileY, GRID_TILE_WIDTH, GRID_TILE_HEIGHT};

        // Soft shadow for all tiles
        Rectangle shadowRect = {tileX + 2, tileY + 2, GRID_TILE_WIDTH, GRID_TILE_HEIGHT};
        DrawRectangleRounded(shadowRect, 0.12f, 8, ColorAlpha(BLACK, isSelected ? 0.12f : 0.06f));

        // Tile background - white with subtle tint when selected
        Color tileBg = isSelected ? GRID_TILE_BG : GRID_TILE_HOVER;
        DrawRectangleRounded(tileRect, 0.12f, 8, tileBg);

        // Selection color - blue for folders, orange for plugins
        static const Color APPLE_ORANGE = {255, 159, 10, 255};
        Color selectionColor = isFolder ? APPLE_BLUE : APPLE_ORANGE;

        // Border - colored when selected
        Color borderColor = isSelected ? selectionColor : GRID_BORDER;
        DrawRectangleRoundedLines(tileRect, 0.12f, 8, borderColor);

        // Selection indicator - left edge colored bar
        if (isSelected) {
            Rectangle accentBar = {tileX, tileY + 10, 4, GRID_TILE_HEIGHT - 20};
            DrawRectangleRounded(accentBar, 1.0f, 4, selectionColor);
        }

        // Icon circle on left side of tile
        // Fades out when selected
        float iconRadius = 50;
        float iconX = tileX + 70;
        float iconY = tileY + GRID_TILE_HEIGHT / 2;

        // Use blue for folders, cycle traffic light colors for plugins
        Color iconColors[] = {APPLE_RED, APPLE_YELLOW, APPLE_GREEN};
        Color iconColor = isFolder ? APPLE_BLUE : iconColors[i % 3];

        // Fade out icon when selected
        float iconAlpha = isSelected ? 0.0f : 1.0f;
        if (iconAlpha > 0.0f) {
            Color iconBg = ColorAlpha(iconColor, 0.08f * iconAlpha);
            DrawCircle((int)iconX, (int)iconY, iconRadius, iconBg);
            DrawCircleLines((int)iconX, (int)iconY, iconRadius, ColorAlpha(iconColor, 0.4f * iconAlpha));

            // Initial letter (F for folders)
            const char *iconChar = isFolder ? "F" : NULL;
            char initial[2] = {0};
            if (!iconChar && itemName && itemName[0]) {
                initial[0] = itemName[0];
                initial[1] = '\0';
                iconChar = initial;
            }
            if (iconChar) {
                float initialSize = 40;
                Vector2 initialDim = MeasureTextEx(gridFont, iconChar, initialSize, 1);
                Color initialColor = ColorAlpha(iconColor, 0.7f * iconAlpha);
                DrawTextEx(gridFont, iconChar,
                          (Vector2){iconX - initialDim.x / 2, iconY - initialDim.y / 2},
                          initialSize, 1, initialColor);
            }
        }

        // Item name - bigger when selected, positioned differently
        float textX = isSelected ? (tileX + 30) : (iconX + iconRadius + 30);
        float maxTextWidth = isSelected ? (GRID_TILE_WIDTH - 60) : (GRID_TILE_WIDTH - (textX - tileX) - 20);

        Color nameColor = isSelected ? GRID_TEXT_PRIMARY : GRID_TEXT_SECONDARY;
        float nameSize = isSelected ? 36 : 28;
        Vector2 nameDim = itemName ? MeasureTextEx(gridFont, itemName, nameSize, 1) : (Vector2){0, 0};

        // Vertically center the name in the tile
        float nameY = tileY + (GRID_TILE_HEIGHT - nameDim.y) / 2;

        // Truncate if too long
        if (itemName && nameDim.x > maxTextWidth) {
            char truncName[32];
            int maxChars = (int)(maxTextWidth / (nameDim.x / strlen(itemName)));
            if (maxChars > 28) maxChars = 28;
            if (maxChars > 3) {
                strncpy(truncName, itemName, maxChars - 3);
                truncName[maxChars - 3] = '.';
                truncName[maxChars - 2] = '.';
                truncName[maxChars - 1] = '.';
                truncName[maxChars] = '\0';
                DrawTextEx(gridFont, truncName, (Vector2){textX, nameY}, nameSize, 1, nameColor);
            }
        } else if (itemName) {
            DrawTextEx(gridFont, itemName, (Vector2){textX, nameY}, nameSize, 1, nameColor);
        }

        // Index badge in corner - subtle
        char indexStr[16];
        snprintf(indexStr, sizeof(indexStr), "%d", i + 1);
        Vector2 indexSize = MeasureTextEx(gridFont, indexStr, 14, 1);
        float indexXPos = tileX + GRID_TILE_WIDTH - indexSize.x - 12;
        float indexYPos = tileY + GRID_TILE_HEIGHT - 24;
        DrawTextEx(gridFont, indexStr, (Vector2){indexXPos, indexYPos}, 14, 1, GRID_TEXT_DIM);
    }

    EndScissorMode();

    // Page indicator at bottom
    char pageStr[32];
    snprintf(pageStr, sizeof(pageStr), "%d of %d", selected + 1, itemCount);
    Vector2 pageSize = MeasureTextEx(gridFont, pageStr, 16, 1);
    DrawTextEx(gridFont, pageStr, (Vector2){(SCREEN_WIDTH - pageSize.x) / 2, SCREEN_HEIGHT - 30}, 16, 1, GRID_TEXT_SECONDARY);
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

    // Load saved menu style from config
    g_menuStyle = (MenuScrollStyle)LlzConfigGetMenuStyle();
    printf("Loaded menu style: %s\n", g_styleNames[g_menuStyle]);

    if (!LlzDisplayInit()) {
        fprintf(stderr, "Failed to initialize display. Check DRM permissions and cabling.\n");
        return 1;
    }
    LlzInputInit();
    LoadMenuFont();

    // Initialize SDK media system for Redis access (needed by auto-blur background)
    LlzMediaInit(NULL);

    // Initialize SDK background system for animated menu backgrounds
    LlzBackgroundInit(SCREEN_WIDTH, SCREEN_HEIGHT);
    LlzBackgroundSetColors(COLOR_ACCENT, COLOR_ACCENT_DIM);
    LlzBackgroundSetEnabled(true);  // Enable background by default for main menu

    // Load saved background style from config
    LlzConfigBackgroundStyle savedBgStyle = LlzConfigGetBackgroundStyle();
    LlzBackgroundSetStyle((LlzBackgroundStyle)savedBgStyle, false);
    printf("Loaded background style: %d\n", savedBgStyle);

    char pluginDir[512];
    const char *working = GetWorkingDirectory();
    snprintf(pluginDir, sizeof(pluginDir), "%s/plugins", working ? working : ".");

    // Use global registry so helper functions can access it
    memset(&g_registry, 0, sizeof(g_registry));
    LoadPlugins(pluginDir, &g_registry);

    // Load visibility configuration and build menu items
    LoadPluginVisibility(&g_registry);
    BuildMenuItems(&g_registry, &g_menuItems);
    printf("Menu built: %d items (%d folders + home plugins)\n", g_menuItems.count,
           g_menuItems.count > 0 ? (int)(g_menuItems.count -
               (g_registry.count > 0 ? g_registry.count : 0)) : 0);

    // Create initial snapshot for change detection
    g_pluginSnapshot = CreatePluginSnapshot(pluginDir);

    int selectedIndex = 0;
    if (selectedIndex >= g_registry.count) selectedIndex = g_registry.count - 1;
    bool runningPlugin = false;
    LoadedPlugin *active = NULL;
    int lastPluginIndex = -1;  // Track last opened plugin for quick reopen

    // Check for startup plugin configuration
    if (LlzConfigHasStartupPlugin() && g_registry.count > 0) {
        const char *startupName = LlzConfigGetStartupPlugin();
        int startupIndex = -1;

        // Find the plugin by name (check displayName, api->name, and filename)
        for (int i = 0; i < g_registry.count; i++) {
            // Check exact match on displayName or api->name
            if (strcmp(g_registry.items[i].displayName, startupName) == 0 ||
                (g_registry.items[i].api && g_registry.items[i].api->name &&
                 strcmp(g_registry.items[i].api->name, startupName) == 0)) {
                startupIndex = i;
                break;
            }

            // Also check case-insensitive match (settings plugin may capitalize differently)
            if (strcasecmp(g_registry.items[i].displayName, startupName) == 0 ||
                (g_registry.items[i].api && g_registry.items[i].api->name &&
                 strcasecmp(g_registry.items[i].api->name, startupName) == 0)) {
                startupIndex = i;
                break;
            }

            // Check against filename (e.g., "nowplaying.so" -> "nowplaying")
            // Settings plugin generates names from filenames, so this helps match
            const char *path = g_registry.items[i].path;
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
            active = &g_registry.items[startupIndex];
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
                    // Refresh plugins
                    int changes = RefreshPlugins(pluginDir, &g_registry);
                    if (changes > 0) {
                        printf("Plugins refreshed: %d change(s)\n", changes);

                        // Update snapshot
                        FreePluginSnapshot(&g_pluginSnapshot);
                        g_pluginSnapshot = CreatePluginSnapshot(pluginDir);

                        // Reload visibility config and rebuild menu items
                        LoadPluginVisibility(&g_registry);
                        FreeMenuItems(&g_menuItems);
                        BuildMenuItems(&g_registry, &g_menuItems);

                        // Exit folder view if we were inside (plugins may have changed)
                        if (g_insideFolder) {
                            FreeFolderPlugins(g_folderPlugins);
                            g_folderPlugins = NULL;
                            g_folderPluginCount = 0;
                            g_insideFolder = false;
                        }

                        // Reset selection
                        selectedIndex = 0;
                        g_scrollOffset = 0;
                        g_targetScrollOffset = 0;

                        // Reset last plugin index if that plugin was removed
                        if (lastPluginIndex >= g_registry.count) {
                            lastPluginIndex = -1;
                        }
                    }
                }
            }

            // Determine current item count based on view
            int currentItemCount = g_insideFolder ? g_folderPluginCount : g_menuItems.count;

            bool downKey = IsKeyPressed(KEY_DOWN) || inputState.downPressed || (inputState.scrollDelta > 0.0f);
            bool upKey = IsKeyPressed(KEY_UP) || inputState.upPressed || (inputState.scrollDelta < 0.0f);

            if (downKey && currentItemCount > 0) {
                selectedIndex = (selectedIndex + 1) % currentItemCount;
            }
            if (upKey && currentItemCount > 0) {
                selectedIndex = (selectedIndex - 1 + currentItemCount) % currentItemCount;
            }

            // Cycle background style with screenshot button (or button4)
            if (inputState.screenshotPressed || inputState.button4Pressed) {
                LlzBackgroundCycleNext();
                // Save to config for persistence across reboots
                LlzConfigSetBackgroundStyle((LlzConfigBackgroundStyle)LlzBackgroundGetStyle());
            }

            // Cycle menu navigation style with button3 release (display mode button)
            if (inputState.button3Pressed) {
                CycleMenuStyle();
            }

            // Back button handling
            if (inputState.backReleased) {
                if (g_insideFolder) {
                    // Exit folder, return to main menu
                    FreeFolderPlugins(g_folderPlugins);
                    g_folderPlugins = NULL;
                    g_folderPluginCount = 0;
                    g_insideFolder = false;
                    selectedIndex = 0;  // Reset to top of menu
                    g_scrollOffset = 0;
                    g_targetScrollOffset = 0;
                } else if (lastPluginIndex >= 0 && lastPluginIndex < g_registry.count) {
                    // Reopen last plugin
                    active = &g_registry.items[lastPluginIndex];
                    if (active && active->api && active->api->init) {
                        active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
                    }
                    runningPlugin = true;
                    continue;
                }
            }

            bool selectPressed = IsKeyPressed(KEY_ENTER) || inputState.selectPressed;
            if (selectPressed && currentItemCount > 0) {
                if (g_insideFolder) {
                    // Launch plugin from folder
                    int pluginIdx = g_folderPlugins[selectedIndex];
                    lastPluginIndex = pluginIdx;
                    active = &g_registry.items[pluginIdx];
                    if (active && active->api && active->api->init) {
                        active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
                    }
                    runningPlugin = true;
                    continue;
                } else {
                    // Main menu: check if folder or plugin
                    MenuItem *item = &g_menuItems.items[selectedIndex];
                    if (item->type == MENU_ITEM_FOLDER) {
                        // Enter folder
                        g_currentFolder = item->folder.category;
                        g_folderPlugins = GetFolderPlugins(&g_registry, g_currentFolder, &g_folderPluginCount);
                        g_insideFolder = true;
                        selectedIndex = 0;
                        g_scrollOffset = 0;
                        g_targetScrollOffset = 0;
                    } else {
                        // Launch plugin
                        int pluginIdx = item->plugin.pluginIndex;
                        lastPluginIndex = pluginIdx;
                        active = &g_registry.items[pluginIdx];
                        if (active && active->api && active->api->init) {
                            active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
                        }
                        runningPlugin = true;
                        continue;
                    }
                }
            }

            LlzDisplayBegin();
            DrawPluginMenu(&g_registry, selectedIndex, delta);
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
                // Store API pointer before shutdown (plugin may clear its state)
                const LlzPluginAPI *closingApi = active->api;

                if (closingApi->shutdown) closingApi->shutdown();

                // Check wants_refresh AFTER shutdown so saved config is used
                bool needsRefresh = closingApi->wants_refresh && closingApi->wants_refresh();

                // Refresh menu if plugin requested it (e.g., plugin manager, menu sorter)
                if (needsRefresh) {
                    LoadPluginVisibility(&g_registry);
                    FreeMenuItems(&g_menuItems);
                    BuildMenuItems(&g_registry, &g_menuItems);

                    // Exit folder view since menu structure may have changed
                    if (g_insideFolder) {
                        g_insideFolder = false;
                        FreeFolderPlugins(g_folderPlugins);
                        g_folderPlugins = NULL;
                        g_folderPluginCount = 0;
                    }

                    // Reset selection to top of list after refresh
                    selectedIndex = 0;
                }

                // Check if the plugin requested opening another plugin
                if (LlzHasRequestedPlugin()) {
                    const char *requestedName = LlzGetRequestedPlugin();
                    if (requestedName) {
                        // Find the requested plugin by name
                        int foundIndex = -1;
                        for (int i = 0; i < g_registry.count; i++) {
                            if (strcmp(g_registry.items[i].displayName, requestedName) == 0 ||
                                (g_registry.items[i].api && g_registry.items[i].api->name &&
                                 strcmp(g_registry.items[i].api->name, requestedName) == 0)) {
                                foundIndex = i;
                                break;
                            }
                        }

                        LlzClearRequestedPlugin();

                        if (foundIndex >= 0) {
                            // Open the requested plugin directly
                            selectedIndex = foundIndex;
                            lastPluginIndex = foundIndex;
                            active = &g_registry.items[foundIndex];
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

                // Clear manual blur override so auto-blur from Redis takes over
                LlzBackgroundClearManualBlur();
            }
        }
    }

    if (active && active->api && active->api->shutdown) {
        active->api->shutdown();
    }
    FreeFolderPlugins(g_folderPlugins);
    FreeMenuItems(&g_menuItems);
    FreePluginSnapshot(&g_pluginSnapshot);
    UnloadPlugins(&g_registry);
    UnloadIBrandFont();
    UnloadTracklisterFont();
    UnloadOmicronFont();
    UnloadMenuFont();
    LlzBackgroundShutdown();
    LlzMediaShutdown();
    LlzInputShutdown();
    LlzDisplayShutdown();
    LlzConfigShutdown();
    return 0;
}
