#include "llz_notification.h"
#include "llz_sdk_navigation.h"
#include <string.h>
#include <stdio.h>

// ===== External render functions (implemented in notification_render.c) =====
extern void LlzNotifyComputeBounds(LlzNotification *notif, int screenWidth, int screenHeight);
extern void LlzNotifyRenderNotification(const LlzNotification *notif, int screenWidth, int screenHeight);

// ===== Global State =====
static LlzNotifyQueue g_queue;
static LlzNotification g_current;
static int g_screenWidth = 800;
static int g_screenHeight = 480;
static bool g_initialized = false;

// Animation constants (matching 2048 ticker pattern)
static const float DEFAULT_FADE_SPEED = 4.0f;
static const float DEFAULT_DURATION = 5.0f;
static const float DEFAULT_FADE_IN = 0.25f;
static const float DEFAULT_FADE_OUT = 0.2f;

// ===== Queue Operations =====

static void QueueInit(LlzNotifyQueue *q)
{
    memset(q, 0, sizeof(LlzNotifyQueue));
    q->nextId = 1;
}

static int QueuePush(LlzNotifyQueue *q, const LlzNotifyConfig *config)
{
    if (q->count >= LLZ_NOTIFY_QUEUE_MAX) {
        return 0;  // Queue full
    }

    q->queue[q->tail] = *config;
    q->tail = (q->tail + 1) % LLZ_NOTIFY_QUEUE_MAX;
    q->count++;

    return q->nextId++;
}

static bool QueuePop(LlzNotifyQueue *q, LlzNotifyConfig *out)
{
    if (q->count == 0) {
        return false;
    }

    if (out) {
        *out = q->queue[q->head];
    }
    q->head = (q->head + 1) % LLZ_NOTIFY_QUEUE_MAX;
    q->count--;

    return true;
}

static void QueueClear(LlzNotifyQueue *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

// ===== Lifecycle =====

void LlzNotifyInit(int screenWidth, int screenHeight)
{
    g_screenWidth = screenWidth;
    g_screenHeight = screenHeight;
    QueueInit(&g_queue);
    memset(&g_current, 0, sizeof(g_current));
    g_current.active = false;
    g_initialized = true;
}

void LlzNotifyShutdown(void)
{
    QueueClear(&g_queue);
    memset(&g_current, 0, sizeof(g_current));
    g_initialized = false;
}

// ===== Configuration =====

LlzNotifyConfig LlzNotifyConfigDefault(LlzNotifyStyle style)
{
    LlzNotifyConfig config;
    memset(&config, 0, sizeof(config));

    config.style = style;
    config.duration = DEFAULT_DURATION;
    config.fadeInDuration = DEFAULT_FADE_IN;
    config.fadeOutDuration = DEFAULT_FADE_OUT;
    config.bgColor = (Color){20, 20, 30, 245};
    config.textColor = WHITE;
    config.accentColor = (Color){88, 166, 255, 255};
    config.cornerRadius = 0.15f;

    switch (style) {
        case LLZ_NOTIFY_BANNER:
            config.position = LLZ_NOTIFY_POS_TOP;
            break;
        case LLZ_NOTIFY_TOAST:
            config.position = LLZ_NOTIFY_POS_BOTTOM_RIGHT;
            break;
        case LLZ_NOTIFY_DIALOG:
            config.dismissOnTapOutside = true;
            config.duration = 0;  // Dialogs don't auto-dismiss
            break;
    }

    return config;
}

// ===== Internal Helpers =====

static void ActivateNotification(const LlzNotifyConfig *config, int id)
{
    g_current.config = *config;
    g_current.animState = LLZ_NOTIFY_ANIM_FADE_IN;
    g_current.elapsed = 0.0f;
    g_current.totalVisible = 0.0f;
    g_current.alpha = 0.0f;
    g_current.active = true;
    g_current.id = id;

    LlzNotifyComputeBounds(&g_current, g_screenWidth, g_screenHeight);
}

static void DismissCurrentInternal(bool wasTapped)
{
    if (!g_current.active) return;

    g_current.animState = LLZ_NOTIFY_ANIM_FADE_OUT;
    g_current.elapsed = 0.0f;

    // Fire dismiss callback (but only once when starting fade out)
    // The actual callback firing happens when animation completes
}

static void CompleteCurrentNotification(bool wasTapped)
{
    if (!g_current.active) return;

    // Fire dismiss callback
    if (g_current.config.onDismiss) {
        g_current.config.onDismiss(wasTapped, g_current.config.callbackUserData);
    }

    g_current.active = false;
    g_current.alpha = 0.0f;
}

static void TryAdvanceQueue(void)
{
    if (g_current.active) return;

    LlzNotifyConfig nextConfig;
    if (QueuePop(&g_queue, &nextConfig)) {
        ActivateNotification(&nextConfig, g_queue.nextId++);
    }
}

// ===== Showing Notifications =====

int LlzNotifyShow(const LlzNotifyConfig *config)
{
    if (!g_initialized || !config) return 0;

    // If nothing is showing, display immediately
    if (!g_current.active) {
        int id = g_queue.nextId++;
        ActivateNotification(config, id);
        return id;
    }

    // Otherwise queue it
    return QueuePush(&g_queue, config);
}

int LlzNotifyBanner(const char *message, float duration, LlzNotifyPosition position)
{
    LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_BANNER);
    if (message) {
        strncpy(config.message, message, LLZ_NOTIFY_TEXT_MAX - 1);
    }
    config.duration = duration;
    config.position = position;
    return LlzNotifyShow(&config);
}

int LlzNotifyBannerWithIcon(const char *message, const char *icon, float duration, LlzNotifyPosition position)
{
    LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_BANNER);
    if (message) {
        strncpy(config.message, message, LLZ_NOTIFY_TEXT_MAX - 1);
    }
    if (icon) {
        strncpy(config.iconText, icon, LLZ_NOTIFY_ICON_MAX - 1);
    }
    config.duration = duration;
    config.position = position;
    return LlzNotifyShow(&config);
}

int LlzNotifyToast(const char *message, float duration, LlzNotifyPosition position)
{
    LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_TOAST);
    if (message) {
        strncpy(config.message, message, LLZ_NOTIFY_TEXT_MAX - 1);
    }
    config.duration = duration;
    config.position = position;
    return LlzNotifyShow(&config);
}

int LlzNotifyToastWithIcon(const char *message, const char *icon, float duration, LlzNotifyPosition position)
{
    LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_TOAST);
    if (message) {
        strncpy(config.message, message, LLZ_NOTIFY_TEXT_MAX - 1);
    }
    if (icon) {
        strncpy(config.iconText, icon, LLZ_NOTIFY_ICON_MAX - 1);
    }
    config.duration = duration;
    config.position = position;
    return LlzNotifyShow(&config);
}

int LlzNotifyDialog(const char *title, const char *message,
                    const char *buttons[], int buttonCount,
                    LlzNotifyButtonCallback onButton, void *userData)
{
    LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_DIALOG);

    if (title) {
        strncpy(config.title, title, LLZ_NOTIFY_TEXT_MAX - 1);
    }
    if (message) {
        strncpy(config.message, message, LLZ_NOTIFY_TEXT_MAX - 1);
    }

    config.buttonCount = (buttonCount > LLZ_NOTIFY_MAX_BUTTONS)
                         ? LLZ_NOTIFY_MAX_BUTTONS : buttonCount;

    for (int i = 0; i < config.buttonCount; i++) {
        if (buttons[i]) {
            strncpy(config.buttons[i].text, buttons[i], LLZ_NOTIFY_BUTTON_TEXT_MAX - 1);
        }
        // Last button is primary (usually "OK" or "Confirm")
        bool isPrimary = (i == config.buttonCount - 1);
        config.buttons[i].bgColor = isPrimary
                                    ? (Color){96, 178, 255, 255}
                                    : (Color){60, 60, 80, 255};
        config.buttons[i].textColor = WHITE;
        config.buttons[i].isPrimary = isPrimary;
    }

    config.onButtonPress = onButton;
    config.callbackUserData = userData;
    config.duration = 0;  // Dialogs don't auto-dismiss

    return LlzNotifyShow(&config);
}

// ===== Input Handling =====

static void HandleBannerToastInput(const LlzInputState *input)
{
    if (!input) return;

    bool tapped = input->tap;
    Vector2 tapPos = input->tapPosition;

    // Also check mouse click for desktop
    if (!tapped && input->mouseJustReleased) {
        tapped = true;
        tapPos = input->mousePos;
    }

    if (!tapped) return;

    // Check if tap is within bounds
    if (CheckCollisionPointRec(tapPos, g_current.bounds)) {
        // Fire tap callback
        if (g_current.config.onTap) {
            g_current.config.onTap(g_current.config.callbackUserData);
        }

        // Handle plugin navigation
        if (g_current.config.openPluginOnTap[0] != '\0') {
            LlzRequestOpenPlugin(g_current.config.openPluginOnTap);
        }

        DismissCurrentInternal(true);
    }
}

static void HandleDialogInput(const LlzInputState *input)
{
    if (!input) return;

    bool tapped = input->tap;
    Vector2 tapPos = input->tapPosition;

    // Also check mouse click for desktop
    if (!tapped && input->mouseJustReleased) {
        tapped = true;
        tapPos = input->mousePos;
    }

    if (!tapped) return;

    // Check button hits
    for (int i = 0; i < g_current.config.buttonCount; i++) {
        if (CheckCollisionPointRec(tapPos, g_current.buttonRects[i])) {
            if (g_current.config.onButtonPress) {
                g_current.config.onButtonPress(i, g_current.config.callbackUserData);
            }
            DismissCurrentInternal(true);
            return;
        }
    }

    // Check tap outside dialog
    if (g_current.config.dismissOnTapOutside &&
        !CheckCollisionPointRec(tapPos, g_current.bounds)) {
        DismissCurrentInternal(false);
    }
}

static void HandleNotificationInput(const LlzInputState *input)
{
    if (g_current.config.style == LLZ_NOTIFY_DIALOG) {
        HandleDialogInput(input);
    } else {
        HandleBannerToastInput(input);
    }
}

// ===== Update =====

bool LlzNotifyUpdate(const LlzInputState *input, float deltaTime)
{
    if (!g_initialized) return false;

    // Try to advance queue if nothing is active
    if (!g_current.active) {
        TryAdvanceQueue();
        return false;
    }

    g_current.elapsed += deltaTime;

    // Update animation state machine
    switch (g_current.animState) {
        case LLZ_NOTIFY_ANIM_FADE_IN: {
            float fadeSpeed = 1.0f / g_current.config.fadeInDuration;
            if (g_current.config.fadeInDuration <= 0) fadeSpeed = DEFAULT_FADE_SPEED;

            g_current.alpha += deltaTime * fadeSpeed;
            if (g_current.alpha >= 1.0f) {
                g_current.alpha = 1.0f;
                g_current.animState = LLZ_NOTIFY_ANIM_VISIBLE;
                g_current.elapsed = 0.0f;
            }
            break;
        }

        case LLZ_NOTIFY_ANIM_VISIBLE:
            g_current.totalVisible += deltaTime;

            // Check timeout (duration > 0 means auto-dismiss)
            if (g_current.config.duration > 0.0f &&
                g_current.totalVisible >= g_current.config.duration) {

                // Fire timeout callback
                if (g_current.config.onTimeout) {
                    g_current.config.onTimeout(g_current.config.callbackUserData);
                }

                DismissCurrentInternal(false);
            }
            break;

        case LLZ_NOTIFY_ANIM_FADE_OUT: {
            float fadeSpeed = 1.0f / g_current.config.fadeOutDuration;
            if (g_current.config.fadeOutDuration <= 0) fadeSpeed = DEFAULT_FADE_SPEED;

            g_current.alpha -= deltaTime * fadeSpeed;
            if (g_current.alpha <= 0.0f) {
                g_current.alpha = 0.0f;
                CompleteCurrentNotification(false);
                TryAdvanceQueue();
            }
            break;
        }

        default:
            break;
    }

    // Handle input only when sufficiently visible (matching ticker pattern: alpha > 0.5)
    if (g_current.alpha > 0.5f && input) {
        HandleNotificationInput(input);
    }

    return g_current.active;
}

// ===== Draw =====

void LlzNotifyDraw(void)
{
    if (!g_initialized || !g_current.active || g_current.alpha <= 0.01f) return;

    LlzNotifyRenderNotification(&g_current, g_screenWidth, g_screenHeight);
}

// ===== Queries =====

bool LlzNotifyIsVisible(void)
{
    return g_initialized && g_current.active && g_current.alpha > 0.01f;
}

bool LlzNotifyIsBlocking(void)
{
    return g_initialized && g_current.active &&
           g_current.config.style == LLZ_NOTIFY_DIALOG &&
           g_current.alpha > 0.5f;
}

float LlzNotifyGetAlpha(void)
{
    if (!g_initialized || !g_current.active) return 0.0f;
    return g_current.alpha;
}

int LlzNotifyGetCurrentId(void)
{
    if (!g_initialized || !g_current.active) return 0;
    return g_current.id;
}

int LlzNotifyGetQueueCount(void)
{
    if (!g_initialized) return 0;
    return g_queue.count;
}

// ===== Control =====

void LlzNotifyDismissCurrent(void)
{
    if (!g_initialized || !g_current.active) return;
    DismissCurrentInternal(false);
}

bool LlzNotifyDismiss(int notificationId)
{
    if (!g_initialized) return false;

    // Check if current notification matches
    if (g_current.active && g_current.id == notificationId) {
        DismissCurrentInternal(false);
        return true;
    }

    // TODO: Could search queue and remove, but for simplicity just return false
    return false;
}

void LlzNotifyClearQueue(void)
{
    if (!g_initialized) return;
    QueueClear(&g_queue);
}

void LlzNotifyClearAll(void)
{
    if (!g_initialized) return;

    if (g_current.active) {
        g_current.active = false;
        g_current.alpha = 0.0f;
    }
    QueueClear(&g_queue);
}
