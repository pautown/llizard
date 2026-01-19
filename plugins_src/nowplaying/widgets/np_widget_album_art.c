#include "nowplaying/widgets/np_widget_album_art.h"
#include "nowplaying/core/np_theme.h"
#include "nowplaying/widgets/np_widget_label.h"
#include <stddef.h>

void NpAlbumArtInit(NpAlbumArt *art, Rectangle bounds) {
    art->bounds = bounds;
    art->texture = NULL;
    art->accentColor = NpThemeGetColor(NP_COLOR_ACCENT);
    art->roundness = 0.12f;
    art->showBorder = true;
}

void NpAlbumArtDraw(const NpAlbumArt *art) {
    if (art->texture && art->texture->id != 0) {
        // Draw texture scaled to bounds
        Rectangle source = {0, 0, (float)art->texture->width, (float)art->texture->height};
        DrawTexturePro(*art->texture, source, art->bounds, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        // Draw gradient placeholder
        Color accent = art->accentColor;
        Color accentSoft = NpThemeGetColor(NP_COLOR_ACCENT_SOFT);

        DrawRectangleGradientV(
            (int)art->bounds.x, (int)art->bounds.y,
            (int)art->bounds.width, (int)art->bounds.height,
            (Color){accent.r, accent.g, accent.b, 230},
            (Color){accentSoft.r, accentSoft.g, accentSoft.b, 200}
        );

        // Draw "Album art" placeholder text
        NpLabelDrawCenteredInRect(NP_TYPO_DETAIL, "Album art", art->bounds, NULL);
    }

    // Draw border
    if (art->showBorder) {
        Color borderColor = NpThemeGetColor(NP_COLOR_BORDER);
        DrawRectangleRoundedLines(art->bounds, art->roundness, 18, borderColor);
    }
}

void NpAlbumArtSetTexture(NpAlbumArt *art, Texture2D *texture) {
    art->texture = texture;
}
