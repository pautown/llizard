#include "plugin_loader.h"

#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ComparePlugins(const void *a, const void *b)
{
    const LoadedPlugin *pa = (const LoadedPlugin *)a;
    const LoadedPlugin *pb = (const LoadedPlugin *)b;
    return strcasecmp(pa->displayName, pb->displayName);
}

static bool EndsWithSharedObject(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && strcmp(dot, ".so") == 0;
}

bool LoadPlugins(const char *directory, PluginRegistry *registry)
{
    if (!registry) return false;
    registry->items = NULL;
    registry->count = 0;

    DIR *dir = opendir(directory);
    if (!dir) {
        return false;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (!EndsWithSharedObject(entry->d_name)) continue;

        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, entry->d_name);

        void *handle = dlopen(fullPath, RTLD_NOW);
        if (!handle) {
            fprintf(stderr, "Failed to load plugin %s: %s\n", fullPath, dlerror());
            continue;
        }

        LlzGetPluginFunc getter = (LlzGetPluginFunc)dlsym(handle, "LlzGetPlugin");
        if (!getter) {
            fprintf(stderr, "Plugin %s missing LlzGetPlugin symbol\n", fullPath);
            dlclose(handle);
            continue;
        }

        const LlzPluginAPI *api = getter();
        if (!api || !api->name || !api->draw || !api->update) {
            fprintf(stderr, "Plugin %s returned invalid API\n", fullPath);
            dlclose(handle);
            continue;
        }

        LoadedPlugin *resized = realloc(registry->items, sizeof(LoadedPlugin) * (registry->count + 1));
        if (!resized) {
            dlclose(handle);
            break;
        }
        registry->items = resized;

        LoadedPlugin *slot = &registry->items[registry->count];
        memset(slot, 0, sizeof(*slot));
        slot->handle = handle;
        slot->api = api;
        strncpy(slot->path, fullPath, sizeof(slot->path) - 1);
        strncpy(slot->displayName, api->name, sizeof(slot->displayName) - 1);
        registry->count++;
    }

    closedir(dir);

    if (registry->count > 1) {
        qsort(registry->items, registry->count, sizeof(LoadedPlugin), ComparePlugins);
    }

    return registry->count > 0;
}

void UnloadPlugins(PluginRegistry *registry)
{
    if (!registry || !registry->items) return;

    for (int i = 0; i < registry->count; ++i) {
        if (registry->items[i].handle) {
            dlclose(registry->items[i].handle);
        }
    }

    free(registry->items);
    registry->items = NULL;
    registry->count = 0;
}
