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

PluginDirSnapshot CreatePluginSnapshot(const char *directory)
{
    PluginDirSnapshot snapshot = {NULL, 0};

    DIR *dir = opendir(directory);
    if (!dir) return snapshot;

    // First pass: count .so files
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (EndsWithSharedObject(entry->d_name)) count++;
    }

    if (count == 0) {
        closedir(dir);
        return snapshot;
    }

    // Allocate array
    snapshot.filenames = (char **)malloc(count * sizeof(char *));
    if (!snapshot.filenames) {
        closedir(dir);
        return snapshot;
    }

    // Second pass: store filenames
    rewinddir(dir);
    int idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < count) {
        if (entry->d_name[0] == '.') continue;
        if (!EndsWithSharedObject(entry->d_name)) continue;

        snapshot.filenames[idx] = strdup(entry->d_name);
        if (snapshot.filenames[idx]) idx++;
    }
    snapshot.count = idx;

    closedir(dir);
    return snapshot;
}

bool HasPluginDirectoryChanged(const char *directory, const PluginDirSnapshot *snapshot)
{
    if (!snapshot) return true;

    DIR *dir = opendir(directory);
    if (!dir) return snapshot->count > 0;

    // Count current .so files and check if all snapshot files still exist
    int currentCount = 0;
    bool *found = NULL;
    if (snapshot->count > 0) {
        found = (bool *)calloc(snapshot->count, sizeof(bool));
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (!EndsWithSharedObject(entry->d_name)) continue;

        currentCount++;

        // Check if this file is in the snapshot
        if (found) {
            for (int i = 0; i < snapshot->count; i++) {
                if (strcmp(entry->d_name, snapshot->filenames[i]) == 0) {
                    found[i] = true;
                    break;
                }
            }
        }
    }

    closedir(dir);

    // Different count means changed
    if (currentCount != snapshot->count) {
        free(found);
        return true;
    }

    // Check if all snapshot files were found
    bool allFound = true;
    if (found) {
        for (int i = 0; i < snapshot->count; i++) {
            if (!found[i]) {
                allFound = false;
                break;
            }
        }
        free(found);
    }

    return !allFound;
}

void FreePluginSnapshot(PluginDirSnapshot *snapshot)
{
    if (!snapshot) return;

    if (snapshot->filenames) {
        for (int i = 0; i < snapshot->count; i++) {
            free(snapshot->filenames[i]);
        }
        free(snapshot->filenames);
    }
    snapshot->filenames = NULL;
    snapshot->count = 0;
}

// Helper to extract basename from full path
static const char *GetBasename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// Helper to check if a plugin with given basename is in the registry
static int FindPluginByBasename(const PluginRegistry *registry, const char *basename)
{
    for (int i = 0; i < registry->count; i++) {
        const char *regBasename = GetBasename(registry->items[i].path);
        if (strcmp(regBasename, basename) == 0) {
            return i;
        }
    }
    return -1;
}

int RefreshPlugins(const char *directory, PluginRegistry *registry)
{
    if (!registry) return 0;

    int changes = 0;

    // Create snapshot of current directory
    PluginDirSnapshot current = CreatePluginSnapshot(directory);

    // Mark plugins for removal (those not in current directory)
    bool *keep = NULL;
    if (registry->count > 0) {
        keep = (bool *)calloc(registry->count, sizeof(bool));

        for (int i = 0; i < registry->count; i++) {
            const char *basename = GetBasename(registry->items[i].path);
            bool found = false;
            for (int j = 0; j < current.count; j++) {
                if (strcmp(basename, current.filenames[j]) == 0) {
                    found = true;
                    break;
                }
            }
            keep[i] = found;
            if (!found) changes++;
        }
    }

    // Find new plugins (those not in registry)
    int newCount = 0;
    for (int i = 0; i < current.count; i++) {
        if (FindPluginByBasename(registry, current.filenames[i]) < 0) {
            newCount++;
        }
    }
    changes += newCount;

    // If no changes, we're done
    if (changes == 0) {
        FreePluginSnapshot(&current);
        free(keep);
        return 0;
    }

    // Build new registry
    int newTotalCount = 0;
    for (int i = 0; i < registry->count; i++) {
        if (keep && keep[i]) newTotalCount++;
    }
    newTotalCount += newCount;

    LoadedPlugin *newItems = NULL;
    if (newTotalCount > 0) {
        newItems = (LoadedPlugin *)malloc(sizeof(LoadedPlugin) * newTotalCount);
    }

    int idx = 0;

    // Copy kept plugins
    for (int i = 0; i < registry->count; i++) {
        if (keep && keep[i]) {
            newItems[idx++] = registry->items[i];
        } else if (registry->items[i].handle) {
            // Unload removed plugin
            printf("Plugin removed: %s\n", registry->items[i].displayName);
            dlclose(registry->items[i].handle);
        }
    }

    // Load new plugins
    for (int i = 0; i < current.count; i++) {
        if (FindPluginByBasename(registry, current.filenames[i]) >= 0) continue;

        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, current.filenames[i]);

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

        if (idx < newTotalCount) {
            LoadedPlugin *slot = &newItems[idx];
            memset(slot, 0, sizeof(*slot));
            slot->handle = handle;
            slot->api = api;
            strncpy(slot->path, fullPath, sizeof(slot->path) - 1);
            strncpy(slot->displayName, api->name, sizeof(slot->displayName) - 1);
            idx++;
            printf("Plugin added: %s\n", api->name);
        } else {
            dlclose(handle);
        }
    }

    // Replace registry
    free(registry->items);
    registry->items = newItems;
    registry->count = idx;

    // Sort by name
    if (registry->count > 1) {
        qsort(registry->items, registry->count, sizeof(LoadedPlugin), ComparePlugins);
    }

    FreePluginSnapshot(&current);
    free(keep);

    return changes;
}
