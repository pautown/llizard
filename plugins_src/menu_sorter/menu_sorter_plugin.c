/**
 * Menu Sorter - Reorder plugins and folders in the main menu
 *
 * Allows users to customize the order of items in the main menu:
 * - Move folders and home-pinned plugins up/down
 * - Order is saved and persists across restarts
 *
 * Controls:
 *   UP/DOWN or SCROLL  - Navigate through items
 *   SWIPE LEFT/RIGHT   - Move selected item up/down in the list
 *   SELECT (hold)      - Move item to top
 *   BACK               - Save and exit
 */

#include "llz_sdk.h"
#include "llizard_plugin.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <dlfcn.h>
#include <math.h>

// ============================================================================
// Constants
// ============================================================================

#define MS_MAX_ITEMS 64
#define MS_NAME_MAX 128

#define MS_SCREEN_WIDTH 800
#define MS_SCREEN_HEIGHT 480

// Item types
typedef enum {
    MS_ITEM_FOLDER = 0,
    MS_ITEM_PLUGIN
} MSItemType;

// Modern color palette
static const Color MS_COLOR_BG = {18, 18, 24, 255};
static const Color MS_COLOR_CARD = {28, 30, 38, 255};
static const Color MS_COLOR_CARD_SELECTED = {38, 42, 55, 255};
static const Color MS_COLOR_ACCENT = {100, 180, 255, 255};
static const Color MS_COLOR_FOLDER = {255, 180, 100, 255};
static const Color MS_COLOR_PLUGIN = {100, 200, 140, 255};
static const Color MS_COLOR_TEXT = {240, 240, 245, 255};
static const Color MS_COLOR_TEXT_DIM = {130, 135, 150, 255};
static const Color MS_COLOR_HEADER = {24, 26, 34, 255};
static const Color MS_COLOR_MOVE_UP = {100, 200, 140, 255};
static const Color MS_COLOR_MOVE_DOWN = {255, 140, 100, 255};

// ============================================================================
// Menu Item
// ============================================================================

typedef struct {
    MSItemType type;
    char name[MS_NAME_MAX];        // Display name
    char key[MS_NAME_MAX];         // Config key (category name or filename)
    int sortIndex;
    LlzPluginCategory category;    // For folders
} MSMenuItem;

// ============================================================================
// State
// ============================================================================

static MSMenuItem g_items[MS_MAX_ITEMS];
static int g_itemCount = 0;
static int g_selectedIndex = 0;
static float g_scrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;
static bool g_wantsClose = false;
static bool g_configChanged = false;
static Font g_font;

static int g_screenWidth = MS_SCREEN_WIDTH;
static int g_screenHeight = MS_SCREEN_HEIGHT;

// Animation state
static float g_animTime = 0.0f;
static float g_moveFlash = 0.0f;
static int g_lastMoveDirection = 0;  // -1 up, 1 down, 0 none

// ============================================================================
// Configuration File Handling
// ============================================================================

static const char *GetSortConfigPath(void) {
#ifdef PLATFORM_DRM
    return "/var/llizard/menu_sort_order.ini";
#else
    return "./menu_sort_order.ini";
#endif
}

static const char *GetVisibilityConfigPath(void) {
#ifdef PLATFORM_DRM
    return "/var/llizard/plugin_visibility.ini";
#else
    return "./plugin_visibility.ini";
#endif
}

static const char *GetPluginsDir(void) {
#ifdef PLATFORM_DRM
    return "/usr/lib/llizard/plugins";
#else
    return "./plugins";
#endif
}

static void LoadSortConfig(void) {
    FILE *f = fopen(GetSortConfigPath(), "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        char *nl = strchr(value, '\n');
        if (nl) *nl = '\0';

        int sortIndex = atoi(value);

        // Find item and set sort index
        for (int i = 0; i < g_itemCount; i++) {
            if (strcmp(g_items[i].key, key) == 0) {
                g_items[i].sortIndex = sortIndex;
                break;
            }
        }
    }

    fclose(f);
}

static void SaveSortConfig(void) {
    FILE *f = fopen(GetSortConfigPath(), "w");
    if (!f) {
        printf("[MenuSorter] Failed to save config to %s\n", GetSortConfigPath());
        return;
    }

    fprintf(f, "# Menu sort order configuration\n");
    fprintf(f, "# Format: key=index (lower index = higher in list)\n");
    fprintf(f, "# Folders use category names, plugins use filenames\n\n");

    for (int i = 0; i < g_itemCount; i++) {
        fprintf(f, "%s=%d\n", g_items[i].key, g_items[i].sortIndex);
    }

    fclose(f);
    printf("[MenuSorter] Saved config to %s\n", GetSortConfigPath());
}

// ============================================================================
// Item Discovery
// ============================================================================

static int CompareByIndex(const void *a, const void *b) {
    const MSMenuItem *ia = (const MSMenuItem *)a;
    const MSMenuItem *ib = (const MSMenuItem *)b;
    return ia->sortIndex - ib->sortIndex;
}

static void DiscoverItems(void) {
    g_itemCount = 0;

    // Load visibility config to know which plugins are HOME vs FOLDER
    // We need to show: folders with FOLDER-visibility plugins, and HOME plugins

    // First, track which categories have folder-visibility plugins
    bool categoryHasPlugins[LLZ_CATEGORY_COUNT] = {false};

    // Read visibility config
    FILE *visFile = fopen(GetVisibilityConfigPath(), "r");

    // Also scan plugins directory
    DIR *dir = opendir(GetPluginsDir());
    if (!dir) {
        printf("[MenuSorter] Failed to open plugins directory: %s\n", GetPluginsDir());
        return;
    }

    // Temporary storage for plugin info
    typedef struct {
        char filename[MS_NAME_MAX];
        char name[MS_NAME_MAX];
        LlzPluginCategory category;
        int visibility;  // 0=home, 1=folder, 2=hidden
    } TempPlugin;
    TempPlugin plugins[MS_MAX_ITEMS];
    int pluginCount = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && pluginCount < MS_MAX_ITEMS) {
        if (entry->d_name[0] == '.') continue;
        const char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".so") != 0) continue;

        // Skip menu sorter itself (can't sort itself)
        if (strstr(entry->d_name, "menu_sorter") != NULL) continue;

        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", GetPluginsDir(), entry->d_name);

        void *handle = dlopen(fullPath, RTLD_NOW);
        if (!handle) continue;

        typedef const LlzPluginAPI *(*GetPluginFunc)(void);
        GetPluginFunc getter = (GetPluginFunc)dlsym(handle, "LlzGetPlugin");
        if (!getter) {
            dlclose(handle);
            continue;
        }

        const LlzPluginAPI *api = getter();
        if (!api || !api->name) {
            dlclose(handle);
            continue;
        }

        TempPlugin *p = &plugins[pluginCount];
        strncpy(p->filename, entry->d_name, MS_NAME_MAX - 1);
        strncpy(p->name, api->name, MS_NAME_MAX - 1);
        p->category = api->category;
        p->visibility = 1;  // Default to folder

        pluginCount++;
        dlclose(handle);
    }
    closedir(dir);

    // Read visibility config to update plugin visibility
    if (visFile) {
        char line[512];
        while (fgets(line, sizeof(line), visFile)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;

            char *eq = strchr(line, '=');
            if (!eq) continue;

            *eq = '\0';
            char *filename = line;
            char *value = eq + 1;

            char *nl = strchr(value, '\n');
            if (nl) *nl = '\0';

            // Parse visibility:category
            char *colon = strchr(value, ':');
            int category = -1;
            if (colon) {
                *colon = '\0';
                category = atoi(colon + 1);
            }

            for (int i = 0; i < pluginCount; i++) {
                if (strcmp(plugins[i].filename, filename) == 0) {
                    if (strcmp(value, "home") == 0) {
                        plugins[i].visibility = 0;
                    } else if (strcmp(value, "folder") == 0) {
                        plugins[i].visibility = 1;
                    } else if (strcmp(value, "hidden") == 0) {
                        plugins[i].visibility = 2;
                    }
                    if (category >= 0 && category < LLZ_CATEGORY_COUNT) {
                        plugins[i].category = (LlzPluginCategory)category;
                    }
                    break;
                }
            }
        }
        fclose(visFile);
    }

    // Count which categories have folder-visibility plugins
    for (int i = 0; i < pluginCount; i++) {
        if (plugins[i].visibility == 1) {  // folder
            if (plugins[i].category < LLZ_CATEGORY_COUNT) {
                categoryHasPlugins[plugins[i].category] = true;
            }
        }
    }

    // Add folders that have plugins
    int defaultIndex = 0;
    for (int c = 0; c < LLZ_CATEGORY_COUNT; c++) {
        if (categoryHasPlugins[c]) {
            MSMenuItem *item = &g_items[g_itemCount];
            item->type = MS_ITEM_FOLDER;
            strncpy(item->name, LLZ_CATEGORY_NAMES[c], MS_NAME_MAX - 1);
            snprintf(item->key, MS_NAME_MAX, "folder:%s", LLZ_CATEGORY_NAMES[c]);
            item->sortIndex = defaultIndex++;
            item->category = (LlzPluginCategory)c;
            g_itemCount++;
        }
    }

    // Add HOME plugins
    for (int i = 0; i < pluginCount; i++) {
        if (plugins[i].visibility == 0) {  // home
            MSMenuItem *item = &g_items[g_itemCount];
            item->type = MS_ITEM_PLUGIN;
            strncpy(item->name, plugins[i].name, MS_NAME_MAX - 1);
            snprintf(item->key, MS_NAME_MAX, "plugin:%s", plugins[i].filename);
            item->sortIndex = defaultIndex++;
            item->category = plugins[i].category;
            g_itemCount++;
        }
    }

    printf("[MenuSorter] Discovered %d items\n", g_itemCount);

    // Load saved sort order
    LoadSortConfig();

    // Sort by index
    if (g_itemCount > 1) {
        qsort(g_items, g_itemCount, sizeof(MSMenuItem), CompareByIndex);
    }

    // Normalize indices (0, 1, 2, ...)
    for (int i = 0; i < g_itemCount; i++) {
        g_items[i].sortIndex = i;
    }
}

// ============================================================================
// Drawing
// ============================================================================

static void DrawHeader(void) {
    DrawRectangleGradientV(0, 0, g_screenWidth, 70, MS_COLOR_HEADER, MS_COLOR_BG);

    const char *title = "Menu Sorter";
    float titleSize = 32;
    Vector2 titleDim = MeasureTextEx(g_font, title, titleSize, 2);
    DrawTextEx(g_font, title, (Vector2){(g_screenWidth - titleDim.x) / 2, 12}, titleSize, 2, MS_COLOR_TEXT);

    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "Arrange %d items", g_itemCount);
    float subSize = 16;
    Vector2 subDim = MeasureTextEx(g_font, subtitle, subSize, 1);
    DrawTextEx(g_font, subtitle, (Vector2){(g_screenWidth - subDim.x) / 2, 46}, subSize, 1, MS_COLOR_TEXT_DIM);

    float pulse = 0.7f + 0.3f * sinf(g_animTime * 2.0f);
    DrawRectangle(g_screenWidth / 2 - 50, 68, 100, 2, ColorAlpha(MS_COLOR_ACCENT, pulse));
}

static void DrawItemCard(MSMenuItem *item, int index, float y) {
    bool selected = (index == g_selectedIndex);
    float cardX = 20;
    float cardWidth = g_screenWidth - 40;
    float cardHeight = 60;

    // Flash effect when moved
    Color cardBg = MS_COLOR_CARD;
    if (selected) {
        cardBg = MS_COLOR_CARD_SELECTED;
        if (g_moveFlash > 0) {
            Color flashColor = (g_lastMoveDirection < 0) ? MS_COLOR_MOVE_UP : MS_COLOR_MOVE_DOWN;
            cardBg = ColorAlpha(flashColor, 0.3f * g_moveFlash);
        }
    }

    Rectangle cardRect = {cardX, y, cardWidth, cardHeight};
    DrawRectangleRounded(cardRect, 0.15f, 8, cardBg);

    // Selection indicator
    if (selected) {
        Color accentColor = (item->type == MS_ITEM_FOLDER) ? MS_COLOR_FOLDER : MS_COLOR_PLUGIN;
        DrawRectangleRounded((Rectangle){cardX, y + 8, 4, cardHeight - 16}, 0.5f, 4, accentColor);
        DrawRectangleRoundedLines(cardRect, 0.15f, 8, ColorAlpha(accentColor, 0.4f));
    }

    // Index number
    char indexStr[8];
    snprintf(indexStr, sizeof(indexStr), "%d", index + 1);
    float indexSize = 18;
    Color indexColor = selected ? MS_COLOR_TEXT : MS_COLOR_TEXT_DIM;
    DrawTextEx(g_font, indexStr, (Vector2){cardX + 16, y + (cardHeight - indexSize) / 2}, indexSize, 1, indexColor);

    // Type icon
    float iconX = cardX + 50;
    float iconY = y + cardHeight / 2;
    Color typeColor = (item->type == MS_ITEM_FOLDER) ? MS_COLOR_FOLDER : MS_COLOR_PLUGIN;
    DrawCircle((int)iconX, (int)iconY, 16, ColorAlpha(typeColor, 0.2f));

    const char *typeIcon = (item->type == MS_ITEM_FOLDER) ? "F" : "P";
    float iconSize = 16;
    Vector2 iconDim = MeasureTextEx(g_font, typeIcon, iconSize, 1);
    DrawTextEx(g_font, typeIcon, (Vector2){iconX - iconDim.x / 2, iconY - iconDim.y / 2}, iconSize, 1, typeColor);

    // Name
    float textX = iconX + 28;
    Color nameColor = selected ? MS_COLOR_TEXT : ColorAlpha(MS_COLOR_TEXT, 0.85f);
    DrawTextEx(g_font, item->name, (Vector2){textX, y + 12}, 22, 1, nameColor);

    // Type label
    const char *typeLabel = (item->type == MS_ITEM_FOLDER) ? "Folder" : "Home Plugin";
    DrawTextEx(g_font, typeLabel, (Vector2){textX, y + 38}, 14, 1, ColorAlpha(typeColor, 0.7f));

    // Move arrows on right when selected
    if (selected) {
        float arrowX = cardX + cardWidth - 60;
        float arrowY = y + cardHeight / 2;

        // Up arrow (if not at top)
        if (index > 0) {
            Color upColor = ColorAlpha(MS_COLOR_MOVE_UP, 0.8f);
            DrawTextEx(g_font, "^", (Vector2){arrowX, arrowY - 16}, 20, 1, upColor);
        }

        // Down arrow (if not at bottom)
        if (index < g_itemCount - 1) {
            Color downColor = ColorAlpha(MS_COLOR_MOVE_DOWN, 0.8f);
            DrawTextEx(g_font, "v", (Vector2){arrowX, arrowY + 4}, 20, 1, downColor);
        }
    }
}

static void DrawItemList(void) {
    float startY = 80;
    float itemHeight = 68;
    float visibleHeight = g_screenHeight - startY - 50;

    g_scrollOffset += (g_targetScrollOffset - g_scrollOffset) * 0.15f;

    BeginScissorMode(0, (int)startY, g_screenWidth, (int)visibleHeight);

    for (int i = 0; i < g_itemCount; i++) {
        float itemY = startY + i * itemHeight - g_scrollOffset;
        if (itemY < startY - itemHeight || itemY > g_screenHeight) continue;
        DrawItemCard(&g_items[i], i, itemY);
    }

    EndScissorMode();

    // Scroll indicators
    if (g_scrollOffset > 5) {
        for (int i = 0; i < 20; i++) {
            float alpha = (20 - i) / 20.0f * 0.8f;
            DrawRectangle(0, (int)startY + i, g_screenWidth, 1, ColorAlpha(MS_COLOR_BG, alpha));
        }
    }

    float maxScroll = g_itemCount * itemHeight - visibleHeight;
    if (maxScroll > 0 && g_scrollOffset < maxScroll - 5) {
        int bottomY = (int)(startY + visibleHeight);
        for (int i = 0; i < 20; i++) {
            float alpha = i / 20.0f * 0.8f;
            DrawRectangle(0, bottomY - 20 + i, g_screenWidth, 1, ColorAlpha(MS_COLOR_BG, alpha));
        }
    }
}

static void DrawFooter(void) {
    float footerY = g_screenHeight - 44;
    DrawRectangleGradientV(0, (int)footerY, g_screenWidth, 44, ColorAlpha(MS_COLOR_BG, 0), MS_COLOR_HEADER);

    const char *hint = "UP/DOWN: Select | SWIPE: Move | BACK: Save";
    float hintSize = 14;
    Vector2 hintDim = MeasureTextEx(g_font, hint, hintSize, 1);
    DrawTextEx(g_font, hint, (Vector2){(g_screenWidth - hintDim.x) / 2, footerY + 16}, hintSize, 1, MS_COLOR_TEXT_DIM);

    if (g_configChanged) {
        DrawCircle(30, (int)footerY + 22, 6, MS_COLOR_ACCENT);
    }
}

// ============================================================================
// Input Handling
// ============================================================================

static void EnsureSelectedVisible(void) {
    float startY = 80;
    float itemHeight = 68;
    float visibleHeight = g_screenHeight - startY - 50;

    float selectedY = g_selectedIndex * itemHeight;
    float maxScroll = g_itemCount * itemHeight - visibleHeight;
    if (maxScroll < 0) maxScroll = 0;

    if (selectedY < g_targetScrollOffset) {
        g_targetScrollOffset = selectedY;
    } else if (selectedY > g_targetScrollOffset + visibleHeight - itemHeight) {
        g_targetScrollOffset = selectedY - visibleHeight + itemHeight;
    }

    if (g_targetScrollOffset < 0) g_targetScrollOffset = 0;
    if (g_targetScrollOffset > maxScroll) g_targetScrollOffset = maxScroll;
}

static void MoveItem(int direction) {
    if (g_itemCount < 2) return;

    int newIndex = g_selectedIndex + direction;
    if (newIndex < 0 || newIndex >= g_itemCount) return;

    // Swap items
    MSMenuItem temp = g_items[g_selectedIndex];
    g_items[g_selectedIndex] = g_items[newIndex];
    g_items[newIndex] = temp;

    // Update sort indices
    g_items[g_selectedIndex].sortIndex = g_selectedIndex;
    g_items[newIndex].sortIndex = newIndex;

    g_selectedIndex = newIndex;
    g_configChanged = true;
    g_moveFlash = 1.0f;
    g_lastMoveDirection = direction;

    EnsureSelectedVisible();
}

static void HandleInput(const LlzInputState *input) {
    // Navigation
    if (input->upPressed || input->scrollDelta < 0) {
        if (g_selectedIndex > 0) {
            g_selectedIndex--;
            EnsureSelectedVisible();
        }
    }
    if (input->downPressed || input->scrollDelta > 0) {
        if (g_selectedIndex < g_itemCount - 1) {
            g_selectedIndex++;
            EnsureSelectedVisible();
        }
    }

    // Move item with swipe
    if (input->swipeLeft || input->swipeUp) {
        MoveItem(-1);  // Move up in list
    }
    if (input->swipeRight || input->swipeDown) {
        MoveItem(1);   // Move down in list
    }

    // Move to top/bottom with select hold
    if (input->selectHold) {
        // Move to top
        while (g_selectedIndex > 0) {
            MoveItem(-1);
        }
    }

    // Exit on back (use backReleased for proper release detection)
    if (input->backReleased) {
        if (g_configChanged) {
            SaveSortConfig();
        }
        g_wantsClose = true;
    }
}

// ============================================================================
// Plugin API
// ============================================================================

static void PluginInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;
    g_configChanged = false;
    g_selectedIndex = 0;
    g_scrollOffset = 0.0f;
    g_targetScrollOffset = 0.0f;
    g_itemCount = 0;
    g_animTime = 0.0f;
    g_moveFlash = 0.0f;
    g_lastMoveDirection = 0;

    g_font = LlzFontGetDefault();

    DiscoverItems();

    printf("[MenuSorter] Initialized with %d items\n", g_itemCount);
}

static void PluginUpdate(const LlzInputState *input, float deltaTime) {
    g_animTime += deltaTime;

    // Fade move flash
    if (g_moveFlash > 0) {
        g_moveFlash -= deltaTime * 3.0f;
        if (g_moveFlash < 0) g_moveFlash = 0;
    }

    HandleInput(input);
}

static void PluginDraw(void) {
    ClearBackground(MS_COLOR_BG);

    DrawItemList();
    DrawHeader();
    DrawFooter();
}

static void PluginShutdown(void) {
    if (g_configChanged) {
        SaveSortConfig();
    }
    printf("[MenuSorter] Shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

static bool PluginWantsRefresh(void) {
    // Request menu refresh if sort order was changed
    return g_configChanged;
}

// ============================================================================
// Plugin Export
// ============================================================================

static LlzPluginAPI g_api = {
    .name = "Menu Sorter",
    .description = "Reorder plugins and folders in the main menu",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .wants_refresh = PluginWantsRefresh,
    .handles_back_button = true,
    .category = LLZ_CATEGORY_UTILITIES
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
