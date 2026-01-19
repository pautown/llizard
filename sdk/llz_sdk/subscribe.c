#include "llz_sdk_subscribe.h"
#include "llz_sdk_media.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Subscription storage
typedef struct {
    void *callback;
    void *userData;
    LlzSubscriptionId id;
    bool active;
} Subscription;

typedef struct {
    Subscription subs[LLZ_MAX_SUBSCRIPTIONS];
    int count;
} SubscriptionList;

// Global subscription state
static SubscriptionList g_subscriptions[LLZ_EVENT_COUNT];
static LlzSubscriptionId g_nextId = 1;
static bool g_initialized = false;

// Previous state for change detection
static LlzMediaState g_prevMedia;
static LlzConnectionStatus g_prevConnection;
static bool g_prevMediaValid = false;
static bool g_prevConnectionValid = false;

// Notification queue for programmatic notifications
#define MAX_PENDING_NOTIFICATIONS 16
typedef struct {
    LlzNotifyLevel level;
    char source[64];
    char message[256];
    bool pending;
} PendingNotification;

static PendingNotification g_notifications[MAX_PENDING_NOTIFICATIONS];
static int g_notificationHead = 0;
static int g_notificationTail = 0;

// Initialize subscription system (called internally)
static void llz_sub_init(void)
{
    if (g_initialized) return;

    memset(g_subscriptions, 0, sizeof(g_subscriptions));
    memset(&g_prevMedia, 0, sizeof(g_prevMedia));
    memset(&g_prevConnection, 0, sizeof(g_prevConnection));
    memset(g_notifications, 0, sizeof(g_notifications));

    g_nextId = 1;
    g_prevMediaValid = false;
    g_prevConnectionValid = false;
    g_notificationHead = 0;
    g_notificationTail = 0;
    g_initialized = true;
}

// Add a subscription to a list
static LlzSubscriptionId llz_sub_add(LlzEventType type, void *callback, void *userData)
{
    llz_sub_init();

    if (type >= LLZ_EVENT_COUNT) return 0;
    if (!callback) return 0;

    SubscriptionList *list = &g_subscriptions[type];

    // Find empty slot
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        if (!list->subs[i].active) {
            list->subs[i].callback = callback;
            list->subs[i].userData = userData;
            list->subs[i].id = g_nextId++;
            list->subs[i].active = true;
            list->count++;
            return list->subs[i].id;
        }
    }

    // No space available
    return 0;
}

// Remove a subscription by ID
static bool llz_sub_remove(LlzSubscriptionId id)
{
    if (id <= 0) return false;

    for (int type = 0; type < LLZ_EVENT_COUNT; type++) {
        SubscriptionList *list = &g_subscriptions[type];
        for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
            if (list->subs[i].active && list->subs[i].id == id) {
                list->subs[i].active = false;
                list->subs[i].callback = NULL;
                list->subs[i].userData = NULL;
                list->subs[i].id = 0;
                list->count--;
                return true;
            }
        }
    }

    return false;
}

// Public subscription functions
LlzSubscriptionId LlzSubscribeTrackChanged(LlzTrackChangedCallback callback, void *userData)
{
    return llz_sub_add(LLZ_EVENT_TRACK_CHANGED, (void *)callback, userData);
}

LlzSubscriptionId LlzSubscribePlaystateChanged(LlzPlaystateChangedCallback callback, void *userData)
{
    return llz_sub_add(LLZ_EVENT_PLAYSTATE_CHANGED, (void *)callback, userData);
}

LlzSubscriptionId LlzSubscribeVolumeChanged(LlzVolumeChangedCallback callback, void *userData)
{
    return llz_sub_add(LLZ_EVENT_VOLUME_CHANGED, (void *)callback, userData);
}

LlzSubscriptionId LlzSubscribePositionChanged(LlzPositionChangedCallback callback, void *userData)
{
    return llz_sub_add(LLZ_EVENT_POSITION_CHANGED, (void *)callback, userData);
}

LlzSubscriptionId LlzSubscribeConnectionChanged(LlzConnectionChangedCallback callback, void *userData)
{
    return llz_sub_add(LLZ_EVENT_CONNECTION_CHANGED, (void *)callback, userData);
}

LlzSubscriptionId LlzSubscribeAlbumArtChanged(LlzAlbumArtChangedCallback callback, void *userData)
{
    return llz_sub_add(LLZ_EVENT_ALBUM_ART_CHANGED, (void *)callback, userData);
}

LlzSubscriptionId LlzSubscribeNotification(LlzNotificationCallback callback, void *userData)
{
    return llz_sub_add(LLZ_EVENT_NOTIFICATION, (void *)callback, userData);
}

void LlzUnsubscribe(LlzSubscriptionId id)
{
    llz_sub_remove(id);
}

void LlzUnsubscribeAll(LlzEventType eventType)
{
    if (eventType >= LLZ_EVENT_COUNT) return;

    SubscriptionList *list = &g_subscriptions[eventType];
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        list->subs[i].active = false;
        list->subs[i].callback = NULL;
        list->subs[i].userData = NULL;
        list->subs[i].id = 0;
    }
    list->count = 0;
}

int LlzGetSubscriptionCount(LlzEventType eventType)
{
    if (eventType >= LLZ_EVENT_COUNT) return 0;
    return g_subscriptions[eventType].count;
}

bool LlzHasActiveSubscriptions(void)
{
    for (int type = 0; type < LLZ_EVENT_COUNT; type++) {
        if (g_subscriptions[type].count > 0) return true;
    }
    return false;
}

void LlzPostNotification(LlzNotifyLevel level, const char *source, const char *message)
{
    llz_sub_init();

    // Add to circular buffer
    int next = (g_notificationTail + 1) % MAX_PENDING_NOTIFICATIONS;
    if (next == g_notificationHead) {
        // Buffer full, drop oldest
        g_notificationHead = (g_notificationHead + 1) % MAX_PENDING_NOTIFICATIONS;
    }

    PendingNotification *notif = &g_notifications[g_notificationTail];
    notif->level = level;
    notif->pending = true;

    if (source) {
        strncpy(notif->source, source, sizeof(notif->source) - 1);
        notif->source[sizeof(notif->source) - 1] = '\0';
    } else {
        notif->source[0] = '\0';
    }

    if (message) {
        strncpy(notif->message, message, sizeof(notif->message) - 1);
        notif->message[sizeof(notif->message) - 1] = '\0';
    } else {
        notif->message[0] = '\0';
    }

    g_notificationTail = next;
}

// Dispatch callbacks for a specific event type
static void llz_dispatch_track_changed(const LlzMediaState *media)
{
    SubscriptionList *list = &g_subscriptions[LLZ_EVENT_TRACK_CHANGED];
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        if (list->subs[i].active && list->subs[i].callback) {
            LlzTrackChangedCallback cb = (LlzTrackChangedCallback)list->subs[i].callback;
            cb(media->track, media->artist, media->album, list->subs[i].userData);
        }
    }
}

static void llz_dispatch_playstate_changed(bool isPlaying)
{
    SubscriptionList *list = &g_subscriptions[LLZ_EVENT_PLAYSTATE_CHANGED];
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        if (list->subs[i].active && list->subs[i].callback) {
            LlzPlaystateChangedCallback cb = (LlzPlaystateChangedCallback)list->subs[i].callback;
            cb(isPlaying, list->subs[i].userData);
        }
    }
}

static void llz_dispatch_volume_changed(int volumePercent)
{
    SubscriptionList *list = &g_subscriptions[LLZ_EVENT_VOLUME_CHANGED];
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        if (list->subs[i].active && list->subs[i].callback) {
            LlzVolumeChangedCallback cb = (LlzVolumeChangedCallback)list->subs[i].callback;
            cb(volumePercent, list->subs[i].userData);
        }
    }
}

static void llz_dispatch_position_changed(int positionSeconds, int durationSeconds)
{
    SubscriptionList *list = &g_subscriptions[LLZ_EVENT_POSITION_CHANGED];
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        if (list->subs[i].active && list->subs[i].callback) {
            LlzPositionChangedCallback cb = (LlzPositionChangedCallback)list->subs[i].callback;
            cb(positionSeconds, durationSeconds, list->subs[i].userData);
        }
    }
}

static void llz_dispatch_connection_changed(bool connected, const char *deviceName)
{
    SubscriptionList *list = &g_subscriptions[LLZ_EVENT_CONNECTION_CHANGED];
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        if (list->subs[i].active && list->subs[i].callback) {
            LlzConnectionChangedCallback cb = (LlzConnectionChangedCallback)list->subs[i].callback;
            cb(connected, deviceName, list->subs[i].userData);
        }
    }
}

static void llz_dispatch_album_art_changed(const char *artPath)
{
    SubscriptionList *list = &g_subscriptions[LLZ_EVENT_ALBUM_ART_CHANGED];
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        if (list->subs[i].active && list->subs[i].callback) {
            LlzAlbumArtChangedCallback cb = (LlzAlbumArtChangedCallback)list->subs[i].callback;
            cb(artPath, list->subs[i].userData);
        }
    }
}

static void llz_dispatch_notification(const PendingNotification *notif)
{
    SubscriptionList *list = &g_subscriptions[LLZ_EVENT_NOTIFICATION];
    for (int i = 0; i < LLZ_MAX_SUBSCRIPTIONS; i++) {
        if (list->subs[i].active && list->subs[i].callback) {
            LlzNotificationCallback cb = (LlzNotificationCallback)list->subs[i].callback;
            cb(notif->level, notif->source, notif->message, list->subs[i].userData);
        }
    }
}

// Check if track info changed
static bool llz_track_changed(const LlzMediaState *a, const LlzMediaState *b)
{
    if (strcmp(a->track, b->track) != 0) return true;
    if (strcmp(a->artist, b->artist) != 0) return true;
    if (strcmp(a->album, b->album) != 0) return true;
    return false;
}

void LlzSubscriptionPoll(void)
{
    llz_sub_init();

    // Skip if no subscriptions active
    if (!LlzHasActiveSubscriptions()) return;

    // Fetch current media state
    LlzMediaState currentMedia;
    bool mediaValid = LlzMediaGetState(&currentMedia);

    // Fetch current connection status
    LlzConnectionStatus currentConnection;
    bool connectionValid = LlzMediaGetConnection(&currentConnection);

    // Check for media changes
    if (mediaValid) {
        if (!g_prevMediaValid) {
            // First valid state - dispatch all events
            if (g_subscriptions[LLZ_EVENT_TRACK_CHANGED].count > 0) {
                llz_dispatch_track_changed(&currentMedia);
            }
            if (g_subscriptions[LLZ_EVENT_PLAYSTATE_CHANGED].count > 0) {
                llz_dispatch_playstate_changed(currentMedia.isPlaying);
            }
            if (g_subscriptions[LLZ_EVENT_VOLUME_CHANGED].count > 0 && currentMedia.volumePercent >= 0) {
                llz_dispatch_volume_changed(currentMedia.volumePercent);
            }
            if (g_subscriptions[LLZ_EVENT_POSITION_CHANGED].count > 0) {
                llz_dispatch_position_changed(currentMedia.positionSeconds, currentMedia.durationSeconds);
            }
            if (g_subscriptions[LLZ_EVENT_ALBUM_ART_CHANGED].count > 0 && currentMedia.albumArtPath[0]) {
                llz_dispatch_album_art_changed(currentMedia.albumArtPath);
            }
        } else {
            // Compare with previous state

            // Track changed?
            if (g_subscriptions[LLZ_EVENT_TRACK_CHANGED].count > 0) {
                if (llz_track_changed(&currentMedia, &g_prevMedia)) {
                    llz_dispatch_track_changed(&currentMedia);
                }
            }

            // Playstate changed?
            if (g_subscriptions[LLZ_EVENT_PLAYSTATE_CHANGED].count > 0) {
                if (currentMedia.isPlaying != g_prevMedia.isPlaying) {
                    llz_dispatch_playstate_changed(currentMedia.isPlaying);
                }
            }

            // Volume changed?
            if (g_subscriptions[LLZ_EVENT_VOLUME_CHANGED].count > 0) {
                if (currentMedia.volumePercent != g_prevMedia.volumePercent &&
                    currentMedia.volumePercent >= 0) {
                    llz_dispatch_volume_changed(currentMedia.volumePercent);
                }
            }

            // Position changed? (allow 1 second tolerance to avoid noise)
            if (g_subscriptions[LLZ_EVENT_POSITION_CHANGED].count > 0) {
                int posDiff = abs(currentMedia.positionSeconds - g_prevMedia.positionSeconds);
                int durDiff = currentMedia.durationSeconds != g_prevMedia.durationSeconds;
                // Only dispatch if position changed significantly or duration changed
                if (posDiff >= 1 || durDiff) {
                    llz_dispatch_position_changed(currentMedia.positionSeconds, currentMedia.durationSeconds);
                }
            }

            // Album art changed?
            if (g_subscriptions[LLZ_EVENT_ALBUM_ART_CHANGED].count > 0) {
                if (strcmp(currentMedia.albumArtPath, g_prevMedia.albumArtPath) != 0) {
                    llz_dispatch_album_art_changed(currentMedia.albumArtPath);
                }
            }
        }

        // Save current state for next poll
        g_prevMedia = currentMedia;
        g_prevMediaValid = true;
    }

    // Check for connection changes
    if (connectionValid) {
        if (g_subscriptions[LLZ_EVENT_CONNECTION_CHANGED].count > 0) {
            if (!g_prevConnectionValid ||
                currentConnection.connected != g_prevConnection.connected ||
                strcmp(currentConnection.deviceName, g_prevConnection.deviceName) != 0) {
                llz_dispatch_connection_changed(currentConnection.connected, currentConnection.deviceName);
            }
        }

        g_prevConnection = currentConnection;
        g_prevConnectionValid = true;
    }

    // Dispatch pending notifications
    if (g_subscriptions[LLZ_EVENT_NOTIFICATION].count > 0) {
        while (g_notificationHead != g_notificationTail) {
            PendingNotification *notif = &g_notifications[g_notificationHead];
            if (notif->pending) {
                llz_dispatch_notification(notif);
                notif->pending = false;
            }
            g_notificationHead = (g_notificationHead + 1) % MAX_PENDING_NOTIFICATIONS;
        }
    }
}
