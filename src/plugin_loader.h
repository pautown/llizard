#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include "llizard_plugin.h"
#include <stdbool.h>

typedef struct {
    char displayName[128];
    char path[512];
    void *handle;
    const LlzPluginAPI *api;
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

#endif
