/**
 * shapes.c - Shape Drawing Module Implementation
 *
 * Provides gem-themed shape drawing for the llizardgui SDK.
 */

#include "llz_sdk_shapes.h"
#include <math.h>

// ============================================================================
// Color Tables
// ============================================================================

static const Color GEM_COLORS_BASE[] = {
    {220, 50, 50, 255},     /* RUBY */
    {255, 140, 0, 255},     /* AMBER */
    {255, 220, 0, 255},     /* TOPAZ */
    {50, 200, 80, 255},     /* EMERALD */
    {60, 120, 230, 255},    /* SAPPHIRE */
    {150, 80, 200, 255},    /* AMETHYST */
    {230, 230, 250, 255},   /* DIAMOND */
    {255, 105, 180, 255}    /* PINK */
};

static const Color GEM_COLORS_LIGHT[] = {
    {255, 120, 120, 255},   /* RUBY */
    {255, 190, 80, 255},    /* AMBER */
    {255, 255, 120, 255},   /* TOPAZ */
    {120, 255, 150, 255},   /* EMERALD */
    {140, 180, 255, 255},   /* SAPPHIRE */
    {200, 150, 255, 255},   /* AMETHYST */
    {255, 255, 255, 255},   /* DIAMOND */
    {255, 182, 213, 255}    /* PINK */
};

static const Color GEM_COLORS_DARK[] = {
    {150, 20, 20, 255},     /* RUBY */
    {180, 80, 0, 255},      /* AMBER */
    {180, 150, 0, 255},     /* TOPAZ */
    {20, 120, 40, 255},     /* EMERALD */
    {30, 70, 160, 255},     /* SAPPHIRE */
    {90, 40, 140, 255},     /* AMETHYST */
    {180, 180, 200, 255},   /* DIAMOND */
    {199, 21, 133, 255}     /* PINK */
};

static const char* GEM_COLOR_NAMES[] = {
    "Ruby",
    "Amber",
    "Topaz",
    "Emerald",
    "Sapphire",
    "Amethyst",
    "Diamond",
    "Pink"
};

static const char* SHAPE_NAMES[] = {
    "Circle",
    "Square",
    "Diamond",
    "Tall Diamond",
    "Triangle",
    "Hexagon",
    "Octagon",
    "Kite",
    "Star",
    "Dutch Cut"
};

// ============================================================================
// Color Functions
// ============================================================================

Color LlzGetGemColor(LlzGemColor gem) {
    if (gem < 0 || gem >= LLZ_GEM_COUNT) return WHITE;
    return GEM_COLORS_BASE[gem];
}

Color LlzGetGemColorLight(LlzGemColor gem) {
    if (gem < 0 || gem >= LLZ_GEM_COUNT) return WHITE;
    return GEM_COLORS_LIGHT[gem];
}

Color LlzGetGemColorDark(LlzGemColor gem) {
    if (gem < 0 || gem >= LLZ_GEM_COUNT) return GRAY;
    return GEM_COLORS_DARK[gem];
}

const char* LlzGetGemColorName(LlzGemColor gem) {
    if (gem < 0 || gem >= LLZ_GEM_COUNT) return "Unknown";
    return GEM_COLOR_NAMES[gem];
}

const char* LlzGetShapeName(LlzShapeType shape) {
    if (shape < 0 || shape >= LLZ_SHAPE_COUNT) return "Unknown";
    return SHAPE_NAMES[shape];
}

// ============================================================================
// Helper Functions
// ============================================================================

static Color LerpColor(Color a, Color b, float t) {
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

static Color GetLightVariant(Color base) {
    return LerpColor(base, WHITE, 0.4f);
}

static Color GetDarkVariant(Color base) {
    return LerpColor(base, BLACK, 0.3f);
}

// ============================================================================
// Shape Drawing Functions
// ============================================================================

void LlzDrawCircle(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* Shadow */
    DrawCircle((int)(cx + 2), (int)(cy + 2), size, dark);

    /* Main body */
    DrawCircle((int)cx, (int)cy, size, color);

    /* Light inner */
    DrawCircle((int)(cx - size * 0.1f), (int)(cy - size * 0.1f), size * 0.7f, light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - size * 0.3f), (int)(cy - size * 0.3f), size * 0.25f, highlight);
}

void LlzDrawSquare(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    float half = size * 0.9f;

    /* Shadow */
    DrawRectangle((int)(cx - half + 2), (int)(cy - half + 2), (int)(half * 2), (int)(half * 2), dark);

    /* Main body */
    DrawRectangle((int)(cx - half), (int)(cy - half), (int)(half * 2), (int)(half * 2), color);

    /* Light facet (upper-left) */
    Vector2 tl = {cx - half, cy - half};
    Vector2 tr = {cx + half, cy - half};
    Vector2 center = {cx, cy};
    Vector2 bl = {cx - half, cy + half};
    DrawTriangle(tl, tr, center, light);
    DrawTriangle(tl, center, bl, light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - half * 0.4f), (int)(cy - half * 0.4f), size * 0.2f, highlight);
}

void LlzDrawDiamond(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* Diamond cut - square with large bevels, nearly 4-sided */
    float s = size * 0.9f;
    float bevel = s * 0.95f;

    /* 8 vertices for beveled square (clockwise from top-left) */
    Vector2 verts[8] = {
        {cx - s + bevel, cy - s},        /* Top edge left */
        {cx + s - bevel, cy - s},        /* Top edge right */
        {cx + s, cy - s + bevel},        /* Right edge top */
        {cx + s, cy + s - bevel},        /* Right edge bottom */
        {cx + s - bevel, cy + s},        /* Bottom edge right */
        {cx - s + bevel, cy + s},        /* Bottom edge left */
        {cx - s, cy + s - bevel},        /* Left edge bottom */
        {cx - s, cy - s + bevel}         /* Left edge top */
    };

    /* Shadow */
    Vector2 sVerts[8];
    for (int i = 0; i < 8; i++) {
        sVerts[i] = (Vector2){verts[i].x + 2, verts[i].y + 2};
    }
    for (int i = 1; i < 7; i++) {
        DrawTriangle(sVerts[0], sVerts[i + 1], sVerts[i], dark);
    }

    /* Main body */
    for (int i = 1; i < 7; i++) {
        DrawTriangle(verts[0], verts[i + 1], verts[i], color);
    }

    /* Light facet (top and left edges) */
    DrawTriangle(verts[7], verts[0], verts[1], light);
    DrawTriangle(verts[7], verts[1], verts[2], light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - s * 0.3f), (int)(cy - s * 0.3f), size * 0.18f, highlight);
}

void LlzDrawTallDiamond(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* Tall diamond cut - vertical rectangle with large bevels, nearly 4-sided */
    float w = size * 0.6f;
    float h = size * 1.0f;
    float bevelW = w * 0.95f;
    float bevelH = h * 0.95f;

    /* 8 vertices for beveled rectangle (clockwise from top-left) */
    Vector2 verts[8] = {
        {cx - w + bevelW, cy - h},        /* Top edge left */
        {cx + w - bevelW, cy - h},        /* Top edge right */
        {cx + w, cy - h + bevelH},        /* Right edge top */
        {cx + w, cy + h - bevelH},        /* Right edge bottom */
        {cx + w - bevelW, cy + h},        /* Bottom edge right */
        {cx - w + bevelW, cy + h},        /* Bottom edge left */
        {cx - w, cy + h - bevelH},        /* Left edge bottom */
        {cx - w, cy - h + bevelH}         /* Left edge top */
    };

    /* Shadow */
    Vector2 sVerts[8];
    for (int i = 0; i < 8; i++) {
        sVerts[i] = (Vector2){verts[i].x + 2, verts[i].y + 2};
    }
    for (int i = 1; i < 7; i++) {
        DrawTriangle(sVerts[0], sVerts[i + 1], sVerts[i], dark);
    }

    /* Main body */
    for (int i = 1; i < 7; i++) {
        DrawTriangle(verts[0], verts[i + 1], verts[i], color);
    }

    /* Light facet (top and left edges) */
    DrawTriangle(verts[7], verts[0], verts[1], light);
    DrawTriangle(verts[7], verts[1], verts[2], light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - w * 0.4f), (int)(cy - h * 0.4f), size * 0.18f, highlight);
}

void LlzDrawTriangle(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* Triangle - top point, bottom-left, bottom-right (counter-clockwise) */
    float s = size * 0.9f;

    Vector2 top = {cx, cy - s};
    Vector2 bottomLeft = {cx - s, cy + s * 0.7f};
    Vector2 bottomRight = {cx + s, cy + s * 0.7f};

    /* Shadow */
    DrawTriangle(
        (Vector2){top.x + 2, top.y + 2},
        (Vector2){bottomLeft.x + 2, bottomLeft.y + 2},
        (Vector2){bottomRight.x + 2, bottomRight.y + 2},
        dark);

    /* Main body */
    DrawTriangle(top, bottomLeft, bottomRight, color);

    /* Light facet (left side) */
    Vector2 center = {cx, cy};
    DrawTriangle(top, bottomLeft, center, light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - s * 0.25f), (int)(cy - s * 0.2f), size * 0.18f, highlight);
}

void LlzDrawHexagon(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* Shadow */
    DrawPoly((Vector2){cx + 2, cy + 2}, 6, size, 30.0f, dark);

    /* Main body */
    DrawPoly((Vector2){cx, cy}, 6, size, 30.0f, color);

    /* Light inner */
    DrawPoly((Vector2){cx - size * 0.1f, cy - size * 0.1f}, 6, size * 0.6f, 30.0f, light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - size * 0.25f), (int)(cy - size * 0.25f), size * 0.18f, highlight);
}

void LlzDrawOctagon(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* Shadow */
    DrawPoly((Vector2){cx + 2, cy + 2}, 8, size, 22.5f, dark);

    /* Main body */
    DrawPoly((Vector2){cx, cy}, 8, size, 22.5f, color);

    /* Light inner */
    DrawPoly((Vector2){cx - size * 0.1f, cy - size * 0.1f}, 8, size * 0.6f, 22.5f, light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - size * 0.25f), (int)(cy - size * 0.25f), size * 0.18f, highlight);
}

void LlzDrawKite(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* Kite - 4 vertices: top (long), left, bottom (short), right */
    float s = size * 0.9f;

    Vector2 top = {cx, cy - s * 1.1f};
    Vector2 left = {cx - s * 0.6f, cy - s * 0.1f};
    Vector2 bottom = {cx, cy + s * 0.6f};
    Vector2 right = {cx + s * 0.6f, cy - s * 0.1f};

    /* Shadow - two triangles (counter-clockwise) */
    DrawTriangle(
        (Vector2){top.x + 2, top.y + 2},
        (Vector2){left.x + 2, left.y + 2},
        (Vector2){bottom.x + 2, bottom.y + 2},
        dark);
    DrawTriangle(
        (Vector2){top.x + 2, top.y + 2},
        (Vector2){bottom.x + 2, bottom.y + 2},
        (Vector2){right.x + 2, right.y + 2},
        dark);

    /* Main body - two triangles (counter-clockwise) */
    DrawTriangle(top, left, bottom, color);
    DrawTriangle(top, bottom, right, color);

    /* Light facet (left side) */
    Vector2 center = {cx, cy - s * 0.2f};
    DrawTriangle(top, left, center, light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - s * 0.2f), (int)(cy - s * 0.4f), size * 0.18f, highlight);
}

void LlzDrawStar(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* 5-pointed star using angle calculations like penrose example */
    float s = size * 0.9f;
    float outerR = s;
    float innerR = s * 0.4f;

    /* Calculate 10 vertices: 5 outer points, 5 inner valleys */
    Vector2 outer[5];
    Vector2 inner[5];

    for (int i = 0; i < 5; i++) {
        float outerAngle = DEG2RAD * (i * 72.0f - 90.0f);
        float innerAngle = DEG2RAD * (i * 72.0f + 36.0f - 90.0f);

        outer[i] = (Vector2){cx + cosf(outerAngle) * outerR, cy + sinf(outerAngle) * outerR};
        inner[i] = (Vector2){cx + cosf(innerAngle) * innerR, cy + sinf(innerAngle) * innerR};
    }

    Vector2 center = {cx, cy};

    /* Shadow - draw each point as a triangle (outer tip + two inner valleys) */
    for (int i = 0; i < 5; i++) {
        int prev = (i + 4) % 5;
        DrawTriangle(
            (Vector2){outer[i].x + 2, outer[i].y + 2},
            (Vector2){inner[prev].x + 2, inner[prev].y + 2},
            (Vector2){inner[i].x + 2, inner[i].y + 2},
            dark);
    }
    /* Shadow - inner pentagon using kite approach (two triangles per section) */
    Vector2 sCenter = {cx + 2, cy + 2};
    for (int i = 0; i < 5; i++) {
        int next = (i + 1) % 5;
        Vector2 sInnerCurr = {inner[i].x + 2, inner[i].y + 2};
        Vector2 sInnerNext = {inner[next].x + 2, inner[next].y + 2};
        /* Kite: two triangles from center to adjacent inner points */
        DrawTriangle(sCenter, sInnerNext, sInnerCurr, dark);
    }

    /* Main body - draw each star point as a triangle */
    for (int i = 0; i < 5; i++) {
        int prev = (i + 4) % 5;
        DrawTriangle(outer[i], inner[prev], inner[i], color);
    }

    /* Main body - fill center pentagon using kite approach */
    for (int i = 0; i < 5; i++) {
        int next = (i + 1) % 5;
        /* Each kite section: center to two adjacent inner valley points */
        /* Draw as two triangles meeting at midpoint for proper fill */
        Vector2 midpoint = {
            (inner[i].x + inner[next].x) / 2.0f,
            (inner[i].y + inner[next].y) / 2.0f
        };
        DrawTriangle(center, midpoint, inner[i], color);
        DrawTriangle(center, inner[next], midpoint, color);
    }

    /* Light facet (top point) */
    DrawTriangle(outer[0], inner[4], inner[0], light);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx), (int)(cy - s * 0.3f), size * 0.15f, highlight);
}

void LlzDrawDutchCut(float cx, float cy, float size, Color color) {
    Color dark = GetDarkVariant(color);
    Color light = GetLightVariant(color);

    /* Dutch/Emerald cut - rectangle with beveled corners */
    float w = size * 1.0f;
    float h = size * 0.7f;
    float bevel = size * 0.25f;

    /* 8 vertices for beveled rectangle */
    Vector2 verts[8] = {
        {cx - w + bevel, cy - h},       /* Top-left bevel */
        {cx + w - bevel, cy - h},       /* Top-right bevel */
        {cx + w, cy - h + bevel},       /* Right-top bevel */
        {cx + w, cy + h - bevel},       /* Right-bottom bevel */
        {cx + w - bevel, cy + h},       /* Bottom-right bevel */
        {cx - w + bevel, cy + h},       /* Bottom-left bevel */
        {cx - w, cy + h - bevel},       /* Left-bottom bevel */
        {cx - w, cy - h + bevel}        /* Left-top bevel */
    };

    /* Shadow */
    Vector2 sVerts[8];
    for (int i = 0; i < 8; i++) {
        sVerts[i] = (Vector2){verts[i].x + 2, verts[i].y + 2};
    }
    for (int i = 1; i < 7; i++) {
        DrawTriangle(sVerts[0], sVerts[i + 1], sVerts[i], dark);
    }

    /* Main body */
    for (int i = 1; i < 7; i++) {
        DrawTriangle(verts[0], verts[i + 1], verts[i], color);
    }

    /* Light facet (top edge) */
    DrawTriangle(verts[7], verts[0], verts[1], light);
    DrawTriangle(verts[7], verts[1], verts[2], light);

    /* Inner rectangle for step-cut effect */
    float innerScale = 0.6f;
    Color innerColor = LerpColor(color, light, 0.3f);
    DrawRectangle((int)(cx - w * innerScale), (int)(cy - h * innerScale),
                  (int)(w * innerScale * 2), (int)(h * innerScale * 2), innerColor);

    /* Highlight */
    Color highlight = WHITE;
    highlight.a = 180;
    DrawCircle((int)(cx - w * 0.3f), (int)(cy - h * 0.3f), size * 0.15f, highlight);
}

// ============================================================================
// Generic Shape Drawing
// ============================================================================

void LlzDrawShape(LlzShapeType shape, float cx, float cy, float size, Color color) {
    switch (shape) {
        case LLZ_SHAPE_CIRCLE:      LlzDrawCircle(cx, cy, size, color); break;
        case LLZ_SHAPE_SQUARE:      LlzDrawSquare(cx, cy, size, color); break;
        case LLZ_SHAPE_DIAMOND:     LlzDrawDiamond(cx, cy, size, color); break;
        case LLZ_SHAPE_TALL_DIAMOND: LlzDrawTallDiamond(cx, cy, size, color); break;
        case LLZ_SHAPE_TRIANGLE:    LlzDrawTriangle(cx, cy, size, color); break;
        case LLZ_SHAPE_HEXAGON:     LlzDrawHexagon(cx, cy, size, color); break;
        case LLZ_SHAPE_OCTAGON:     LlzDrawOctagon(cx, cy, size, color); break;
        case LLZ_SHAPE_KITE:        LlzDrawKite(cx, cy, size, color); break;
        case LLZ_SHAPE_STAR:        LlzDrawStar(cx, cy, size, color); break;
        case LLZ_SHAPE_DUTCH_CUT:   LlzDrawDutchCut(cx, cy, size, color); break;
        default: break;
    }
}

void LlzDrawGemShape(LlzShapeType shape, float cx, float cy, float size, LlzGemColor gem) {
    Color color = LlzGetGemColor(gem);
    LlzDrawShape(shape, cx, cy, size, color);
}
