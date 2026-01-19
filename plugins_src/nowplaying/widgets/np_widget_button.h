#ifndef NP_PLUGIN_WIDGET_BUTTON_H
#define NP_PLUGIN_WIDGET_BUTTON_H
#include "raylib.h"
#include <stdbool.h>

typedef struct {
    Rectangle bounds;
    const char *label;
    bool isActive;      // Highlighted/selected state
    bool isHovered;
    float roundness;
} NpButton;

void NpButtonInit(NpButton *btn, Rectangle bounds, const char *label);
bool NpButtonUpdate(NpButton *btn, Vector2 mousePos, bool mousePressed, bool mouseJustPressed);
void NpButtonDraw(const NpButton *btn);
void NpButtonDrawWithColors(const NpButton *btn, Color bg, Color text);

#endif
