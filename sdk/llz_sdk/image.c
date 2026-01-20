#include "llz_sdk_image.h"
#include "rlgl.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

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

// Helper: Draw a textured rounded rectangle using rlgl primitives
// sourceRect is the texture source area, destRect is where to draw
static void DrawTextureRoundedInternal(Texture2D texture, Rectangle sourceRect, Rectangle destRect,
                                        float roundness, int segments, Color tint) {
    if (texture.id == 0) return;
    if (roundness < 0.0f) roundness = 0.0f;
    if (roundness > 1.0f) roundness = 1.0f;
    if (segments < 4) segments = 4;

    float width = destRect.width;
    float height = destRect.height;

    // Calculate corner radius based on shorter side
    float radius = (width < height ? width : height) * roundness * 0.5f;
    if (radius <= 0.0f) {
        // No rounding needed, just draw normally
        DrawTexturePro(texture, sourceRect, destRect, (Vector2){0, 0}, 0.0f, tint);
        return;
    }

    // Clamp radius to not exceed half the shorter side
    float maxRadius = (width < height ? width : height) * 0.5f;
    if (radius > maxRadius) radius = maxRadius;

    // Calculate texture coordinates (normalized 0-1)
    float texLeft = sourceRect.x / (float)texture.width;
    float texTop = sourceRect.y / (float)texture.height;
    float texRight = (sourceRect.x + sourceRect.width) / (float)texture.width;
    float texBottom = (sourceRect.y + sourceRect.height) / (float)texture.height;

    // Calculate texture coordinate offsets for the radius
    float texRadiusX = (radius / destRect.width) * (texRight - texLeft);
    float texRadiusY = (radius / destRect.height) * (texBottom - texTop);

    // Screen positions
    float x = destRect.x;
    float y = destRect.y;

    // Step angle for corner arcs (in degrees for consistency with raylib)
    float stepLength = 90.0f / (float)segments;

    // Corner centers (screen coords) and their texture coordinate centers
    // Order: top-left, top-right, bottom-right, bottom-left
    float centerX[4] = { x + radius, x + width - radius, x + width - radius, x + radius };
    float centerY[4] = { y + radius, y + radius, y + height - radius, y + height - radius };
    float texCenterX[4] = { texLeft + texRadiusX, texRight - texRadiusX, texRight - texRadiusX, texLeft + texRadiusX };
    float texCenterY[4] = { texTop + texRadiusY, texTop + texRadiusY, texBottom - texRadiusY, texBottom - texRadiusY };
    float startAngles[4] = { 180.0f, 270.0f, 0.0f, 90.0f };

    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);

    rlColor4ub(tint.r, tint.g, tint.b, tint.a);

    // Draw all 4 corners using quads (each quad = 2 triangles forming a pie slice)
    // Following raylib's pattern from DrawRectangleRounded
    for (int k = 0; k < 4; k++) {
        float angle = startAngles[k];
        float cx = centerX[k];
        float cy = centerY[k];
        float tcx = texCenterX[k];
        float tcy = texCenterY[k];

        for (int i = 0; i < segments / 2; i++) {
            float a0 = angle * (PI / 180.0f);
            float a1 = (angle + stepLength) * (PI / 180.0f);
            float a2 = (angle + stepLength * 2) * (PI / 180.0f);

            // Quad vertex 0: center
            rlTexCoord2f(tcx, tcy);
            rlVertex2f(cx, cy);

            // Quad vertex 1: outer point at angle + stepLength*2
            rlTexCoord2f(tcx + cosf(a2) * texRadiusX, tcy + sinf(a2) * texRadiusY);
            rlVertex2f(cx + cosf(a2) * radius, cy + sinf(a2) * radius);

            // Quad vertex 2: outer point at angle + stepLength
            rlTexCoord2f(tcx + cosf(a1) * texRadiusX, tcy + sinf(a1) * texRadiusY);
            rlVertex2f(cx + cosf(a1) * radius, cy + sinf(a1) * radius);

            // Quad vertex 3: outer point at angle
            rlTexCoord2f(tcx + cosf(a0) * texRadiusX, tcy + sinf(a0) * texRadiusY);
            rlVertex2f(cx + cosf(a0) * radius, cy + sinf(a0) * radius);

            angle += stepLength * 2;
        }

        // Handle odd number of segments (one extra triangle)
        if (segments % 2) {
            float a0 = angle * (PI / 180.0f);
            float a1 = (angle + stepLength) * (PI / 180.0f);

            // Draw as a degenerate quad (triangle)
            rlTexCoord2f(tcx, tcy);
            rlVertex2f(cx, cy);

            rlTexCoord2f(tcx + cosf(a1) * texRadiusX, tcy + sinf(a1) * texRadiusY);
            rlVertex2f(cx + cosf(a1) * radius, cy + sinf(a1) * radius);

            rlTexCoord2f(tcx + cosf(a0) * texRadiusX, tcy + sinf(a0) * texRadiusY);
            rlVertex2f(cx + cosf(a0) * radius, cy + sinf(a0) * radius);

            // Repeat center to complete the quad
            rlTexCoord2f(tcx, tcy);
            rlVertex2f(cx, cy);
        }
    }

    // Draw the 3 rectangular sections

    // Section 2: Top middle (between top-left and top-right corners)
    rlTexCoord2f(texLeft + texRadiusX, texTop);
    rlVertex2f(x + radius, y);
    rlTexCoord2f(texLeft + texRadiusX, texTop + texRadiusY);
    rlVertex2f(x + radius, y + radius);
    rlTexCoord2f(texRight - texRadiusX, texTop + texRadiusY);
    rlVertex2f(x + width - radius, y + radius);
    rlTexCoord2f(texRight - texRadiusX, texTop);
    rlVertex2f(x + width - radius, y);

    // Section 4: Right side (between top-right and bottom-right corners)
    rlTexCoord2f(texRight - texRadiusX, texTop + texRadiusY);
    rlVertex2f(x + width - radius, y + radius);
    rlTexCoord2f(texRight - texRadiusX, texBottom - texRadiusY);
    rlVertex2f(x + width - radius, y + height - radius);
    rlTexCoord2f(texRight, texBottom - texRadiusY);
    rlVertex2f(x + width, y + height - radius);
    rlTexCoord2f(texRight, texTop + texRadiusY);
    rlVertex2f(x + width, y + radius);

    // Section 6: Bottom middle (between bottom-right and bottom-left corners)
    rlTexCoord2f(texLeft + texRadiusX, texBottom - texRadiusY);
    rlVertex2f(x + radius, y + height - radius);
    rlTexCoord2f(texLeft + texRadiusX, texBottom);
    rlVertex2f(x + radius, y + height);
    rlTexCoord2f(texRight - texRadiusX, texBottom);
    rlVertex2f(x + width - radius, y + height);
    rlTexCoord2f(texRight - texRadiusX, texBottom - texRadiusY);
    rlVertex2f(x + width - radius, y + height - radius);

    // Section 8: Left side (between bottom-left and top-left corners)
    rlTexCoord2f(texLeft, texTop + texRadiusY);
    rlVertex2f(x, y + radius);
    rlTexCoord2f(texLeft, texBottom - texRadiusY);
    rlVertex2f(x, y + height - radius);
    rlTexCoord2f(texLeft + texRadiusX, texBottom - texRadiusY);
    rlVertex2f(x + radius, y + height - radius);
    rlTexCoord2f(texLeft + texRadiusX, texTop + texRadiusY);
    rlVertex2f(x + radius, y + radius);

    // Section 9: Center rectangle
    rlTexCoord2f(texLeft + texRadiusX, texTop + texRadiusY);
    rlVertex2f(x + radius, y + radius);
    rlTexCoord2f(texLeft + texRadiusX, texBottom - texRadiusY);
    rlVertex2f(x + radius, y + height - radius);
    rlTexCoord2f(texRight - texRadiusX, texBottom - texRadiusY);
    rlVertex2f(x + width - radius, y + height - radius);
    rlTexCoord2f(texRight - texRadiusX, texTop + texRadiusY);
    rlVertex2f(x + width - radius, y + radius);

    rlEnd();
    rlSetTexture(0);
}

void LlzDrawTextureRounded(Texture2D texture, Rectangle destRect, float roundness, int segments, Color tint) {
    if (texture.id == 0) return;
    Rectangle sourceRect = {0, 0, (float)texture.width, (float)texture.height};
    DrawTextureRoundedInternal(texture, sourceRect, destRect, roundness, segments, tint);
}

void LlzDrawTextureRoundedCover(Texture2D texture, Rectangle destRect, float roundness, int segments, Color tint) {
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

    DrawTextureRoundedInternal(texture, sourceRect, destRect, roundness, segments, tint);
}

void LlzDrawTextureRoundedContain(Texture2D texture, Rectangle destRect, float roundness, int segments, Color tint) {
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
    DrawTextureRoundedInternal(texture, sourceRect, actualDest, roundness, segments, tint);
}
