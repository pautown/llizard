#ifndef LLZ_SDK_SUBSCRIBE_H
#define LLZ_SDK_SUBSCRIBE_H

#include "llz_sdk_media.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of subscriptions per event type
#define LLZ_MAX_SUBSCRIPTIONS 8

// Event types for subscription
typedef enum {
    LLZ_EVENT_TRACK_CHANGED,      // New track started (title, artist, or album changed)
    LLZ_EVENT_PLAYSTATE_CHANGED,  // Play/pause state changed
    LLZ_EVENT_VOLUME_CHANGED,     // Volume level changed
    LLZ_EVENT_POSITION_CHANGED,   // Playback position changed (frequent, ~1 update/sec)
    LLZ_EVENT_CONNECTION_CHANGED, // BLE connection status changed
    LLZ_EVENT_ALBUM_ART_CHANGED,  // Album art path changed
    LLZ_EVENT_NOTIFICATION,       // Generic notification from system
    LLZ_EVENT_COUNT
} LlzEventType;

// Callback type definitions - each event type has a specific callback signature
// All callbacks receive userData pointer passed during subscription

// Track changed: title, artist, or album changed
typedef void (*LlzTrackChangedCallback)(
    const char *track,
    const char *artist,
    const char *album,
    void *userData
);

// Playstate changed: playing/paused
typedef void (*LlzPlaystateChangedCallback)(
    bool isPlaying,
    void *userData
);

// Volume changed
typedef void (*LlzVolumeChangedCallback)(
    int volumePercent,
    void *userData
);

// Position changed (playback progress)
typedef void (*LlzPositionChangedCallback)(
    int positionSeconds,
    int durationSeconds,
    void *userData
);

// Connection status changed
typedef void (*LlzConnectionChangedCallback)(
    bool connected,
    const char *deviceName,
    void *userData
);

// Album art changed
typedef void (*LlzAlbumArtChangedCallback)(
    const char *artPath,
    void *userData
);

// Notification types for generic notifications
typedef enum {
    LLZ_NOTIFY_INFO,
    LLZ_NOTIFY_WARNING,
    LLZ_NOTIFY_ERROR,
    LLZ_NOTIFY_SYSTEM
} LlzNotifyLevel;

// Generic notification
typedef void (*LlzNotificationCallback)(
    LlzNotifyLevel level,
    const char *source,
    const char *message,
    void *userData
);

// Subscription ID (0 = invalid/failed)
typedef int LlzSubscriptionId;

// Subscribe to track changes (title, artist, album)
// Returns subscription ID, or 0 on failure
LlzSubscriptionId LlzSubscribeTrackChanged(LlzTrackChangedCallback callback, void *userData);

// Subscribe to playstate changes (play/pause)
LlzSubscriptionId LlzSubscribePlaystateChanged(LlzPlaystateChangedCallback callback, void *userData);

// Subscribe to volume changes
LlzSubscriptionId LlzSubscribeVolumeChanged(LlzVolumeChangedCallback callback, void *userData);

// Subscribe to position changes (frequent updates during playback)
// Note: Position updates are rate-limited to ~1 per second to avoid excessive callbacks
LlzSubscriptionId LlzSubscribePositionChanged(LlzPositionChangedCallback callback, void *userData);

// Subscribe to connection status changes
LlzSubscriptionId LlzSubscribeConnectionChanged(LlzConnectionChangedCallback callback, void *userData);

// Subscribe to album art changes
LlzSubscriptionId LlzSubscribeAlbumArtChanged(LlzAlbumArtChangedCallback callback, void *userData);

// Subscribe to system notifications
LlzSubscriptionId LlzSubscribeNotification(LlzNotificationCallback callback, void *userData);

// Unsubscribe by ID
// Safe to call with invalid ID (no-op)
void LlzUnsubscribe(LlzSubscriptionId id);

// Unsubscribe all callbacks for a given event type
void LlzUnsubscribeAll(LlzEventType eventType);

// Poll for changes and dispatch callbacks
// Should be called in plugin update loop (e.g., every frame or at desired poll rate)
// This checks Redis for changes and calls registered callbacks
void LlzSubscriptionPoll(void);

// Post a notification programmatically (useful for plugins to notify each other)
void LlzPostNotification(LlzNotifyLevel level, const char *source, const char *message);

// Get current subscription count for an event type
int LlzGetSubscriptionCount(LlzEventType eventType);

// Check if any subscriptions are active
bool LlzHasActiveSubscriptions(void);

#ifdef __cplusplus
}
#endif

#endif // LLZ_SDK_SUBSCRIBE_H
