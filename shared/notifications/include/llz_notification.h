#ifndef LLZ_NOTIFICATION_H
#define LLZ_NOTIFICATION_H

#include "llz_notification_types.h"
#include "llz_sdk_input.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===== Initialization & Lifecycle =====

// Initialize the notification system with screen dimensions
void LlzNotifyInit(int screenWidth, int screenHeight);

// Shutdown and cleanup
void LlzNotifyShutdown(void);

// ===== Configuration Helpers =====

// Get a default configuration for a style (call before customizing)
LlzNotifyConfig LlzNotifyConfigDefault(LlzNotifyStyle style);

// ===== Showing Notifications =====

// Show a notification with full configuration
// Returns notification ID (> 0) on success, 0 on failure
int LlzNotifyShow(const LlzNotifyConfig *config);

// Convenience: Show a simple banner notification
// Returns notification ID (> 0) on success, 0 on failure
int LlzNotifyBanner(const char *message, float duration, LlzNotifyPosition position);

// Convenience: Show a banner with icon
int LlzNotifyBannerWithIcon(const char *message, const char *icon, float duration, LlzNotifyPosition position);

// Convenience: Show a toast notification
// Returns notification ID (> 0) on success, 0 on failure
int LlzNotifyToast(const char *message, float duration, LlzNotifyPosition position);

// Convenience: Show a toast with icon
int LlzNotifyToastWithIcon(const char *message, const char *icon, float duration, LlzNotifyPosition position);

// Convenience: Show a dialog with buttons
// Returns notification ID (> 0) on success, 0 on failure
int LlzNotifyDialog(const char *title, const char *message,
                    const char *buttons[], int buttonCount,
                    LlzNotifyButtonCallback onButton, void *userData);

// ===== Updating & Drawing =====

// Update notification system - call every frame
// Returns true if a notification is currently visible
bool LlzNotifyUpdate(const LlzInputState *input, float deltaTime);

// Draw current notification - call after your main draw
void LlzNotifyDraw(void);

// ===== Queries =====

// Check if any notification is currently visible
bool LlzNotifyIsVisible(void);

// Check if a dialog is currently blocking input
bool LlzNotifyIsBlocking(void);

// Get current notification alpha (for layering effects)
float LlzNotifyGetAlpha(void);

// Get current notification ID (0 if none)
int LlzNotifyGetCurrentId(void);

// Get number of queued notifications (not including current)
int LlzNotifyGetQueueCount(void);

// ===== Control =====

// Dismiss the current notification manually
void LlzNotifyDismissCurrent(void);

// Dismiss a specific notification by ID (returns true if found and dismissed)
bool LlzNotifyDismiss(int notificationId);

// Clear all queued notifications (does not dismiss current)
void LlzNotifyClearQueue(void);

// Clear everything including current notification
void LlzNotifyClearAll(void);

#ifdef __cplusplus
}
#endif

#endif // LLZ_NOTIFICATION_H
