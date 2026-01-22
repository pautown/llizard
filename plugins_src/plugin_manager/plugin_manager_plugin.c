/**
 * Plugin Manager - Configure plugin visibility in the main menu
 *
 * Allows users to configure how plugins appear in the main menu:
 * - HOME: Plugin appears directly on the home screen
 * - FOLDER: Plugin appears in its category folder (Media, Games, etc.)
 * - HIDDEN: Plugin is not shown in the menu at all
 *
 * Configuration is stored in plugin_visibility.ini and read by the main host.
 *
 * Controls:
 *   UP/DOWN or SCROLL - Navigate through plugins
 *   SELECT or TAP     - Cycle visibility mode (Home → Folder → Hidden)
 *   BACK              - Exit and save changes
 */

#include "llz_sdk.h"
#include "llizard_plugin.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <dlfcn.h>

// ============================================================================
// Constants
// ============================================================================

#define PM_MAX_PLUGINS 64
#define PM_NAME_MAX 64
#define PM_PATH_MAX 256

#define PM_SCREEN_WIDTH 800
#define PM_SCREEN_HEIGHT 480

// Visibility modes
typedef enum {
    PM_VIS_HOME = 0,    // Show on home screen
    PM_VIS_FOLDER,      // Show in category folder
    PM_VIS_HIDDEN,      // Don't show at all
    PM_VIS_COUNT
} PMVisibility;

static const char *PM_VIS_NAMES[] = {"Home", "Folder", "Hidden"};
static const Color PM_VIS_COLORS[] = {
    {100, 220, 100, 255},  // Home - green
    {100, 180, 255, 255},  // Folder - blue
    {180, 100, 100, 255}   // Hidden - red
};

// ============================================================================
// Plugin Entry
// ============================================================================

typedef struct {
    char name[PM_NAME_MAX];
    char filename[PM_NAME_MAX];
    LlzPluginCategory category;
    PMVisibility visibility;
    bool loaded;
} PMPluginEntry;

// ============================================================================
// State
// ============================================================================

static PMPluginEntry g_plugins[PM_MAX_PLUGINS];
static int g_pluginCount = 0;
static int g_selectedIndex = 0;
static float g_scrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;
static bool g_wantsClose = false;
static bool g_configChanged = false;
static Font g_font;

static int g_screenWidth = PM_SCREEN_WIDTH;
static int g_screenHeight = PM_SCREEN_HEIGHT;

// ============================================================================
// Configuration File Handling
// ============================================================================

static const char *GetConfigPath(void) {
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

static void LoadVisibilityConfig(void) {
    FILE *f = fopen(GetConfigPath(), "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;

        // Parse "filename=visibility"
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *filename = line;
        char *visStr = eq + 1;

        // Trim newline
        char *nl = strchr(visStr, '\n');
        if (nl) *nl = '\0';

        // Find plugin and set visibility
        for (int i = 0; i < g_pluginCount; i++) {
            if (strcmp(g_plugins[i].filename, filename) == 0) {
                if (strcmp(visStr, "home") == 0) {
                    g_plugins[i].visibility = PM_VIS_HOME;
                } else if (strcmp(visStr, "folder") == 0) {
                    g_plugins[i].visibility = PM_VIS_FOLDER;
                } else if (strcmp(visStr, "hidden") == 0) {
                    g_plugins[i].visibility = PM_VIS_HIDDEN;
                }
                break;
            }
        }
    }

    fclose(f);
}

static void SaveVisibilityConfig(void) {
    FILE *f = fopen(GetConfigPath(), "w");
    if (!f) {
        printf("[PluginManager] Failed to save config to %s\n", GetConfigPath());
        return;
    }

    fprintf(f, "# Plugin visibility configuration\n");
    fprintf(f, "# Values: home, folder, hidden\n\n");

    for (int i = 0; i < g_pluginCount; i++) {
        const char *visStr = "folder";
        switch (g_plugins[i].visibility) {
            case PM_VIS_HOME: visStr = "home"; break;
            case PM_VIS_FOLDER: visStr = "folder"; break;
            case PM_VIS_HIDDEN: visStr = "hidden"; break;
            default: break;
        }
        fprintf(f, "%s=%s\n", g_plugins[i].filename, visStr);
    }

    fclose(f);
    printf("[PluginManager] Saved config to %s\n", GetConfigPath());
}

// ============================================================================
// Plugin Discovery
// ============================================================================

static int ComparePluginsByCategory(const void *a, const void *b) {
    const PMPluginEntry *pa = (const PMPluginEntry *)a;
    const PMPluginEntry *pb = (const PMPluginEntry *)b;

    // First sort by category
    if (pa->category != pb->category) {
        return (int)pa->category - (int)pb->category;
    }
    // Then by name
    return strcasecmp(pa->name, pb->name);
}

static void DiscoverPlugins(void) {
    g_pluginCount = 0;

    DIR *dir = opendir(GetPluginsDir());
    if (!dir) {
        printf("[PluginManager] Failed to open plugins directory: %s\n", GetPluginsDir());
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && g_pluginCount < PM_MAX_PLUGINS) {
        // Skip hidden files and non-.so files
        if (entry->d_name[0] == '.') continue;
        const char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".so") != 0) continue;

        // Skip the plugin manager itself
        if (strstr(entry->d_name, "plugin_manager") != NULL) continue;

        char fullPath[PM_PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", GetPluginsDir(), entry->d_name);

        // Try to load plugin to get metadata
        void *handle = dlopen(fullPath, RTLD_NOW);
        if (!handle) {
            printf("[PluginManager] Failed to load %s: %s\n", entry->d_name, dlerror());
            continue;
        }

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

        PMPluginEntry *p = &g_plugins[g_pluginCount];
        strncpy(p->name, api->name, PM_NAME_MAX - 1);
        p->name[PM_NAME_MAX - 1] = '\0';
        strncpy(p->filename, entry->d_name, PM_NAME_MAX - 1);
        p->filename[PM_NAME_MAX - 1] = '\0';
        p->category = api->category;
        p->visibility = PM_VIS_FOLDER;  // Default to folder view
        p->loaded = true;

        g_pluginCount++;
        dlclose(handle);
    }

    closedir(dir);

    // Sort by category then name
    if (g_pluginCount > 1) {
        qsort(g_plugins, g_pluginCount, sizeof(PMPluginEntry), ComparePluginsByCategory);
    }

    printf("[PluginManager] Discovered %d plugins\n", g_pluginCount);

    // Load saved visibility settings
    LoadVisibilityConfig();
}

// ============================================================================
// Drawing
// ============================================================================

static void DrawHeader(void) {
    // Title bar
    DrawRectangle(0, 0, g_screenWidth, 60, (Color){30, 32, 44, 255});

    const char *title = "Plugin Manager";
    int titleSize = 28;
    int titleWidth = MeasureText(title, titleSize);
    DrawText(title, (g_screenWidth - titleWidth) / 2, 16, titleSize, WHITE);

    // Subtitle
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "%d plugins", g_pluginCount);
    int subSize = 16;
    int subWidth = MeasureText(subtitle, subSize);
    DrawText(subtitle, (g_screenWidth - subWidth) / 2, 42, subSize, (Color){160, 165, 180, 255});
}

static void DrawPluginList(void) {
    int startY = 70;
    int itemHeight = 56;
    int visibleItems = (g_screenHeight - startY - 50) / itemHeight;

    // Apply smooth scrolling
    g_scrollOffset += (g_targetScrollOffset - g_scrollOffset) * 0.2f;

    // Category header tracking
    LlzPluginCategory lastCategory = LLZ_CATEGORY_COUNT;

    int yOffset = startY - (int)g_scrollOffset;

    for (int i = 0; i < g_pluginCount; i++) {
        PMPluginEntry *p = &g_plugins[i];

        // Draw category header if changed
        if (p->category != lastCategory) {
            lastCategory = p->category;

            // Only draw if visible
            if (yOffset > startY - 30 && yOffset < g_screenHeight) {
                const char *catName = (p->category < LLZ_CATEGORY_COUNT) ?
                    LLZ_CATEGORY_NAMES[p->category] : "Unknown";

                DrawRectangle(0, yOffset, g_screenWidth, 28, (Color){25, 27, 35, 255});
                DrawText(catName, 20, yOffset + 6, 16, (Color){120, 180, 255, 255});
            }
            yOffset += 30;
        }

        // Skip if not visible
        if (yOffset < startY - itemHeight || yOffset > g_screenHeight) {
            yOffset += itemHeight;
            continue;
        }

        // Selection highlight
        bool selected = (i == g_selectedIndex);
        Color bgColor = selected ? (Color){50, 55, 70, 255} : (Color){35, 38, 50, 255};
        DrawRectangle(10, yOffset, g_screenWidth - 20, itemHeight - 4, bgColor);

        if (selected) {
            DrawRectangle(10, yOffset, 4, itemHeight - 4, (Color){100, 180, 255, 255});
        }

        // Plugin name
        DrawText(p->name, 24, yOffset + 8, 22, WHITE);

        // Filename (smaller)
        DrawText(p->filename, 24, yOffset + 32, 14, (Color){120, 125, 140, 255});

        // Visibility badge
        int badgeWidth = 70;
        int badgeX = g_screenWidth - badgeWidth - 30;
        int badgeY = yOffset + 14;

        Color badgeColor = PM_VIS_COLORS[p->visibility];
        DrawRectangleRounded((Rectangle){badgeX, badgeY, badgeWidth, 24}, 0.5f, 8, badgeColor);

        const char *visName = PM_VIS_NAMES[p->visibility];
        int visWidth = MeasureText(visName, 14);
        DrawText(visName, badgeX + (badgeWidth - visWidth) / 2, badgeY + 5, 14, WHITE);

        yOffset += itemHeight;
    }
}

static void DrawFooter(void) {
    int footerY = g_screenHeight - 40;
    DrawRectangle(0, footerY, g_screenWidth, 40, (Color){30, 32, 44, 255});

    const char *hint = "UP/DOWN: Navigate | SELECT: Change | BACK: Save & Exit";
    int hintWidth = MeasureText(hint, 14);
    DrawText(hint, (g_screenWidth - hintWidth) / 2, footerY + 12, 14, (Color){140, 145, 160, 255});

    // Changed indicator
    if (g_configChanged) {
        DrawText("*", 20, footerY + 10, 18, (Color){255, 200, 100, 255});
    }
}

// ============================================================================
// Input Handling
// ============================================================================

static void CycleVisibility(int index) {
    if (index < 0 || index >= g_pluginCount) return;

    PMPluginEntry *p = &g_plugins[index];
    p->visibility = (p->visibility + 1) % PM_VIS_COUNT;
    g_configChanged = true;
}

static void EnsureSelectedVisible(void) {
    int startY = 70;
    int itemHeight = 56;

    // Calculate approximate position of selected item
    int categoryHeaders = 0;
    LlzPluginCategory lastCat = LLZ_CATEGORY_COUNT;
    for (int i = 0; i <= g_selectedIndex && i < g_pluginCount; i++) {
        if (g_plugins[i].category != lastCat) {
            categoryHeaders++;
            lastCat = g_plugins[i].category;
        }
    }

    int selectedY = g_selectedIndex * itemHeight + categoryHeaders * 30;
    int visibleHeight = g_screenHeight - startY - 50;

    if (selectedY < g_targetScrollOffset) {
        g_targetScrollOffset = selectedY;
    } else if (selectedY > g_targetScrollOffset + visibleHeight - itemHeight) {
        g_targetScrollOffset = selectedY - visibleHeight + itemHeight;
    }

    if (g_targetScrollOffset < 0) g_targetScrollOffset = 0;
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
        if (g_selectedIndex < g_pluginCount - 1) {
            g_selectedIndex++;
            EnsureSelectedVisible();
        }
    }

    // Cycle visibility on select or tap
    if (input->selectPressed || input->tap) {
        CycleVisibility(g_selectedIndex);
    }

    // Exit on back
    if (input->backPressed) {
        if (g_configChanged) {
            SaveVisibilityConfig();
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
    g_pluginCount = 0;

    g_font = LlzFontGetDefault();

    DiscoverPlugins();

    printf("[PluginManager] Initialized with %d plugins\n", g_pluginCount);
}

static void PluginUpdate(const LlzInputState *input, float deltaTime) {
    (void)deltaTime;
    HandleInput(input);
}

static void PluginDraw(void) {
    ClearBackground((Color){20, 22, 30, 255});

    DrawHeader();
    DrawPluginList();
    DrawFooter();
}

static void PluginShutdown(void) {
    if (g_configChanged) {
        SaveVisibilityConfig();
    }
    printf("[PluginManager] Shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin Export
// ============================================================================

static LlzPluginAPI g_api = {
    .name = "Plugin Manager",
    .description = "Configure which plugins appear in the menu",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .category = LLZ_CATEGORY_UTILITIES
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
