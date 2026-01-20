#include "nowplaying/widgets/np_widget_album_art.h"
#include "nowplaying/core/np_theme.h"
#include "nowplaying/widgets/np_widget_label.h"
#include "llz_sdk_image.h"
#include <stddef.h>

void NpAlbumArtInit(NpAlbumArt *art, Rectangle bounds) {
    art->bounds = bounds;
    art->texture = NULL;
    art->accentColor = NpThemeGetColor(NP_COLOR_ACCENT);
    art->roundness = 0.12f;
    art->showBorder = true;
}

void NpAlbumArtDraw(const NpAlbumArt *art) {
    // Number of segments for smooth rounded corners (16 gives good quality)
    const int segments = 16;

    if (art->texture && art->texture->id != 0) {
        // Draw texture with rounded corners using SDK function
        LlzDrawTextureRounded(*art->texture, art->bounds, art->roundness, segments, WHITE);
    } else {
        // Draw gradient placeholder with rounded corners
        Color accent = art->accentColor;
        Color accentSoft = NpThemeGetColor(NP_COLOR_ACCENT_SOFT);

        // Draw rounded rectangle as placeholder background
        DrawRectangleRounded(art->bounds, art->roundness, segments,
            (Color){accent.r, accent.g, accent.b, 230});

        // Draw "Album art" placeholder text
        NpLabelDrawCenteredInRect(NP_TYPO_DETAIL, "Album art", art->bounds, NULL);
    }

    // Draw border
    if (art->showBorder) {
        Color borderColor = NpThemeGetColor(NP_COLOR_BORDER);
        DrawRectangleRoundedLines(art->bounds, art->roundness, segments, borderColor);
    }
}

void NpAlbumArtSetTexture(NpAlbumArt *art, Texture2D *texture) {
    art->texture = texture;
}
