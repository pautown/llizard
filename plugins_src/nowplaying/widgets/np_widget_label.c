#include "nowplaying/widgets/np_widget_label.h"

void NpLabelInit(NpLabel *label, Vector2 pos, const char *text, NpTypographyId typo) {
    label->position = pos;
    label->text = text;
    label->typography = typo;
    label->align = NP_LABEL_ALIGN_LEFT;
    label->maxWidth = 0;
}

void NpLabelDraw(const NpLabel *label) {
    NpLabelDrawColored(label, NpThemeGetColor(NP_COLOR_TEXT_PRIMARY));
}

void NpLabelDrawColored(const NpLabel *label, Color color) {
    if (!label->text) return;

    Vector2 pos = label->position;

    if (label->align != NP_LABEL_ALIGN_LEFT) {
        float textWidth = NpThemeMeasureTextWidth(label->typography, label->text);
        if (label->align == NP_LABEL_ALIGN_CENTER) {
            pos.x -= textWidth * 0.5f;
        } else if (label->align == NP_LABEL_ALIGN_RIGHT) {
            pos.x -= textWidth;
        }
    }

    NpThemeDrawTextColored(label->typography, label->text, pos, color);
}

void NpLabelDrawCenteredInRect(NpTypographyId typo, const char *text, Rectangle bounds, Color *color) {
    if (!text) return;

    float textWidth = NpThemeMeasureTextWidth(typo, text);
    float textHeight = NpThemeGetLineHeight(typo);
    Vector2 pos = {
        bounds.x + (bounds.width - textWidth) * 0.5f,
        bounds.y + (bounds.height - textHeight) * 0.5f
    };

    if (color) {
        NpThemeDrawTextColored(typo, text, pos, *color);
    } else {
        NpThemeDrawText(typo, text, pos);
    }
}
