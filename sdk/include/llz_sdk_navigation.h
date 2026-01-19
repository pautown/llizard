#ifndef LLZ_SDK_NAVIGATION_H
#define LLZ_SDK_NAVIGATION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLZ_PLUGIN_NAME_MAX 128

// Request that the host open a different plugin after this plugin closes.
// The plugin name should match the display name shown in the menu.
// Only one request can be pending at a time (later calls overwrite earlier ones).
// Returns true if the request was stored successfully.
bool LlzRequestOpenPlugin(const char *pluginName);

// Get the currently requested plugin name.
// Returns NULL if no plugin has been requested.
// The returned pointer is valid until the next LlzRequestOpenPlugin or LlzClearRequestedPlugin call.
const char *LlzGetRequestedPlugin(void);

// Clear any pending plugin open request.
// The host should call this after handling the request.
void LlzClearRequestedPlugin(void);

// Check if a plugin open request is pending.
bool LlzHasRequestedPlugin(void);

#ifdef __cplusplus
}
#endif

#endif // LLZ_SDK_NAVIGATION_H
