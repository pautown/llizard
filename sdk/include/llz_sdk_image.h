#ifndef LLZ_SDK_IMAGE_H
#define LLZ_SDK_IMAGE_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a blurred and optionally darkened version of an image.
 *
 * @param source The source image to blur
 * @param blurRadius Blur radius in pixels (1-20 recommended, higher = more blur)
 * @param darkenAmount Amount to darken (0.0 = no darkening, 1.0 = fully black)
 * @return New Image with blur applied (caller must call UnloadImage when done)
 */
Image LlzImageBlur(Image source, int blurRadius, float darkenAmount);

/**
 * Creates a blurred texture from an existing texture.
 * Useful for creating background effects from album art.
 *
 * @param source The source texture to blur
 * @param blurRadius Blur radius in pixels (1-20 recommended)
 * @param darkenAmount Amount to darken (0.0 = no darkening, 1.0 = fully black)
 * @return New Texture2D with blur applied (caller must call UnloadTexture when done)
 */
Texture2D LlzTextureBlur(Texture2D source, int blurRadius, float darkenAmount);

/**
 * Draws a texture stretched to fill a rectangle while maintaining aspect ratio coverage.
 * Similar to CSS background-size: cover - the image fills the area completely,
 * cropping edges if necessary.
 *
 * @param texture The texture to draw
 * @param destRect The destination rectangle to fill
 * @param tint Color tint to apply
 */
void LlzDrawTextureCover(Texture2D texture, Rectangle destRect, Color tint);

/**
 * Draws a texture fitted within a rectangle while maintaining aspect ratio.
 * Similar to CSS background-size: contain - the entire image is visible,
 * with letterboxing if necessary.
 *
 * @param texture The texture to draw
 * @param destRect The destination rectangle to fit within
 * @param tint Color tint to apply
 */
void LlzDrawTextureContain(Texture2D texture, Rectangle destRect, Color tint);

#ifdef __cplusplus
}
#endif

#endif
