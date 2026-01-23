/**
 * llz_sdk_shapes.h - Shape Drawing Module for llizardgui SDK
 *
 * Provides a collection of gem-themed shapes and colors for consistent
 * visual styling across plugins.
 *
 * Usage:
 *   // Draw a ruby-colored diamond
 *   LlzDrawDiamond(400, 240, 40, LLZ_COLOR_RUBY);
 *
 *   // Draw a sapphire-colored tall diamond
 *   LlzDrawTallDiamond(200, 200, 50, LLZ_COLOR_SAPPHIRE);
 */

#ifndef LLZ_SDK_SHAPES_H
#define LLZ_SDK_SHAPES_H

#include "raylib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Gem Colors
// ============================================================================

/** Ruby - Rich red */
#define LLZ_COLOR_RUBY          (Color){220, 50, 50, 255}
#define LLZ_COLOR_RUBY_LIGHT    (Color){255, 120, 120, 255}
#define LLZ_COLOR_RUBY_DARK     (Color){150, 20, 20, 255}

/** Amber - Warm orange */
#define LLZ_COLOR_AMBER         (Color){255, 140, 0, 255}
#define LLZ_COLOR_AMBER_LIGHT   (Color){255, 190, 80, 255}
#define LLZ_COLOR_AMBER_DARK    (Color){180, 80, 0, 255}

/** Topaz - Golden yellow */
#define LLZ_COLOR_TOPAZ         (Color){255, 220, 0, 255}
#define LLZ_COLOR_TOPAZ_LIGHT   (Color){255, 255, 120, 255}
#define LLZ_COLOR_TOPAZ_DARK    (Color){180, 150, 0, 255}

/** Emerald - Rich green */
#define LLZ_COLOR_EMERALD       (Color){50, 200, 80, 255}
#define LLZ_COLOR_EMERALD_LIGHT (Color){120, 255, 150, 255}
#define LLZ_COLOR_EMERALD_DARK  (Color){20, 120, 40, 255}

/** Sapphire - Deep blue */
#define LLZ_COLOR_SAPPHIRE      (Color){60, 120, 230, 255}
#define LLZ_COLOR_SAPPHIRE_LIGHT (Color){140, 180, 255, 255}
#define LLZ_COLOR_SAPPHIRE_DARK (Color){30, 70, 160, 255}

/** Amethyst - Royal purple */
#define LLZ_COLOR_AMETHYST      (Color){150, 80, 200, 255}
#define LLZ_COLOR_AMETHYST_LIGHT (Color){200, 150, 255, 255}
#define LLZ_COLOR_AMETHYST_DARK (Color){90, 40, 140, 255}

/** Diamond - Brilliant white */
#define LLZ_COLOR_DIAMOND       (Color){230, 230, 250, 255}
#define LLZ_COLOR_DIAMOND_LIGHT (Color){255, 255, 255, 255}
#define LLZ_COLOR_DIAMOND_DARK  (Color){180, 180, 200, 255}

/** Pink - Rose quartz */
#define LLZ_COLOR_PINK          (Color){255, 105, 180, 255}
#define LLZ_COLOR_PINK_LIGHT    (Color){255, 182, 213, 255}
#define LLZ_COLOR_PINK_DARK     (Color){199, 21, 133, 255}

// ============================================================================
// Color Enumeration
// ============================================================================

typedef enum {
    LLZ_GEM_RUBY,
    LLZ_GEM_AMBER,
    LLZ_GEM_TOPAZ,
    LLZ_GEM_EMERALD,
    LLZ_GEM_SAPPHIRE,
    LLZ_GEM_AMETHYST,
    LLZ_GEM_DIAMOND,
    LLZ_GEM_PINK,
    LLZ_GEM_COUNT
} LlzGemColor;

/**
 * Get the base color for a gem type.
 */
Color LlzGetGemColor(LlzGemColor gem);

/**
 * Get the light variant color for a gem type.
 */
Color LlzGetGemColorLight(LlzGemColor gem);

/**
 * Get the dark variant color for a gem type.
 */
Color LlzGetGemColorDark(LlzGemColor gem);

/**
 * Get the name of a gem color.
 */
const char* LlzGetGemColorName(LlzGemColor gem);

// ============================================================================
// Shape Enumeration
// ============================================================================

typedef enum {
    LLZ_SHAPE_CIRCLE,
    LLZ_SHAPE_SQUARE,
    LLZ_SHAPE_DIAMOND,
    LLZ_SHAPE_TALL_DIAMOND,
    LLZ_SHAPE_TRIANGLE,
    LLZ_SHAPE_HEXAGON,
    LLZ_SHAPE_OCTAGON,
    LLZ_SHAPE_KITE,
    LLZ_SHAPE_STAR,
    LLZ_SHAPE_DUTCH_CUT,
    LLZ_SHAPE_COUNT
} LlzShapeType;

/**
 * Get the name of a shape type.
 */
const char* LlzGetShapeName(LlzShapeType shape);

// ============================================================================
// Shape Drawing Functions
// ============================================================================

/**
 * Draw a circle shape with gem styling.
 * @param cx Center X position
 * @param cy Center Y position
 * @param size Radius of the shape
 * @param color Base color
 */
void LlzDrawCircle(float cx, float cy, float size, Color color);

/**
 * Draw a square shape with gem styling.
 */
void LlzDrawSquare(float cx, float cy, float size, Color color);

/**
 * Draw a diamond shape (square rotated 45 degrees).
 */
void LlzDrawDiamond(float cx, float cy, float size, Color color);

/**
 * Draw a tall diamond shape (elongated vertically).
 */
void LlzDrawTallDiamond(float cx, float cy, float size, Color color);

/**
 * Draw a triangle shape (pointing up).
 */
void LlzDrawTriangle(float cx, float cy, float size, Color color);

/**
 * Draw a hexagon shape.
 */
void LlzDrawHexagon(float cx, float cy, float size, Color color);

/**
 * Draw an octagon shape.
 */
void LlzDrawOctagon(float cx, float cy, float size, Color color);

/**
 * Draw a kite/rhombus shape.
 */
void LlzDrawKite(float cx, float cy, float size, Color color);

/**
 * Draw a star shape (5-pointed).
 */
void LlzDrawStar(float cx, float cy, float size, Color color);

/**
 * Draw a dutch cut shape (rectangular emerald cut).
 */
void LlzDrawDutchCut(float cx, float cy, float size, Color color);

/**
 * Draw any shape by type enum.
 * @param shape The shape type to draw
 * @param cx Center X position
 * @param cy Center Y position
 * @param size Size of the shape
 * @param color Base color
 */
void LlzDrawShape(LlzShapeType shape, float cx, float cy, float size, Color color);

/**
 * Draw a shape with full gem styling (shadow, highlight, facets).
 * @param shape The shape type to draw
 * @param cx Center X position
 * @param cy Center Y position
 * @param size Size of the shape
 * @param gem Gem color to use
 */
void LlzDrawGemShape(LlzShapeType shape, float cx, float cy, float size, LlzGemColor gem);

#ifdef __cplusplus
}
#endif

#endif // LLZ_SDK_SHAPES_H
