#include "llz_sdk_navigation.h"

#include <string.h>

// Static storage for the requested plugin name
static char g_requestedPlugin[LLZ_PLUGIN_NAME_MAX] = {0};
static bool g_hasRequest = false;

bool LlzRequestOpenPlugin(const char *pluginName)
{
    if (!pluginName || pluginName[0] == '\0') {
        // Clear request if empty name provided
        g_requestedPlugin[0] = '\0';
        g_hasRequest = false;
        return true;
    }

    strncpy(g_requestedPlugin, pluginName, sizeof(g_requestedPlugin) - 1);
    g_requestedPlugin[sizeof(g_requestedPlugin) - 1] = '\0';
    g_hasRequest = true;

    return true;
}

const char *LlzGetRequestedPlugin(void)
{
    if (!g_hasRequest || g_requestedPlugin[0] == '\0') {
        return NULL;
    }
    return g_requestedPlugin;
}

void LlzClearRequestedPlugin(void)
{
    g_requestedPlugin[0] = '\0';
    g_hasRequest = false;
}

bool LlzHasRequestedPlugin(void)
{
    return g_hasRequest && g_requestedPlugin[0] != '\0';
}
