#ifndef LLZ_NOTIFICATION_TYPES_H
#define LLZ_NOTIFICATION_TYPES_H

#include <stdbool.h>
#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===== Constants =====

#define LLZ_NOTIFY_TEXT_MAX 256
#define LLZ_NOTIFY_ICON_MAX 8
#define LLZ_NOTIFY_BUTTON_TEXT_MAX 32
#define LLZ_NOTIFY_MAX_BUTTONS 3
#define LLZ_NOTIFY_PLUGIN_NAME_MAX 128
#define LLZ_NOTIFY_QUEUE_MAX 16

// ===== Enumerations =====

// Notification display style
typedef enum {
    LLZ_NOTIFY_BANNER,    // Horizontal bar at screen edge (top or bottom)
    LLZ_NOTIFY_TOAST,     // Small popup in a corner
    LLZ_NOTIFY_DIALOG     // Centered modal with buttons
} LlzNotifyStyle;

// Position for banners and toasts
typedef enum {
    LLZ_NOTIFY_POS_TOP,
    LLZ_NOTIFY_POS_BOTTOM,
    LLZ_NOTIFY_POS_TOP_LEFT,
    LLZ_NOTIFY_POS_TOP_RIGHT,
    LLZ_NOTIFY_POS_BOTTOM_LEFT,
    LLZ_NOTIFY_POS_BOTTOM_RIGHT
} LlzNotifyPosition;

// Animation state machine
typedef enum {
    LLZ_NOTIFY_ANIM_NONE,
    LLZ_NOTIFY_ANIM_FADE_IN,
    LLZ_NOTIFY_ANIM_VISIBLE,
    LLZ_NOTIFY_ANIM_FADE_OUT
} LlzNotifyAnimState;

// ===== Callback Types =====

// Generic callback (for onTap, onTimeout)
typedef void (*LlzNotifyCallback)(void *userData);

// Dismiss callback with reason
typedef void (*LlzNotifyDismissCallback)(bool wasTapped, void *userData);

// Button press callback for dialogs
typedef void (*LlzNotifyButtonCallback)(int buttonIndex, void *userData);

// ===== Structures =====

// Dialog button definition
typedef struct {
    char text[LLZ_NOTIFY_BUTTON_TEXT_MAX];
    Color bgColor;
    Color textColor;
    bool isPrimary;
} LlzNotifyButton;

// Notification configuration (user-facing)
typedef struct {
    LlzNotifyStyle style;
    LlzNotifyPosition position;

    // Content
    char title[LLZ_NOTIFY_TEXT_MAX];      // Title (mainly for dialogs)
    char message[LLZ_NOTIFY_TEXT_MAX];    // Main text content
    char iconText[LLZ_NOTIFY_ICON_MAX];   // Unicode icon (e.g., "???", "!", "???")

    // Timing
    float duration;           // Display duration in seconds (0 = until dismissed manually)
    float fadeInDuration;     // Fade in time (default 0.25f)
    float fadeOutDuration;    // Fade out time (default 0.2f)

    // Appearance
    Color bgColor;            // Background color
    Color textColor;          // Text color
    Color accentColor;        // Accent/border color
    float cornerRadius;       // Rounded corner radius (0.0 - 0.5)

    // Callbacks
    LlzNotifyCallback onTap;              // Called when notification is tapped
    LlzNotifyDismissCallback onDismiss;   // Called when dismissed (tap or timeout)
    LlzNotifyCallback onTimeout;          // Called specifically on timeout
    void *callbackUserData;               // User data passed to callbacks

    // Dialog-specific options
    LlzNotifyButton buttons[LLZ_NOTIFY_MAX_BUTTONS];
    int buttonCount;
    LlzNotifyButtonCallback onButtonPress;
    bool dismissOnTapOutside;             // For dialogs: dismiss when tapping outside

    // Navigation (optional)
    char openPluginOnTap[LLZ_NOTIFY_PLUGIN_NAME_MAX];  // Plugin name to open on tap
} LlzNotifyConfig;

// Internal notification state (not exposed to users)
typedef struct {
    LlzNotifyConfig config;
    LlzNotifyAnimState animState;
    float elapsed;            // Time in current animation state
    float totalVisible;       // Total time visible (for timeout)
    float alpha;              // Current alpha (0.0 - 1.0)
    Rectangle bounds;         // Computed bounds for hit testing
    Rectangle buttonRects[LLZ_NOTIFY_MAX_BUTTONS];  // Button rectangles for dialogs
    bool active;              // Is this notification slot active
    int id;                   // Unique notification ID
} LlzNotification;

// Notification queue (internal)
typedef struct {
    LlzNotifyConfig queue[LLZ_NOTIFY_QUEUE_MAX];
    int head;            // Next slot to read
    int tail;            // Next slot to write
    int count;           // Current queue size
    int nextId;          // Auto-incrementing ID
} LlzNotifyQueue;

#ifdef __cplusplus
}
#endif

#endif // LLZ_NOTIFICATION_TYPES_H
