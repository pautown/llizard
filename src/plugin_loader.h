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

bool LoadPlugins(const char *directory, PluginRegistry *registry);
void UnloadPlugins(PluginRegistry *registry);

#endif
