#ifndef LLZ_SDK_LAYOUT_H
#define LLZ_SDK_LAYOUT_H

#include "raylib.h"

typedef struct {
    Rectangle rect;
    float padding;
} LlzLayout;

Rectangle LlzLayoutSection(const Rectangle *bounds, float top, float height);
Rectangle LlzLayoutInset(const Rectangle *bounds, float inset);
void LlzLayoutSplitHorizontal(const Rectangle *bounds, float ratio, Rectangle *left, Rectangle *right);
void LlzLayoutSplitVertical(const Rectangle *bounds, float ratio, Rectangle *top, Rectangle *bottom);

#endif
