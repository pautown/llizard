#ifndef LLIZARD_PLUGIN_H
#define LLIZARD_PLUGIN_H

#include <stdbool.h>
#include "llz_sdk_input.h"

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
} LlzPluginAPI;

typedef const LlzPluginAPI *(*LlzGetPluginFunc)(void);

#endif
