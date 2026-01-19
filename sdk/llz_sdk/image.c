#include "llz_sdk_image.h"
#include <stdlib.h>
#include <string.h>

// Box blur implementation - fast and good quality with multiple passes
static void BoxBlurHorizontal(Color *src, Color *dst, int width, int height, int radius) {
    float invRadius = 1.0f / (float)(radius * 2 + 1);

    for (int y = 0; y < height; y++) {
        int rowOffset = y * width;

        // Initialize accumulator with first pixel repeated for left edge
        float r = 0, g = 0, b = 0, a = 0;
        for (int x = -radius; x <= radius; x++) {
            int idx = rowOffset + (x < 0 ? 0 : x);
            r += src[idx].r;
            g += src[idx].g;
            b += src[idx].b;
            a += src[idx].a;
        }

        for (int x = 0; x < width; x++) {
            dst[rowOffset + x] = (Color){
                (unsigned char)(r * invRadius),
                (unsigned char)(g * invRadius),
                (unsigned char)(b * invRadius),
                (unsigned char)(a * invRadius)
            };

            // Slide window
            int leftIdx = x - radius;
            int rightIdx = x + radius + 1;

            // Clamp indices
            if (leftIdx < 0) leftIdx = 0;
            if (rightIdx >= width) rightIdx = width - 1;

            r += src[rowOffset + rightIdx].r - src[rowOffset + leftIdx].r;
            g += src[rowOffset + rightIdx].g - src[rowOffset + leftIdx].g;
            b += src[rowOffset + rightIdx].b - src[rowOffset + leftIdx].b;
            a += src[rowOffset + rightIdx].a - src[rowOffset + leftIdx].a;
        }
    }
}

static void BoxBlurVertical(Color *src, Color *dst, int width, int height, int radius) {
    float invRadius = 1.0f / (float)(radius * 2 + 1);

    for (int x = 0; x < width; x++) {
        // Initialize accumulator with first pixel repeated for top edge
        float r = 0, g = 0, b = 0, a = 0;
        for (int y = -radius; y <= radius; y++) {
            int idx = (y < 0 ? 0 : y) * width + x;
            r += src[idx].r;
            g += src[idx].g;
            b += src[idx].b;
            a += src[idx].a;
        }

        for (int y = 0; y < height; y++) {
            dst[y * width + x] = (Color){
                (unsigned char)(r * invRadius),
                (unsigned char)(g * invRadius),
                (unsigned char)(b * invRadius),
                (unsigned char)(a * invRadius)
            };

            // Slide window
            int topIdx = y - radius;
            int bottomIdx = y + radius + 1;

            // Clamp indices
            if (topIdx < 0) topIdx = 0;
            if (bottomIdx >= height) bottomIdx = height - 1;

            r += src[bottomIdx * width + x].r - src[topIdx * width + x].r;
            g += src[bottomIdx * width + x].g - src[topIdx * width + x].g;
            b += src[bottomIdx * width + x].b - src[topIdx * width + x].b;
            a += src[bottomIdx * width + x].a - src[topIdx * width + x].a;
        }
    }
}

Image LlzImageBlur(Image source, int blurRadius, float darkenAmount) {
    if (source.data == NULL || source.width <= 0 || source.height <= 0) {
        return source;
    }

    // Clamp parameters
    if (blurRadius < 1) blurRadius = 1;
    if (blurRadius > 50) blurRadius = 50;
    if (darkenAmount < 0.0f) darkenAmount = 0.0f;
    if (darkenAmount > 1.0f) darkenAmount = 1.0f;

    // Create a copy of the image and ensure it's in RGBA format
    Image result = ImageCopy(source);
    ImageFormat(&result, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    int width = result.width;
    int height = result.height;
    int pixelCount = width * height;

    Color *pixels = (Color *)result.data;
    Color *temp = (Color *)malloc(pixelCount * sizeof(Color));
    if (!temp) {
        return result;
    }

    // Three-pass box blur approximates Gaussian blur
    // Each pass with radius/3 gives good quality
    int passRadius = blurRadius / 3;
    if (passRadius < 1) passRadius = 1;

    for (int pass = 0; pass < 3; pass++) {
        BoxBlurHorizontal(pixels, temp, width, height, passRadius);
        BoxBlurVertical(temp, pixels, width, height, passRadius);
    }

    // Apply darkening
    if (darkenAmount > 0.0f) {
        float brightnessMultiplier = 1.0f - darkenAmount;
        for (int i = 0; i < pixelCount; i++) {
            pixels[i].r = (unsigned char)(pixels[i].r * brightnessMultiplier);
            pixels[i].g = (unsigned char)(pixels[i].g * brightnessMultiplier);
            pixels[i].b = (unsigned char)(pixels[i].b * brightnessMultiplier);
        }
    }

    free(temp);
    return result;
}

Texture2D LlzTextureBlur(Texture2D source, int blurRadius, float darkenAmount) {
    if (source.id == 0) {
        return source;
    }

    // Get image from texture
    Image img = LoadImageFromTexture(source);

    // Apply blur
    Image blurred = LlzImageBlur(img, blurRadius, darkenAmount);

    // Upload to new texture
    Texture2D result = LoadTextureFromImage(blurred);

    // Cleanup
    UnloadImage(img);
    UnloadImage(blurred);

    return result;
}

void LlzDrawTextureCover(Texture2D texture, Rectangle destRect, Color tint) {
    if (texture.id == 0) return;

    float texRatio = (float)texture.width / (float)texture.height;
    float destRatio = destRect.width / destRect.height;

    Rectangle sourceRect;

    if (texRatio > destRatio) {
        // Image is wider - crop sides
        float visibleWidth = texture.height * destRatio;
        sourceRect = (Rectangle){
            (texture.width - visibleWidth) * 0.5f,
            0,
            visibleWidth,
            (float)texture.height
        };
    } else {
        // Image is taller - crop top/bottom
        float visibleHeight = texture.width / destRatio;
        sourceRect = (Rectangle){
            0,
            (texture.height - visibleHeight) * 0.5f,
            (float)texture.width,
            visibleHeight
        };
    }

    DrawTexturePro(texture, sourceRect, destRect, (Vector2){0, 0}, 0.0f, tint);
}

void LlzDrawTextureContain(Texture2D texture, Rectangle destRect, Color tint) {
    if (texture.id == 0) return;

    float texRatio = (float)texture.width / (float)texture.height;
    float destRatio = destRect.width / destRect.height;

    Rectangle actualDest;

    if (texRatio > destRatio) {
        // Image is wider - fit to width, letterbox height
        float height = destRect.width / texRatio;
        actualDest = (Rectangle){
            destRect.x,
            destRect.y + (destRect.height - height) * 0.5f,
            destRect.width,
            height
        };
    } else {
        // Image is taller - fit to height, letterbox width
        float width = destRect.height * texRatio;
        actualDest = (Rectangle){
            destRect.x + (destRect.width - width) * 0.5f,
            destRect.y,
            width,
            destRect.height
        };
    }

    Rectangle sourceRect = {0, 0, (float)texture.width, (float)texture.height};
    DrawTexturePro(texture, sourceRect, actualDest, (Vector2){0, 0}, 0.0f, tint);
}
