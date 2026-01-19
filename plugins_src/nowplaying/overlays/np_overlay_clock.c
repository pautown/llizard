#include "nowplaying/overlays/np_overlay_clock.h"
#include "nowplaying/core/np_theme.h"
#include "raymath.h"
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#ifndef DEG2RAD
#define DEG2RAD (PI / 180.0f)
#endif

// Clock style names for display
static const char *CLOCK_STYLE_NAMES[] = {
    "Digital",
    "Full Screen",
    "Analog"
};

// Helper function to ease animations
static float EaseOutCubic(float t) {
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

// Draw digital clock style with glow effect
static void DrawClockStyleDigital(Rectangle bounds, const struct tm *timeinfo, const char *timeStr,
                                  const char *dateStr, Color textColor, Color accentColor) {
    int screenCenterX = (int)(bounds.x + bounds.width / 2);
    int screenCenterY = (int)(bounds.y + bounds.height / 2);

    Font font = NpThemeGetFont();

    // Large digital time display
    float timeFontSize = 120.0f;
    float timeSpacing = 2.0f;
    Vector2 timeMeasure = MeasureTextEx(font, timeStr, timeFontSize, timeSpacing);
    Vector2 timePos = {
        screenCenterX - timeMeasure.x / 2,
        screenCenterY - 80
    };

    // Draw glow effect (multiple passes with alpha)
    Color glowColor = accentColor;
    glowColor.a = 60;
    for (int i = 1; i <= 3; i++) {
        Vector2 glowPos = { timePos.x, timePos.y + i * 2 };
        DrawTextEx(font, timeStr, glowPos, timeFontSize, timeSpacing, glowColor);
    }

    // Draw main time text
    DrawTextEx(font, timeStr, timePos, timeFontSize, timeSpacing, textColor);

    // Draw date below time
    float dateFontSize = 28.0f;
    float dateSpacing = 1.5f;
    Vector2 dateMeasure = MeasureTextEx(font, dateStr, dateFontSize, dateSpacing);
    Vector2 datePos = {
        screenCenterX - dateMeasure.x / 2,
        timePos.y + timeFontSize + 30
    };
    DrawTextEx(font, dateStr, datePos, dateFontSize, dateSpacing, textColor);

    // Draw seconds (smaller, to the right)
    char secondsStr[8];
    snprintf(secondsStr, sizeof(secondsStr), ":%02d", timeinfo->tm_sec);
    float secFontSize = 48.0f;
    float secSpacing = 1.5f;
    Vector2 secPos = {
        timePos.x + timeMeasure.x + 20,
        timePos.y + 20
    };
    Color secColor = textColor;
    secColor.a = (unsigned char)(secColor.a * 0.85f);
    DrawTextEx(font, secondsStr, secPos, secFontSize, secSpacing, secColor);
}

// Draw fullscreen clock style with minimal design
static void DrawClockStyleFullscreen(Rectangle bounds, const struct tm *timeinfo, const char *timeStr,
                                     const char *dateStr, Color textColor, Color accent) {
    float centerX = bounds.x + bounds.width * 0.5f;
    float baselineY = bounds.y + bounds.height * 0.35f;

    Font font = NpThemeGetFont();

    // Large time display
    float timeFontSize = bounds.height * 0.34f;
    float timeSpacing = 4.0f;
    Vector2 timeMeasure = MeasureTextEx(font, timeStr, timeFontSize, timeSpacing);
    Vector2 timePos = {
        centerX - timeMeasure.x / 2.0f,
        baselineY - timeFontSize * 0.45f
    };

    // Draw highlight background rectangle
    Rectangle highlight = {
        timePos.x - 60,
        timePos.y - timeFontSize * 0.35f,
        timeMeasure.x + 120,
        timeFontSize * 1.7f
    };
    DrawRectangleRounded(highlight, 0.22f, 16, ColorAlpha(accent, 0.18f));
    DrawRectangleRoundedLines(highlight, 0.22f, 16, ColorAlpha(accent, 0.3f));

    // Draw glow effect
    Color glowColor = ColorAlpha(accent, 0.4f);
    for (int i = 0; i < 4; ++i) {
        Vector2 glowPos = { timePos.x, timePos.y + i * 2.5f };
        DrawTextEx(font, timeStr, glowPos, timeFontSize, timeSpacing, glowColor);
    }

    // Draw main time
    DrawTextEx(font, timeStr, timePos, timeFontSize, timeSpacing, textColor);

    // Draw seconds + AM/PM badge
    float secondsFontSize = timeFontSize * 0.28f;
    float secondsSpacing = 1.0f;
    char secondsStr[8];
    snprintf(secondsStr, sizeof(secondsStr), ":%02d", timeinfo->tm_sec);
    Vector2 secondsMeasure = MeasureTextEx(font, secondsStr, secondsFontSize, secondsSpacing);
    Vector2 secondsPos = {
        timePos.x + timeMeasure.x + 30,
        timePos.y + timeFontSize * 0.35f
    };
    Color secondsColor = textColor;
    secondsColor.a = (unsigned char)(secondsColor.a * 0.9f);
    DrawTextEx(font, secondsStr, secondsPos, secondsFontSize, secondsSpacing, secondsColor);

    // AM/PM badge
    const char *amPm = (timeinfo->tm_hour >= 12) ? "PM" : "AM";
    float badgeFont = secondsFontSize * 0.8f;
    Vector2 amPmMeasure = MeasureTextEx(font, amPm, badgeFont, 1.0f);
    float badgeWidth = amPmMeasure.x + 24;
    Rectangle badgeRect = {
        secondsPos.x + secondsMeasure.x + 18,
        secondsPos.y - 6,
        badgeWidth,
        badgeFont * 1.8f
    };
    DrawRectangleRounded(badgeRect, 0.45f, 6, ColorAlpha(accent, 0.3f));
    Vector2 badgePos = {
        badgeRect.x + (badgeRect.width - badgeWidth + 24) / 2.0f,
        badgeRect.y + (badgeRect.height - badgeFont) / 2.4f
    };
    DrawTextEx(font, amPm, badgePos, badgeFont, 1.0f, textColor);

    // Date footer
    float dateFontSize = bounds.height * 0.085f;
    float dateSpacing = 1.6f;
    Vector2 dateMeasure = MeasureTextEx(font, dateStr, dateFontSize, dateSpacing);
    Vector2 datePos = {
        centerX - dateMeasure.x / 2.0f,
        highlight.y + highlight.height + 50
    };
    Color dateColor = textColor;
    dateColor.a = (unsigned char)(dateColor.a * 0.9f);
    DrawTextEx(font, dateStr, datePos, dateFontSize, dateSpacing, dateColor);
}

// Draw analog clock style with clock hands
static void DrawClockStyleAnalog(Rectangle bounds, const struct tm *timeinfo, const char *dateStr,
                                Color textColor, Color accentColor, Color bgDark) {
    int screenCenterX = (int)(bounds.x + bounds.width / 2);
    int screenCenterY = (int)(bounds.y + bounds.height / 2 - 20);
    float clockRadius = 140.0f;

    // Draw clock face with rings
    DrawCircle(screenCenterX, screenCenterY, clockRadius + 8, accentColor);
    DrawCircle(screenCenterX, screenCenterY, clockRadius + 4, bgDark);
    DrawCircle(screenCenterX, screenCenterY, clockRadius, textColor);

    Color faceColor = bgDark;
    faceColor.a = 250;
    DrawCircle(screenCenterX, screenCenterY, clockRadius - 4, faceColor);

    // Draw hour markers
    for (int i = 0; i < 12; i++) {
        float angle = (i * 30.0f - 90.0f) * DEG2RAD;
        float markerLength = (i % 3 == 0) ? 15.0f : 8.0f;
        float markerWidth = (i % 3 == 0) ? 3.0f : 2.0f;

        float outerX = screenCenterX + cosf(angle) * (clockRadius - 12);
        float outerY = screenCenterY + sinf(angle) * (clockRadius - 12);
        float innerX = screenCenterX + cosf(angle) * (clockRadius - 12 - markerLength);
        float innerY = screenCenterY + sinf(angle) * (clockRadius - 12 - markerLength);

        DrawLineEx((Vector2){innerX, innerY}, (Vector2){outerX, outerY}, markerWidth, textColor);
    }

    // Calculate hand angles
    float secondAngle = (timeinfo->tm_sec * 6.0f - 90.0f) * DEG2RAD;
    float minuteAngle = ((timeinfo->tm_min + timeinfo->tm_sec / 60.0f) * 6.0f - 90.0f) * DEG2RAD;
    float hourAngle = (((timeinfo->tm_hour % 12) + timeinfo->tm_min / 60.0f) * 30.0f - 90.0f) * DEG2RAD;

    // Draw hour hand
    float hourLength = clockRadius * 0.5f;
    float hourEndX = screenCenterX + cosf(hourAngle) * hourLength;
    float hourEndY = screenCenterY + sinf(hourAngle) * hourLength;
    DrawLineEx((Vector2){screenCenterX, screenCenterY}, (Vector2){hourEndX, hourEndY}, 8.0f, textColor);

    // Draw minute hand
    float minuteLength = clockRadius * 0.7f;
    float minuteEndX = screenCenterX + cosf(minuteAngle) * minuteLength;
    float minuteEndY = screenCenterY + sinf(minuteAngle) * minuteLength;
    DrawLineEx((Vector2){screenCenterX, screenCenterY}, (Vector2){minuteEndX, minuteEndY}, 6.0f, textColor);

    // Draw second hand
    float secondLength = clockRadius * 0.75f;
    float secondEndX = screenCenterX + cosf(secondAngle) * secondLength;
    float secondEndY = screenCenterY + sinf(secondAngle) * secondLength;
    DrawLineEx((Vector2){screenCenterX, screenCenterY}, (Vector2){secondEndX, secondEndY}, 2.0f, accentColor);

    // Draw center circle
    DrawCircle(screenCenterX, screenCenterY, 8, accentColor);
    DrawCircle(screenCenterX, screenCenterY, 6, bgDark);

    // Draw date below clock
    Font font = NpThemeGetFont();
    float dateFontSize = 24.0f;
    float dateSpacing = 1.5f;
    Vector2 dateMeasure = MeasureTextEx(font, dateStr, dateFontSize, dateSpacing);
    Vector2 datePos = {
        screenCenterX - dateMeasure.x / 2,
        screenCenterY + clockRadius + 50
    };
    DrawTextEx(font, dateStr, datePos, dateFontSize, dateSpacing, textColor);
}

// Store current UI colors for volume overlay
static const NpAlbumArtUIColors *s_clockUIColors = NULL;

// Draw volume overlay (when scrolling)
static void DrawVolumeOverlay(Rectangle bounds, int volume, float alpha) {
    if (alpha <= 0.0f) return;

    Font font = NpThemeGetFont();
    Color textColor = NpThemeGetColorAlpha(NP_COLOR_TEXT_PRIMARY, alpha);
    Color accentColor;
    if (s_clockUIColors && s_clockUIColors->hasColors) {
        accentColor = ColorAlpha(s_clockUIColors->accent, alpha);
    } else {
        accentColor = NpThemeGetColorAlpha(NP_COLOR_ACCENT, alpha);
    }
    Color bgColor = NpThemeGetColorAlpha(NP_COLOR_BG_MEDIUM, alpha * 0.9f);

    // Calculate centered panel
    float panelWidth = 400;
    float panelHeight = 150;
    Rectangle panelBounds = {
        bounds.x + (bounds.width - panelWidth) * 0.5f,
        bounds.y + (bounds.height - panelHeight) * 0.5f,
        panelWidth,
        panelHeight
    };

    // Draw panel background
    DrawRectangleRounded(panelBounds, 0.15f, 12, bgColor);
    DrawRectangleRoundedLines(panelBounds, 0.15f, 12, ColorAlpha(accentColor, 0.5f));

    // Draw "Volume" label
    const char *label = "VOLUME";
    float labelSize = 24.0f;
    Vector2 labelMeasure = MeasureTextEx(font, label, labelSize, 1.5f);
    Vector2 labelPos = {
        panelBounds.x + (panelBounds.width - labelMeasure.x) / 2,
        panelBounds.y + 25
    };
    DrawTextEx(font, label, labelPos, labelSize, 1.5f, textColor);

    // Draw volume percentage
    char volumeStr[8];
    snprintf(volumeStr, sizeof(volumeStr), "%d%%", volume);
    float volumeSize = 48.0f;
    Vector2 volumeMeasure = MeasureTextEx(font, volumeStr, volumeSize, 2.0f);
    Vector2 volumePos = {
        panelBounds.x + (panelBounds.width - volumeMeasure.x) / 2,
        panelBounds.y + 65
    };
    DrawTextEx(font, volumeStr, volumePos, volumeSize, 2.0f, accentColor);

    // Draw progress bar
    float barWidth = panelWidth - 60;
    float barHeight = 8;
    Rectangle barBounds = {
        panelBounds.x + 30,
        panelBounds.y + panelHeight - 30,
        barWidth,
        barHeight
    };
    DrawRectangleRounded(barBounds, 0.5f, 4, ColorAlpha(textColor, 0.2f * alpha));

    Rectangle fillBounds = barBounds;
    fillBounds.width = barWidth * (volume / 100.0f);
    DrawRectangleRounded(fillBounds, 0.5f, 4, accentColor);
}

void NpClockOverlayInit(NpClockOverlay *overlay) {
    overlay->bounds = (Rectangle){0, 0, 800, 480};
    overlay->currentStyle = NP_CLOCK_STYLE_DIGITAL;
    overlay->volumeOverlayAlpha = 0.0f;
    overlay->volumeOverlayTimeout = 0.0f;
    overlay->lastVolume = 50;
}

void NpClockOverlayUpdate(NpClockOverlay *overlay, float deltaTime) {
    // Update volume overlay fade out
    if (overlay->volumeOverlayTimeout > 0.0f) {
        overlay->volumeOverlayTimeout -= deltaTime;
        if (overlay->volumeOverlayTimeout <= 0.0f) {
            overlay->volumeOverlayTimeout = 0.0f;
        }
    }

    // Calculate volume overlay alpha (fade out in last 0.5 seconds)
    if (overlay->volumeOverlayTimeout > 0.5f) {
        overlay->volumeOverlayAlpha = 1.0f;
    } else if (overlay->volumeOverlayTimeout > 0.0f) {
        overlay->volumeOverlayAlpha = overlay->volumeOverlayTimeout / 0.5f;
    } else {
        overlay->volumeOverlayAlpha = 0.0f;
    }
}

void NpClockOverlayDraw(const NpClockOverlay *overlay, float alpha, const NpAlbumArtUIColors *uiColors) {
    // Store for use in volume overlay
    s_clockUIColors = uiColors;

    // Get current time
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    // Format time in 12-hour format
    int hour12 = timeinfo->tm_hour % 12;
    if (hour12 == 0) hour12 = 12;  // Convert 0 to 12

    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour12, timeinfo->tm_min);

    // Format date
    const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const char *months[] = {"January", "February", "March", "April", "May", "June",
                           "July", "August", "September", "October", "November", "December"};

    char dateStr[64];
    snprintf(dateStr, sizeof(dateStr), "%s, %s %d",
             weekdays[timeinfo->tm_wday],
             months[timeinfo->tm_mon],
             timeinfo->tm_mday);

    // Get colors - use album art colors if available, otherwise theme colors
    Color textColor = NpThemeGetColorAlpha(NP_COLOR_TEXT_PRIMARY, alpha);
    Color textSecondary = NpThemeGetColorAlpha(NP_COLOR_TEXT_SECONDARY, alpha);
    Color accent;
    if (uiColors && uiColors->hasColors) {
        accent = ColorAlpha(uiColors->accent, alpha);
    } else {
        accent = NpThemeGetColorAlpha(NP_COLOR_ACCENT, alpha);
    }
    Color bgDark = NpThemeGetColor(NP_COLOR_BG_DARK);

    // Draw the selected clock style
    switch (overlay->currentStyle) {
        case NP_CLOCK_STYLE_DIGITAL:
            DrawClockStyleDigital(overlay->bounds, timeinfo, timeStr, dateStr, textColor, accent);
            break;
        case NP_CLOCK_STYLE_FULLSCREEN:
            DrawClockStyleFullscreen(overlay->bounds, timeinfo, timeStr, dateStr, textColor, accent);
            break;
        case NP_CLOCK_STYLE_ANALOG:
            DrawClockStyleAnalog(overlay->bounds, timeinfo, dateStr, textColor, accent, bgDark);
            break;
        default:
            break;
    }

    // Draw style indicator at bottom
    Font font = NpThemeGetFont();
    int screenCenterX = (int)(overlay->bounds.x + overlay->bounds.width / 2);

    // Draw subtle accent line
    int lineWidth = 300;
    int lineY = (int)(overlay->bounds.y + overlay->bounds.height - 140);
    DrawRectangleRounded((Rectangle){ screenCenterX - lineWidth / 2.0f, lineY, lineWidth, 2 },
                         0.5f, 4, accent);

    // Draw clock style name
    char styleText[64];
    snprintf(styleText, sizeof(styleText), "%s Clock", CLOCK_STYLE_NAMES[overlay->currentStyle]);

    float styleFontSize = 18.0f;
    float styleSpacing = 1.2f;
    Vector2 styleMeasure = MeasureTextEx(font, styleText, styleFontSize, styleSpacing);
    Vector2 stylePos = {
        screenCenterX - styleMeasure.x / 2,
        lineY + 20
    };
    DrawTextEx(font, styleText, stylePos, styleFontSize, styleSpacing, textSecondary);

    // Draw hint text at bottom
    const char *hintText = "Press button to cycle clock style";
    float hintFontSize = 14.0f;
    float hintSpacing = 1.0f;
    Vector2 hintMeasure = MeasureTextEx(font, hintText, hintFontSize, hintSpacing);
    Vector2 hintPos = {
        screenCenterX - hintMeasure.x / 2,
        overlay->bounds.y + overlay->bounds.height - 60
    };
    Color hintColor = textSecondary;
    hintColor.a = (unsigned char)(hintColor.a * 0.7f);
    DrawTextEx(font, hintText, hintPos, hintFontSize, hintSpacing, hintColor);

    // Draw volume overlay if active
    if (overlay->volumeOverlayAlpha > 0.0f) {
        DrawVolumeOverlay(overlay->bounds, overlay->lastVolume, overlay->volumeOverlayAlpha);
    }
}

void NpClockOverlayCycleStyle(NpClockOverlay *overlay) {
    overlay->currentStyle = (NpClockStyle)((overlay->currentStyle + 1) % NP_CLOCK_STYLE_COUNT);
}

void NpClockOverlayShowVolume(NpClockOverlay *overlay, int volume) {
    overlay->lastVolume = volume;
    overlay->volumeOverlayTimeout = 2.0f;  // Show for 2 seconds
    overlay->volumeOverlayAlpha = 1.0f;
}
