#include "llz_sdk_layout.h"

Rectangle LlzLayoutSection(const Rectangle *bounds, float top, float height)
{
    Rectangle r = *bounds;
    r.y += top;
    r.height = height;
    return r;
}

Rectangle LlzLayoutInset(const Rectangle *bounds, float inset)
{
    Rectangle r = *bounds;
    r.x += inset;
    r.y += inset;
    r.width -= inset * 2.0f;
    r.height -= inset * 2.0f;
    return r;
}

void LlzLayoutSplitHorizontal(const Rectangle *bounds, float ratio, Rectangle *left, Rectangle *right)
{
    Rectangle l = *bounds;
    l.width = bounds->width * ratio;
    Rectangle r = *bounds;
    r.x += l.width;
    r.width = bounds->width - l.width;
    if (left) *left = l;
    if (right) *right = r;
}

void LlzLayoutSplitVertical(const Rectangle *bounds, float ratio, Rectangle *top, Rectangle *bottom)
{
    Rectangle t = *bounds;
    t.height = bounds->height * ratio;
    Rectangle b = *bounds;
    b.y += t.height;
    b.height = bounds->height - t.height;
    if (top) *top = t;
    if (bottom) *bottom = b;
}
