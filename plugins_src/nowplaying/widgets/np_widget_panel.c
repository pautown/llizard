#include "nowplaying/widgets/np_widget_panel.h"
#include "nowplaying/core/np_theme.h"

void NpPanelInit(NpPanel *panel, Rectangle bounds) {
    panel->bounds = bounds;
    panel->roundness = 0.2f;
    panel->hasGradient = false;
    panel->alpha = 255;
}

void NpPanelDraw(const NpPanel *panel) {
    NpPanelDrawWithAlpha(panel, (float)panel->alpha / 255.0f);
}

void NpPanelDrawWithAlpha(const NpPanel *panel, float alpha) {
    Color panelColor = NpThemeGetColor(NP_COLOR_PANEL_HOVER);
    panelColor.a = (unsigned char)(alpha * 255.0f);

    DrawRectangleRounded(panel->bounds, panel->roundness, 12, panelColor);
}
