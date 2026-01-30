#include "menu_theme_private.h"
#include <string.h>

// Menu context state
static bool g_insideFolder = false;
static LlzPluginCategory g_currentFolder = LLZ_CATEGORY_MEDIA;
static int *g_folderPlugins = NULL;
static int g_folderPluginCount = 0;

// Menu items reference
static const MenuItemList *g_menuItems = NULL;
static const PluginRegistry *g_registry = NULL;

void MenuThemeSetMenuItems(const MenuItemList *items, const PluginRegistry *registry)
{
    g_menuItems = items;
    g_registry = registry;
}

void MenuThemeSetFolderContext(bool inside, LlzPluginCategory category,
                                int *plugins, int count)
{
    g_insideFolder = inside;
    g_currentFolder = category;
    g_folderPlugins = plugins;
    g_folderPluginCount = count;
}

bool MenuThemeIsInsideFolder(void)
{
    return g_insideFolder;
}

LlzPluginCategory MenuThemeGetCurrentFolder(void)
{
    return g_currentFolder;
}

// Get total item count for current menu context
int MenuThemeGetItemCount(void)
{
    if (g_insideFolder) {
        return g_folderPluginCount;
    }
    return g_menuItems ? g_menuItems->count : 0;
}

// Get display name for item at index
const char* MenuThemeGetItemName(int index)
{
    if (g_insideFolder) {
        if (index < 0 || index >= g_folderPluginCount || !g_folderPlugins) return "";
        int pluginIdx = g_folderPlugins[index];
        if (!g_registry || pluginIdx < 0 || pluginIdx >= g_registry->count) return "";
        return g_registry->items[pluginIdx].displayName;
    } else {
        if (!g_menuItems || index < 0 || index >= g_menuItems->count) return "";
        return g_menuItems->items[index].displayName;
    }
}

// Get description for item at index (folders return NULL)
const char* MenuThemeGetItemDescription(int index)
{
    if (g_insideFolder) {
        if (index < 0 || index >= g_folderPluginCount || !g_folderPlugins) return NULL;
        int pluginIdx = g_folderPlugins[index];
        if (!g_registry || pluginIdx < 0 || pluginIdx >= g_registry->count) return NULL;
        if (g_registry->items[pluginIdx].api) {
            return g_registry->items[pluginIdx].api->description;
        }
        return NULL;
    } else {
        if (!g_menuItems || index < 0 || index >= g_menuItems->count) return NULL;
        MenuItem *item = &g_menuItems->items[index];
        if (item->type == MENU_ITEM_FOLDER) return NULL;
        int pluginIdx = item->plugin.pluginIndex;
        if (!g_registry || pluginIdx < 0 || pluginIdx >= g_registry->count) return NULL;
        if (g_registry->items[pluginIdx].api) {
            return g_registry->items[pluginIdx].api->description;
        }
        return NULL;
    }
}

// Check if item at index is a folder
bool MenuThemeIsItemFolder(int index)
{
    if (g_insideFolder) return false;  // Inside folder = all items are plugins
    if (!g_menuItems || index < 0 || index >= g_menuItems->count) return false;
    return g_menuItems->items[index].type == MENU_ITEM_FOLDER;
}

// Get folder plugin count (for folder items)
int MenuThemeGetFolderPluginCount(int index)
{
    if (g_insideFolder) return 0;
    if (!g_menuItems || index < 0 || index >= g_menuItems->count) return 0;
    MenuItem *item = &g_menuItems->items[index];
    if (item->type != MENU_ITEM_FOLDER) return 0;
    return item->folder.pluginCount;
}
