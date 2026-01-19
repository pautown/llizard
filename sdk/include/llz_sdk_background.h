#ifndef LLZ_SDK_BACKGROUND_H
#define LLZ_SDK_BACKGROUND_H

#include "raylib.h"
#include <stdbool.h>

/**
 * Animated Background System for llizardgui
 *
 * Provides animated background effects that can be used by the host menu
 * and any plugins. Backgrounds automatically adapt to provided colors
 * (e.g., from album art) for a cohesive visual experience.
 *
 * Usage:
 *   LlzBackgroundInit(screenWidth, screenHeight);
 *   LlzBackgroundSetColors(primary, accent);  // Optional: set custom colors
 *
 *   // In update loop:
 *   LlzBackgroundUpdate(deltaTime);
 *   if (buttonPressed) LlzBackgroundCycleNext();
 *
 *   // In draw loop:
 *   LlzBackgroundDraw();
 *   LlzBackgroundDrawIndicator();  // Optional: show style name overlay
 *
 *   LlzBackgroundShutdown();
 */

// Background style types
typedef enum {
    LLZ_BG_STYLE_PULSE = 0,       // Pulse Glow - breathing gradient circles
    LLZ_BG_STYLE_AURORA,          // Aurora Sweep - flowing gradient bands
    LLZ_BG_STYLE_RADIAL,          // Radial Echo - expanding rings
    LLZ_BG_STYLE_WAVE,            // Neon Strands - flowing sine waves
    LLZ_BG_STYLE_GRID,            // Grid Spark - drifting grid with glows
    LLZ_BG_STYLE_BLUR,            // Blurred texture (requires texture set)
    LLZ_BG_STYLE_CONSTELLATION,   // Constellation - connected floating stars
    LLZ_BG_STYLE_LIQUID,          // Liquid Gradient - morphing color blobs
    LLZ_BG_STYLE_BOKEH,           // Bokeh Lights - soft floating circles
    LLZ_BG_STYLE_COUNT
} LlzBackgroundStyle;

// Color palette for backgrounds (6 colors derived from primary/accent)
typedef struct {
    Color colors[6];
} LlzBackgroundPalette;

/**
 * Initialize the background system.
 * Must be called before any other background functions.
 *
 * @param screenWidth  Display width in pixels
 * @param screenHeight Display height in pixels
 */
void LlzBackgroundInit(int screenWidth, int screenHeight);

/**
 * Shutdown the background system and free resources.
 */
void LlzBackgroundShutdown(void);

/**
 * Update background animations.
 * Call once per frame.
 *
 * @param deltaTime Time since last frame in seconds
 */
void LlzBackgroundUpdate(float deltaTime);

/**
 * Draw the current background.
 * Should be called at the start of the draw phase, before other content.
 */
void LlzBackgroundDraw(void);

/**
 * Draw the style indicator overlay (shows style name when cycling).
 * Should be called after main content is drawn.
 */
void LlzBackgroundDrawIndicator(void);

/**
 * Cycle to the next background style.
 * Triggers a smooth transition animation.
 */
void LlzBackgroundCycleNext(void);

/**
 * Set the current background style directly.
 *
 * @param style  The style to switch to
 * @param animate  If true, animate the transition; if false, switch immediately
 */
void LlzBackgroundSetStyle(LlzBackgroundStyle style, bool animate);

/**
 * Get the current background style.
 *
 * @return The currently active style
 */
LlzBackgroundStyle LlzBackgroundGetStyle(void);

/**
 * Check if any background style is active.
 *
 * @return true if a background is enabled, false if disabled
 */
bool LlzBackgroundIsEnabled(void);

/**
 * Enable or disable the background system.
 * When disabled, LlzBackgroundDraw() does nothing.
 *
 * @param enabled  true to enable, false to disable
 */
void LlzBackgroundSetEnabled(bool enabled);

/**
 * Set custom colors for the background palette.
 * The palette is generated from these two base colors.
 * If not called, uses default theme colors.
 *
 * @param primary  Primary/average color (used for base tones)
 * @param accent   Accent color (vibrant, used for highlights)
 */
void LlzBackgroundSetColors(Color primary, Color accent);

/**
 * Clear custom colors and revert to default theme colors.
 */
void LlzBackgroundClearColors(void);

/**
 * Set the blurred texture for BG_STYLE_BLUR.
 * The texture should be pre-blurred album art or similar.
 *
 * @param texture      Current blurred texture (or empty texture if none)
 * @param prevTexture  Previous texture for crossfade (or empty if none)
 * @param currentAlpha Alpha for current texture (0.0-1.0)
 * @param prevAlpha    Alpha for previous texture (0.0-1.0)
 */
void LlzBackgroundSetBlurTexture(Texture2D texture, Texture2D prevTexture,
                                  float currentAlpha, float prevAlpha);

/**
 * Set the "energy" level for responsive backgrounds (like Wave).
 * Can be linked to playback state (1.0 = playing, 0.0 = paused).
 *
 * @param energy  Energy level from 0.0 to 1.0
 */
void LlzBackgroundSetEnergy(float energy);

/**
 * Get the name of a background style.
 *
 * @param style  The style to get the name of
 * @return Human-readable style name
 */
const char* LlzBackgroundGetStyleName(LlzBackgroundStyle style);

/**
 * Get the total number of background styles.
 *
 * @return Number of available styles
 */
int LlzBackgroundGetStyleCount(void);

/**
 * Get the current background palette.
 * Useful for plugins that want to match their UI to the background colors.
 *
 * @return Pointer to the current palette (valid until next SetColors call)
 */
const LlzBackgroundPalette* LlzBackgroundGetPalette(void);

#endif // LLZ_SDK_BACKGROUND_H
