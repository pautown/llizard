#ifndef LLIZARD_PLUGIN_H
#define LLIZARD_PLUGIN_H

#include <stdbool.h>
#include "llz_sdk_input.h"

/**
 * Plugin categories for organizing plugins in the main menu.
 * Categories determine default folder grouping in the plugin selector.
 */
typedef enum {
    LLZ_CATEGORY_MEDIA = 0,      // Music, podcasts, videos, album art viewers
    LLZ_CATEGORY_UTILITIES,      // Settings, system tools, plugin manager
    LLZ_CATEGORY_GAMES,          // All games and entertainment
    LLZ_CATEGORY_INFO,           // Clocks, weather, status displays
    LLZ_CATEGORY_DEBUG,          // Development and debugging tools
    LLZ_CATEGORY_COUNT
} LlzPluginCategory;

/**
 * Category names for display purposes.
 */
static const char *LLZ_CATEGORY_NAMES[] = {
    "Media",
    "Utilities",
    "Games",
    "Info",
    "Debug"
};

typedef struct LlzPluginAPI {
    const char *name;
    const char *description;
    void (*init)(int screenWidth, int screenHeight);
    void (*update)(const LlzInputState *input, float deltaTime);
    void (*draw)(void);
    void (*shutdown)(void);
    bool (*wants_close)(void);

    // If true, host will NOT handle back button - plugin is responsible for
    // setting wants_close when appropriate. Useful for plugins with hierarchical
    // navigation where back should navigate up before exiting.
    bool handles_back_button;

    // Plugin category for menu organization (default: LLZ_CATEGORY_MEDIA)
    LlzPluginCategory category;

    // Optional: If provided and returns true, host will rebuild menu items
    // when plugin closes. Used by plugins that modify visibility or sort order.
    // Default behavior (NULL): no refresh
    bool (*wants_refresh)(void);
} LlzPluginAPI;

typedef const LlzPluginAPI *(*LlzGetPluginFunc)(void);

#endif
