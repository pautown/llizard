#include "nowplaying/overlays/np_overlay_template.h"
#include "nowplaying/core/np_theme.h"
#include "nowplaying/widgets/np_widget_panel.h"
#include "nowplaying/widgets/np_widget_label.h"

void NpTemplateOverlayInit(NpTemplateOverlay *overlay) {
    overlay->bounds = (Rectangle){0, 0, 800, 480};
    overlay->title = "Template Overlay";
    overlay->message = "This is a template pane that fades in and out.\nPress Back to close.";
}

void NpTemplateOverlayDraw(const NpTemplateOverlay *overlay, float alpha) {
    // Calculate centered panel
    float panelWidth = 500;
    float panelHeight = 200;
    Rectangle panelBounds = {
        overlay->bounds.x + (overlay->bounds.width - panelWidth) * 0.5f,
        overlay->bounds.y + (overlay->bounds.height - panelHeight) * 0.5f,
        panelWidth,
        panelHeight
    };

    // Draw panel background
    NpPanel panel;
    NpPanelInit(&panel, panelBounds);
    panel.roundness = 0.15f;
    NpPanelDrawWithAlpha(&panel, alpha * 0.95f);

    // Draw border
    Color borderColor = NpThemeGetColorAlpha(NP_COLOR_BORDER, alpha);
    DrawRectangleRoundedLines(panelBounds, 0.15f, 12, borderColor);

    // Draw title
    Color titleColor = NpThemeGetColorAlpha(NP_COLOR_TEXT_PRIMARY, alpha);
    float titleY = panelBounds.y + 30;
    NpLabelDrawCenteredInRect(NP_TYPO_TITLE, overlay->title,
        (Rectangle){panelBounds.x, titleY, panelBounds.width, NpThemeGetLineHeight(NP_TYPO_TITLE)},
        &titleColor);

    // Draw message
    Color messageColor = NpThemeGetColorAlpha(NP_COLOR_TEXT_SECONDARY, alpha);
    float messageY = panelBounds.y + 90;
    NpLabelDrawCenteredInRect(NP_TYPO_BODY, overlay->message,
        (Rectangle){panelBounds.x, messageY, panelBounds.width, NpThemeGetLineHeight(NP_TYPO_BODY)},
        &messageColor);

    // Draw hint at bottom
    Color hintColor = NpThemeGetColorAlpha(NP_COLOR_TEXT_SECONDARY, alpha * 0.7f);
    float hintY = panelBounds.y + panelBounds.height - 50;
    NpLabelDrawCenteredInRect(NP_TYPO_DETAIL, "Press Back to close",
        (Rectangle){panelBounds.x, hintY, panelBounds.width, NpThemeGetLineHeight(NP_TYPO_DETAIL)},
        &hintColor);
}

void NpTemplateOverlaySetMessage(NpTemplateOverlay *overlay, const char *title, const char *message) {
    if (title) overlay->title = title;
    if (message) overlay->message = message;
}
