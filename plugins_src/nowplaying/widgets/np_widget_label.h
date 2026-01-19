#ifndef NP_PLUGIN_WIDGET_LABEL_H
#define NP_PLUGIN_WIDGET_LABEL_H
#include "raylib.h"
#include "../core/np_theme.h"

typedef enum {
    NP_LABEL_ALIGN_LEFT,
    NP_LABEL_ALIGN_CENTER,
    NP_LABEL_ALIGN_RIGHT
} NpLabelAlign;

typedef struct {
    Vector2 position;
    const char *text;
    NpTypographyId typography;
    NpLabelAlign align;
    float maxWidth;     // 0 for no limit
} NpLabel;

void NpLabelInit(NpLabel *label, Vector2 pos, const char *text, NpTypographyId typo);
void NpLabelDraw(const NpLabel *label);
void NpLabelDrawColored(const NpLabel *label, Color color);
void NpLabelDrawCenteredInRect(NpTypographyId typo, const char *text, Rectangle bounds, Color *color);

#endif
