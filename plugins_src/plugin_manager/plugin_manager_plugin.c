/**
 * Plugin Manager - Configure plugin visibility in the main menu
 *
 * Allows users to configure how plugins appear in the main menu:
 * - HOME: Plugin appears directly on the home screen (pinned)
 * - FOLDER: Plugin appears in its category folder (Media, Games, etc.)
 * - HIDDEN: Plugin is not shown in the menu at all
 *
 * Configuration is stored in plugin_visibility.ini and read by the main host.
 *
 * Controls:
 *   UP/DOWN or SCROLL  - Navigate through plugins
 *   SELECT (tap)       - Cycle visibility mode quickly
 *   SELECT (hold)      - Open dropdown to choose placement
 *   BACK               - Exit and save changes
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

#define PM_MAX_PLUGINS 64
#define PM_NAME_MAX 64
#define PM_PATH_MAX 256

#define PM_SCREEN_WIDTH 800
#define PM_SCREEN_HEIGHT 480

// Hold time for long-press (in seconds)
#define PM_HOLD_THRESHOLD 0.5f

// Visibility modes
typedef enum {
    PM_VIS_HOME = 0,    // Show on home screen
    PM_VIS_FOLDER,      // Show in category folder
    PM_VIS_HIDDEN,      // Don't show at all
    PM_VIS_COUNT
} PMVisibility;

static const char *PM_VIS_NAMES[] = {"Home", "Folder", "Hidden"};

// Modern color palette
static const Color PM_COLOR_BG = {18, 18, 24, 255};
static const Color PM_COLOR_CARD = {28, 30, 38, 255};
static const Color PM_COLOR_CARD_SELECTED = {38, 42, 55, 255};
static const Color PM_COLOR_ACCENT = {100, 140, 255, 255};
static const Color PM_COLOR_HOME = {80, 200, 120, 255};
static const Color PM_COLOR_FOLDER = {100, 160, 255, 255};
static const Color PM_COLOR_HIDDEN = {255, 100, 100, 255};
static const Color PM_COLOR_TEXT = {240, 240, 245, 255};
static const Color PM_COLOR_TEXT_DIM = {130, 135, 150, 255};
static const Color PM_COLOR_HEADER = {24, 26, 34, 255};
static const Color PM_COLOR_POPUP_BG = {32, 34, 44, 250};
static const Color PM_COLOR_POPUP_ITEM = {42, 46, 58, 255};
static const Color PM_COLOR_POPUP_HOVER = {52, 58, 75, 255};

// Category colors for dropdown
static const Color PM_CATEGORY_COLORS[] = {
    {255, 140, 100, 255},  // Media - orange
    {100, 200, 255, 255},  // Utilities - cyan
    {255, 180, 100, 255},  // Games - gold
    {180, 140, 255, 255},  // Info - purple
    {255, 100, 140, 255},  // Debug - pink
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
// Dropdown Menu State
// ============================================================================

typedef enum {
    DROPDOWN_OPTION_HOME = 0,
    DROPDOWN_OPTION_MEDIA,
    DROPDOWN_OPTION_UTILITIES,
    DROPDOWN_OPTION_GAMES,
    DROPDOWN_OPTION_INFO,
    DROPDOWN_OPTION_DEBUG,
    DROPDOWN_OPTION_HIDDEN,
    DROPDOWN_OPTION_COUNT
} DropdownOption;

static const char *DROPDOWN_LABELS[] = {
    "Pin to Home",
    "Media Folder",
    "Utilities Folder",
    "Games Folder",
    "Info Folder",
    "Debug Folder",
    "Hide Plugin"
};

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

// Long-press and dropdown state
static bool g_selectHeld = false;
static float g_holdTime = 0.0f;
static bool g_dropdownOpen = false;
static int g_dropdownSelection = 0;
static float g_dropdownAlpha = 0.0f;

// Animation state
static float g_animTime = 0.0f;

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
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *filename = line;
        char *visStr = eq + 1;

        char *nl = strchr(visStr, '\n');
        if (nl) *nl = '\0';

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

static int ComparePluginsByName(const void *a, const void *b) {
    const PMPluginEntry *pa = (const PMPluginEntry *)a;
    const PMPluginEntry *pb = (const PMPluginEntry *)b;
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
        if (entry->d_name[0] == '.') continue;
        const char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".so") != 0) continue;

        // Skip the plugin manager itself
        if (strstr(entry->d_name, "plugin_manager") != NULL) continue;

        char fullPath[PM_PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", GetPluginsDir(), entry->d_name);

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
        p->visibility = PM_VIS_FOLDER;
        p->loaded = true;

        g_pluginCount++;
        dlclose(handle);
    }

    closedir(dir);

    // Sort alphabetically by name
    if (g_pluginCount > 1) {
        qsort(g_plugins, g_pluginCount, sizeof(PMPluginEntry), ComparePluginsByName);
    }

    printf("[PluginManager] Discovered %d plugins\n", g_pluginCount);
    LoadVisibilityConfig();
}

// ============================================================================
// Drawing Utilities
// ============================================================================

static Color GetVisibilityColor(PMVisibility vis) {
    switch (vis) {
        case PM_VIS_HOME: return PM_COLOR_HOME;
        case PM_VIS_FOLDER: return PM_COLOR_FOLDER;
        case PM_VIS_HIDDEN: return PM_COLOR_HIDDEN;
        default: return PM_COLOR_TEXT_DIM;
    }
}

static const char *GetVisibilityIcon(PMVisibility vis) {
    switch (vis) {
        case PM_VIS_HOME: return "H";
        case PM_VIS_FOLDER: return "F";
        case PM_VIS_HIDDEN: return "X";
        default: return "?";
    }
}

// ============================================================================
// Drawing
// ============================================================================

static void DrawHeader(void) {
    // Gradient header background
    DrawRectangleGradientV(0, 0, g_screenWidth, 70, PM_COLOR_HEADER, PM_COLOR_BG);

    // Title with icon
    const char *title = "Plugin Manager";
    float titleSize = 32;
    Vector2 titleDim = MeasureTextEx(g_font, title, titleSize, 2);
    float titleX = (g_screenWidth - titleDim.x) / 2;
    DrawTextEx(g_font, title, (Vector2){titleX, 12}, titleSize, 2, PM_COLOR_TEXT);

    // Subtitle with plugin count
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "%d plugins available", g_pluginCount);
    float subSize = 16;
    Vector2 subDim = MeasureTextEx(g_font, subtitle, subSize, 1);
    DrawTextEx(g_font, subtitle, (Vector2){(g_screenWidth - subDim.x) / 2, 46}, subSize, 1, PM_COLOR_TEXT_DIM);

    // Accent line
    float pulse = 0.7f + 0.3f * sinf(g_animTime * 2.0f);
    Color accentPulse = ColorAlpha(PM_COLOR_ACCENT, pulse);
    DrawRectangle(g_screenWidth / 2 - 60, 68, 120, 2, accentPulse);
}

static void DrawPluginCard(PMPluginEntry *p, int index, float y) {
    bool selected = (index == g_selectedIndex);
    float cardX = 20;
    float cardWidth = g_screenWidth - 40;
    float cardHeight = 64;

    // Card background with hover effect
    Color cardBg = selected ? PM_COLOR_CARD_SELECTED : PM_COLOR_CARD;
    Rectangle cardRect = {cardX, y, cardWidth, cardHeight};
    DrawRectangleRounded(cardRect, 0.15f, 8, cardBg);

    // Selection indicator (left accent bar)
    if (selected) {
        Color visColor = GetVisibilityColor(p->visibility);
        Rectangle accentBar = {cardX, y + 8, 4, cardHeight - 16};
        DrawRectangleRounded(accentBar, 0.5f, 4, visColor);

        // Subtle glow effect
        DrawRectangleRoundedLines(cardRect, 0.15f, 8, ColorAlpha(visColor, 0.3f));
    }

    // Plugin icon (first letter in circle)
    float iconX = cardX + 24;
    float iconY = y + cardHeight / 2;
    float iconRadius = 20;
    Color visColor = GetVisibilityColor(p->visibility);
    DrawCircle((int)iconX, (int)iconY, iconRadius, ColorAlpha(visColor, 0.2f));
    DrawCircleLines((int)iconX, (int)iconY, iconRadius, ColorAlpha(visColor, 0.5f));

    char initial[2] = {p->name[0], '\0'};
    float initialSize = 20;
    Vector2 initialDim = MeasureTextEx(g_font, initial, initialSize, 1);
    DrawTextEx(g_font, initial,
               (Vector2){iconX - initialDim.x / 2, iconY - initialDim.y / 2},
               initialSize, 1, visColor);

    // Plugin name
    float textX = iconX + iconRadius + 16;
    Color nameColor = selected ? PM_COLOR_TEXT : ColorAlpha(PM_COLOR_TEXT, 0.85f);
    DrawTextEx(g_font, p->name, (Vector2){textX, y + 14}, 22, 1, nameColor);

    // Category label
    const char *catName = (p->category < LLZ_CATEGORY_COUNT) ? LLZ_CATEGORY_NAMES[p->category] : "Unknown";
    Color catColor = (p->category < LLZ_CATEGORY_COUNT) ? PM_CATEGORY_COLORS[p->category] : PM_COLOR_TEXT_DIM;
    DrawTextEx(g_font, catName, (Vector2){textX, y + 40}, 14, 1, ColorAlpha(catColor, 0.7f));

    // Visibility badge on right
    float badgeWidth = 80;
    float badgeHeight = 28;
    float badgeX = cardX + cardWidth - badgeWidth - 16;
    float badgeY = y + (cardHeight - badgeHeight) / 2;

    Rectangle badgeRect = {badgeX, badgeY, badgeWidth, badgeHeight};
    DrawRectangleRounded(badgeRect, 0.5f, 8, ColorAlpha(visColor, 0.2f));
    DrawRectangleRoundedLines(badgeRect, 0.5f, 8, ColorAlpha(visColor, 0.5f));

    const char *visLabel = PM_VIS_NAMES[p->visibility];
    float visSize = 14;
    Vector2 visDim = MeasureTextEx(g_font, visLabel, visSize, 1);
    DrawTextEx(g_font, visLabel,
               (Vector2){badgeX + (badgeWidth - visDim.x) / 2, badgeY + (badgeHeight - visDim.y) / 2},
               visSize, 1, visColor);

    // Hold progress indicator when holding select
    if (selected && g_selectHeld && !g_dropdownOpen) {
        float progress = g_holdTime / PM_HOLD_THRESHOLD;
        if (progress > 0 && progress < 1) {
            float barWidth = cardWidth - 8;
            float barHeight = 3;
            float barX = cardX + 4;
            float barY = y + cardHeight - 6;
            DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, ColorAlpha(PM_COLOR_TEXT_DIM, 0.3f));
            DrawRectangle((int)barX, (int)barY, (int)(barWidth * progress), (int)barHeight, PM_COLOR_ACCENT);
        }
    }
}

static void DrawPluginList(void) {
    float startY = 80;
    float itemHeight = 72;
    float visibleHeight = g_screenHeight - startY - 50;

    // Smooth scrolling
    g_scrollOffset += (g_targetScrollOffset - g_scrollOffset) * 0.15f;

    // Clipping region
    BeginScissorMode(0, (int)startY, g_screenWidth, (int)visibleHeight);

    for (int i = 0; i < g_pluginCount; i++) {
        float itemY = startY + i * itemHeight - g_scrollOffset;

        // Skip items outside visible area
        if (itemY < startY - itemHeight || itemY > g_screenHeight) continue;

        DrawPluginCard(&g_plugins[i], i, itemY);
    }

    EndScissorMode();

    // Scroll fade indicators
    if (g_scrollOffset > 5) {
        for (int i = 0; i < 20; i++) {
            float alpha = (20 - i) / 20.0f * 0.8f;
            DrawRectangle(0, (int)startY + i, g_screenWidth, 1, ColorAlpha(PM_COLOR_BG, alpha));
        }
    }

    float maxScroll = g_pluginCount * itemHeight - visibleHeight;
    if (maxScroll > 0 && g_scrollOffset < maxScroll - 5) {
        int bottomY = (int)(startY + visibleHeight);
        for (int i = 0; i < 20; i++) {
            float alpha = i / 20.0f * 0.8f;
            DrawRectangle(0, bottomY - 20 + i, g_screenWidth, 1, ColorAlpha(PM_COLOR_BG, alpha));
        }
    }
}

static void DrawDropdown(void) {
    if (g_dropdownAlpha <= 0) return;

    // Darken background
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, ColorAlpha(BLACK, 0.6f * g_dropdownAlpha));

    // Dropdown panel
    float panelWidth = 300;
    float itemHeight = 48;
    float panelHeight = DROPDOWN_OPTION_COUNT * itemHeight + 20;
    float panelX = (g_screenWidth - panelWidth) / 2;
    float panelY = (g_screenHeight - panelHeight) / 2;

    // Panel background with shadow
    Rectangle shadowRect = {panelX + 4, panelY + 4, panelWidth, panelHeight};
    DrawRectangleRounded(shadowRect, 0.08f, 8, ColorAlpha(BLACK, 0.4f * g_dropdownAlpha));

    Rectangle panelRect = {panelX, panelY, panelWidth, panelHeight};
    DrawRectangleRounded(panelRect, 0.08f, 8, ColorAlpha(PM_COLOR_POPUP_BG, g_dropdownAlpha));
    DrawRectangleRoundedLines(panelRect, 0.08f, 8, ColorAlpha(PM_COLOR_ACCENT, 0.3f * g_dropdownAlpha));

    // Title
    PMPluginEntry *p = &g_plugins[g_selectedIndex];
    const char *title = p->name;
    float titleSize = 18;
    Vector2 titleDim = MeasureTextEx(g_font, title, titleSize, 1);
    DrawTextEx(g_font, title,
               (Vector2){panelX + (panelWidth - titleDim.x) / 2, panelY + 10},
               titleSize, 1, ColorAlpha(PM_COLOR_TEXT, g_dropdownAlpha));

    // Divider
    DrawRectangle((int)(panelX + 20), (int)(panelY + 38), (int)(panelWidth - 40), 1,
                  ColorAlpha(PM_COLOR_TEXT_DIM, 0.3f * g_dropdownAlpha));

    // Options
    float optionY = panelY + 48;
    for (int i = 0; i < DROPDOWN_OPTION_COUNT; i++) {
        bool selected = (i == g_dropdownSelection);
        float optX = panelX + 10;
        float optWidth = panelWidth - 20;

        // Option background
        Rectangle optRect = {optX, optionY, optWidth, itemHeight - 4};
        Color optBg = selected ? PM_COLOR_POPUP_HOVER : PM_COLOR_POPUP_ITEM;
        DrawRectangleRounded(optRect, 0.2f, 6, ColorAlpha(optBg, g_dropdownAlpha));

        // Selection indicator
        if (selected) {
            DrawRectangleRounded((Rectangle){optX, optionY + 6, 3, itemHeight - 16}, 0.5f, 4,
                                 ColorAlpha(PM_COLOR_ACCENT, g_dropdownAlpha));
        }

        // Icon color based on option type
        Color iconColor;
        if (i == DROPDOWN_OPTION_HOME) {
            iconColor = PM_COLOR_HOME;
        } else if (i == DROPDOWN_OPTION_HIDDEN) {
            iconColor = PM_COLOR_HIDDEN;
        } else {
            // Folder options - use category color
            int catIndex = i - 1;  // MEDIA starts at index 1
            iconColor = (catIndex < LLZ_CATEGORY_COUNT) ? PM_CATEGORY_COLORS[catIndex] : PM_COLOR_FOLDER;
        }

        // Icon circle
        float iconX = optX + 24;
        float iconY = optionY + itemHeight / 2 - 2;
        DrawCircle((int)iconX, (int)iconY, 12, ColorAlpha(iconColor, 0.3f * g_dropdownAlpha));

        // Icon letter
        char iconLetter;
        if (i == DROPDOWN_OPTION_HOME) iconLetter = 'H';
        else if (i == DROPDOWN_OPTION_HIDDEN) iconLetter = 'X';
        else iconLetter = LLZ_CATEGORY_NAMES[i - 1][0];  // First letter of category

        char iconStr[2] = {iconLetter, '\0'};
        Vector2 iconDim = MeasureTextEx(g_font, iconStr, 14, 1);
        DrawTextEx(g_font, iconStr,
                   (Vector2){iconX - iconDim.x / 2, iconY - iconDim.y / 2},
                   14, 1, ColorAlpha(iconColor, g_dropdownAlpha));

        // Label
        const char *label = DROPDOWN_LABELS[i];
        Color labelColor = selected ? PM_COLOR_TEXT : ColorAlpha(PM_COLOR_TEXT, 0.8f);
        DrawTextEx(g_font, label, (Vector2){optX + 48, optionY + 14}, 18, 1,
                   ColorAlpha(labelColor, g_dropdownAlpha));

        // Current indicator (checkmark if this is current setting)
        bool isCurrent = false;
        if (i == DROPDOWN_OPTION_HOME && p->visibility == PM_VIS_HOME) isCurrent = true;
        else if (i == DROPDOWN_OPTION_HIDDEN && p->visibility == PM_VIS_HIDDEN) isCurrent = true;
        else if (i >= DROPDOWN_OPTION_MEDIA && i <= DROPDOWN_OPTION_DEBUG &&
                 p->visibility == PM_VIS_FOLDER && p->category == (LlzPluginCategory)(i - 1)) {
            isCurrent = true;
        }

        if (isCurrent) {
            const char *check = "*";
            DrawTextEx(g_font, check, (Vector2){optX + optWidth - 30, optionY + 12}, 20, 1,
                       ColorAlpha(PM_COLOR_ACCENT, g_dropdownAlpha));
        }

        optionY += itemHeight;
    }

    // Hint at bottom
    const char *hint = "Scroll to select, Press to confirm";
    float hintSize = 12;
    Vector2 hintDim = MeasureTextEx(g_font, hint, hintSize, 1);
    DrawTextEx(g_font, hint,
               (Vector2){(g_screenWidth - hintDim.x) / 2, panelY + panelHeight + 10},
               hintSize, 1, ColorAlpha(PM_COLOR_TEXT_DIM, g_dropdownAlpha * 0.7f));
}

static void DrawFooter(void) {
    float footerY = g_screenHeight - 44;

    // Footer background
    DrawRectangleGradientV(0, (int)footerY, g_screenWidth, 44, ColorAlpha(PM_COLOR_BG, 0), PM_COLOR_HEADER);

    // Hint text
    const char *hint = g_dropdownOpen ? "BACK: Cancel" : "Hold SELECT for options | BACK: Save & Exit";
    float hintSize = 14;
    Vector2 hintDim = MeasureTextEx(g_font, hint, hintSize, 1);
    DrawTextEx(g_font, hint, (Vector2){(g_screenWidth - hintDim.x) / 2, footerY + 16}, hintSize, 1, PM_COLOR_TEXT_DIM);

    // Changed indicator
    if (g_configChanged && !g_dropdownOpen) {
        DrawCircle(30, (int)footerY + 22, 6, PM_COLOR_ACCENT);
    }
}

// ============================================================================
// Input Handling
// ============================================================================

static void ApplyDropdownSelection(void) {
    if (g_selectedIndex < 0 || g_selectedIndex >= g_pluginCount) return;

    PMPluginEntry *p = &g_plugins[g_selectedIndex];

    switch (g_dropdownSelection) {
        case DROPDOWN_OPTION_HOME:
            p->visibility = PM_VIS_HOME;
            break;
        case DROPDOWN_OPTION_MEDIA:
        case DROPDOWN_OPTION_UTILITIES:
        case DROPDOWN_OPTION_GAMES:
        case DROPDOWN_OPTION_INFO:
        case DROPDOWN_OPTION_DEBUG:
            p->visibility = PM_VIS_FOLDER;
            p->category = (LlzPluginCategory)(g_dropdownSelection - 1);
            break;
        case DROPDOWN_OPTION_HIDDEN:
            p->visibility = PM_VIS_HIDDEN;
            break;
        default:
            break;
    }

    g_configChanged = true;
}

static void CycleVisibility(int index) {
    if (index < 0 || index >= g_pluginCount) return;

    PMPluginEntry *p = &g_plugins[index];
    p->visibility = (p->visibility + 1) % PM_VIS_COUNT;
    g_configChanged = true;
}

static void EnsureSelectedVisible(void) {
    float startY = 80;
    float itemHeight = 72;
    float visibleHeight = g_screenHeight - startY - 50;

    float selectedY = g_selectedIndex * itemHeight;
    float maxScroll = g_pluginCount * itemHeight - visibleHeight;
    if (maxScroll < 0) maxScroll = 0;

    if (selectedY < g_targetScrollOffset) {
        g_targetScrollOffset = selectedY;
    } else if (selectedY > g_targetScrollOffset + visibleHeight - itemHeight) {
        g_targetScrollOffset = selectedY - visibleHeight + itemHeight;
    }

    if (g_targetScrollOffset < 0) g_targetScrollOffset = 0;
    if (g_targetScrollOffset > maxScroll) g_targetScrollOffset = maxScroll;
}

static void HandleInput(const LlzInputState *input, float deltaTime) {
    (void)deltaTime;

    if (g_dropdownOpen) {
        // Dropdown navigation
        if (input->upPressed || input->scrollDelta < 0) {
            g_dropdownSelection = (g_dropdownSelection - 1 + DROPDOWN_OPTION_COUNT) % DROPDOWN_OPTION_COUNT;
        }
        if (input->downPressed || input->scrollDelta > 0) {
            g_dropdownSelection = (g_dropdownSelection + 1) % DROPDOWN_OPTION_COUNT;
        }

        // Confirm selection
        if (input->selectPressed || input->tap) {
            ApplyDropdownSelection();
            g_dropdownOpen = false;
        }

        // Cancel
        if (input->backPressed) {
            g_dropdownOpen = false;
        }

        return;
    }

    // Regular navigation
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

    // Update hold time for progress bar display
    g_holdTime = input->selectHoldTime;
    g_selectHeld = (g_holdTime > 0 && g_holdTime < PM_HOLD_THRESHOLD);

    // Long press detected by SDK - open dropdown
    if (input->selectHold) {
        g_dropdownOpen = true;
        g_dropdownSelection = 0;

        // Pre-select current setting in dropdown
        PMPluginEntry *p = &g_plugins[g_selectedIndex];
        if (p->visibility == PM_VIS_HOME) {
            g_dropdownSelection = DROPDOWN_OPTION_HOME;
        } else if (p->visibility == PM_VIS_HIDDEN) {
            g_dropdownSelection = DROPDOWN_OPTION_HIDDEN;
        } else {
            g_dropdownSelection = DROPDOWN_OPTION_MEDIA + p->category;
        }

        g_selectHeld = false;
        g_holdTime = 0.0f;
    }

    // Short press - cycle visibility (only on release, when hold wasn't triggered)
    if (input->selectPressed && input->selectHoldTime < PM_HOLD_THRESHOLD) {
        CycleVisibility(g_selectedIndex);
    }

    // Handle tap separately (touch input)
    if (input->tap) {
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
    g_selectHeld = false;
    g_holdTime = 0.0f;
    g_dropdownOpen = false;
    g_dropdownSelection = 0;
    g_dropdownAlpha = 0.0f;
    g_animTime = 0.0f;

    g_font = LlzFontGetDefault();

    DiscoverPlugins();

    printf("[PluginManager] Initialized with %d plugins\n", g_pluginCount);
}

static void PluginUpdate(const LlzInputState *input, float deltaTime) {
    g_animTime += deltaTime;

    // Animate dropdown alpha
    float targetAlpha = g_dropdownOpen ? 1.0f : 0.0f;
    g_dropdownAlpha += (targetAlpha - g_dropdownAlpha) * 10.0f * deltaTime;
    if (fabsf(g_dropdownAlpha - targetAlpha) < 0.01f) {
        g_dropdownAlpha = targetAlpha;
    }

    HandleInput(input, deltaTime);
}

static void PluginDraw(void) {
    ClearBackground(PM_COLOR_BG);

    DrawPluginList();
    DrawHeader();
    DrawFooter();

    // Dropdown overlay (drawn last)
    DrawDropdown();
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
