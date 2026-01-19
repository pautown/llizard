#ifndef NP_PLUGIN_WIDGET_PANEL_H
#define NP_PLUGIN_WIDGET_PANEL_H
#include "raylib.h"

typedef struct {
    Rectangle bounds;
    float roundness;
    bool hasGradient;
    unsigned char alpha;
} NpPanel;

void NpPanelInit(NpPanel *panel, Rectangle bounds);
void NpPanelDraw(const NpPanel *panel);
void NpPanelDrawWithAlpha(const NpPanel *panel, float alpha);

#endif
