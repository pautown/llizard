#include "llz_sdk_media.h"

#include "hiredis.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define LLZ_MEDIA_DEFAULT_HOST "127.0.0.1"
#define LLZ_MEDIA_DEFAULT_PORT 6379
#define LLZ_MEDIA_HOST_MAX 128

static const LlzMediaKeyMap g_defaultKeyMap = {
    .trackTitle = "media:track",
    .artistName = "media:artist",
    .albumName = "media:album",
    .isPlaying = "media:playing",
    .durationSeconds = "media:duration",
    .progressSeconds = "media:progress",
    .albumArtPath = "media:album_art_path",
    .volumePercent = "media:volume",
    .bleConnected = "system:ble_connected",
    .bleName = "system:ble_name",
    .playbackCommandQueue = "system:playback_cmd_q",
    .albumArtRequest = "mediadash:albumart:request",
    .podcastRequestQueue = "podcast:request_q",
    .podcastShowName = "podcast:show_name",
    .podcastEpisodeTitle = "podcast:episode_title",
    .podcastEpisodeDescription = "podcast:episode_description",
    .podcastEpisodeList = "podcast:episode_list",
    .podcastEpisodeCount = "podcast:episode_count",
    .podcastAuthor = "podcast:author",
    .podcastArtPath = "podcast:art_path",
    .lyricsEnabled = "lyrics:enabled",
    .lyricsData = "lyrics:data",
    .lyricsHash = "lyrics:hash",
    .lyricsSynced = "lyrics:synced",
};

static char g_host[LLZ_MEDIA_HOST_MAX] = LLZ_MEDIA_DEFAULT_HOST;
static int g_port = LLZ_MEDIA_DEFAULT_PORT;
static LlzMediaKeyMap g_activeKeys;
static redisContext *g_mediaCtx = NULL;
static bool g_lastStateValid = false;
static bool g_lastIsPlaying = false;

static void llz_media_disconnect(void)
{
    if (g_mediaCtx) {
        redisFree(g_mediaCtx);
        g_mediaCtx = NULL;
    }
}

static bool llz_media_connect(void)
{
    llz_media_disconnect();

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 500000;

    g_mediaCtx = redisConnectWithTimeout(g_host, g_port, timeout);
    if (!g_mediaCtx || g_mediaCtx->err) {
        llz_media_disconnect();
        return false;
    }

    redisSetTimeout(g_mediaCtx, timeout);
    return true;
}

static bool llz_media_ensure_connection(void)
{
    if (g_mediaCtx) {
        return true;
    }
    return llz_media_connect();
}

static void llz_media_apply_keymap(const LlzMediaKeyMap *keyMap)
{
    g_activeKeys = g_defaultKeyMap;
    if (!keyMap) return;

#define COPY_KEY(field) \
    do { if (keyMap->field) g_activeKeys.field = keyMap->field; } while (0)

    COPY_KEY(trackTitle);
    COPY_KEY(artistName);
    COPY_KEY(albumName);
    COPY_KEY(isPlaying);
    COPY_KEY(durationSeconds);
    COPY_KEY(progressSeconds);
    COPY_KEY(albumArtPath);
    COPY_KEY(volumePercent);
    COPY_KEY(bleConnected);
    COPY_KEY(bleName);
    COPY_KEY(playbackCommandQueue);
    COPY_KEY(albumArtRequest);
    COPY_KEY(podcastRequestQueue);
    COPY_KEY(podcastShowName);
    COPY_KEY(podcastEpisodeTitle);
    COPY_KEY(podcastEpisodeDescription);
    COPY_KEY(podcastEpisodeList);
    COPY_KEY(podcastEpisodeCount);
    COPY_KEY(podcastAuthor);
    COPY_KEY(podcastArtPath);
    COPY_KEY(lyricsEnabled);
    COPY_KEY(lyricsData);
    COPY_KEY(lyricsHash);
    COPY_KEY(lyricsSynced);

#undef COPY_KEY
}

static redisReply *llz_media_command(const char *fmt, ...)
{
    if (!llz_media_ensure_connection()) return NULL;

    va_list args;
    va_start(args, fmt);
    redisReply *reply = redisvCommand(g_mediaCtx, fmt, args);
    va_end(args);

    if (!reply) {
        llz_media_disconnect();
        if (!llz_media_ensure_connection()) {
            return NULL;
        }
        va_start(args, fmt);
        reply = redisvCommand(g_mediaCtx, fmt, args);
        va_end(args);
    }

    return reply;
}

static void llz_media_copy_reply(char *dst, size_t dstSize, const redisReply *reply)
{
    if (!dst || dstSize == 0) return;
    const char *src = "";
    if (reply && reply->type == REDIS_REPLY_STRING && reply->str) {
        src = reply->str;
    }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

static int llz_media_reply_int(const redisReply *reply)
{
    if (!reply) return 0;
    if (reply->type == REDIS_REPLY_INTEGER) {
        return (int)reply->integer;
    }
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        return (int)strtol(reply->str, NULL, 10);
    }
    return 0;
}

static bool llz_media_reply_bool(const redisReply *reply)
{
    if (!reply) return false;
    if (reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer != 0;
    }
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        return strcmp(reply->str, "true") == 0 || strcmp(reply->str, "1") == 0;
    }
    return false;
}

bool LlzMediaInit(const LlzMediaConfig *config)
{
    llz_media_apply_keymap(config ? config->keyMap : NULL);

    if (config && config->host && config->host[0] != '\0') {
        strncpy(g_host, config->host, sizeof(g_host) - 1);
        g_host[sizeof(g_host) - 1] = '\0';
    } else {
        strncpy(g_host, LLZ_MEDIA_DEFAULT_HOST, sizeof(g_host) - 1);
        g_host[sizeof(g_host) - 1] = '\0';
    }

    g_port = (config && config->port > 0) ? config->port : LLZ_MEDIA_DEFAULT_PORT;

    return llz_media_connect();
}

void LlzMediaShutdown(void)
{
    llz_media_disconnect();
    g_lastStateValid = false;
}

bool LlzMediaGetState(LlzMediaState *outState)
{
    if (!outState) return false;
    memset(outState, 0, sizeof(*outState));
    outState->volumePercent = -1;

    redisReply *reply = llz_media_command(
        "MGET %s %s %s %s %s %s %s %s",
        g_activeKeys.trackTitle,
        g_activeKeys.artistName,
        g_activeKeys.albumName,
        g_activeKeys.isPlaying,
        g_activeKeys.durationSeconds,
        g_activeKeys.progressSeconds,
        g_activeKeys.albumArtPath,
        g_activeKeys.volumePercent
    );

    if (!reply) return false;
    bool success = false;

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 8) {
        llz_media_copy_reply(outState->track, sizeof(outState->track), reply->element[0]);
        llz_media_copy_reply(outState->artist, sizeof(outState->artist), reply->element[1]);
        llz_media_copy_reply(outState->album, sizeof(outState->album), reply->element[2]);
        outState->isPlaying = llz_media_reply_bool(reply->element[3]);
        outState->durationSeconds = llz_media_reply_int(reply->element[4]);
        outState->positionSeconds = llz_media_reply_int(reply->element[5]);
        llz_media_copy_reply(outState->albumArtPath, sizeof(outState->albumArtPath), reply->element[6]);
        if (reply->element[7] && reply->element[7]->type != REDIS_REPLY_NIL) {
            outState->volumePercent = llz_media_reply_int(reply->element[7]);
        }
        outState->updatedAt = (int64_t)time(NULL);

        g_lastIsPlaying = outState->isPlaying;
        g_lastStateValid = true;
        success = true;
    }

    freeReplyObject(reply);
    return success;
}

bool LlzMediaGetConnection(LlzConnectionStatus *outStatus)
{
    if (!outStatus) return false;
    memset(outStatus, 0, sizeof(*outStatus));

    redisReply *reply = llz_media_command(
        "MGET %s %s",
        g_activeKeys.bleConnected,
        g_activeKeys.bleName
    );

    if (!reply) return false;
    bool success = false;

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 2) {
        outStatus->connected = llz_media_reply_bool(reply->element[0]);
        llz_media_copy_reply(outStatus->deviceName, sizeof(outStatus->deviceName), reply->element[1]);
        if (outStatus->deviceName[0] == '\0') {
            strncpy(outStatus->deviceName, outStatus->connected ? "Unknown Device" : "Not Connected", sizeof(outStatus->deviceName) - 1);
            outStatus->deviceName[sizeof(outStatus->deviceName) - 1] = '\0';
        }
        success = true;
    }

    freeReplyObject(reply);
    return success;
}

float LlzMediaGetProgressPercent(const LlzMediaState *state)
{
    if (!state || state->durationSeconds <= 0) return 0.0f;
    float pct = (float)state->positionSeconds / (float)state->durationSeconds;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    return pct;
}

static const char *llz_media_action_string(LlzPlaybackCommand action, int *value)
{
    switch (action) {
        case LLZ_PLAYBACK_PLAY: return "play";
        case LLZ_PLAYBACK_PAUSE: return "pause";
        case LLZ_PLAYBACK_NEXT: return "next";
        case LLZ_PLAYBACK_PREVIOUS: return "previous";
        case LLZ_PLAYBACK_SEEK_TO:
            if (value && *value < 0) *value = 0;
            return "seek";
        case LLZ_PLAYBACK_SET_VOLUME:
            if (value) {
                if (*value < 0) *value = 0;
                if (*value > 100) *value = 100;
            }
            return "volume";
        case LLZ_PLAYBACK_TOGGLE:
            if (g_lastStateValid && g_lastIsPlaying) {
                return "pause";
            }
            return "play";
        default:
            return NULL;
    }
}

static bool llz_media_push_command(const char *action, int value)
{
    if (!action || !g_activeKeys.playbackCommandQueue) return false;

    long long ts = (long long)time(NULL);
    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"%s\",\"value\":%d,\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        action,
        value,
        ts
    );

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);
    return success;
}

bool LlzMediaSendCommand(LlzPlaybackCommand action, int value)
{
    const char *actionStr = llz_media_action_string(action, &value);
    if (!actionStr) return false;
    return llz_media_push_command(actionStr, value);
}

bool LlzMediaSeekSeconds(int seconds)
{
    return LlzMediaSendCommand(LLZ_PLAYBACK_SEEK_TO, seconds);
}

bool LlzMediaSetVolume(int percent)
{
    return LlzMediaSendCommand(LLZ_PLAYBACK_SET_VOLUME, percent);
}

// CRC32 implementation matching Android's java.util.zip.CRC32
static uint32_t llz_crc32_table[256];
static bool llz_crc32_table_init = false;

static void llz_crc32_init_table(void)
{
    if (llz_crc32_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        llz_crc32_table[i] = crc;
    }
    llz_crc32_table_init = true;
}

static uint32_t llz_crc32(const char *data, size_t len)
{
    llz_crc32_init_table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = llz_crc32_table[(crc ^ (unsigned char)data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// Helper to skip leading whitespace
static const char *llz_skip_whitespace(const char *s)
{
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    return s;
}

// Helper to find end of string (excluding trailing whitespace)
static const char *llz_find_trimmed_end(const char *s)
{
    const char *end = s + strlen(s);
    while (end > s && (*(end-1) == ' ' || *(end-1) == '\t' || *(end-1) == '\n' || *(end-1) == '\r')) end--;
    return end;
}

// UTF-8 aware lowercase conversion to match Go/Android behavior
// Decodes a UTF-8 codepoint from the string, returns bytes consumed (1-4), or 0 on error
static int llz_utf8_decode(const unsigned char *s, uint32_t *codepoint)
{
    if (!s || !codepoint) return 0;

    unsigned char c = s[0];

    // ASCII (0x00-0x7F) - single byte
    if (c < 0x80) {
        *codepoint = c;
        return 1;
    }
    // 2-byte sequence (0xC0-0xDF lead byte)
    else if ((c & 0xE0) == 0xC0) {
        if ((s[1] & 0xC0) != 0x80) return 0; // Invalid continuation
        *codepoint = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    // 3-byte sequence (0xE0-0xEF lead byte)
    else if ((c & 0xF0) == 0xE0) {
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return 0;
        *codepoint = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    // 4-byte sequence (0xF0-0xF7 lead byte)
    else if ((c & 0xF8) == 0xF0) {
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return 0;
        *codepoint = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }

    return 0; // Invalid UTF-8
}

// Encode a codepoint to UTF-8, returns bytes written (1-4)
static int llz_utf8_encode(uint32_t codepoint, unsigned char *out)
{
    if (codepoint < 0x80) {
        out[0] = (unsigned char)codepoint;
        return 1;
    }
    else if (codepoint < 0x800) {
        out[0] = 0xC0 | (codepoint >> 6);
        out[1] = 0x80 | (codepoint & 0x3F);
        return 2;
    }
    else if (codepoint < 0x10000) {
        out[0] = 0xE0 | (codepoint >> 12);
        out[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        out[2] = 0x80 | (codepoint & 0x3F);
        return 3;
    }
    else if (codepoint < 0x110000) {
        out[0] = 0xF0 | (codepoint >> 18);
        out[1] = 0x80 | ((codepoint >> 12) & 0x3F);
        out[2] = 0x80 | ((codepoint >> 6) & 0x3F);
        out[3] = 0x80 | (codepoint & 0x3F);
        return 4;
    }
    return 0; // Invalid codepoint
}

// Convert a Unicode codepoint to lowercase (handles common scripts)
static uint32_t llz_unicode_tolower(uint32_t cp)
{
    // ASCII uppercase A-Z
    if (cp >= 0x41 && cp <= 0x5A) {
        return cp + 0x20;
    }

    // Latin-1 Supplement: À-Ö (C0-D6) and Ø-Þ (D8-DE)
    if ((cp >= 0xC0 && cp <= 0xD6) || (cp >= 0xD8 && cp <= 0xDE)) {
        return cp + 0x20;
    }

    // Latin Extended-A: pairs of upper/lower (0x100-0x17F)
    // Most are simple +1 for lowercase (uppercase on even, lowercase on odd)
    if (cp >= 0x100 && cp <= 0x137) {
        if (cp % 2 == 0) return cp + 1;
    }
    if (cp >= 0x139 && cp <= 0x148) {
        if (cp % 2 == 1) return cp + 1;
    }
    if (cp >= 0x14A && cp <= 0x177) {
        if (cp % 2 == 0) return cp + 1;
    }
    if (cp == 0x178) return 0xFF; // Ÿ -> ÿ
    if (cp >= 0x179 && cp <= 0x17E) {
        if (cp % 2 == 1) return cp + 1;
    }

    // Cyrillic: А-Я (U+0410-U+042F) -> а-я (U+0430-U+044F)
    if (cp >= 0x0410 && cp <= 0x042F) {
        return cp + 0x20;
    }

    // Cyrillic: Ѐ-Џ (U+0400-U+040F) -> ѐ-џ (U+0450-U+045F)
    if (cp >= 0x0400 && cp <= 0x040F) {
        return cp + 0x50;
    }

    // Cyrillic Extended (common): Ё (U+0401) already handled above
    // Additional Cyrillic pairs (0x0460-0x0481) - alternating upper/lower
    if (cp >= 0x0460 && cp <= 0x0481) {
        if (cp % 2 == 0) return cp + 1;
    }

    // Greek uppercase: Α-Ρ (U+0391-U+03A1) -> α-ρ (U+03B1-U+03C1)
    if (cp >= 0x0391 && cp <= 0x03A1) {
        return cp + 0x20;
    }
    // Greek uppercase: Σ-Ω (U+03A3-U+03A9) -> σ-ω (U+03C3-U+03C9)
    if (cp >= 0x03A3 && cp <= 0x03A9) {
        return cp + 0x20;
    }

    return cp; // No conversion needed
}

// Copy string to buffer with UTF-8 aware lowercase conversion
// Returns number of bytes written (not including null terminator)
static size_t llz_utf8_tolower(const char *src, const char *srcEnd, char *dst, size_t dstSize)
{
    const unsigned char *s = (const unsigned char *)src;
    const unsigned char *end = (const unsigned char *)srcEnd;
    unsigned char *d = (unsigned char *)dst;
    size_t written = 0;

    while (s < end && written < dstSize - 4) { // Leave room for max UTF-8 char
        uint32_t cp;
        int consumed = llz_utf8_decode(s, &cp);

        if (consumed == 0) {
            // Invalid UTF-8, copy byte as-is
            *d++ = *s++;
            written++;
            continue;
        }

        // Convert to lowercase
        uint32_t lowerCp = llz_unicode_tolower(cp);

        // Encode back to UTF-8
        int encoded = llz_utf8_encode(lowerCp, d);
        if (encoded == 0) {
            // Encoding failed, skip
            s += consumed;
            continue;
        }

        s += consumed;
        d += encoded;
        written += encoded;
    }

    return written;
}

const char *LlzMediaGenerateArtHash(const char *artist, const char *album)
{
    static char hashBuffer[16];
    static char inputBuffer[512];

    if (!artist) artist = "";
    if (!album) album = "";

    // Build "artist.trim().lowercase()|album.trim().lowercase()" string
    // Uses UTF-8 aware lowercase to match Go/Android behavior for non-English characters
    char *p = inputBuffer;
    size_t remaining = sizeof(inputBuffer) - 1;

    // Copy artist lowercase (trimmed) - UTF-8 aware
    const char *artistStart = llz_skip_whitespace(artist);
    const char *artistEnd = llz_find_trimmed_end(artist);
    size_t artistBytes = llz_utf8_tolower(artistStart, artistEnd, p, remaining);
    p += artistBytes;
    remaining -= artistBytes;

    // Add separator
    if (remaining > 0) {
        *p++ = '|';
        remaining--;
    }

    // Copy album lowercase (trimmed) - UTF-8 aware
    const char *albumStart = llz_skip_whitespace(album);
    const char *albumEnd = llz_find_trimmed_end(album);
    size_t albumBytes = llz_utf8_tolower(albumStart, albumEnd, p, remaining);
    p += albumBytes;

    *p = '\0';

    // Generate CRC32 and format as decimal string (matches Android)
    uint32_t crc = llz_crc32(inputBuffer, strlen(inputBuffer));
    snprintf(hashBuffer, sizeof(hashBuffer), "%u", crc);

    return hashBuffer;
}

bool LlzMediaRequestAlbumArt(const char *hash)
{
    if (!g_activeKeys.albumArtRequest) return false;

    // If no hash provided, we need to generate from current track
    // The caller should use LlzMediaGenerateArtHash with current artist/album
    if (!hash || hash[0] == '\0') {
        return false;
    }

    long long ts = (long long)time(NULL);

    // Store album art request as JSON to the Redis key
    // Format matches golang_ble_client's AlbumArtRequest struct
    redisReply *reply = llz_media_command(
        "SET %s {\"hash\":\"%s\",\"timestamp\":%lld}",
        g_activeKeys.albumArtRequest,
        hash,
        ts
    );

    if (!reply) return false;
    bool success = (reply->type == REDIS_REPLY_STATUS &&
                    reply->str && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    return success;
}

bool LlzMediaRequestBLEReconnect(void)
{
    long long ts = (long long)time(NULL);

    // Set Redis key to signal golang_ble_client to reconnect
    // The BLE client should poll this key and trigger reconnection when value changes
    redisReply *reply = llz_media_command(
        "SET system:ble_reconnect_request %lld",
        ts
    );

    if (!reply) return false;
    bool success = (reply->type == REDIS_REPLY_STATUS &&
                    reply->str && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    return success;
}

bool LlzMediaIsBLEServiceRunning(void)
{
#ifdef PLATFORM_DRM
    // Use runit sv status to check if mercury service is running
    // sv status returns 0 if service is running, non-zero otherwise
    int result = system("sv status mercury > /dev/null 2>&1");
    return (result == 0);
#else
    // On desktop, assume service is always "running" (development mode)
    return true;
#endif
}

bool LlzMediaRestartBLEService(void)
{
#ifdef PLATFORM_DRM
    // Use runit sv restart to restart the mercury service
    int result = system("sv restart mercury");
    return (result == 0);
#else
    // On desktop, just return true (no service to restart)
    return true;
#endif
}

bool LlzMediaRequestPodcastInfo(void)
{
    // Use the playback command queue so the golang BLE client picks it up
    if (!g_activeKeys.playbackCommandQueue) return false;

    long long ts = (long long)time(NULL);
    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"request_podcast_info\",\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        ts
    );

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);
    return success;
}

bool LlzMediaGetPodcastState(LlzPodcastState *outState)
{
    if (!outState) return false;
    memset(outState, 0, sizeof(*outState));

    redisReply *reply = llz_media_command(
        "MGET %s %s %s %s %s %s",
        g_activeKeys.podcastShowName,
        g_activeKeys.podcastEpisodeTitle,
        g_activeKeys.podcastEpisodeDescription,
        g_activeKeys.podcastAuthor,
        g_activeKeys.podcastArtPath,
        g_activeKeys.podcastEpisodeCount
    );

    if (!reply) return false;
    bool success = false;

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 6) {
        llz_media_copy_reply(outState->showName, sizeof(outState->showName), reply->element[0]);
        llz_media_copy_reply(outState->episodeTitle, sizeof(outState->episodeTitle), reply->element[1]);
        llz_media_copy_reply(outState->episodeDescription, sizeof(outState->episodeDescription), reply->element[2]);
        llz_media_copy_reply(outState->author, sizeof(outState->author), reply->element[3]);
        llz_media_copy_reply(outState->artPath, sizeof(outState->artPath), reply->element[4]);
        outState->episodeCount = llz_media_reply_int(reply->element[5]);
        success = true;
    }

    freeReplyObject(reply);

    // Also fetch playback state from regular media keys
    if (success) {
        redisReply *playbackReply = llz_media_command(
            "MGET %s %s %s",
            g_activeKeys.isPlaying,
            g_activeKeys.durationSeconds,
            g_activeKeys.progressSeconds
        );

        if (playbackReply && playbackReply->type == REDIS_REPLY_ARRAY && playbackReply->elements >= 3) {
            outState->isPlaying = llz_media_reply_bool(playbackReply->element[0]);
            outState->durationSeconds = llz_media_reply_int(playbackReply->element[1]);
            outState->positionSeconds = llz_media_reply_int(playbackReply->element[2]);
        }

        if (playbackReply) {
            freeReplyObject(playbackReply);
        }
    }

    return success;
}

bool LlzMediaGetPodcastEpisodes(char *outJson, size_t maxLen)
{
    if (!outJson || maxLen == 0) return false;
    outJson[0] = '\0';

    if (!g_activeKeys.podcastEpisodeList) return false;

    redisReply *reply = llz_media_command(
        "GET %s",
        g_activeKeys.podcastEpisodeList
    );

    if (!reply) return false;
    bool success = false;

    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        strncpy(outJson, reply->str, maxLen - 1);
        outJson[maxLen - 1] = '\0';
        success = true;
    }

    freeReplyObject(reply);
    return success;
}

int LlzMediaGetPodcastCount(void)
{
    redisReply *reply = llz_media_command("GET podcast:count");

    if (!reply) return 0;
    int count = 0;

    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        count = atoi(reply->str);
    } else if (reply->type == REDIS_REPLY_INTEGER) {
        count = (int)reply->integer;
    }

    freeReplyObject(reply);
    return count;
}

bool LlzMediaGetPodcastLibrary(char *outJson, size_t maxLen)
{
    if (!outJson || maxLen == 0) return false;
    outJson[0] = '\0';

    redisReply *reply = llz_media_command("GET podcast:library");

    if (!reply) return false;
    bool success = false;

    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        strncpy(outJson, reply->str, maxLen - 1);
        outJson[maxLen - 1] = '\0';
        success = true;
    }

    freeReplyObject(reply);
    return success;
}

bool LlzMediaPlayEpisode(const char *episodeHash)
{
    if (!episodeHash || episodeHash[0] == '\0') return false;
    if (!g_activeKeys.playbackCommandQueue) return false;

    long long ts = (long long)time(NULL);

    // Push command with episode hash (new preferred format)
    // Format: {"action":"play_episode","episodeHash":"<hash>","timestamp":<ts>}
    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"play_episode\",\"episodeHash\":\"%s\",\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        episodeHash,
        ts
    );

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    if (success) {
        printf("SDK: Queued play_episode command: episodeHash=%s\n", episodeHash);
    }

    return success;
}

bool LlzMediaPlayPodcastEpisode(const char *podcastId, int episodeIndex)
{
    if (!podcastId || podcastId[0] == '\0') return false;
    if (episodeIndex < 0) return false;
    if (!g_activeKeys.playbackCommandQueue) return false;

    long long ts = (long long)time(NULL);

    // Push command with podcast-specific fields (DEPRECATED - use LlzMediaPlayEpisode instead)
    // Format: {"action":"play_podcast_episode","podcastId":"<id>","episodeIndex":<index>,"timestamp":<ts>}
    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"play_podcast_episode\",\"podcastId\":\"%s\",\"episodeIndex\":%d,\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        podcastId,
        episodeIndex,
        ts
    );

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    if (success) {
        printf("SDK: Queued play_podcast_episode command (DEPRECATED): podcast=%s, episode=%d\n", podcastId, episodeIndex);
    }

    return success;
}

// ============================================================================
// Lazy Loading Podcast API Implementation
// ============================================================================

bool LlzMediaRequestPodcastList(void)
{
    if (!g_activeKeys.playbackCommandQueue) return false;

    long long ts = (long long)time(NULL);

    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"request_podcast_list\",\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        ts
    );

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    if (success) {
        printf("SDK: Requested podcast list (A-Z channels)\n");
    }

    return success;
}

bool LlzMediaRequestRecentEpisodes(int limit)
{
    if (!g_activeKeys.playbackCommandQueue) return false;
    if (limit <= 0) limit = 30;  // Default limit

    long long ts = (long long)time(NULL);

    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"request_recent_episodes\",\"limit\":%d,\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        limit,
        ts
    );

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    if (success) {
        printf("SDK: Requested recent episodes (limit=%d)\n", limit);
    }

    return success;
}

bool LlzMediaRequestPodcastEpisodes(const char *podcastId, int offset, int limit)
{
    if (!podcastId || podcastId[0] == '\0') return false;
    if (!g_activeKeys.playbackCommandQueue) return false;
    if (offset < 0) offset = 0;
    if (limit <= 0) limit = 15;  // Default page size

    long long ts = (long long)time(NULL);

    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"request_podcast_episodes\",\"podcastId\":\"%s\",\"offset\":%d,\"limit\":%d,\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        podcastId,
        offset,
        limit,
        ts
    );

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    if (success) {
        printf("SDK: Requested episodes for podcast=%s (offset=%d, limit=%d)\n", podcastId, offset, limit);
    }

    return success;
}

bool LlzMediaGetPodcastList(char *outJson, size_t maxLen)
{
    if (!outJson || maxLen == 0) return false;
    outJson[0] = '\0';

    redisReply *reply = llz_media_command("GET podcast:list");

    if (!reply) return false;
    bool success = false;

    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        size_t len = strlen(reply->str);
        if (len < maxLen) {
            strcpy(outJson, reply->str);
            success = true;
        }
    }

    freeReplyObject(reply);
    return success;
}

bool LlzMediaGetRecentEpisodes(char *outJson, size_t maxLen)
{
    if (!outJson || maxLen == 0) return false;
    outJson[0] = '\0';

    redisReply *reply = llz_media_command("GET podcast:recent_episodes");

    if (!reply) return false;
    bool success = false;

    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        size_t len = strlen(reply->str);
        if (len < maxLen) {
            strcpy(outJson, reply->str);
            success = true;
        }
    }

    freeReplyObject(reply);
    return success;
}

bool LlzMediaGetPodcastEpisodesForId(const char *podcastId, char *outJson, size_t maxLen)
{
    if (!podcastId || podcastId[0] == '\0') return false;
    if (!outJson || maxLen == 0) return false;
    outJson[0] = '\0';

    // Redis key is podcast:episodes:<podcastId>
    redisReply *reply = llz_media_command("GET podcast:episodes:%s", podcastId);

    if (!reply) return false;
    bool success = false;

    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        size_t len = strlen(reply->str);
        if (len < maxLen) {
            strcpy(outJson, reply->str);
            success = true;
        }
    }

    freeReplyObject(reply);
    return success;
}

// ============================================================================
// Lyrics API Implementation
// ============================================================================

bool LlzLyricsIsEnabled(void)
{
    redisReply *reply = llz_media_command("GET %s", g_activeKeys.lyricsEnabled);
    if (!reply) {
        printf("[LYRICS] LlzLyricsIsEnabled: Redis query failed\n");
        return false;
    }

    bool enabled = false;
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        enabled = strcmp(reply->str, "true") == 0 || strcmp(reply->str, "1") == 0;
    }

    freeReplyObject(reply);
    return enabled;
}

bool LlzLyricsSetEnabled(bool enabled)
{
    printf("[LYRICS] LlzLyricsSetEnabled: Setting lyrics enabled=%d\n", enabled);

    redisReply *reply = llz_media_command(
        "SET %s %s",
        g_activeKeys.lyricsEnabled,
        enabled ? "true" : "false"
    );

    if (!reply) {
        printf("[LYRICS] LlzLyricsSetEnabled: Redis command failed\n");
        return false;
    }
    bool success = (reply->type == REDIS_REPLY_STATUS &&
                    reply->str && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);

    if (success) {
        printf("[LYRICS] LlzLyricsSetEnabled: Lyrics %s successfully\n", enabled ? "enabled" : "disabled");
    } else {
        printf("[LYRICS] LlzLyricsSetEnabled: Failed to set lyrics enabled state\n");
    }

    return success;
}

bool LlzLyricsGetJson(char *outJson, size_t maxLen)
{
    if (!outJson || maxLen == 0) return false;

    redisReply *reply = llz_media_command("GET %s", g_activeKeys.lyricsData);
    if (!reply) {
        printf("[LYRICS] LlzLyricsGetJson: Redis query failed\n");
        return false;
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        size_t len = strlen(reply->str);
        if (len < maxLen) {
            strcpy(outJson, reply->str);
            success = true;
            printf("[LYRICS] LlzLyricsGetJson: Retrieved lyrics JSON (len=%zu)\n", len);
        } else {
            printf("[LYRICS] LlzLyricsGetJson: Buffer too small (need=%zu, have=%zu)\n", len, maxLen);
        }
    } else {
        printf("[LYRICS] LlzLyricsGetJson: No lyrics data in Redis\n");
    }

    freeReplyObject(reply);
    return success;
}

bool LlzLyricsGetHash(char *outHash, size_t maxLen)
{
    if (!outHash || maxLen == 0) return false;

    redisReply *reply = llz_media_command("GET %s", g_activeKeys.lyricsHash);
    if (!reply) return false;

    bool success = false;
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        size_t len = strlen(reply->str);
        if (len < maxLen) {
            strcpy(outHash, reply->str);
            success = true;
        }
    }

    freeReplyObject(reply);
    return success;
}

bool LlzLyricsAreSynced(void)
{
    redisReply *reply = llz_media_command("GET %s", g_activeKeys.lyricsSynced);
    if (!reply) return false;

    bool synced = false;
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        synced = strcmp(reply->str, "true") == 0 || strcmp(reply->str, "1") == 0;
    }

    freeReplyObject(reply);
    return synced;
}

// Simple JSON parser for lyrics - parses the lyrics:data JSON
// This is a minimal parser for the specific lyrics JSON format
static bool parse_lyrics_json(const char *json, LlzLyricsData *outLyrics)
{
    if (!json || !outLyrics) return false;

    memset(outLyrics, 0, sizeof(LlzLyricsData));

    // Find hash
    const char *hashStart = strstr(json, "\"hash\":\"");
    if (hashStart) {
        hashStart += 8;
        const char *hashEnd = strchr(hashStart, '"');
        if (hashEnd) {
            size_t len = hashEnd - hashStart;
            if (len < sizeof(outLyrics->hash)) {
                strncpy(outLyrics->hash, hashStart, len);
            }
        }
    }

    // Find synced
    const char *syncedStart = strstr(json, "\"synced\":");
    if (syncedStart) {
        syncedStart += 9;
        outLyrics->synced = (strncmp(syncedStart, "true", 4) == 0);
    }

    // Count lines
    int lineCount = 0;
    const char *linesStart = strstr(json, "\"lines\":[");
    if (linesStart) {
        const char *p = linesStart;
        while ((p = strstr(p, "{\"t\":")) != NULL) {
            lineCount++;
            p++;
        }
    }

    if (lineCount == 0) {
        return outLyrics->hash[0] != '\0';  // Return true if we at least got the hash
    }

    // Allocate lines
    outLyrics->lines = (LlzLyricsLine *)malloc(lineCount * sizeof(LlzLyricsLine));
    if (!outLyrics->lines) return false;

    // Parse each line
    const char *p = linesStart;
    int idx = 0;

    while (idx < lineCount && (p = strstr(p, "{\"t\":")) != NULL) {
        LlzLyricsLine *line = &outLyrics->lines[idx];
        memset(line, 0, sizeof(LlzLyricsLine));

        // Parse timestamp
        p += 5;  // Skip {"t":
        line->timestampMs = strtoll(p, NULL, 10);

        // Find text
        const char *textStart = strstr(p, "\"l\":\"");
        if (textStart) {
            textStart += 5;
            const char *textEnd = strchr(textStart, '"');
            if (textEnd) {
                size_t len = textEnd - textStart;
                if (len >= sizeof(line->text)) {
                    len = sizeof(line->text) - 1;
                }
                strncpy(line->text, textStart, len);
            }
        }

        idx++;
        p++;
    }

    outLyrics->lineCount = idx;
    return true;
}

bool LlzLyricsGet(LlzLyricsData *outLyrics)
{
    if (!outLyrics) return false;

    printf("[LYRICS] LlzLyricsGet: Fetching lyrics from Redis\n");

    memset(outLyrics, 0, sizeof(LlzLyricsData));

    // Get JSON from Redis
    char *jsonBuf = (char *)malloc(64 * 1024);  // 64KB buffer for lyrics JSON
    if (!jsonBuf) {
        printf("[LYRICS] LlzLyricsGet: Failed to allocate JSON buffer\n");
        return false;
    }

    bool success = LlzLyricsGetJson(jsonBuf, 64 * 1024);
    if (success) {
        success = parse_lyrics_json(jsonBuf, outLyrics);
        if (success) {
            printf("[LYRICS] LlzLyricsGet: Parsed lyrics - hash='%s', synced=%d, lineCount=%d\n",
                   outLyrics->hash, outLyrics->synced, outLyrics->lineCount);
        } else {
            printf("[LYRICS] LlzLyricsGet: Failed to parse lyrics JSON\n");
        }
    }

    free(jsonBuf);
    return success;
}

void LlzLyricsFree(LlzLyricsData *lyrics)
{
    if (!lyrics) return;

    if (lyrics->lines) {
        free(lyrics->lines);
        lyrics->lines = NULL;
    }
    lyrics->lineCount = 0;
}

int LlzLyricsFindCurrentLine(int64_t positionMs, const LlzLyricsData *lyrics)
{
    if (!lyrics || !lyrics->lines || lyrics->lineCount == 0) {
        return -1;
    }

    // For unsynced lyrics, return line based on rough position
    if (!lyrics->synced) {
        return -1;
    }

    // Binary search for the current line
    int left = 0;
    int right = lyrics->lineCount - 1;
    int result = -1;

    while (left <= right) {
        int mid = (left + right) / 2;
        if (lyrics->lines[mid].timestampMs <= positionMs) {
            result = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return result;
}

const char *LlzLyricsGenerateHash(const char *artist, const char *track)
{
    static char hashBuf[32];

    if (!artist || !track) {
        hashBuf[0] = '\0';
        return hashBuf;
    }

    // Create lowercase "artist|track" string
    char input[512];
    int i, j = 0;

    for (i = 0; artist[i] && j < 250; i++) {
        input[j++] = (artist[i] >= 'A' && artist[i] <= 'Z') ? artist[i] + 32 : artist[i];
    }
    input[j++] = '|';
    for (i = 0; track[i] && j < 510; i++) {
        input[j++] = (track[i] >= 'A' && track[i] <= 'Z') ? track[i] + 32 : track[i];
    }
    input[j] = '\0';

    // CRC32 calculation (same algorithm as Android/Go)
    uint32_t crc = 0xFFFFFFFF;
    for (i = 0; input[i]; i++) {
        crc ^= (uint8_t)input[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    crc ^= 0xFFFFFFFF;

    // Return as decimal string (matching Go/Android format)
    snprintf(hashBuf, sizeof(hashBuf), "%u", crc);
    return hashBuf;
}

bool LlzLyricsRequest(const char *artist, const char *track)
{
    if (!artist || !track || artist[0] == '\0' || track[0] == '\0') {
        printf("[LYRICS] LlzLyricsRequest: Invalid artist or track (null or empty)\n");
        return false;
    }

    if (!g_activeKeys.playbackCommandQueue) {
        printf("[LYRICS] LlzLyricsRequest: Playback command queue not configured\n");
        return false;
    }

    long long ts = (long long)time(NULL);

    // Push lyrics request command to the playback command queue
    // Format matches the LyricsRequest struct in golang_ble_client/Android
    // {"action":"request_lyrics","artist":"...","track":"...","timestamp":...}
    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"request_lyrics\",\"artist\":\"%s\",\"track\":\"%s\",\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        artist,
        track,
        ts
    );

    if (!reply) {
        printf("[LYRICS] LlzLyricsRequest: Redis command failed\n");
        return false;
    }

    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    if (success) {
        printf("[LYRICS] LlzLyricsRequest: Queued lyrics request for '%s' - '%s'\n", artist, track);
    } else {
        printf("[LYRICS] LlzLyricsRequest: Failed to queue lyrics request\n");
    }

    return success;
}

bool LlzLyricsStore(const char *lyricsJson, const char *hash, bool synced)
{
    if (!lyricsJson || lyricsJson[0] == '\0') {
        printf("[LYRICS] LlzLyricsStore: Invalid lyrics JSON (null or empty)\n");
        return false;
    }

    if (!hash || hash[0] == '\0') {
        printf("[LYRICS] LlzLyricsStore: Invalid hash (null or empty)\n");
        return false;
    }

    printf("[LYRICS] LlzLyricsStore: Storing lyrics (hash='%s', synced=%d, jsonLen=%zu)\n",
           hash, synced, strlen(lyricsJson));

    // Store lyrics data
    redisReply *reply = llz_media_command("SET %s %s", g_activeKeys.lyricsData, lyricsJson);
    if (!reply) {
        printf("[LYRICS] LlzLyricsStore: Failed to store lyrics data\n");
        return false;
    }
    bool dataOk = (reply->type == REDIS_REPLY_STATUS && reply->str && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    if (!dataOk) {
        printf("[LYRICS] LlzLyricsStore: Failed to store lyrics data (not OK response)\n");
        return false;
    }

    // Store lyrics hash
    reply = llz_media_command("SET %s %s", g_activeKeys.lyricsHash, hash);
    if (!reply) {
        printf("[LYRICS] LlzLyricsStore: Failed to store lyrics hash\n");
        return false;
    }
    bool hashOk = (reply->type == REDIS_REPLY_STATUS && reply->str && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    if (!hashOk) {
        printf("[LYRICS] LlzLyricsStore: Failed to store lyrics hash (not OK response)\n");
        return false;
    }

    // Store synced flag
    reply = llz_media_command("SET %s %s", g_activeKeys.lyricsSynced, synced ? "true" : "false");
    if (!reply) {
        printf("[LYRICS] LlzLyricsStore: Failed to store synced flag\n");
        return false;
    }
    bool syncedOk = (reply->type == REDIS_REPLY_STATUS && reply->str && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);

    if (syncedOk) {
        printf("[LYRICS] LlzLyricsStore: Lyrics stored successfully\n");
    } else {
        printf("[LYRICS] LlzLyricsStore: Failed to store synced flag (not OK response)\n");
    }

    return syncedOk;
}

// ============================================================================
// Media Channels API Implementation
// ============================================================================

bool LlzMediaRequestChannels(void)
{
    if (!g_activeKeys.playbackCommandQueue) return false;

    long long ts = (long long)time(NULL);

    redisReply *reply = llz_media_command(
        "LPUSH %s {\"action\":\"request_media_channels\",\"timestamp\":%lld}",
        g_activeKeys.playbackCommandQueue,
        ts
    );

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    if (success) {
        printf("[MEDIA_CHANNELS] Requested media channel list from Android\n");
    }

    return success;
}

bool LlzMediaGetChannels(LlzMediaChannels *outChannels)
{
    if (!outChannels) return false;
    memset(outChannels, 0, sizeof(*outChannels));

    redisReply *reply = llz_media_command("GET media:channels");
    if (!reply) return false;

    bool success = false;
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        // Parse JSON: {"channels":["Spotify","YouTube"],"count":2,"timestamp":123}
        // Simple parsing - find channels array
        const char *json = reply->str;

        // Find "count":
        const char *countPtr = strstr(json, "\"count\":");
        if (countPtr) {
            outChannels->count = atoi(countPtr + 8);
            if (outChannels->count > LLZ_MEDIA_CHANNEL_MAX) {
                outChannels->count = LLZ_MEDIA_CHANNEL_MAX;
            }
        }

        // Find "timestamp":
        const char *tsPtr = strstr(json, "\"timestamp\":");
        if (tsPtr) {
            outChannels->timestamp = strtoll(tsPtr + 12, NULL, 10);
        }

        // Find "channels":[ and parse array
        const char *arrStart = strstr(json, "\"channels\":[");
        if (arrStart) {
            arrStart = strchr(arrStart, '[');
            if (arrStart) {
                arrStart++; // Skip '['
                int idx = 0;
                while (idx < outChannels->count && idx < LLZ_MEDIA_CHANNEL_MAX) {
                    // Find next string
                    const char *strStart = strchr(arrStart, '"');
                    if (!strStart) break;
                    strStart++; // Skip opening quote

                    const char *strEnd = strchr(strStart, '"');
                    if (!strEnd) break;

                    size_t len = strEnd - strStart;
                    if (len >= LLZ_MEDIA_CHANNEL_NAME_MAX) {
                        len = LLZ_MEDIA_CHANNEL_NAME_MAX - 1;
                    }
                    strncpy(outChannels->channels[idx], strStart, len);
                    outChannels->channels[idx][len] = '\0';

                    idx++;
                    arrStart = strEnd + 1;

                    // Skip to next element or end
                    while (*arrStart && *arrStart != '"' && *arrStart != ']') {
                        arrStart++;
                    }
                    if (*arrStart == ']') break;
                }
                success = true;
            }
        }
    }

    freeReplyObject(reply);
    return success;
}

bool LlzMediaGetChannelsJson(char *outJson, size_t maxLen)
{
    if (!outJson || maxLen == 0) return false;
    outJson[0] = '\0';

    redisReply *reply = llz_media_command("GET media:channels");
    if (!reply) return false;

    bool success = false;
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        size_t len = strlen(reply->str);
        if (len < maxLen) {
            strcpy(outJson, reply->str);
            success = true;
        }
    }

    freeReplyObject(reply);
    return success;
}

bool LlzMediaSelectChannel(const char *channelName)
{
    if (!channelName || channelName[0] == '\0') return false;
    if (!llz_media_ensure_connection()) return false;

    // Build JSON command: {"action":"select_media_channel","channel":"Spotify"}
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "{\"action\":\"select_media_channel\",\"channel\":\"%s\",\"timestamp\":%lld}",
             channelName, (long long)time(NULL));

    printf("[MEDIA_CHANNELS] Selecting channel: %s\n", channelName);

    redisReply *reply = llz_media_command("RPUSH %s %s",
                                          g_activeKeys.playbackCommandQueue, cmd);
    if (!reply) return false;

    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    // Also store locally in Redis for quick access
    if (success) {
        reply = llz_media_command("SET media:controlled_channel %s", channelName);
        if (reply) freeReplyObject(reply);
    }

    return success;
}

bool LlzMediaGetControlledChannel(char *outChannel, size_t maxLen)
{
    if (!outChannel || maxLen == 0) return false;
    outChannel[0] = '\0';

    redisReply *reply = llz_media_command("GET media:controlled_channel");
    if (!reply) return false;

    bool success = false;
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        size_t len = strlen(reply->str);
        if (len < maxLen) {
            strcpy(outChannel, reply->str);
            success = true;
        }
    }

    freeReplyObject(reply);
    return success;
}
