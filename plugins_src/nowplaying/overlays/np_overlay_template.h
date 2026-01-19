#ifndef NP_PLUGIN_OVERLAY_TEMPLATE_H
#define NP_PLUGIN_OVERLAY_TEMPLATE_H

#include "raylib.h"

// Template overlay - a simple placeholder pane that fades in/out
typedef struct {
    Rectangle bounds;
    const char *title;
    const char *message;
} NpTemplateOverlay;

void NpTemplateOverlayInit(NpTemplateOverlay *overlay);
void NpTemplateOverlayDraw(const NpTemplateOverlay *overlay, float alpha);
void NpTemplateOverlaySetMessage(NpTemplateOverlay *overlay, const char *title, const char *message);

#endif
