#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include "llizard_plugin.h"
#include <stdbool.h>

// Plugin visibility modes (matches plugin_manager)
typedef enum {
    PLUGIN_VIS_HOME = 0,    // Show on home screen (pinned)
    PLUGIN_VIS_FOLDER,      // Show in category folder
    PLUGIN_VIS_HIDDEN       // Don't show at all
} PluginVisibility;

typedef struct {
    char displayName[128];
    char path[512];
    char filename[128];     // Just the filename (e.g., "nowplaying.so")
    void *handle;
    const LlzPluginAPI *api;
    LlzPluginCategory category;
    PluginVisibility visibility;
} LoadedPlugin;

typedef struct {
    LoadedPlugin *items;
    int count;
} PluginRegistry;

// Snapshot of plugin directory for change detection
typedef struct {
    char **filenames;   // Array of .so filenames (basename only)
    int count;
} PluginDirSnapshot;

bool LoadPlugins(const char *directory, PluginRegistry *registry);
void UnloadPlugins(PluginRegistry *registry);

// Create a snapshot of .so files in the plugin directory
PluginDirSnapshot CreatePluginSnapshot(const char *directory);

// Compare current directory state to snapshot
// Returns true if directory contents have changed
bool HasPluginDirectoryChanged(const char *directory, const PluginDirSnapshot *snapshot);

// Free snapshot memory
void FreePluginSnapshot(PluginDirSnapshot *snapshot);

// Refresh plugins: unload removed, load new, preserve existing
// Returns the number of changes (added + removed)
int RefreshPlugins(const char *directory, PluginRegistry *registry);

// Load visibility configuration from plugin_visibility.ini
// Call after LoadPlugins to apply saved visibility settings
void LoadPluginVisibility(PluginRegistry *registry);

// Save visibility configuration to plugin_visibility.ini
void SavePluginVisibility(const PluginRegistry *registry);

// ============================================================================
// Menu Item System - supports folders and plugins
// ============================================================================

typedef enum {
    MENU_ITEM_FOLDER = 0,   // A category folder
    MENU_ITEM_PLUGIN        // A plugin entry
} MenuItemType;

typedef struct {
    MenuItemType type;
    union {
        struct {
            LlzPluginCategory category;
            int pluginCount;        // Number of plugins in this folder
        } folder;
        struct {
            int pluginIndex;        // Index into PluginRegistry.items
        } plugin;
    };
    char displayName[128];
    char sortKey[128];              // Key for sort order config (e.g., "folder:Media" or "plugin:nowplaying.so")
    int sortIndex;                  // Sort order (lower = higher in list)
} MenuItem;

typedef struct {
    MenuItem *items;
    int count;
    int capacity;
} MenuItemList;

// Build menu item list from registry based on visibility settings
// Returns a list containing: folders first (for FOLDER visibility), then HOME plugins
// Items are sorted by sortIndex after loading sort order config
void BuildMenuItems(const PluginRegistry *registry, MenuItemList *menuItems);

// Get plugins for a specific category folder (FOLDER visibility only)
// Returns indices into registry->items, count is number of plugins
int *GetFolderPlugins(const PluginRegistry *registry, LlzPluginCategory category, int *outCount);

// Free menu item list
void FreeMenuItems(MenuItemList *menuItems);

// Free folder plugins array
void FreeFolderPlugins(int *indices);

// Load sort order configuration from menu_sort_order.ini
// Call after BuildMenuItems to apply saved sort order
void LoadMenuSortOrder(MenuItemList *menuItems);

// Sort menu items by their sortIndex
void SortMenuItems(MenuItemList *menuItems);

#endif
