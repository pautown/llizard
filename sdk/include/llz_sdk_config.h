#ifndef LLZ_SDK_CONFIG_H
#define LLZ_SDK_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Screen rotation values (degrees clockwise)
 */
typedef enum {
    LLZ_ROTATION_0 = 0,
    LLZ_ROTATION_90 = 90,
    LLZ_ROTATION_180 = 180,
    LLZ_ROTATION_270 = 270
} LlzRotation;

/**
 * Special brightness value for automatic (sensor-based) brightness
 */
#define LLZ_BRIGHTNESS_AUTO (-1)

/**
 * Special value for startup_plugin meaning no specific plugin (show menu).
 */
#define LLZ_STARTUP_MENU ""

/**
 * Maximum length for startup plugin name.
 */
#define LLZ_STARTUP_PLUGIN_MAX_LEN 64

/**
 * Menu navigation style values
 */
typedef enum {
    LLZ_MENU_STYLE_LIST = 0,      // Classic vertical list
    LLZ_MENU_STYLE_CAROUSEL,      // Horizontal carousel
    LLZ_MENU_STYLE_CARDS,         // Large single card
    LLZ_MENU_STYLE_CARTHING,      // Spotify CarThing minimal style
    LLZ_MENU_STYLE_GRID,          // Apple Music grid
    LLZ_MENU_STYLE_COUNT
} LlzMenuStyle;

/**
 * Global configuration structure accessible to all plugins.
 * Changes are automatically persisted to the config file.
 */
typedef struct {
    int brightness;                              // 0-100 percent, or LLZ_BRIGHTNESS_AUTO for auto mode
    LlzRotation rotation;                        // Screen rotation
    char startup_plugin[LLZ_STARTUP_PLUGIN_MAX_LEN];  // Plugin to launch on boot (empty = show menu)
    LlzMenuStyle menu_style;                     // Menu navigation style
} LlzConfig;

/**
 * Default config file path.
 * On CarThing: /var/llizard/config.ini
 * On Desktop: ./llizard_config.ini (in current directory)
 */
#define LLZ_CONFIG_PATH_CARTHING "/var/llizard/config.ini"
#define LLZ_CONFIG_PATH_DESKTOP  "./llizard_config.ini"

/**
 * Initialize the config system. Loads config from file or creates defaults.
 * Called automatically by the host application.
 * @return true on success
 */
bool LlzConfigInit(void);

/**
 * Shutdown the config system. Saves any pending changes.
 */
void LlzConfigShutdown(void);

/**
 * Get a read-only pointer to the current configuration.
 * @return Pointer to config (never NULL after init)
 */
const LlzConfig *LlzConfigGet(void);

/**
 * Get the current brightness setting.
 * @return Brightness value 0-100
 */
int LlzConfigGetBrightness(void);

/**
 * Set the brightness and save to config file.
 * On CarThing: Stops the auto_brightness service when setting manual values.
 * @param brightness Value 0-100 (clamped), or LLZ_BRIGHTNESS_AUTO for auto mode
 * @return true if saved successfully
 */
bool LlzConfigSetBrightness(int brightness);

/**
 * Check if brightness is in automatic mode.
 * @return true if auto brightness is enabled
 */
bool LlzConfigIsAutoBrightness(void);

/**
 * Enable automatic brightness control.
 * On CarThing: Uses the ambient light sensor (TMD2772) to adjust brightness.
 * @return true if auto mode was enabled successfully
 */
bool LlzConfigSetAutoBrightness(void);

/**
 * Read the current ambient light level from the light sensor.
 * On CarThing: Returns lux value from TMD2772 sensor.
 * On Desktop: Always returns -1 (not available).
 * @return Light level in lux, or -1 if not available
 */
int LlzConfigReadAmbientLight(void);

/**
 * Toggle brightness between off (0) and the previous brightness level.
 * Used for quick screen on/off toggle via hardware button.
 * This bypasses the minimum brightness limit for settings UI.
 * @return true if screen is now on, false if screen is now off
 */
bool LlzConfigToggleBrightness(void);

/**
 * Get the current screen rotation.
 * @return Rotation value
 */
LlzRotation LlzConfigGetRotation(void);

/**
 * Set the screen rotation and save to config file.
 * @param rotation Rotation value (must be 0, 90, 180, or 270)
 * @return true if saved successfully
 */
bool LlzConfigSetRotation(LlzRotation rotation);

/**
 * Get the startup plugin name.
 * @return Plugin name, or empty string "" if set to show menu
 */
const char *LlzConfigGetStartupPlugin(void);

/**
 * Set the startup plugin and save to config file.
 * Pass NULL or empty string to set to menu (no startup plugin).
 * @param pluginName Name of the plugin to launch on boot
 * @return true if saved successfully
 */
bool LlzConfigSetStartupPlugin(const char *pluginName);

/**
 * Check if a startup plugin is configured.
 * @return true if a startup plugin is set (not menu)
 */
bool LlzConfigHasStartupPlugin(void);

/**
 * Get the current menu navigation style.
 * @return Menu style value
 */
LlzMenuStyle LlzConfigGetMenuStyle(void);

/**
 * Set the menu navigation style and save to config file.
 * @param style Menu style value (clamped to valid range)
 * @return true if saved successfully
 */
bool LlzConfigSetMenuStyle(LlzMenuStyle style);

/**
 * Force reload configuration from file.
 * @return true if loaded successfully
 */
bool LlzConfigReload(void);

/**
 * Force save current configuration to file.
 * @return true if saved successfully
 */
bool LlzConfigSave(void);

/**
 * Apply brightness to the system (platform-specific).
 * On CarThing: writes to /sys/class/backlight/...
 * On Desktop: no-op (just stores the value)
 */
void LlzConfigApplyBrightness(void);

/**
 * Get the path to the global config file.
 * @return Path string (do not free)
 */
const char *LlzConfigGetPath(void);

/**
 * Get the config directory path.
 * On CarThing: /var/llizard/
 * On Desktop: ./
 * @return Directory path string (do not free)
 */
const char *LlzConfigGetDirectory(void);

// ============================================================================
// Plugin Configuration System
// ============================================================================

/**
 * Maximum number of entries in a plugin config.
 */
#define LLZ_PLUGIN_CONFIG_MAX_ENTRIES 64

/**
 * A single key-value entry in a plugin config.
 */
typedef struct {
    char key[64];
    char value[256];
} LlzPluginConfigEntry;

/**
 * Plugin configuration handle.
 * Each plugin can have its own config file with custom settings.
 */
typedef struct {
    char pluginName[64];
    char filePath[512];
    LlzPluginConfigEntry entries[LLZ_PLUGIN_CONFIG_MAX_ENTRIES];
    int entryCount;
    bool modified;
    bool initialized;
} LlzPluginConfig;

/**
 * Initialize a plugin configuration.
 * If the config file doesn't exist, it will be created with the provided defaults.
 *
 * @param config Pointer to plugin config struct (caller allocates)
 * @param pluginName Name of the plugin (used for filename: <name>_config.ini)
 * @param defaults Array of default entries to use if config doesn't exist (can be NULL)
 * @param defaultCount Number of default entries
 * @return true if initialized successfully
 *
 * Example:
 *   LlzPluginConfig myConfig;
 *   LlzPluginConfigEntry defaults[] = {
 *       {"theme", "dark"},
 *       {"volume", "80"},
 *   };
 *   LlzPluginConfigInit(&myConfig, "myplugin", defaults, 2);
 */
bool LlzPluginConfigInit(LlzPluginConfig *config, const char *pluginName,
                         const LlzPluginConfigEntry *defaults, int defaultCount);

/**
 * Free plugin config resources.
 * Saves any pending changes before cleanup.
 *
 * @param config Plugin config to free
 */
void LlzPluginConfigFree(LlzPluginConfig *config);

/**
 * Get a string value from plugin config.
 *
 * @param config Plugin config handle
 * @param key Key to look up
 * @return Value string, or NULL if key not found
 */
const char *LlzPluginConfigGetString(LlzPluginConfig *config, const char *key);

/**
 * Get an integer value from plugin config.
 *
 * @param config Plugin config handle
 * @param key Key to look up
 * @param defaultValue Value to return if key not found or invalid
 * @return Integer value
 */
int LlzPluginConfigGetInt(LlzPluginConfig *config, const char *key, int defaultValue);

/**
 * Get a float value from plugin config.
 *
 * @param config Plugin config handle
 * @param key Key to look up
 * @param defaultValue Value to return if key not found or invalid
 * @return Float value
 */
float LlzPluginConfigGetFloat(LlzPluginConfig *config, const char *key, float defaultValue);

/**
 * Get a boolean value from plugin config.
 * Recognizes: "true", "false", "1", "0", "yes", "no"
 *
 * @param config Plugin config handle
 * @param key Key to look up
 * @param defaultValue Value to return if key not found or invalid
 * @return Boolean value
 */
bool LlzPluginConfigGetBool(LlzPluginConfig *config, const char *key, bool defaultValue);

/**
 * Set a string value in plugin config.
 * Creates the key if it doesn't exist.
 *
 * @param config Plugin config handle
 * @param key Key to set
 * @param value Value to set
 * @return true if set successfully
 */
bool LlzPluginConfigSetString(LlzPluginConfig *config, const char *key, const char *value);

/**
 * Set an integer value in plugin config.
 *
 * @param config Plugin config handle
 * @param key Key to set
 * @param value Value to set
 * @return true if set successfully
 */
bool LlzPluginConfigSetInt(LlzPluginConfig *config, const char *key, int value);

/**
 * Set a float value in plugin config.
 *
 * @param config Plugin config handle
 * @param key Key to set
 * @param value Value to set
 * @return true if set successfully
 */
bool LlzPluginConfigSetFloat(LlzPluginConfig *config, const char *key, float value);

/**
 * Set a boolean value in plugin config.
 *
 * @param config Plugin config handle
 * @param key Key to set
 * @param value Value to set
 * @return true if set successfully
 */
bool LlzPluginConfigSetBool(LlzPluginConfig *config, const char *key, bool value);

/**
 * Save plugin config to file.
 * Called automatically on LlzPluginConfigFree, but can be called manually.
 *
 * @param config Plugin config handle
 * @return true if saved successfully
 */
bool LlzPluginConfigSave(LlzPluginConfig *config);

/**
 * Reload plugin config from file.
 * Discards any unsaved changes.
 *
 * @param config Plugin config handle
 * @return true if reloaded successfully
 */
bool LlzPluginConfigReload(LlzPluginConfig *config);

/**
 * Check if a key exists in the plugin config.
 *
 * @param config Plugin config handle
 * @param key Key to check
 * @return true if key exists
 */
bool LlzPluginConfigHasKey(LlzPluginConfig *config, const char *key);

#ifdef __cplusplus
}
#endif

#endif
