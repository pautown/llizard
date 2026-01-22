// LLZ Survivors - Plugin Entry Point
// Vampire Survivors/Brotato-lite arena survival game for CarThing

#include "llizard_plugin.h"
#include "llzsurvivors_game.h"

// =============================================================================
// PLUGIN CALLBACKS
// =============================================================================

static void PluginInit(int screenWidth, int screenHeight) {
    GameInit(screenWidth, screenHeight);
}

static void PluginUpdate(const LlzInputState *input, float deltaTime) {
    GameUpdate(input, deltaTime);
}

static void PluginDraw(void) {
    GameDraw();
}

static void PluginShutdown(void) {
    GameShutdown();
}

static bool PluginWantsClose(void) {
    return GameWantsClose();
}

// =============================================================================
// PLUGIN API EXPORT
// =============================================================================

static LlzPluginAPI g_api = {
    .name = "LLZ Survivors",
    .description = "Arena survival - dodge enemies, collect XP, upgrade!",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .handles_back_button = true,
    .category = LLZ_CATEGORY_GAMES
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
