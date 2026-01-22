/**
 * llz_sdk_font.h - Font Loading Module for llizardgui SDK
 *
 * Provides centralized font loading with automatic path resolution
 * for both CarThing device and desktop development environments.
 *
 * Usage:
 *   // Initialize once at plugin startup
 *   LlzFontInit();
 *
 *   // Get the default UI font
 *   Font font = LlzFontGetDefault();
 *
 *   // Load a specific font size
 *   Font largeFont = LlzFontLoad(LLZ_FONT_UI, 32);
 *
 *   // Cleanup at shutdown (optional - fonts auto-cleanup)
 *   LlzFontShutdown();
 */

#ifndef LLZ_SDK_FONT_H
#define LLZ_SDK_FONT_H

#include "raylib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Font Types
// ============================================================================

/**
 * Available font types in the SDK.
 */
typedef enum {
    LLZ_FONT_UI,          /**< Primary UI font (ZegoeUI) - good for menus, labels */
    LLZ_FONT_UI_BOLD,     /**< Bold variant of UI font */
    LLZ_FONT_MONO,        /**< Monospace font for code/technical display */
    LLZ_FONT_DISPLAY,     /**< Display/decorative font (Quincy Caps) - all caps, good for titles */
    LLZ_FONT_COUNT        /**< Number of font types (internal use) */
} LlzFontType;

/**
 * Default font sizes for common use cases.
 */
#define LLZ_FONT_SIZE_SMALL    16
#define LLZ_FONT_SIZE_NORMAL   20
#define LLZ_FONT_SIZE_LARGE    28
#define LLZ_FONT_SIZE_TITLE    36
#define LLZ_FONT_SIZE_HEADING  48

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize the font system.
 * Locates font files and prepares for loading.
 * Called automatically by the host, but safe to call from plugins.
 *
 * @return true if initialization succeeded
 */
bool LlzFontInit(void);

/**
 * Shutdown the font system and free all loaded fonts.
 * Called automatically by the host at shutdown.
 */
void LlzFontShutdown(void);

/**
 * Check if the font system is initialized.
 *
 * @return true if fonts are available
 */
bool LlzFontIsReady(void);

// ============================================================================
// Font Loading
// ============================================================================

/**
 * Get the default UI font at the default size (20px).
 * This is the recommended font for general plugin use.
 * The returned font is cached and managed by the SDK.
 *
 * @return Font handle (never invalid if LlzFontInit succeeded)
 */
Font LlzFontGetDefault(void);

/**
 * Get a font at a specific size.
 * Fonts are cached internally - repeated calls with same parameters
 * return the same font instance.
 *
 * @param type Font type to load
 * @param size Font size in pixels
 * @return Font handle
 */
Font LlzFontGet(LlzFontType type, int size);

/**
 * Load a font with custom settings.
 * Use this for special cases requiring specific glyph sets.
 * The caller is responsible for unloading with UnloadFont().
 *
 * @param type Font type to load
 * @param size Font size in pixels
 * @param codepoints Array of Unicode codepoints to load (NULL for default ASCII)
 * @param codepointCount Number of codepoints (0 for default 256)
 * @return Font handle (caller must unload)
 */
Font LlzFontLoadCustom(LlzFontType type, int size, int* codepoints, int codepointCount);

// ============================================================================
// Font Path Utilities
// ============================================================================

/**
 * Get the path to a font file.
 * Useful for plugins that need to load fonts themselves.
 *
 * @param type Font type
 * @return Path string (do not free), or NULL if not found
 */
const char* LlzFontGetPath(LlzFontType type);

/**
 * Get the fonts directory path.
 * On CarThing: /var/local/fonts/ or /tmp/fonts/
 * On Desktop: ./fonts/ or system fonts
 *
 * @return Directory path (do not free)
 */
const char* LlzFontGetDirectory(void);

// ============================================================================
// Text Drawing Helpers
// ============================================================================

/**
 * Draw text using the default SDK font.
 * Convenience wrapper around DrawTextEx with sensible defaults.
 *
 * @param text Text to draw
 * @param x X position
 * @param y Y position
 * @param fontSize Font size in pixels
 * @param color Text color
 */
void LlzDrawText(const char* text, int x, int y, int fontSize, Color color);

/**
 * Draw text centered horizontally.
 *
 * @param text Text to draw
 * @param centerX Center X position
 * @param y Y position
 * @param fontSize Font size in pixels
 * @param color Text color
 */
void LlzDrawTextCentered(const char* text, int centerX, int y, int fontSize, Color color);

/**
 * Draw text with a shadow effect.
 *
 * @param text Text to draw
 * @param x X position
 * @param y Y position
 * @param fontSize Font size in pixels
 * @param color Text color
 * @param shadowColor Shadow color
 */
void LlzDrawTextShadow(const char* text, int x, int y, int fontSize, Color color, Color shadowColor);

/**
 * Measure text width using the SDK font.
 *
 * @param text Text to measure
 * @param fontSize Font size in pixels
 * @return Width in pixels
 */
int LlzMeasureText(const char* text, int fontSize);

/**
 * Measure text size using the SDK font.
 *
 * @param text Text to measure
 * @param fontSize Font size in pixels
 * @return Size vector (width, height)
 */
Vector2 LlzMeasureTextEx(const char* text, int fontSize);

#ifdef __cplusplus
}
#endif

#endif // LLZ_SDK_FONT_H
