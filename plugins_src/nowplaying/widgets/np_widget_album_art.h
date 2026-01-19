#ifndef NP_PLUGIN_WIDGET_ALBUM_ART_H
#define NP_PLUGIN_WIDGET_ALBUM_ART_H
#include "raylib.h"

typedef struct {
    Rectangle bounds;
    Texture2D *texture;     // NULL for gradient placeholder
    Color accentColor;
    float roundness;
    bool showBorder;
} NpAlbumArt;

void NpAlbumArtInit(NpAlbumArt *art, Rectangle bounds);
void NpAlbumArtDraw(const NpAlbumArt *art);
void NpAlbumArtSetTexture(NpAlbumArt *art, Texture2D *texture);

#endif
