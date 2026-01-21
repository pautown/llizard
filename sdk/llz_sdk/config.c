#include "llz_sdk_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// Default configuration values
#define DEFAULT_BRIGHTNESS 80
#define DEFAULT_ROTATION LLZ_ROTATION_0

// Global config state
static LlzConfig g_config = {
    .brightness = DEFAULT_BRIGHTNESS,
    .rotation = DEFAULT_ROTATION,
    .startup_plugin = ""
};
static bool g_initialized = false;
static char g_configPath[512] = {0};
static int g_brightnessBeforeOff = DEFAULT_BRIGHTNESS;  // Stored brightness for toggle
static bool g_screenOff = false;  // Track if screen is "off" via toggle

// Platform-specific backlight path for CarThing (Amlogic backlight controller)
#ifdef PLATFORM_DRM
#define BACKLIGHT_PATH "/sys/class/backlight/aml-bl/brightness"
#define BACKLIGHT_MAX_PATH "/sys/class/backlight/aml-bl/max_brightness"
#define LIGHT_SENSOR_PATH "/sys/bus/iio/devices/iio:device0/in_illuminance0_input"
#define AUTO_BRIGHTNESS_SERVICE "auto_brightness"
#endif

static const char *GetConfigPath(void) {
    if (g_configPath[0] != '\0') {
        return g_configPath;
    }

#ifdef PLATFORM_DRM
    strncpy(g_configPath, LLZ_CONFIG_PATH_CARTHING, sizeof(g_configPath) - 1);
#else
    strncpy(g_configPath, LLZ_CONFIG_PATH_DESKTOP, sizeof(g_configPath) - 1);
#endif
    return g_configPath;
}

static bool EnsureConfigDirectory(void) {
#ifdef PLATFORM_DRM
    // Create /var/llizard if it doesn't exist
    struct stat st = {0};
    if (stat("/var/llizard", &st) == -1) {
        if (mkdir("/var/llizard", 0755) != 0) {
            printf("[CONFIG] Failed to create /var/llizard: %s\n", strerror(errno));
            return false;
        }
        printf("[CONFIG] Created /var/llizard directory\n");
    }
#endif
    return true;
}

static bool ParseLine(const char *line, char *key, size_t keySize, char *value, size_t valueSize) {
    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == ';' || line[0] == '\n' || line[0] == '\0') {
        return false;
    }

    const char *eq = strchr(line, '=');
    if (!eq) return false;

    // Extract key (trim whitespace)
    size_t keyLen = eq - line;
    while (keyLen > 0 && (line[keyLen - 1] == ' ' || line[keyLen - 1] == '\t')) keyLen--;
    if (keyLen == 0 || keyLen >= keySize) return false;

    const char *keyStart = line;
    while (*keyStart == ' ' || *keyStart == '\t') { keyStart++; keyLen--; }

    strncpy(key, keyStart, keyLen);
    key[keyLen] = '\0';

    // Extract value (trim whitespace and newline)
    const char *valStart = eq + 1;
    while (*valStart == ' ' || *valStart == '\t') valStart++;

    size_t valLen = strlen(valStart);
    while (valLen > 0 && (valStart[valLen - 1] == '\n' || valStart[valLen - 1] == '\r' ||
                          valStart[valLen - 1] == ' ' || valStart[valLen - 1] == '\t')) {
        valLen--;
    }
    if (valLen >= valueSize) valLen = valueSize - 1;

    strncpy(value, valStart, valLen);
    value[valLen] = '\0';

    return true;
}

static bool LoadConfig(void) {
    const char *path = GetConfigPath();
    FILE *file = fopen(path, "r");
    if (!file) {
        printf("[CONFIG] Config file not found at %s, using defaults\n", path);
        return false;
    }

    char line[256];
    char key[64], value[128];

    while (fgets(line, sizeof(line), file)) {
        if (!ParseLine(line, key, sizeof(key), value, sizeof(value))) {
            continue;
        }

        if (strcmp(key, "brightness") == 0) {
            if (strcmp(value, "auto") == 0) {
                g_config.brightness = LLZ_BRIGHTNESS_AUTO;
                printf("[CONFIG] Loaded brightness=AUTO\n");
            } else {
                int val = atoi(value);
                if (val < 0) val = 0;
                if (val > 100) val = 100;
                g_config.brightness = val;
                printf("[CONFIG] Loaded brightness=%d\n", g_config.brightness);
            }
        } else if (strcmp(key, "rotation") == 0) {
            int val = atoi(value);
            if (val == 0 || val == 90 || val == 180 || val == 270) {
                g_config.rotation = (LlzRotation)val;
                printf("[CONFIG] Loaded rotation=%d\n", g_config.rotation);
            }
        } else if (strcmp(key, "startup_plugin") == 0) {
            strncpy(g_config.startup_plugin, value, LLZ_STARTUP_PLUGIN_MAX_LEN - 1);
            g_config.startup_plugin[LLZ_STARTUP_PLUGIN_MAX_LEN - 1] = '\0';
            printf("[CONFIG] Loaded startup_plugin=%s\n",
                   g_config.startup_plugin[0] ? g_config.startup_plugin : "(menu)");
        }
    }

    fclose(file);
    printf("[CONFIG] Configuration loaded from %s\n", path);
    return true;
}

static bool SaveConfig(void) {
    if (!EnsureConfigDirectory()) {
        return false;
    }

    const char *path = GetConfigPath();
    FILE *file = fopen(path, "w");
    if (!file) {
        printf("[CONFIG] Failed to open %s for writing: %s\n", path, strerror(errno));
        return false;
    }

    fprintf(file, "# llizard configuration\n");
    fprintf(file, "# Auto-generated - do not edit while app is running\n\n");
    if (g_config.brightness == LLZ_BRIGHTNESS_AUTO) {
        fprintf(file, "brightness=auto\n");
    } else {
        fprintf(file, "brightness=%d\n", g_config.brightness);
    }
    fprintf(file, "rotation=%d\n", g_config.rotation);
    fprintf(file, "startup_plugin=%s\n", g_config.startup_plugin);

    fclose(file);
    printf("[CONFIG] Configuration saved to %s\n", path);
    return true;
}

bool LlzConfigInit(void) {
    if (g_initialized) {
        return true;
    }

    // Set defaults
    g_config.brightness = DEFAULT_BRIGHTNESS;
    g_config.rotation = DEFAULT_ROTATION;
    g_config.startup_plugin[0] = '\0';

    // Try to load from file
    LoadConfig();

    g_initialized = true;
    printf("[CONFIG] Config system initialized (brightness=%d, rotation=%d, startup=%s)\n",
           g_config.brightness, g_config.rotation,
           g_config.startup_plugin[0] ? g_config.startup_plugin : "menu");

    // Apply brightness on startup
    LlzConfigApplyBrightness();

    return true;
}

void LlzConfigShutdown(void) {
    if (!g_initialized) return;

    SaveConfig();
    g_initialized = false;
    printf("[CONFIG] Config system shutdown\n");
}

const LlzConfig *LlzConfigGet(void) {
    return &g_config;
}

int LlzConfigGetBrightness(void) {
    return g_config.brightness;
}

// Helper to control the auto_brightness service
#ifdef PLATFORM_DRM
static bool ControlAutoBrightnessService(bool start) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "sv %s %s >/dev/null 2>&1",
             start ? "start" : "stop", AUTO_BRIGHTNESS_SERVICE);
    int ret = system(cmd);
    printf("[CONFIG] %s auto_brightness service: %s\n",
           start ? "Starting" : "Stopping",
           ret == 0 ? "success" : "failed");
    return ret == 0;
}
#endif

bool LlzConfigSetBrightness(int brightness) {
    // Handle auto brightness mode
    if (brightness == LLZ_BRIGHTNESS_AUTO) {
        return LlzConfigSetAutoBrightness();
    }

    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;

    if (g_config.brightness == brightness) {
        return true;  // No change needed
    }

#ifdef PLATFORM_DRM
    // Stop auto_brightness service when switching to manual mode
    if (g_config.brightness == LLZ_BRIGHTNESS_AUTO) {
        ControlAutoBrightnessService(false);
    }
#endif

    g_config.brightness = brightness;
    printf("[CONFIG] Brightness set to %d%%\n", brightness);

    LlzConfigApplyBrightness();
    return SaveConfig();
}

bool LlzConfigIsAutoBrightness(void) {
    return g_config.brightness == LLZ_BRIGHTNESS_AUTO;
}

bool LlzConfigSetAutoBrightness(void) {
    if (g_config.brightness == LLZ_BRIGHTNESS_AUTO) {
        return true;  // Already in auto mode
    }

#ifdef PLATFORM_DRM
    // Start the auto_brightness service
    if (!ControlAutoBrightnessService(true)) {
        printf("[CONFIG] Failed to start auto_brightness service\n");
        return false;
    }
#endif

    g_config.brightness = LLZ_BRIGHTNESS_AUTO;
    printf("[CONFIG] Brightness set to AUTO\n");
    return SaveConfig();
}

int LlzConfigReadAmbientLight(void) {
#ifdef PLATFORM_DRM
    FILE *file = fopen(LIGHT_SENSOR_PATH, "r");
    if (!file) {
        return -1;
    }
    int lux = -1;
    if (fscanf(file, "%d", &lux) != 1) {
        lux = -1;
    }
    fclose(file);
    return lux;
#else
    return -1;  // Not available on desktop
#endif
}

bool LlzConfigToggleBrightness(void) {
    if (g_screenOff) {
        // Screen is off, turn it back on to the saved brightness
        g_screenOff = false;
        printf("[CONFIG] Screen ON - restoring brightness to %d%%\n", g_brightnessBeforeOff);

        // Restore the saved brightness (or auto mode)
        if (g_brightnessBeforeOff == LLZ_BRIGHTNESS_AUTO) {
            LlzConfigSetAutoBrightness();
        } else {
            g_config.brightness = g_brightnessBeforeOff;
            LlzConfigApplyBrightness();
        }
        return true;  // Screen is now on
    } else {
        // Screen is on, turn it off (set brightness to 0)
        g_screenOff = true;

        // Save current brightness before turning off
        g_brightnessBeforeOff = g_config.brightness;
        printf("[CONFIG] Screen OFF - saved brightness %d%%\n", g_brightnessBeforeOff);

#ifdef PLATFORM_DRM
        // Stop auto_brightness service if running
        ControlAutoBrightnessService(false);

        // Write max value to backlight (inverted: 255 = darkest/off)
        FILE *blFile = fopen(BACKLIGHT_PATH, "w");
        if (blFile) {
            fprintf(blFile, "255");
            fclose(blFile);
            printf("[CONFIG] Backlight set to 255 (screen off, inverted)\n");
        }
#else
        printf("[CONFIG] Screen OFF (desktop - no hardware control)\n");
#endif
        return false;  // Screen is now off
    }
}

LlzRotation LlzConfigGetRotation(void) {
    return g_config.rotation;
}

bool LlzConfigSetRotation(LlzRotation rotation) {
    // Validate rotation value
    if (rotation != LLZ_ROTATION_0 && rotation != LLZ_ROTATION_90 &&
        rotation != LLZ_ROTATION_180 && rotation != LLZ_ROTATION_270) {
        printf("[CONFIG] Invalid rotation value: %d\n", rotation);
        return false;
    }

    if (g_config.rotation == rotation) {
        return true;  // No change needed
    }

    g_config.rotation = rotation;
    printf("[CONFIG] Rotation set to %d\n", rotation);

    return SaveConfig();
}

const char *LlzConfigGetStartupPlugin(void) {
    return g_config.startup_plugin;
}

bool LlzConfigSetStartupPlugin(const char *pluginName) {
    const char *newValue = (pluginName && pluginName[0]) ? pluginName : "";

    // Check if value changed
    if (strcmp(g_config.startup_plugin, newValue) == 0) {
        return true;  // No change needed
    }

    // Update the value
    if (newValue[0]) {
        strncpy(g_config.startup_plugin, newValue, LLZ_STARTUP_PLUGIN_MAX_LEN - 1);
        g_config.startup_plugin[LLZ_STARTUP_PLUGIN_MAX_LEN - 1] = '\0';
    } else {
        g_config.startup_plugin[0] = '\0';
    }

    printf("[CONFIG] Startup plugin set to: %s\n",
           g_config.startup_plugin[0] ? g_config.startup_plugin : "(menu)");

    return SaveConfig();
}

bool LlzConfigHasStartupPlugin(void) {
    return g_config.startup_plugin[0] != '\0';
}

bool LlzConfigReload(void) {
    return LoadConfig();
}

bool LlzConfigSave(void) {
    return SaveConfig();
}

void LlzConfigApplyBrightness(void) {
#ifdef PLATFORM_DRM
    // In auto mode, don't manually write - the service handles it
    if (g_config.brightness == LLZ_BRIGHTNESS_AUTO) {
        printf("[CONFIG] Brightness in AUTO mode - service controls backlight\n");
        return;
    }

    // First, make sure auto_brightness service is stopped
    ControlAutoBrightnessService(false);

    // Read max brightness
    FILE *maxFile = fopen(BACKLIGHT_MAX_PATH, "r");
    int maxBrightness = 255;  // Default fallback
    if (maxFile) {
        if (fscanf(maxFile, "%d", &maxBrightness) != 1) {
            maxBrightness = 255;
        }
        fclose(maxFile);
    }

    // Calculate actual brightness value
    // NOTE: Amlogic backlight is INVERTED: 0 = brightest, 255 = darkest
    // Map 100% -> 1 (brightest usable), 0% -> 255 (darkest)
    int actualBrightness = maxBrightness - ((g_config.brightness * (maxBrightness - 1)) / 100);
    if (actualBrightness < 1) actualBrightness = 1;  // 100% = hardware 1 (brightest)
    if (actualBrightness > maxBrightness - 1) actualBrightness = maxBrightness - 1;  // Never fully off

    // Write to backlight
    FILE *blFile = fopen(BACKLIGHT_PATH, "w");
    if (blFile) {
        fprintf(blFile, "%d", actualBrightness);
        fclose(blFile);
        printf("[CONFIG] Applied brightness: %d%% -> %d/%d (inverted)\n",
               g_config.brightness, actualBrightness, maxBrightness);
    } else {
        printf("[CONFIG] Failed to write to %s: %s\n", BACKLIGHT_PATH, strerror(errno));
    }
#else
    // Desktop: just log, no actual brightness control
    if (g_config.brightness == LLZ_BRIGHTNESS_AUTO) {
        printf("[CONFIG] Brightness set to AUTO (desktop - no hardware control)\n");
    } else {
        printf("[CONFIG] Brightness set to %d%% (desktop - no hardware control)\n", g_config.brightness);
    }
#endif
}

const char *LlzConfigGetPath(void) {
    return GetConfigPath();
}

static char g_configDir[512] = {0};

const char *LlzConfigGetDirectory(void) {
    if (g_configDir[0] != '\0') {
        return g_configDir;
    }

#ifdef PLATFORM_DRM
    strncpy(g_configDir, "/var/llizard/", sizeof(g_configDir) - 1);
#else
    strncpy(g_configDir, "./", sizeof(g_configDir) - 1);
#endif
    return g_configDir;
}

// ============================================================================
// Plugin Configuration System Implementation
// ============================================================================

static bool PluginConfigParseLine(const char *line, char *key, size_t keySize, char *value, size_t valueSize) {
    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == ';' || line[0] == '\n' || line[0] == '\0') {
        return false;
    }

    const char *eq = strchr(line, '=');
    if (!eq) return false;

    // Extract key (trim whitespace)
    size_t keyLen = eq - line;
    while (keyLen > 0 && (line[keyLen - 1] == ' ' || line[keyLen - 1] == '\t')) keyLen--;
    if (keyLen == 0 || keyLen >= keySize) return false;

    const char *keyStart = line;
    while (*keyStart == ' ' || *keyStart == '\t') { keyStart++; keyLen--; }

    strncpy(key, keyStart, keyLen);
    key[keyLen] = '\0';

    // Extract value (trim whitespace and newline)
    const char *valStart = eq + 1;
    while (*valStart == ' ' || *valStart == '\t') valStart++;

    size_t valLen = strlen(valStart);
    while (valLen > 0 && (valStart[valLen - 1] == '\n' || valStart[valLen - 1] == '\r' ||
                          valStart[valLen - 1] == ' ' || valStart[valLen - 1] == '\t')) {
        valLen--;
    }
    if (valLen >= valueSize) valLen = valueSize - 1;

    strncpy(value, valStart, valLen);
    value[valLen] = '\0';

    return true;
}

static bool PluginConfigLoad(LlzPluginConfig *config) {
    FILE *file = fopen(config->filePath, "r");
    if (!file) {
        return false;
    }

    config->entryCount = 0;
    char line[512];
    char key[64], value[256];

    while (fgets(line, sizeof(line), file) && config->entryCount < LLZ_PLUGIN_CONFIG_MAX_ENTRIES) {
        if (PluginConfigParseLine(line, key, sizeof(key), value, sizeof(value))) {
            strncpy(config->entries[config->entryCount].key, key, sizeof(config->entries[0].key) - 1);
            strncpy(config->entries[config->entryCount].value, value, sizeof(config->entries[0].value) - 1);
            config->entryCount++;
        }
    }

    fclose(file);
    printf("[PLUGIN_CONFIG] Loaded %d entries from %s\n", config->entryCount, config->filePath);
    return true;
}

static bool PluginConfigSaveInternal(LlzPluginConfig *config) {
    if (!EnsureConfigDirectory()) {
        return false;
    }

    FILE *file = fopen(config->filePath, "w");
    if (!file) {
        printf("[PLUGIN_CONFIG] Failed to open %s for writing: %s\n", config->filePath, strerror(errno));
        return false;
    }

    fprintf(file, "# %s plugin configuration\n", config->pluginName);
    fprintf(file, "# Auto-generated - edit with care\n\n");

    for (int i = 0; i < config->entryCount; i++) {
        fprintf(file, "%s=%s\n", config->entries[i].key, config->entries[i].value);
    }

    fclose(file);
    config->modified = false;
    printf("[PLUGIN_CONFIG] Saved %d entries to %s\n", config->entryCount, config->filePath);
    return true;
}

bool LlzPluginConfigInit(LlzPluginConfig *config, const char *pluginName,
                         const LlzPluginConfigEntry *defaults, int defaultCount) {
    if (!config || !pluginName) {
        return false;
    }

    memset(config, 0, sizeof(LlzPluginConfig));
    strncpy(config->pluginName, pluginName, sizeof(config->pluginName) - 1);

    // Build file path
    const char *dir = LlzConfigGetDirectory();
    snprintf(config->filePath, sizeof(config->filePath), "%s%s_config.ini", dir, pluginName);

    // Try to load existing config
    if (PluginConfigLoad(config)) {
        config->initialized = true;
        return true;
    }

    // Config doesn't exist - create with defaults
    printf("[PLUGIN_CONFIG] Creating new config for %s with %d defaults\n", pluginName, defaultCount);

    if (defaults && defaultCount > 0) {
        int count = defaultCount;
        if (count > LLZ_PLUGIN_CONFIG_MAX_ENTRIES) {
            count = LLZ_PLUGIN_CONFIG_MAX_ENTRIES;
        }

        for (int i = 0; i < count; i++) {
            strncpy(config->entries[i].key, defaults[i].key, sizeof(config->entries[0].key) - 1);
            strncpy(config->entries[i].value, defaults[i].value, sizeof(config->entries[0].value) - 1);
        }
        config->entryCount = count;
    }

    // Save the new config
    config->modified = true;
    PluginConfigSaveInternal(config);

    config->initialized = true;
    return true;
}

void LlzPluginConfigFree(LlzPluginConfig *config) {
    if (!config || !config->initialized) {
        return;
    }

    if (config->modified) {
        PluginConfigSaveInternal(config);
    }

    memset(config, 0, sizeof(LlzPluginConfig));
}

static LlzPluginConfigEntry *FindEntry(LlzPluginConfig *config, const char *key) {
    if (!config || !key) return NULL;

    for (int i = 0; i < config->entryCount; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            return &config->entries[i];
        }
    }
    return NULL;
}

const char *LlzPluginConfigGetString(LlzPluginConfig *config, const char *key) {
    LlzPluginConfigEntry *entry = FindEntry(config, key);
    return entry ? entry->value : NULL;
}

int LlzPluginConfigGetInt(LlzPluginConfig *config, const char *key, int defaultValue) {
    const char *str = LlzPluginConfigGetString(config, key);
    if (!str) return defaultValue;
    return atoi(str);
}

float LlzPluginConfigGetFloat(LlzPluginConfig *config, const char *key, float defaultValue) {
    const char *str = LlzPluginConfigGetString(config, key);
    if (!str) return defaultValue;
    return (float)atof(str);
}

bool LlzPluginConfigGetBool(LlzPluginConfig *config, const char *key, bool defaultValue) {
    const char *str = LlzPluginConfigGetString(config, key);
    if (!str) return defaultValue;

    if (strcmp(str, "true") == 0 || strcmp(str, "1") == 0 || strcmp(str, "yes") == 0) {
        return true;
    }
    if (strcmp(str, "false") == 0 || strcmp(str, "0") == 0 || strcmp(str, "no") == 0) {
        return false;
    }
    return defaultValue;
}

bool LlzPluginConfigSetString(LlzPluginConfig *config, const char *key, const char *value) {
    if (!config || !key || !value) return false;

    LlzPluginConfigEntry *entry = FindEntry(config, key);
    if (entry) {
        strncpy(entry->value, value, sizeof(entry->value) - 1);
        entry->value[sizeof(entry->value) - 1] = '\0';
        config->modified = true;
        return true;
    }

    // Add new entry
    if (config->entryCount >= LLZ_PLUGIN_CONFIG_MAX_ENTRIES) {
        printf("[PLUGIN_CONFIG] Max entries reached, cannot add key: %s\n", key);
        return false;
    }

    LlzPluginConfigEntry *newEntry = &config->entries[config->entryCount];
    strncpy(newEntry->key, key, sizeof(newEntry->key) - 1);
    newEntry->key[sizeof(newEntry->key) - 1] = '\0';
    strncpy(newEntry->value, value, sizeof(newEntry->value) - 1);
    newEntry->value[sizeof(newEntry->value) - 1] = '\0';
    config->entryCount++;
    config->modified = true;

    return true;
}

bool LlzPluginConfigSetInt(LlzPluginConfig *config, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return LlzPluginConfigSetString(config, key, buf);
}

bool LlzPluginConfigSetFloat(LlzPluginConfig *config, const char *key, float value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6f", value);
    return LlzPluginConfigSetString(config, key, buf);
}

bool LlzPluginConfigSetBool(LlzPluginConfig *config, const char *key, bool value) {
    return LlzPluginConfigSetString(config, key, value ? "true" : "false");
}

bool LlzPluginConfigSave(LlzPluginConfig *config) {
    if (!config || !config->initialized) return false;
    return PluginConfigSaveInternal(config);
}

bool LlzPluginConfigReload(LlzPluginConfig *config) {
    if (!config || !config->initialized) return false;
    return PluginConfigLoad(config);
}

bool LlzPluginConfigHasKey(LlzPluginConfig *config, const char *key) {
    return FindEntry(config, key) != NULL;
}
