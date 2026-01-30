#include "raylib.h"
#include "plugin_loader.h"
#include "llz_sdk.h"
#include "menu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT

// Plugin refresh state
#define PLUGIN_REFRESH_INTERVAL 2.0f
static float g_pluginRefreshTimer = 0.0f;
static PluginDirSnapshot g_pluginSnapshot = {0};

// Folder-based menu state
static MenuItemList g_menuItems = {0};
static bool g_insideFolder = false;
static LlzPluginCategory g_currentFolder = LLZ_CATEGORY_MEDIA;
static int *g_folderPlugins = NULL;
static int g_folderPluginCount = 0;

// Global registry
static PluginRegistry g_registry = {0};

// Color constants for SDK background initialization
static const Color COLOR_ACCENT = {138, 106, 210, 255};
static const Color COLOR_ACCENT_DIM = {90, 70, 140, 255};

int main(void)
{
    // Initialize config system first (before display for brightness)
    LlzConfigInit();

    if (!LlzDisplayInit()) {
        fprintf(stderr, "Failed to initialize display. Check DRM permissions and cabling.\n");
        return 1;
    }
    LlzInputInit();

    // Initialize SDK media system for Redis access (needed by auto-blur background)
    LlzMediaInit(NULL);

    // Initialize SDK background system for animated menu backgrounds
    LlzBackgroundInit(SCREEN_WIDTH, SCREEN_HEIGHT);
    LlzBackgroundSetColors(COLOR_ACCENT, COLOR_ACCENT_DIM);
    LlzBackgroundSetEnabled(true);

    // Load saved background style from config
    LlzConfigBackgroundStyle savedBgStyle = LlzConfigGetBackgroundStyle();
    LlzBackgroundSetStyle((LlzBackgroundStyle)savedBgStyle, false);
    printf("Loaded background style: %d\n", savedBgStyle);

    // Initialize menu theme system
    MenuThemeInit(SCREEN_WIDTH, SCREEN_HEIGHT);
    MenuThemeSetStyle((MenuThemeStyle)LlzConfigGetMenuStyle());
    printf("Loaded menu style: %s\n", MenuThemeGetStyleName(MenuThemeGetStyle()));

    // Load plugins
    char pluginDir[512];
    const char *working = GetWorkingDirectory();
    snprintf(pluginDir, sizeof(pluginDir), "%s/plugins", working ? working : ".");

    memset(&g_registry, 0, sizeof(g_registry));
    LoadPlugins(pluginDir, &g_registry);

    // Load visibility configuration and build menu items
    LoadPluginVisibility(&g_registry);
    BuildMenuItems(&g_registry, &g_menuItems);
    printf("Menu built: %d items\n", g_menuItems.count);

    // Set menu items for theme system
    MenuThemeSetMenuItems(&g_menuItems, &g_registry);

    // Create initial snapshot for change detection
    g_pluginSnapshot = CreatePluginSnapshot(pluginDir);

    int selectedIndex = 0;
    if (selectedIndex >= g_registry.count) selectedIndex = g_registry.count - 1;
    bool runningPlugin = false;
    LoadedPlugin *active = NULL;
    int lastPluginIndex = -1;

    // Check for startup plugin configuration
    if (LlzConfigHasStartupPlugin() && g_registry.count > 0) {
        const char *startupName = LlzConfigGetStartupPlugin();
        int startupIndex = -1;

        for (int i = 0; i < g_registry.count; i++) {
            if (strcmp(g_registry.items[i].displayName, startupName) == 0 ||
                (g_registry.items[i].api && g_registry.items[i].api->name &&
                 strcmp(g_registry.items[i].api->name, startupName) == 0)) {
                startupIndex = i;
                break;
            }
            if (strcasecmp(g_registry.items[i].displayName, startupName) == 0 ||
                (g_registry.items[i].api && g_registry.items[i].api->name &&
                 strcasecmp(g_registry.items[i].api->name, startupName) == 0)) {
                startupIndex = i;
                break;
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

            // Update theme animations
            MenuThemeUpdate(delta);

            // Periodic plugin directory check
            g_pluginRefreshTimer += delta;
            if (g_pluginRefreshTimer >= PLUGIN_REFRESH_INTERVAL) {
                g_pluginRefreshTimer = 0.0f;

                if (HasPluginDirectoryChanged(pluginDir, &g_pluginSnapshot)) {
                    int changes = RefreshPlugins(pluginDir, &g_registry);
                    if (changes > 0) {
                        printf("Plugins refreshed: %d change(s)\n", changes);

                        FreePluginSnapshot(&g_pluginSnapshot);
                        g_pluginSnapshot = CreatePluginSnapshot(pluginDir);

                        LoadPluginVisibility(&g_registry);
                        FreeMenuItems(&g_menuItems);
                        BuildMenuItems(&g_registry, &g_menuItems);
                        MenuThemeSetMenuItems(&g_menuItems, &g_registry);

                        if (g_insideFolder) {
                            FreeFolderPlugins(g_folderPlugins);
                            g_folderPlugins = NULL;
                            g_folderPluginCount = 0;
                            g_insideFolder = false;
                            MenuThemeSetFolderContext(false, LLZ_CATEGORY_MEDIA, NULL, 0);
                        }

                        selectedIndex = 0;
                        MenuThemeResetScroll();

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
                LlzConfigSetBackgroundStyle((LlzConfigBackgroundStyle)LlzBackgroundGetStyle());
            }

            // Cycle menu navigation style with button3 (display mode button)
            if (inputState.button3Pressed) {
                MenuThemeCycleNext();
                LlzConfigSetMenuStyle((LlzMenuStyle)MenuThemeGetStyle());
            }

            // Back button handling
            if (inputState.backReleased) {
                if (g_insideFolder) {
                    FreeFolderPlugins(g_folderPlugins);
                    g_folderPlugins = NULL;
                    g_folderPluginCount = 0;
                    g_insideFolder = false;
                    MenuThemeSetFolderContext(false, LLZ_CATEGORY_MEDIA, NULL, 0);
                    selectedIndex = 0;
                    MenuThemeResetScroll();
                } else if (lastPluginIndex >= 0 && lastPluginIndex < g_registry.count) {
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
                    int pluginIdx = g_folderPlugins[selectedIndex];
                    lastPluginIndex = pluginIdx;
                    active = &g_registry.items[pluginIdx];
                    if (active && active->api && active->api->init) {
                        active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
                    }
                    runningPlugin = true;
                    continue;
                } else {
                    MenuItem *item = &g_menuItems.items[selectedIndex];
                    if (item->type == MENU_ITEM_FOLDER) {
                        g_currentFolder = item->folder.category;
                        g_folderPlugins = GetFolderPlugins(&g_registry, g_currentFolder, &g_folderPluginCount);
                        g_insideFolder = true;
                        MenuThemeSetFolderContext(true, g_currentFolder, g_folderPlugins, g_folderPluginCount);
                        selectedIndex = 0;
                        MenuThemeResetScroll();
                    } else {
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
            MenuThemeDraw(&g_registry, selectedIndex, delta);
            LlzBackgroundDrawIndicator();
            LlzDisplayEnd();
        } else if (active && active->api) {
            if (active->api->update) active->api->update(&inputState, delta);

            LlzDisplayBegin();
            if (active->api->draw) active->api->draw();
            LlzDisplayEnd();

            bool exitRequest = IsKeyReleased(KEY_ESCAPE);
            if (!exitRequest && !active->api->handles_back_button) {
                exitRequest = inputState.backReleased;
            }
            if (!exitRequest && active->api->wants_close) {
                exitRequest = active->api->wants_close();
            }

            if (exitRequest) {
                const LlzPluginAPI *closingApi = active->api;

                if (closingApi->shutdown) closingApi->shutdown();

                bool needsRefresh = closingApi->wants_refresh && closingApi->wants_refresh();

                if (needsRefresh) {
                    LoadPluginVisibility(&g_registry);
                    FreeMenuItems(&g_menuItems);
                    BuildMenuItems(&g_registry, &g_menuItems);
                    MenuThemeSetMenuItems(&g_menuItems, &g_registry);

                    if (g_insideFolder) {
                        g_insideFolder = false;
                        FreeFolderPlugins(g_folderPlugins);
                        g_folderPlugins = NULL;
                        g_folderPluginCount = 0;
                        MenuThemeSetFolderContext(false, LLZ_CATEGORY_MEDIA, NULL, 0);
                    }

                    selectedIndex = 0;
                }

                if (LlzHasRequestedPlugin()) {
                    const char *requestedName = LlzGetRequestedPlugin();
                    if (requestedName) {
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
                            selectedIndex = foundIndex;
                            lastPluginIndex = foundIndex;
                            active = &g_registry.items[foundIndex];
                            if (active->api && active->api->init) {
                                active->api->init(SCREEN_WIDTH, SCREEN_HEIGHT);
                            }
                            continue;
                        }
                    }
                }

                runningPlugin = false;
                active = NULL;
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
    MenuThemeShutdown();
    LlzBackgroundShutdown();
    LlzMediaShutdown();
    LlzInputShutdown();
    LlzDisplayShutdown();
    LlzConfigShutdown();
    return 0;
}
