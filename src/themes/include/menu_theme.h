#ifndef MENU_THEME_H
#define MENU_THEME_H

#include "menu_theme_types.h"
#include "raylib.h"
#include "plugin_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Theme Manager - Public API
// ============================================================================

/**
 * Initialize the menu theme system.
 * Must be called after raylib initialization and before any other theme functions.
 * @param screenWidth Display width in pixels
 * @param screenHeight Display height in pixels
 * @return true on success
 */
bool MenuThemeInit(int screenWidth, int screenHeight);

/**
 * Shutdown the menu theme system.
 * Unloads all fonts and frees resources.
 */
void MenuThemeShutdown(void);

/**
 * Update theme animations (scroll, indicator fade, crossfade).
 * Call once per frame when menu is visible.
 * @param deltaTime Frame delta time in seconds
 */
void MenuThemeUpdate(float deltaTime);

/**
 * Draw the menu using the current theme style.
 * @param registry Plugin registry for menu items
 * @param selected Currently selected index
 * @param deltaTime Frame delta time (for animations)
 */
void MenuThemeDraw(const PluginRegistry *registry, int selected, float deltaTime);

/**
 * Cycle to the next theme style.
 * Updates indicator to show style name briefly.
 */
void MenuThemeCycleNext(void);

/**
 * Set the current theme style directly.
 * @param style The style to set
 */
void MenuThemeSetStyle(MenuThemeStyle style);

/**
 * Get the current theme style.
 * @return Current style enum value
 */
MenuThemeStyle MenuThemeGetStyle(void);

/**
 * Get the display name for a theme style.
 * @param style The style to get name for
 * @return Human-readable style name
 */
const char* MenuThemeGetStyleName(MenuThemeStyle style);

// ============================================================================
// Scroll State Management
// ============================================================================

/**
 * Reset scroll state (call when entering/exiting folders).
 */
void MenuThemeResetScroll(void);

/**
 * Get current scroll offset for list/grid styles.
 */
float MenuThemeGetScrollOffset(void);

/**
 * Get current carousel offset for carousel style.
 */
float MenuThemeGetCarouselOffset(void);

// ============================================================================
// Color Access
// ============================================================================

/**
 * Get the standard dark theme color palette.
 */
const MenuThemeColorPalette* MenuThemeGetColors(void);

/**
 * Get grid-specific white theme colors.
 */
const MenuThemeGridColors* MenuThemeGetGridColors(void);

/**
 * Get dynamic accent color from background palette (if enabled).
 * Falls back to standard accent if no palette available.
 */
Color MenuThemeGetDynamicAccent(void);

/**
 * Get complementary color for dynamic accent.
 */
Color MenuThemeGetComplementaryColor(void);

// ============================================================================
// Font Access
// ============================================================================

/**
 * Get the main menu font.
 */
Font MenuThemeGetFont(void);

/**
 * Get the Omicron font (for CarThing branding).
 */
Font MenuThemeGetOmicronFont(void);

/**
 * Get the Tracklister font (for CarThing text).
 */
Font MenuThemeGetTracklisterFont(void);

/**
 * Get the iBrand font (for Grid style).
 */
Font MenuThemeGetIBrandFont(void);

// ============================================================================
// Menu Context (folder state access for themes)
// ============================================================================

/**
 * Check if currently inside a folder.
 */
bool MenuThemeIsInsideFolder(void);

/**
 * Set folder context state.
 * @param inside Whether inside a folder
 * @param category Current folder category (if inside)
 * @param plugins Array of plugin indices in folder
 * @param count Number of plugins in folder
 */
void MenuThemeSetFolderContext(bool inside, LlzPluginCategory category,
                                int *plugins, int count);

/**
 * Get the current folder category (only valid if inside folder).
 */
LlzPluginCategory MenuThemeGetCurrentFolder(void);

// ============================================================================
// Menu Item Helpers (unified access for all styles)
// ============================================================================

/**
 * Set the menu items for rendering.
 * @param items Menu item list
 * @param registry Plugin registry
 */
void MenuThemeSetMenuItems(const MenuItemList *items, const PluginRegistry *registry);

/**
 * Get total item count for current menu context.
 */
int MenuThemeGetItemCount(void);

/**
 * Get display name for item at index.
 */
const char* MenuThemeGetItemName(int index);

/**
 * Get description for item at index (NULL for folders).
 */
const char* MenuThemeGetItemDescription(int index);

/**
 * Check if item at index is a folder.
 */
bool MenuThemeIsItemFolder(int index);

/**
 * Get plugin count for folder item.
 */
int MenuThemeGetFolderPluginCount(int index);

#ifdef __cplusplus
}
#endif

#endif // MENU_THEME_H
