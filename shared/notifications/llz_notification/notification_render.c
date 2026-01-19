#include "llz_notification_types.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>

// ===== Dimension Constants =====

#define BANNER_HEIGHT 44.0f
#define TOAST_WIDTH 280.0f
#define TOAST_HEIGHT 80.0f
#define TOAST_PADDING 16.0f
#define DIALOG_WIDTH 400.0f
#define DIALOG_MIN_HEIGHT 150.0f
#define BUTTON_HEIGHT 40.0f
#define PADDING 16.0f
#define ICON_SIZE 20
#define FONT_SIZE_LARGE 20
#define FONT_SIZE_MEDIUM 18
#define FONT_SIZE_SMALL 16

// ===== Helper Functions =====

static Color ColorWithAlpha(Color c, float alpha)
{
    return (Color){c.r, c.g, c.b, (unsigned char)(c.a * alpha)};
}

static void DrawTextCentered(const char *text, int x, int y, int width, int fontSize, Color color)
{
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, x + (width - textWidth) / 2, y, fontSize, color);
}

// ===== Bounds Computation =====

static void ComputeBannerBounds(LlzNotification *notif, int sw, int sh)
{
    float y = (notif->config.position == LLZ_NOTIFY_POS_TOP) ? 0 : (sh - BANNER_HEIGHT);
    notif->bounds = (Rectangle){0, y, (float)sw, BANNER_HEIGHT};
}

static void ComputeToastBounds(LlzNotification *notif, int sw, int sh)
{
    float x, y;

    switch (notif->config.position) {
        case LLZ_NOTIFY_POS_TOP:
            x = (sw - TOAST_WIDTH) / 2.0f;
            y = TOAST_PADDING;
            break;
        case LLZ_NOTIFY_POS_BOTTOM:
            x = (sw - TOAST_WIDTH) / 2.0f;
            y = sh - TOAST_HEIGHT - TOAST_PADDING;
            break;
        case LLZ_NOTIFY_POS_TOP_LEFT:
            x = TOAST_PADDING;
            y = TOAST_PADDING;
            break;
        case LLZ_NOTIFY_POS_TOP_RIGHT:
            x = sw - TOAST_WIDTH - TOAST_PADDING;
            y = TOAST_PADDING;
            break;
        case LLZ_NOTIFY_POS_BOTTOM_LEFT:
            x = TOAST_PADDING;
            y = sh - TOAST_HEIGHT - TOAST_PADDING;
            break;
        case LLZ_NOTIFY_POS_BOTTOM_RIGHT:
        default:
            x = sw - TOAST_WIDTH - TOAST_PADDING;
            y = sh - TOAST_HEIGHT - TOAST_PADDING;
            break;
    }

    notif->bounds = (Rectangle){x, y, TOAST_WIDTH, TOAST_HEIGHT};
}

static void ComputeDialogBounds(LlzNotification *notif, int sw, int sh)
{
    // Calculate height based on content
    float height = DIALOG_MIN_HEIGHT;

    // Add space for buttons if present
    if (notif->config.buttonCount > 0) {
        height += BUTTON_HEIGHT + PADDING;
    }

    // Add extra height if there's a title
    if (notif->config.title[0] != '\0') {
        height += 10;
    }

    notif->bounds = (Rectangle){
        (sw - DIALOG_WIDTH) / 2.0f,
        (sh - height) / 2.0f,
        DIALOG_WIDTH,
        height
    };

    // Compute button rectangles
    if (notif->config.buttonCount > 0) {
        float btnY = notif->bounds.y + notif->bounds.height - BUTTON_HEIGHT - PADDING;
        float totalBtnWidth = notif->bounds.width - PADDING * 2;
        float btnSpacing = 10.0f;
        float btnWidth = (totalBtnWidth - btnSpacing * (notif->config.buttonCount - 1)) / notif->config.buttonCount;

        for (int i = 0; i < notif->config.buttonCount; i++) {
            notif->buttonRects[i] = (Rectangle){
                notif->bounds.x + PADDING + i * (btnWidth + btnSpacing),
                btnY,
                btnWidth,
                BUTTON_HEIGHT
            };
        }
    }
}

void LlzNotifyComputeBounds(LlzNotification *notif, int screenWidth, int screenHeight)
{
    switch (notif->config.style) {
        case LLZ_NOTIFY_BANNER:
            ComputeBannerBounds(notif, screenWidth, screenHeight);
            break;
        case LLZ_NOTIFY_TOAST:
            ComputeToastBounds(notif, screenWidth, screenHeight);
            break;
        case LLZ_NOTIFY_DIALOG:
            ComputeDialogBounds(notif, screenWidth, screenHeight);
            break;
    }
}

// ===== Drawing Functions =====

static void DrawBanner(const LlzNotification *notif)
{
    float alpha = notif->alpha;
    Rectangle rect = notif->bounds;
    const LlzNotifyConfig *cfg = &notif->config;

    // Background
    Color bg = ColorWithAlpha(cfg->bgColor, alpha * 0.95f);
    DrawRectangleRec(rect, bg);

    // Accent bar at edge
    Color accent = ColorWithAlpha(cfg->accentColor, alpha);
    float barY = (cfg->position == LLZ_NOTIFY_POS_TOP)
                 ? rect.y + rect.height - 2
                 : rect.y;
    DrawRectangle((int)rect.x, (int)barY, (int)rect.width, 2, accent);

    // Content layout
    float textX = rect.x + PADDING;
    float centerY = rect.y + rect.height / 2;

    // Icon (if present)
    if (cfg->iconText[0] != '\0') {
        Color iconColor = ColorWithAlpha(cfg->accentColor, alpha);
        DrawText(cfg->iconText, (int)textX, (int)(centerY - ICON_SIZE / 2), ICON_SIZE, iconColor);
        textX += ICON_SIZE + 8;
    }

    // Message text
    Color textColor = ColorWithAlpha(cfg->textColor, alpha);

    // Truncate message if too long
    char displayMsg[LLZ_NOTIFY_TEXT_MAX];
    strncpy(displayMsg, cfg->message, sizeof(displayMsg) - 1);
    displayMsg[sizeof(displayMsg) - 1] = '\0';

    int maxWidth = (int)(rect.width - textX - PADDING - 100);  // Leave room for hint
    int msgWidth = MeasureText(displayMsg, FONT_SIZE_MEDIUM);

    if (msgWidth > maxWidth && maxWidth > 30) {
        // Truncate with ellipsis
        int len = strlen(displayMsg);
        while (len > 3 && MeasureText(displayMsg, FONT_SIZE_MEDIUM) > maxWidth) {
            displayMsg[--len] = '\0';
        }
        strcat(displayMsg, "...");
    }

    DrawText(displayMsg, (int)textX, (int)(centerY - FONT_SIZE_MEDIUM / 2), FONT_SIZE_MEDIUM, textColor);

    // Hint text on right (if there's a tap action)
    if (cfg->onTap || cfg->openPluginOnTap[0] != '\0') {
        Color hintColor = ColorWithAlpha((Color){150, 150, 160, 255}, alpha * 0.7f);
        const char *hint = "Tap to open";
        int hintWidth = MeasureText(hint, FONT_SIZE_SMALL - 2);
        DrawText(hint, (int)(rect.x + rect.width - hintWidth - PADDING),
                 (int)(centerY - (FONT_SIZE_SMALL - 2) / 2), FONT_SIZE_SMALL - 2, hintColor);
    }
}

static void DrawToast(const LlzNotification *notif)
{
    float alpha = notif->alpha;
    Rectangle rect = notif->bounds;
    const LlzNotifyConfig *cfg = &notif->config;

    // Rounded background
    Color bg = ColorWithAlpha(cfg->bgColor, alpha * 0.95f);
    DrawRectangleRounded(rect, cfg->cornerRadius, 12, bg);

    // Border
    Color border = ColorWithAlpha(cfg->accentColor, alpha * 0.4f);
    DrawRectangleRoundedLines(rect, cfg->cornerRadius, 12, border);

    // Content layout
    float contentX = rect.x + PADDING;
    float contentY = rect.y + PADDING;

    // Icon (if present)
    if (cfg->iconText[0] != '\0') {
        Color iconColor = ColorWithAlpha(cfg->accentColor, alpha);
        DrawText(cfg->iconText, (int)contentX, (int)contentY, ICON_SIZE + 4, iconColor);
        contentX += ICON_SIZE + 12;
    }

    // Message text (may wrap)
    Color textColor = ColorWithAlpha(cfg->textColor, alpha);

    // Simple text drawing (no word wrap for now)
    DrawText(cfg->message, (int)contentX, (int)(contentY + 4), FONT_SIZE_SMALL, textColor);
}

static void DrawDialog(const LlzNotification *notif, int screenWidth, int screenHeight)
{
    float alpha = notif->alpha;
    Rectangle rect = notif->bounds;
    const LlzNotifyConfig *cfg = &notif->config;

    // Dimmed backdrop (blocking modal)
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorWithAlpha(BLACK, 0.55f * alpha));

    // Dialog panel with shadow
    Color shadowColor = ColorWithAlpha(BLACK, 0.3f * alpha);
    DrawRectangleRounded(
        (Rectangle){rect.x + 4, rect.y + 4, rect.width, rect.height},
        0.08f, 16, shadowColor
    );

    // Main panel
    Color bg = ColorWithAlpha(cfg->bgColor, alpha);
    DrawRectangleRounded(rect, 0.08f, 16, bg);

    // Border
    Color borderColor = ColorWithAlpha(cfg->accentColor, alpha * 0.25f);
    DrawRectangleRoundedLines(rect, 0.08f, 16, borderColor);

    // Content
    float y = rect.y + PADDING;

    // Title
    if (cfg->title[0] != '\0') {
        Color titleColor = ColorWithAlpha(cfg->textColor, alpha);
        DrawText(cfg->title, (int)(rect.x + PADDING), (int)y, FONT_SIZE_LARGE, titleColor);
        y += FONT_SIZE_LARGE + 12;
    }

    // Message
    Color msgColor = ColorWithAlpha(cfg->textColor, alpha * 0.85f);
    DrawText(cfg->message, (int)(rect.x + PADDING), (int)y, FONT_SIZE_MEDIUM, msgColor);

    // Buttons
    for (int i = 0; i < cfg->buttonCount; i++) {
        Rectangle btnRect = notif->buttonRects[i];
        const LlzNotifyButton *btn = &cfg->buttons[i];

        // Button background
        Color btnBg = ColorWithAlpha(btn->bgColor, alpha);
        DrawRectangleRounded(btnRect, 0.25f, 10, btnBg);

        // Button text
        Color btnTextColor = ColorWithAlpha(btn->textColor, alpha);
        int textWidth = MeasureText(btn->text, FONT_SIZE_MEDIUM);
        DrawText(btn->text,
                 (int)(btnRect.x + (btnRect.width - textWidth) / 2),
                 (int)(btnRect.y + (btnRect.height - FONT_SIZE_MEDIUM) / 2),
                 FONT_SIZE_MEDIUM, btnTextColor);
    }
}

void LlzNotifyRenderNotification(const LlzNotification *notif, int screenWidth, int screenHeight)
{
    if (!notif || !notif->active || notif->alpha <= 0.01f) return;

    switch (notif->config.style) {
        case LLZ_NOTIFY_BANNER:
            DrawBanner(notif);
            break;
        case LLZ_NOTIFY_TOAST:
            DrawToast(notif);
            break;
        case LLZ_NOTIFY_DIALOG:
            DrawDialog(notif, screenWidth, screenHeight);
            break;
    }
}
