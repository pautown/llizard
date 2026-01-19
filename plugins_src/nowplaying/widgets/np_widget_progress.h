#ifndef NP_PLUGIN_WIDGET_PROGRESS_H
#define NP_PLUGIN_WIDGET_PROGRESS_H
#include "raylib.h"
#include <stdbool.h>

typedef struct {
    Rectangle bounds;
    float value;        // 0.0 to 1.0
    float roundness;
    bool showThumb;     // Show scrub thumb
} NpProgressBar;

void NpProgressInit(NpProgressBar *bar, Rectangle bounds);
void NpProgressSetValue(NpProgressBar *bar, float value);
void NpProgressDraw(const NpProgressBar *bar);
void NpProgressDrawWithColors(const NpProgressBar *bar, const Color *trackColor, const Color *fillColor, const Color *thumbColor);
bool NpProgressHandleScrub(NpProgressBar *bar, Vector2 mousePos, bool mousePressed, float *newValue);

#endif
