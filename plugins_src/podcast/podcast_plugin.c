/**
 * Podcast Plugin for llizardgui-host
 *
 * A podcast browsing plugin with hierarchical navigation:
 * Tab Selection -> Podcast List -> Episode List
 *
 * Back button navigates up the hierarchy, only exiting from the root screen.
 */

#include "llz_sdk.h"
#include "llz_sdk_subscribe.h"
#include "llz_sdk_navigation.h"
#include "llizard_plugin.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ============================================================================
// Screen States
// ============================================================================

typedef enum {
    SCREEN_TAB_SELECT,      // Root: Choose tab
    SCREEN_PODCAST_LIST,    // Scrollable list of podcasts
    SCREEN_EPISODE_LIST,    // Scrollable list of episodes for a podcast
    SCREEN_RECENT_EPISODES  // Recent episodes across all podcasts
} PodcastScreen;

// ============================================================================
// Data Structures
// ============================================================================

#define MAX_PODCASTS 50
#define MAX_EPISODES_PER_PAGE 15
#define MAX_RECENT_EPISODES 30

// Lightweight podcast channel info (for A-Z list)
typedef struct {
    char id[64];
    char title[128];
    char author[128];
    int episodeCount;       // Total episode count for display
} PodcastChannel;

// Episode info with display data
typedef struct {
    char episodeHash[32];   // Episode hash (CRC32) for playback command
    char title[128];
    char duration[16];
    char publishDate[32];   // Human-readable date for display
    long long pubDate;      // Unix timestamp (ms) for sorting - higher = more recent
} Episode;

// Recent episode with podcast context
typedef struct {
    char episodeHash[32];   // Episode hash (CRC32) for playback command (preferred)
    char podcastId[64];     // DEPRECATED: use episodeHash instead
    char podcastTitle[128];
    char title[128];
    char duration[16];
    char publishDate[32];
    long long pubDate;
    int episodeIndex;       // DEPRECATED: Index within the podcast for playback
} RecentEpisode;

// Current podcast's episode data (loaded on demand)
typedef struct {
    char podcastId[64];
    char podcastTitle[128];
    int totalEpisodes;
    int offset;
    bool hasMore;
    Episode episodes[MAX_EPISODES_PER_PAGE];
    int loadedCount;        // Number of episodes actually loaded
} CurrentPodcastEpisodes;

// ============================================================================
// Redis/Media State (lazy loading)
// ============================================================================

// Media/Redis initialization state
static bool g_mediaInitialized = false;
static float g_refreshTimer = 0.0f;
static const float REFRESH_INTERVAL = 0.5f;  // Poll Redis every 0.5 second for lazy loading

// Podcast channel list (A-Z)
static bool g_podcastListValid = false;
static bool g_podcastListRequested = false;
static PodcastChannel g_podcastChannels[MAX_PODCASTS];
static int g_podcastChannelCount = 0;

// Recent episodes (across all podcasts)
static bool g_recentEpisodesValid = false;
static bool g_recentEpisodesRequested = false;
static RecentEpisode g_recentEpisodeList[MAX_RECENT_EPISODES];
static int g_recentEpisodeListCount = 0;

// Current podcast's episodes (loaded on demand)
static bool g_currentEpisodesValid = false;
static bool g_currentEpisodesRequested = false;
static CurrentPodcastEpisodes g_currentEpisodes;

// ============================================================================
// Plugin State
// ============================================================================

static PodcastScreen g_currentScreen = SCREEN_TAB_SELECT;
static int g_selectedTab = 0;           // 0=Recent Episodes, 1=Recent Podcasts, 2=A-Z
static int g_selectedPodcast = -1;      // Index into podcast channel array
static char g_selectedPodcastId[64] = "";  // ID of selected podcast for episode requests
static int g_listScrollOffset = 0;      // Scroll position for lists
static int g_highlightedItem = 0;       // Currently highlighted item
static bool g_wantsClose = false;
static float g_highlightPulse = 0.0f;   // Animation timer

// Tab count
static const int TAB_COUNT = 3;

// Display constants
static const int SCREEN_WIDTH = 800;
static const int SCREEN_HEIGHT = 480;
static const int HEADER_HEIGHT = 80;
static const int ITEM_HEIGHT = 72;
static const int ITEM_SPACING = 8;
static const int ITEMS_PER_PAGE = 5;
static const int PADDING = 32;
static const int LIST_TOP = 100;

// Modern color palette (matching host interface)
static const Color COLOR_BG_DARK = {18, 18, 22, 255};
static const Color COLOR_BG_GRADIENT = {28, 24, 38, 255};
static const Color COLOR_ACCENT = {138, 106, 210, 255};
static const Color COLOR_ACCENT_DIM = {90, 70, 140, 255};
static const Color COLOR_TEXT_PRIMARY = {245, 245, 250, 255};
static const Color COLOR_TEXT_SECONDARY = {160, 160, 175, 255};
static const Color COLOR_TEXT_DIM = {100, 100, 115, 255};
static const Color COLOR_CARD_BG = {32, 30, 42, 255};
static const Color COLOR_CARD_SELECTED = {48, 42, 68, 255};
static const Color COLOR_CARD_BORDER = {60, 55, 80, 255};

// Legacy color aliases
#define COLOR_TEXT COLOR_TEXT_PRIMARY
#define COLOR_ITEM_BG COLOR_CARD_BG
#define COLOR_ITEM_HIGHLIGHT COLOR_CARD_SELECTED

// Font
static Font g_podcastFont;
static bool g_fontLoaded = false;

// Smooth scroll state
static float g_smoothScrollOffset = 0.0f;
static float g_targetScrollOffset = 0.0f;

// ============================================================================
// Forward Declarations
// ============================================================================

static void RequestPodcastEpisodes(const char *podcastId, int offset, int limit);

// ============================================================================
// Font Loading (using SDK)
// ============================================================================

static int *BuildUnicodeCodepoints(int *outCount) {
    static const int ranges[][2] = {
        {0x0020, 0x007E},   // ASCII
        {0x00A0, 0x00FF},   // Latin-1 Supplement
        {0x0100, 0x017F},   // Latin Extended-A
    };

    const int numRanges = sizeof(ranges) / sizeof(ranges[0]);
    int total = 0;
    for (int i = 0; i < numRanges; i++) {
        total += (ranges[i][1] - ranges[i][0] + 1);
    }

    int *codepoints = (int *)malloc(total * sizeof(int));
    if (!codepoints) {
        *outCount = 0;
        return NULL;
    }

    int idx = 0;
    for (int i = 0; i < numRanges; i++) {
        for (int cp = ranges[i][0]; cp <= ranges[i][1]; cp++) {
            codepoints[idx++] = cp;
        }
    }

    *outCount = total;
    return codepoints;
}

static void LoadPodcastFont(void) {
    // Build custom codepoints for extended Latin support (podcast titles may have accented characters)
    int codepointCount = 0;
    int *codepoints = BuildUnicodeCodepoints(&codepointCount);

    // Use SDK to load font with custom codepoints - handles path discovery automatically
    g_podcastFont = LlzFontLoadCustom(LLZ_FONT_UI, 48, codepoints, codepointCount);

    if (g_podcastFont.texture.id != 0) {
        g_fontLoaded = true;
        SetTextureFilter(g_podcastFont.texture, TEXTURE_FILTER_BILINEAR);
        printf("Podcast: Loaded font via SDK with extended Latin codepoints\n");
    } else {
        // Fallback to default SDK font if custom loading fails
        g_podcastFont = LlzFontGet(LLZ_FONT_UI, 48);
        g_fontLoaded = false;  // Mark as not custom-loaded so we don't unload SDK-cached font
        printf("Podcast: Using SDK default font (custom codepoint loading failed)\n");
    }

    if (codepoints) free(codepoints);
}

static void UnloadPodcastFont(void) {
    // Only unload if we loaded a custom font (LlzFontLoadCustom returns caller-owned font)
    if (g_fontLoaded && g_podcastFont.texture.id != 0) {
        UnloadFont(g_podcastFont);
    }
    g_fontLoaded = false;
}

// ============================================================================
// Smooth Scroll
// ============================================================================

static void UpdateSmoothScroll(float deltaTime) {
    float diff = g_targetScrollOffset - g_smoothScrollOffset;
    float speed = 12.0f;
    g_smoothScrollOffset += diff * speed * deltaTime;
    if (fabsf(diff) < 0.5f) {
        g_smoothScrollOffset = g_targetScrollOffset;
    }
}

static float CalculateTargetScroll(int selected, int totalItems, int visibleItems) {
    if (totalItems <= visibleItems) return 0.0f;

    float itemTotalHeight = ITEM_HEIGHT + ITEM_SPACING;
    float totalListHeight = totalItems * itemTotalHeight;
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 40;
    float maxScroll = totalListHeight - visibleArea;
    if (maxScroll < 0) maxScroll = 0;

    float selectedTop = selected * itemTotalHeight;
    float selectedBottom = selectedTop + ITEM_HEIGHT;

    float visibleTop = g_targetScrollOffset;
    float visibleBottom = g_targetScrollOffset + visibleArea;

    float topMargin = ITEM_HEIGHT * 0.5f;
    float bottomMargin = ITEM_HEIGHT * 1.2f;

    float newTarget = g_targetScrollOffset;

    if (selectedTop < visibleTop + topMargin) {
        newTarget = selectedTop - topMargin;
    } else if (selectedBottom > visibleBottom - bottomMargin) {
        newTarget = selectedBottom - visibleArea + bottomMargin;
    }

    if (newTarget < 0) newTarget = 0;
    if (newTarget > maxScroll) newTarget = maxScroll;

    return newTarget;
}

// ============================================================================
// JSON Parsing Helpers
// ============================================================================

// Skip whitespace
static const char *skipWs(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

// Parse a JSON string value, return pointer after closing quote
static const char *parseString(const char *p, char *out, size_t maxLen) {
    if (*p != '"') return p;
    p++; // Skip opening quote

    size_t i = 0;
    while (*p != '"' && *p != '\0' && i < maxLen - 1) {
        if (*p == '\\' && *(p+1) != '\0') {
            p++; // Skip backslash
        }
        out[i++] = *p++;
    }
    out[i] = '\0';

    if (*p == '"') p++; // Skip closing quote
    return p;
}

// Skip a JSON value (string, number, object, array)
static const char *skipValue(const char *p) {
    p = skipWs(p);
    if (*p == '"') {
        p++;
        while (*p != '"' && *p != '\0') {
            if (*p == '\\' && *(p+1) != '\0') p++;
            p++;
        }
        if (*p == '"') p++;
    } else if (*p == '{') {
        int depth = 1;
        p++;
        while (depth > 0 && *p != '\0') {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            else if (*p == '"') {
                p++;
                while (*p != '"' && *p != '\0') {
                    if (*p == '\\' && *(p+1) != '\0') p++;
                    p++;
                }
            }
            p++;
        }
    } else if (*p == '[') {
        int depth = 1;
        p++;
        while (depth > 0 && *p != '\0') {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            else if (*p == '"') {
                p++;
                while (*p != '"' && *p != '\0') {
                    if (*p == '\\' && *(p+1) != '\0') p++;
                    p++;
                }
            }
            p++;
        }
    } else {
        while (*p != ',' && *p != '}' && *p != ']' && *p != '\0') p++;
    }
    return p;
}

// Parse integer value
static int parseInt(const char *p) {
    int val = 0;
    bool negative = false;
    if (*p == '-') {
        negative = true;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    return negative ? -val : val;
}

// Parse long long value
static long long parseLongLong(const char *p) {
    long long val = 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    return val;
}

// Parse boolean value
static bool parseBool(const char *p) {
    return (*p == 't' || *p == 'T');
}

/**
 * Parse PodcastListResponse JSON
 * Format: {"podcasts":[{"id":"...","title":"...","author":"...","episodeCount":N},...]}
 * Returns number of channels parsed
 */
static int ParsePodcastListJson(const char *json) {
    if (!json || json[0] == '\0') return 0;

    // Find "podcasts" array
    const char *p = strstr(json, "\"podcasts\"");
    if (!p) return 0;

    p = strchr(p, '[');
    if (!p) return 0;
    p++; // Skip '['

    int count = 0;
    while (count < MAX_PODCASTS && *p != '\0') {
        p = skipWs(p);
        if (*p == ']') break;
        if (*p != '{') break;
        p++; // Skip '{'

        PodcastChannel *channel = &g_podcastChannels[count];
        memset(channel, 0, sizeof(PodcastChannel));
        strncpy(channel->title, "Unknown Podcast", sizeof(channel->title) - 1);
        strncpy(channel->author, "Unknown", sizeof(channel->author) - 1);

        while (*p != '\0' && *p != '}') {
            p = skipWs(p);
            if (*p == '}') break;
            if (*p == ',') { p++; continue; }
            if (*p != '"') break;

            p++; // Skip opening quote
            const char *fieldStart = p;
            while (*p != '"' && *p != '\0') p++;
            size_t fieldLen = p - fieldStart;
            if (*p == '"') p++;

            p = skipWs(p);
            if (*p == ':') p++;
            p = skipWs(p);

            if (fieldLen == 2 && strncmp(fieldStart, "id", 2) == 0) {
                p = parseString(p, channel->id, sizeof(channel->id));
            } else if (fieldLen == 5 && strncmp(fieldStart, "title", 5) == 0) {
                p = parseString(p, channel->title, sizeof(channel->title));
            } else if (fieldLen == 6 && strncmp(fieldStart, "author", 6) == 0) {
                p = parseString(p, channel->author, sizeof(channel->author));
            } else if (fieldLen == 12 && strncmp(fieldStart, "episodeCount", 12) == 0) {
                channel->episodeCount = parseInt(p);
                p = skipValue(p);
            } else {
                p = skipValue(p);
            }
        }

        if (*p == '}') p++;
        count++;

        p = skipWs(p);
        if (*p == ',') p++;
    }

    printf("Podcast plugin: Parsed %d podcast channels\n", count);
    return count;
}

/**
 * Parse RecentEpisodesResponse JSON
 * Format: {"episodes":[{"podcastId":"...","podcastTitle":"...","title":"...","duration":N,"publishDate":"...","pubDate":N,"episodeIndex":N},...],"totalCount":N}
 * Returns number of episodes parsed
 */
static int ParseRecentEpisodesJson(const char *json) {
    if (!json || json[0] == '\0') return 0;

    // Find "episodes" array
    const char *p = strstr(json, "\"episodes\"");
    if (!p) return 0;

    p = strchr(p, '[');
    if (!p) return 0;
    p++; // Skip '['

    int count = 0;
    while (count < MAX_RECENT_EPISODES && *p != '\0') {
        p = skipWs(p);
        if (*p == ']') break;
        if (*p != '{') break;
        p++; // Skip '{'

        RecentEpisode *ep = &g_recentEpisodeList[count];
        memset(ep, 0, sizeof(RecentEpisode));
        strncpy(ep->title, "Unknown Episode", sizeof(ep->title) - 1);
        strncpy(ep->duration, "0:00", sizeof(ep->duration) - 1);

        while (*p != '\0' && *p != '}') {
            p = skipWs(p);
            if (*p == '}') break;
            if (*p == ',') { p++; continue; }
            if (*p != '"') break;

            p++; // Skip opening quote
            const char *fieldStart = p;
            while (*p != '"' && *p != '\0') p++;
            size_t fieldLen = p - fieldStart;
            if (*p == '"') p++;

            p = skipWs(p);
            if (*p == ':') p++;
            p = skipWs(p);

            if (fieldLen == 11 && strncmp(fieldStart, "episodeHash", 11) == 0) {
                p = parseString(p, ep->episodeHash, sizeof(ep->episodeHash));
            } else if (fieldLen == 9 && strncmp(fieldStart, "podcastId", 9) == 0) {
                p = parseString(p, ep->podcastId, sizeof(ep->podcastId));
            } else if (fieldLen == 12 && strncmp(fieldStart, "podcastTitle", 12) == 0) {
                p = parseString(p, ep->podcastTitle, sizeof(ep->podcastTitle));
            } else if (fieldLen == 5 && strncmp(fieldStart, "title", 5) == 0) {
                p = parseString(p, ep->title, sizeof(ep->title));
            } else if (fieldLen == 8 && strncmp(fieldStart, "duration", 8) == 0) {
                long long durationMs = parseLongLong(p);
                int totalSecs = (int)(durationMs / 1000);
                snprintf(ep->duration, sizeof(ep->duration), "%d:%02d", totalSecs / 60, totalSecs % 60);
                p = skipValue(p);
            } else if (fieldLen == 11 && strncmp(fieldStart, "publishDate", 11) == 0) {
                p = parseString(p, ep->publishDate, sizeof(ep->publishDate));
            } else if (fieldLen == 7 && strncmp(fieldStart, "pubDate", 7) == 0) {
                ep->pubDate = parseLongLong(p);
                p = skipValue(p);
            } else if (fieldLen == 12 && strncmp(fieldStart, "episodeIndex", 12) == 0) {
                ep->episodeIndex = parseInt(p);
                p = skipValue(p);
            } else {
                p = skipValue(p);
            }
        }

        if (*p == '}') p++;
        count++;

        p = skipWs(p);
        if (*p == ',') p++;
    }

    printf("Podcast plugin: Parsed %d recent episodes\n", count);

    // Debug: Log first few episodes
    if (count > 0) {
        printf("Podcast plugin: Recent episodes (most recent first):\n");
        for (int i = 0; i < count && i < 3; i++) {
            const RecentEpisode *ep = &g_recentEpisodeList[i];
            printf("  %d. %s - %s (pubDate=%lld)\n", i + 1, ep->podcastTitle, ep->title, ep->pubDate);
        }
    }

    return count;
}

/**
 * Parse PodcastEpisodesResponse JSON
 * Format: {"podcastId":"...","podcastTitle":"...","totalEpisodes":N,"offset":N,"hasMore":bool,"episodes":[...]}
 * Returns true if parsed successfully
 */
static bool ParsePodcastEpisodesJson(const char *json) {
    if (!json || json[0] == '\0') return false;

    const char *p = json;

    // Find and skip to opening brace
    p = strchr(p, '{');
    if (!p) return false;
    p++;

    memset(&g_currentEpisodes, 0, sizeof(CurrentPodcastEpisodes));

    // Parse top-level fields
    while (*p != '\0' && *p != '}') {
        p = skipWs(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }
        if (*p != '"') break;

        p++; // Skip opening quote
        const char *fieldStart = p;
        while (*p != '"' && *p != '\0') p++;
        size_t fieldLen = p - fieldStart;
        if (*p == '"') p++;

        p = skipWs(p);
        if (*p == ':') p++;
        p = skipWs(p);

        if (fieldLen == 9 && strncmp(fieldStart, "podcastId", 9) == 0) {
            p = parseString(p, g_currentEpisodes.podcastId, sizeof(g_currentEpisodes.podcastId));
        } else if (fieldLen == 12 && strncmp(fieldStart, "podcastTitle", 12) == 0) {
            p = parseString(p, g_currentEpisodes.podcastTitle, sizeof(g_currentEpisodes.podcastTitle));
        } else if (fieldLen == 13 && strncmp(fieldStart, "totalEpisodes", 13) == 0) {
            g_currentEpisodes.totalEpisodes = parseInt(p);
            p = skipValue(p);
        } else if (fieldLen == 6 && strncmp(fieldStart, "offset", 6) == 0) {
            g_currentEpisodes.offset = parseInt(p);
            p = skipValue(p);
        } else if (fieldLen == 7 && strncmp(fieldStart, "hasMore", 7) == 0) {
            g_currentEpisodes.hasMore = parseBool(p);
            p = skipValue(p);
        } else if (fieldLen == 8 && strncmp(fieldStart, "episodes", 8) == 0) {
            // Parse episodes array
            if (*p != '[') {
                p = skipValue(p);
                continue;
            }
            p++; // Skip '['

            int epCount = 0;
            while (epCount < MAX_EPISODES_PER_PAGE && *p != '\0') {
                p = skipWs(p);
                if (*p == ']') break;
                if (*p != '{') break;
                p++; // Skip '{'

                Episode *ep = &g_currentEpisodes.episodes[epCount];
                memset(ep, 0, sizeof(Episode));
                strncpy(ep->title, "Unknown Episode", sizeof(ep->title) - 1);
                strncpy(ep->duration, "0:00", sizeof(ep->duration) - 1);

                while (*p != '\0' && *p != '}') {
                    p = skipWs(p);
                    if (*p == '}') break;
                    if (*p == ',') { p++; continue; }
                    if (*p != '"') break;

                    p++; // Skip opening quote
                    const char *epFieldStart = p;
                    while (*p != '"' && *p != '\0') p++;
                    size_t epFieldLen = p - epFieldStart;
                    if (*p == '"') p++;

                    p = skipWs(p);
                    if (*p == ':') p++;
                    p = skipWs(p);

                    if (epFieldLen == 11 && strncmp(epFieldStart, "episodeHash", 11) == 0) {
                        p = parseString(p, ep->episodeHash, sizeof(ep->episodeHash));
                    } else if (epFieldLen == 5 && strncmp(epFieldStart, "title", 5) == 0) {
                        p = parseString(p, ep->title, sizeof(ep->title));
                    } else if (epFieldLen == 8 && strncmp(epFieldStart, "duration", 8) == 0) {
                        long long durationMs = parseLongLong(p);
                        int totalSecs = (int)(durationMs / 1000);
                        snprintf(ep->duration, sizeof(ep->duration), "%d:%02d", totalSecs / 60, totalSecs % 60);
                        p = skipValue(p);
                    } else if (epFieldLen == 11 && strncmp(epFieldStart, "publishDate", 11) == 0) {
                        p = parseString(p, ep->publishDate, sizeof(ep->publishDate));
                    } else if (epFieldLen == 7 && strncmp(epFieldStart, "pubDate", 7) == 0) {
                        ep->pubDate = parseLongLong(p);
                        p = skipValue(p);
                    } else {
                        p = skipValue(p);
                    }
                }

                if (*p == '}') p++;
                epCount++;

                p = skipWs(p);
                if (*p == ',') p++;
            }

            g_currentEpisodes.loadedCount = epCount;

            // Skip to end of array
            while (*p != '\0' && *p != ']') p++;
            if (*p == ']') p++;
        } else {
            p = skipValue(p);
        }
    }

    printf("Podcast plugin: Parsed %d episodes for podcast '%s' (offset=%d, hasMore=%s)\n",
           g_currentEpisodes.loadedCount, g_currentEpisodes.podcastTitle,
           g_currentEpisodes.offset, g_currentEpisodes.hasMore ? "true" : "false");

    return g_currentEpisodes.loadedCount > 0;
}

// ============================================================================
// Redis/Media Functions
// ============================================================================

static void MediaInitialize(void) {
    if (g_mediaInitialized) return;

    LlzMediaConfig cfg = {
        .host = "127.0.0.1",
        .port = 6379,
        .keyMap = NULL  // Use default key map
    };

    bool ok = LlzMediaInit(&cfg);
    g_mediaInitialized = true;
    g_refreshTimer = 0.0f;

    if (!ok) {
        printf("Podcast plugin: Redis init failed (will retry in background)\n");
    } else {
        printf("Podcast plugin: Redis initialized successfully\n");
    }
}

static void MediaPollPodcastData(float deltaTime) {
    if (!g_mediaInitialized) return;

    g_refreshTimer += deltaTime;
    if (g_refreshTimer < REFRESH_INTERVAL) return;
    g_refreshTimer = 0.0f;

    static int retryCount = 0;
    static char jsonBuffer[65536];  // 64KB buffer for JSON responses

    // Step 1: Request and poll for podcast channel list (A-Z)
    if (!g_podcastListValid) {
        if (!g_podcastListRequested) {
            if (LlzMediaRequestPodcastList()) {
                printf("Podcast plugin: Requested podcast channel list from Android\n");
                g_podcastListRequested = true;
            }
        }

        // Try to get cached podcast list from Redis
        if (LlzMediaGetPodcastList(jsonBuffer, sizeof(jsonBuffer))) {
            int parsedCount = ParsePodcastListJson(jsonBuffer);
            if (parsedCount > 0) {
                g_podcastChannelCount = parsedCount;
                g_podcastListValid = true;
                printf("Podcast plugin: Loaded %d podcast channels for A-Z view\n", g_podcastChannelCount);
            }
        }
    }

    // Step 2: Request and poll for recent episodes
    if (!g_recentEpisodesValid) {
        if (!g_recentEpisodesRequested) {
            if (LlzMediaRequestRecentEpisodes(MAX_RECENT_EPISODES)) {
                printf("Podcast plugin: Requested recent episodes from Android\n");
                g_recentEpisodesRequested = true;
            }
        }

        // Try to get cached recent episodes from Redis
        if (LlzMediaGetRecentEpisodes(jsonBuffer, sizeof(jsonBuffer))) {
            int parsedCount = ParseRecentEpisodesJson(jsonBuffer);
            if (parsedCount > 0) {
                g_recentEpisodeListCount = parsedCount;
                g_recentEpisodesValid = true;
                printf("Podcast plugin: Loaded %d recent episodes\n", g_recentEpisodeListCount);
            }
        }
    }

    // Step 3: If we're viewing a podcast's episodes, poll for those
    if (g_currentEpisodesRequested && !g_currentEpisodesValid && g_selectedPodcastId[0] != '\0') {
        if (LlzMediaGetPodcastEpisodesForId(g_selectedPodcastId, jsonBuffer, sizeof(jsonBuffer))) {
            if (ParsePodcastEpisodesJson(jsonBuffer)) {
                // Verify it's for the podcast we requested
                if (strcmp(g_currentEpisodes.podcastId, g_selectedPodcastId) == 0) {
                    g_currentEpisodesValid = true;
                    printf("Podcast plugin: Loaded %d episodes for podcast '%s'\n",
                           g_currentEpisodes.loadedCount, g_currentEpisodes.podcastTitle);
                }
            }
        }
    }

    // Debug logging for initial loading
    if (!g_podcastListValid || !g_recentEpisodesValid) {
        retryCount++;
        if (retryCount % 10 == 0) {
            printf("Podcast plugin: Waiting for data... (channels=%s, recent=%s)\n",
                   g_podcastListValid ? "ready" : "loading",
                   g_recentEpisodesValid ? "ready" : "loading");

            // Re-request after multiple attempts
            if (retryCount % 20 == 0) {
                if (!g_podcastListValid) g_podcastListRequested = false;
                if (!g_recentEpisodesValid) g_recentEpisodesRequested = false;
            }
        }
    }
}

// Request episodes for a specific podcast
static void RequestPodcastEpisodes(const char *podcastId, int offset, int limit) {
    if (!g_mediaInitialized || !podcastId) return;

    // Store the requested podcast ID
    strncpy(g_selectedPodcastId, podcastId, sizeof(g_selectedPodcastId) - 1);
    g_selectedPodcastId[sizeof(g_selectedPodcastId) - 1] = '\0';

    // Mark as requested but not yet valid
    g_currentEpisodesValid = false;
    g_currentEpisodesRequested = true;

    if (LlzMediaRequestPodcastEpisodes(podcastId, offset, limit)) {
        printf("Podcast plugin: Requested episodes for podcast '%s' (offset=%d, limit=%d)\n",
               podcastId, offset, limit);
    }
}

// Request more episodes (for "Load More" functionality)
static void RequestMoreEpisodes(void) {
    if (!g_currentEpisodesValid || !g_currentEpisodes.hasMore) return;

    int newOffset = g_currentEpisodes.offset + g_currentEpisodes.loadedCount;
    RequestPodcastEpisodes(g_currentEpisodes.podcastId, newOffset, MAX_EPISODES_PER_PAGE);
}

static void MediaShutdown(void) {
    if (!g_mediaInitialized) return;
    LlzMediaShutdown();
    g_mediaInitialized = false;
}

// ============================================================================
// Playback Transition (auto-switch to Now Playing)
// ============================================================================

// NOTE: We now navigate immediately when SendPlayEpisodeCommand is called,
// so we no longer need a callback-based approach. This simplifies the flow
// and makes the UI more responsive.

// Send play episode command using episode hash and immediately navigate to Now Playing
static bool SendPlayEpisodeCommand(const char *episodeHash) {
    if (!g_mediaInitialized) {
        printf("Podcast plugin: Cannot send command - media not initialized\n");
        return false;
    }

    if (!episodeHash || episodeHash[0] == '\0') {
        printf("Podcast plugin: Cannot send command - no episode hash\n");
        return false;
    }

    // Send the play command via SDK using the new episode hash format
    bool success = LlzMediaPlayEpisode(episodeHash);
    if (success) {
        printf("Podcast plugin: Sent play_episode command for hash=%s\n", episodeHash);
    } else {
        printf("Podcast plugin: Failed to queue play_episode command\n");
    }

    // Navigate to Now Playing immediately (don't wait for playback to start)
    // The Now Playing plugin will show current state or loading indicator
    printf("Podcast plugin: Navigating to Now Playing\n");
    LlzRequestOpenPlugin("Now Playing");
    g_wantsClose = true;

    return success;
}

// ============================================================================
// Helper Functions
// ============================================================================

// No sorting needed - Android sends data pre-sorted

// ============================================================================
// Drawing Helpers
// ============================================================================

static void DrawBackground(void) {
    // Subtle gradient background matching host
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG_DARK, COLOR_BG_GRADIENT);

    // Subtle accent glow at top
    for (int i = 0; i < 3; i++) {
        float alpha = 0.03f - i * 0.01f;
        Color glow = ColorAlpha(COLOR_ACCENT, alpha);
        DrawCircleGradient(SCREEN_WIDTH / 2, -100 + i * 50, 400 - i * 80, glow, ColorAlpha(glow, 0.0f));
    }
}

static void DrawHeader(const char *title, bool showBack) {
    // Header text
    float fontSize = 32;
    float textX = PADDING;

    if (showBack) {
        // Draw back arrow
        DrawTextEx(g_podcastFont, "<", (Vector2){textX, 24}, 28, 1, COLOR_ACCENT);
        textX += 36;
    }

    DrawTextEx(g_podcastFont, title, (Vector2){textX, 24}, fontSize, 2, COLOR_TEXT_PRIMARY);

    // Subtle accent underline
    DrawRectangle(PADDING, 66, 160, 3, COLOR_ACCENT);

    // Instruction text
    const char *instructions = showBack ? "back to return" : "scroll to navigate • select to open";
    DrawTextEx(g_podcastFont, instructions, (Vector2){PADDING, 76}, 14, 1, COLOR_TEXT_DIM);
}

static void DrawListItem(Rectangle bounds, const char *title, const char *subtitle,
                         bool isHighlighted, float pulse) {
    (void)pulse; // Not used with new styling

    Color cardBg = isHighlighted ? COLOR_CARD_SELECTED : COLOR_CARD_BG;
    Color borderColor = isHighlighted ? COLOR_ACCENT : COLOR_CARD_BORDER;

    // Draw card with rounded corners
    DrawRectangleRounded(bounds, 0.15f, 8, cardBg);

    // Selection accent bar on left
    if (isHighlighted) {
        Rectangle accentBar = {bounds.x, bounds.y + 8, 4, bounds.height - 16};
        DrawRectangleRounded(accentBar, 0.5f, 4, COLOR_ACCENT);
    }

    // Subtle border
    DrawRectangleRoundedLines(bounds, 0.15f, 8, ColorAlpha(borderColor, isHighlighted ? 0.6f : 0.2f));

    // Text positioning
    float textX = bounds.x + 20;
    float titleY = bounds.y + 16;
    float subtitleY = bounds.y + 44;

    // Title
    Color titleColor = isHighlighted ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY;
    DrawTextEx(g_podcastFont, title, (Vector2){textX, titleY}, 22, 1.5f, titleColor);

    // Subtitle
    if (subtitle) {
        Color subColor = isHighlighted ? COLOR_TEXT_SECONDARY : COLOR_TEXT_DIM;
        DrawTextEx(g_podcastFont, subtitle, (Vector2){textX, subtitleY}, 15, 1, subColor);
    }

    // Chevron on right for selected items
    if (isHighlighted) {
        DrawTextEx(g_podcastFont, ">", (Vector2){bounds.x + bounds.width - 30, bounds.y + (bounds.height - 20) / 2}, 20, 1, COLOR_ACCENT_DIM);
    }
}

static void DrawScrollIndicator(int currentOffset, int totalItems, int visibleItems) {
    if (totalItems <= visibleItems) return;

    int scrollAreaHeight = SCREEN_HEIGHT - LIST_TOP - 40;
    float scrollRatio = (float)currentOffset / (float)(totalItems - visibleItems);
    float handleHeight = (float)visibleItems / (float)totalItems * scrollAreaHeight;
    if (handleHeight < 40) handleHeight = 40;

    int handleY = LIST_TOP + (int)(scrollRatio * (scrollAreaHeight - handleHeight));

    // Track
    Rectangle track = {SCREEN_WIDTH - 10, LIST_TOP, 4, scrollAreaHeight};
    DrawRectangleRounded(track, 0.5f, 4, ColorAlpha(COLOR_CARD_BORDER, 0.3f));

    // Handle
    Rectangle handle = {SCREEN_WIDTH - 10, handleY, 4, handleHeight};
    DrawRectangleRounded(handle, 0.5f, 4, COLOR_ACCENT_DIM);
}

static void DrawScrollFades(bool canScrollUp, bool canScrollDown) {
    // Top fade
    if (canScrollUp) {
        for (int i = 0; i < 30; i++) {
            float alpha = (30 - i) / 30.0f * 0.8f;
            Color fade = ColorAlpha(COLOR_BG_DARK, alpha);
            DrawRectangle(0, LIST_TOP + i, SCREEN_WIDTH - 16, 1, fade);
        }
        DrawTextEx(g_podcastFont, "^", (Vector2){SCREEN_WIDTH / 2 - 6, LIST_TOP + 4}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.6f));
    }

    // Bottom fade
    if (canScrollDown) {
        int bottomY = SCREEN_HEIGHT - 40;
        for (int i = 0; i < 30; i++) {
            float alpha = i / 30.0f * 0.8f;
            Color fade = ColorAlpha(COLOR_BG_DARK, alpha);
            DrawRectangle(0, bottomY - 30 + i, SCREEN_WIDTH - 16, 1, fade);
        }
        DrawTextEx(g_podcastFont, "v", (Vector2){SCREEN_WIDTH / 2 - 6, bottomY - 18}, 14, 1, ColorAlpha(COLOR_TEXT_DIM, 0.6f));
    }
}

// ============================================================================
// Screen: Tab Selection
// ============================================================================

static void DrawTabSelectScreen(void) {
    DrawBackground();
    DrawHeader("Podcasts", false);

    const char *tabNames[] = {"Recent Episodes", "Recently Updated", "All Podcasts (A-Z)"};
    const char *tabDescs[] = {
        "Latest episodes across all podcasts",
        "Podcasts by most recent update",
        "Browse all podcasts alphabetically"
    };

    int buttonWidth = SCREEN_WIDTH - (PADDING * 2);
    int buttonHeight = ITEM_HEIGHT;
    int startY = LIST_TOP + 10;
    int spacing = ITEM_SPACING;

    for (int i = 0; i < TAB_COUNT; i++) {
        Rectangle bounds = {
            PADDING,
            startY + i * (buttonHeight + spacing),
            buttonWidth,
            buttonHeight
        };

        bool isHighlighted = (g_highlightedItem == i);
        DrawListItem(bounds, tabNames[i], tabDescs[i], isHighlighted, 0);
    }

    // Counter at bottom right
    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d of %d", g_highlightedItem + 1, TAB_COUNT);
    Vector2 counterSize = MeasureTextEx(g_podcastFont, counterStr, 16, 1);
    DrawTextEx(g_podcastFont, counterStr,
              (Vector2){SCREEN_WIDTH - counterSize.x - PADDING, SCREEN_HEIGHT - 28},
              16, 1, COLOR_TEXT_DIM);
}

static void SelectTab(int tabIndex) {
    g_selectedTab = tabIndex;
    g_listScrollOffset = 0;
    g_highlightedItem = 0;

    // Route to appropriate screen based on tab
    if (tabIndex == 0) {
        g_currentScreen = SCREEN_RECENT_EPISODES;
    } else {
        g_currentScreen = SCREEN_PODCAST_LIST;
    }
}

static void UpdateTabSelectScreen(const LlzInputState *input) {
    int delta = 0;

    // Keyboard/button navigation
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    // Scroll wheel
    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? 1 : -1;
    }

    if (delta != 0) {
        g_highlightedItem = (g_highlightedItem + delta + TAB_COUNT) % TAB_COUNT;
    }

    // Tap to select
    if (input->tap) {
        int buttonHeight = 70;
        int startY = HEADER_HEIGHT + 40;
        int spacing = 15;

        for (int i = 0; i < TAB_COUNT; i++) {
            Rectangle bounds = {
                PADDING,
                startY + i * (buttonHeight + spacing),
                SCREEN_WIDTH - (PADDING * 2),
                buttonHeight
            };
            if (CheckCollisionPointRec(input->tapPosition, bounds)) {
                SelectTab(i);
                return;
            }
        }
    }

    // Select button
    if (input->selectPressed) {
        SelectTab(g_highlightedItem);
    }
}

// ============================================================================
// Screen: Podcast List
// ============================================================================

static void DrawPodcastListScreen(void) {
    DrawBackground();

    const char *headerTitle = (g_selectedTab == 1) ? "Recently Updated" : "All Podcasts (A-Z)";
    DrawHeader(headerTitle, true);

    // Show loading message if no data yet
    if (!g_podcastListValid || g_podcastChannelCount == 0) {
        Rectangle bounds = {
            PADDING,
            LIST_TOP + 10,
            SCREEN_WIDTH - (PADDING * 2) - 16,
            ITEM_HEIGHT
        };

        DrawListItem(bounds, "Loading...", "Waiting for podcast data...", true, 0);

        DrawTextEx(g_podcastFont, "Connecting to MediaDash...",
                   (Vector2){PADDING, SCREEN_HEIGHT - 32}, 16, 1, COLOR_TEXT_DIM);
        return;
    }

    // Calculate scroll info
    float itemTotalHeight = ITEM_HEIGHT + ITEM_SPACING;
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 40;
    float totalListHeight = g_podcastChannelCount * itemTotalHeight;
    float maxScroll = totalListHeight - visibleArea;
    if (maxScroll < 0) maxScroll = 0;

    bool canScrollUp = g_smoothScrollOffset > 1.0f;
    bool canScrollDown = g_smoothScrollOffset < maxScroll - 1.0f;

    // Clipping region for list
    BeginScissorMode(0, LIST_TOP, SCREEN_WIDTH, (int)visibleArea);

    // Show list of podcast channels
    for (int i = 0; i < g_podcastChannelCount; i++) {
        float itemY = LIST_TOP + i * itemTotalHeight - g_smoothScrollOffset;

        // Skip items outside visible area
        if (itemY < LIST_TOP - ITEM_HEIGHT || itemY > SCREEN_HEIGHT) continue;

        const PodcastChannel *channel = &g_podcastChannels[i];

        Rectangle bounds = {
            PADDING,
            itemY,
            SCREEN_WIDTH - (PADDING * 2) - 16,
            ITEM_HEIGHT
        };

        bool isHighlighted = (g_highlightedItem == i);

        char subtitle[80];
        snprintf(subtitle, sizeof(subtitle), "%s  •  %d episodes",
                 channel->author, channel->episodeCount);

        DrawListItem(bounds, channel->title, subtitle, isHighlighted, 0);
    }

    EndScissorMode();

    // Scroll indicators
    DrawScrollFades(canScrollUp, canScrollDown);
    DrawScrollIndicator(g_listScrollOffset, g_podcastChannelCount, ITEMS_PER_PAGE);

    // Counter at bottom right
    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d of %d", g_highlightedItem + 1, g_podcastChannelCount);
    Vector2 counterSize = MeasureTextEx(g_podcastFont, counterStr, 16, 1);
    DrawTextEx(g_podcastFont, counterStr,
              (Vector2){SCREEN_WIDTH - counterSize.x - PADDING, SCREEN_HEIGHT - 28},
              16, 1, COLOR_TEXT_DIM);
}

static void UpdatePodcastListScreen(const LlzInputState *input) {
    // Don't allow navigation if no data loaded
    if (!g_podcastListValid || g_podcastChannelCount == 0) {
        return;
    }

    // Handle up/down navigation
    if (input->upPressed || input->scrollDelta < 0) {
        if (g_highlightedItem > 0) {
            g_highlightedItem--;
            if (g_highlightedItem < g_listScrollOffset) {
                g_listScrollOffset = g_highlightedItem;
            }
        }
    }

    if (input->downPressed || input->scrollDelta > 0) {
        if (g_highlightedItem < g_podcastChannelCount - 1) {
            g_highlightedItem++;
            if (g_highlightedItem >= g_listScrollOffset + ITEMS_PER_PAGE) {
                g_listScrollOffset = g_highlightedItem - ITEMS_PER_PAGE + 1;
            }
        }
    }

    // Tap to select
    if (input->tap) {
        int listStartY = HEADER_HEIGHT + 5;
        for (int i = 0; i < ITEMS_PER_PAGE && (g_listScrollOffset + i) < g_podcastChannelCount; i++) {
            int podcastIdx = g_listScrollOffset + i;
            Rectangle bounds = {
                PADDING,
                listStartY + (i * ITEM_HEIGHT),
                SCREEN_WIDTH - (PADDING * 2) - 10,
                ITEM_HEIGHT - 5
            };

            if (CheckCollisionPointRec(input->tapPosition, bounds)) {
                g_selectedPodcast = podcastIdx;
                g_listScrollOffset = 0;
                g_highlightedItem = 0;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;

                // Request episodes for this podcast
                const PodcastChannel *channel = &g_podcastChannels[podcastIdx];
                RequestPodcastEpisodes(channel->id, 0, MAX_EPISODES_PER_PAGE);

                g_currentScreen = SCREEN_EPISODE_LIST;
                return;
            }
        }
    }

    // Select button - go to episodes for highlighted podcast
    if (input->selectPressed) {
        g_selectedPodcast = g_highlightedItem;
        g_listScrollOffset = 0;
        g_highlightedItem = 0;
        g_smoothScrollOffset = 0.0f;
        g_targetScrollOffset = 0.0f;

        // Request episodes for this podcast
        const PodcastChannel *channel = &g_podcastChannels[g_selectedPodcast];
        RequestPodcastEpisodes(channel->id, 0, MAX_EPISODES_PER_PAGE);

        g_currentScreen = SCREEN_EPISODE_LIST;
    }
}

// ============================================================================
// Screen: Episode List
// ============================================================================

static void DrawEpisodeListScreen(void) {
    DrawBackground();

    if (g_selectedPodcast < 0 || g_selectedPodcast >= g_podcastChannelCount) {
        DrawHeader("Episodes", true);
        return;
    }

    // Show podcast title from current episodes (or from channel if still loading)
    const char *podcastTitle = g_currentEpisodesValid ?
        g_currentEpisodes.podcastTitle : g_podcastChannels[g_selectedPodcast].title;
    DrawHeader(podcastTitle, true);

    // Show loading message if episodes not yet loaded
    if (!g_currentEpisodesValid) {
        Rectangle bounds = {
            PADDING,
            LIST_TOP + 10,
            SCREEN_WIDTH - (PADDING * 2) - 16,
            ITEM_HEIGHT
        };

        DrawListItem(bounds, "Loading episodes...", "Fetching from podcast feed...", true, 0);
        return;
    }

    // Count total display items (episodes + optional "Load More" button)
    int displayItemCount = g_currentEpisodes.loadedCount;
    if (g_currentEpisodes.hasMore) {
        displayItemCount++; // Add one for "Load More" item
    }

    if (displayItemCount == 0) {
        Rectangle bounds = {
            PADDING,
            LIST_TOP + 10,
            SCREEN_WIDTH - (PADDING * 2) - 16,
            ITEM_HEIGHT
        };

        DrawListItem(bounds, "No episodes available", "This podcast has no episodes", true, 0);
        return;
    }

    // Calculate scroll info
    float itemTotalHeight = ITEM_HEIGHT + ITEM_SPACING;
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 40;
    float totalListHeight = displayItemCount * itemTotalHeight;
    float maxScroll = totalListHeight - visibleArea;
    if (maxScroll < 0) maxScroll = 0;

    bool canScrollUp = g_smoothScrollOffset > 1.0f;
    bool canScrollDown = g_smoothScrollOffset < maxScroll - 1.0f;

    // Clipping region for list
    BeginScissorMode(0, LIST_TOP, SCREEN_WIDTH, (int)visibleArea);

    // Draw episodes
    for (int i = 0; i < g_currentEpisodes.loadedCount; i++) {
        float itemY = LIST_TOP + i * itemTotalHeight - g_smoothScrollOffset;

        // Skip items outside visible area
        if (itemY < LIST_TOP - ITEM_HEIGHT || itemY > SCREEN_HEIGHT) continue;

        const Episode *episode = &g_currentEpisodes.episodes[i];

        Rectangle bounds = {
            PADDING,
            itemY,
            SCREEN_WIDTH - (PADDING * 2) - 16,
            ITEM_HEIGHT
        };

        bool isHighlighted = (g_highlightedItem == i);

        char subtitle[80];
        snprintf(subtitle, sizeof(subtitle), "%s  •  %s",
                 episode->publishDate, episode->duration);

        DrawListItem(bounds, episode->title, subtitle, isHighlighted, 0);
    }

    // Draw "Load More" button if there are more episodes
    if (g_currentEpisodes.hasMore) {
        int loadMoreIdx = g_currentEpisodes.loadedCount;
        float itemY = LIST_TOP + loadMoreIdx * itemTotalHeight - g_smoothScrollOffset;

        if (itemY >= LIST_TOP - ITEM_HEIGHT && itemY <= SCREEN_HEIGHT) {
            Rectangle bounds = {
                PADDING,
                itemY,
                SCREEN_WIDTH - (PADDING * 2) - 16,
                ITEM_HEIGHT
            };

            bool isHighlighted = (g_highlightedItem == loadMoreIdx);

            char loadMoreText[64];
            int remaining = g_currentEpisodes.totalEpisodes - (g_currentEpisodes.offset + g_currentEpisodes.loadedCount);
            snprintf(loadMoreText, sizeof(loadMoreText), "Load More Episodes (%d remaining)", remaining);

            DrawListItem(bounds, loadMoreText, "Tap to load next 15 episodes", isHighlighted, 0);
        }
    }

    EndScissorMode();

    // Scroll indicators
    DrawScrollFades(canScrollUp, canScrollDown);
    DrawScrollIndicator(g_listScrollOffset, displayItemCount, ITEMS_PER_PAGE);

    // Counter at bottom right - show position within total episodes
    char counterStr[48];
    int displayedSoFar = g_currentEpisodes.offset + g_currentEpisodes.loadedCount;
    snprintf(counterStr, sizeof(counterStr), "%d of %d (showing %d-%d)",
             g_highlightedItem + 1, displayItemCount,
             g_currentEpisodes.offset + 1, displayedSoFar);
    Vector2 counterSize = MeasureTextEx(g_podcastFont, counterStr, 14, 1);
    DrawTextEx(g_podcastFont, counterStr,
              (Vector2){SCREEN_WIDTH - counterSize.x - PADDING, SCREEN_HEIGHT - 28},
              14, 1, COLOR_TEXT_DIM);
}

static void UpdateEpisodeListScreen(const LlzInputState *input) {
    // Don't allow navigation if no data loaded
    if (g_selectedPodcast < 0 || g_selectedPodcast >= g_podcastChannelCount) {
        return;
    }

    if (!g_currentEpisodesValid || g_currentEpisodes.loadedCount == 0) {
        return;
    }

    // Calculate total items including "Load More" button
    int totalItems = g_currentEpisodes.loadedCount;
    if (g_currentEpisodes.hasMore) {
        totalItems++; // Add "Load More" button
    }

    int delta = 0;

    // Keyboard/button navigation
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    // Scroll wheel
    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? 1 : -1;
    }

    if (delta != 0) {
        g_highlightedItem += delta;

        // Clamp to valid range (includes Load More button)
        if (g_highlightedItem < 0) g_highlightedItem = 0;
        if (g_highlightedItem >= totalItems) {
            g_highlightedItem = totalItems - 1;
        }

        // Adjust scroll offset to keep highlighted item visible
        if (g_highlightedItem < g_listScrollOffset) {
            g_listScrollOffset = g_highlightedItem;
        }
        if (g_highlightedItem >= g_listScrollOffset + ITEMS_PER_PAGE) {
            g_listScrollOffset = g_highlightedItem - ITEMS_PER_PAGE + 1;
        }
    }

    // Tap on episode or "Load More" button
    if (input->tap) {
        int listStartY = HEADER_HEIGHT + 5;

        for (int i = 0; i < ITEMS_PER_PAGE && (g_listScrollOffset + i) < totalItems; i++) {
            Rectangle bounds = {
                PADDING,
                listStartY + i * ITEM_HEIGHT,
                SCREEN_WIDTH - (PADDING * 2) - 10,
                ITEM_HEIGHT - 5
            };

            if (CheckCollisionPointRec(input->tapPosition, bounds)) {
                int itemIdx = g_listScrollOffset + i;
                g_highlightedItem = itemIdx;

                // Check if this is the "Load More" button
                if (g_currentEpisodes.hasMore && itemIdx == g_currentEpisodes.loadedCount) {
                    printf("Podcast plugin: Load More tapped - requesting more episodes\n");
                    RequestMoreEpisodes();
                    return;
                }

                // It's an episode - play it
                if (itemIdx < g_currentEpisodes.loadedCount) {
                    const Episode *episode = &g_currentEpisodes.episodes[itemIdx];
                    printf("Playing episode (tap): %s\n", episode->title);

                    // Send play command and navigate to Now Playing
                    SendPlayEpisodeCommand(episode->episodeHash);
                }
                return;
            }
        }
    }

    // Select button - play the episode or trigger Load More
    if (input->selectPressed) {
        // Check if "Load More" is selected
        if (g_currentEpisodes.hasMore && g_highlightedItem == g_currentEpisodes.loadedCount) {
            printf("Podcast plugin: Load More selected - requesting more episodes\n");
            RequestMoreEpisodes();
            return;
        }

        // Play the highlighted episode
        if (g_highlightedItem >= 0 && g_highlightedItem < g_currentEpisodes.loadedCount) {
            const Episode *episode = &g_currentEpisodes.episodes[g_highlightedItem];
            printf("Playing episode: %s\n", episode->title);

            // Send play command and navigate to Now Playing
            SendPlayEpisodeCommand(episode->episodeHash);
        }
    }
}

// ============================================================================
// Screen: Recent Episodes (across all podcasts)
// ============================================================================

static void DrawRecentEpisodesScreen(void) {
    DrawBackground();
    DrawHeader("Recent Episodes", true);

    // Show loading message if no data
    if (!g_recentEpisodesValid || g_recentEpisodeListCount == 0) {
        Rectangle bounds = {
            PADDING,
            LIST_TOP + 10,
            SCREEN_WIDTH - (PADDING * 2) - 16,
            ITEM_HEIGHT
        };

        DrawListItem(bounds, "Loading...", "Fetching recent episodes...", true, 0);
        return;
    }

    // Calculate scroll info
    float itemTotalHeight = ITEM_HEIGHT + ITEM_SPACING;
    float visibleArea = SCREEN_HEIGHT - LIST_TOP - 40;
    float totalListHeight = g_recentEpisodeListCount * itemTotalHeight;
    float maxScroll = totalListHeight - visibleArea;
    if (maxScroll < 0) maxScroll = 0;

    bool canScrollUp = g_smoothScrollOffset > 1.0f;
    bool canScrollDown = g_smoothScrollOffset < maxScroll - 1.0f;

    // Clipping region for list
    BeginScissorMode(0, LIST_TOP, SCREEN_WIDTH, (int)visibleArea);

    for (int i = 0; i < g_recentEpisodeListCount; i++) {
        float itemY = LIST_TOP + i * itemTotalHeight - g_smoothScrollOffset;

        // Skip items outside visible area
        if (itemY < LIST_TOP - ITEM_HEIGHT || itemY > SCREEN_HEIGHT) continue;

        const RecentEpisode *ep = &g_recentEpisodeList[i];

        Rectangle bounds = {
            PADDING,
            itemY,
            SCREEN_WIDTH - (PADDING * 2) - 16,
            ITEM_HEIGHT
        };

        bool isHighlighted = (g_highlightedItem == i);

        // Show podcast name and date in subtitle
        char subtitle[100];
        snprintf(subtitle, sizeof(subtitle), "%s  •  %s",
                 ep->podcastTitle, ep->publishDate);

        DrawListItem(bounds, ep->title, subtitle, isHighlighted, 0);
    }

    EndScissorMode();

    // Scroll indicators
    DrawScrollFades(canScrollUp, canScrollDown);
    DrawScrollIndicator(g_listScrollOffset, g_recentEpisodeListCount, ITEMS_PER_PAGE);

    // Counter at bottom right
    char counterStr[32];
    snprintf(counterStr, sizeof(counterStr), "%d of %d", g_highlightedItem + 1, g_recentEpisodeListCount);
    Vector2 counterSize = MeasureTextEx(g_podcastFont, counterStr, 16, 1);
    DrawTextEx(g_podcastFont, counterStr,
              (Vector2){SCREEN_WIDTH - counterSize.x - PADDING, SCREEN_HEIGHT - 28},
              16, 1, COLOR_TEXT_DIM);
}

static void UpdateRecentEpisodesScreen(const LlzInputState *input) {
    // Don't allow navigation if no data loaded
    if (!g_recentEpisodesValid || g_recentEpisodeListCount == 0) {
        return;
    }

    int delta = 0;

    // Keyboard/button navigation
    if (input->downPressed) delta = 1;
    if (input->upPressed) delta = -1;

    // Scroll wheel
    if (input->scrollDelta != 0) {
        delta = (input->scrollDelta > 0) ? 1 : -1;
    }

    if (delta != 0) {
        g_highlightedItem += delta;

        // Clamp to valid range
        if (g_highlightedItem < 0) g_highlightedItem = 0;
        if (g_highlightedItem >= g_recentEpisodeListCount) {
            g_highlightedItem = g_recentEpisodeListCount - 1;
        }

        // Adjust scroll offset to keep highlighted item visible
        if (g_highlightedItem < g_listScrollOffset) {
            g_listScrollOffset = g_highlightedItem;
        }
        if (g_highlightedItem >= g_listScrollOffset + ITEMS_PER_PAGE) {
            g_listScrollOffset = g_highlightedItem - ITEMS_PER_PAGE + 1;
        }
    }

    // Tap on episode - play the episode and transition to Now Playing
    if (input->tap) {
        int listStartY = HEADER_HEIGHT + 5;

        for (int i = 0; i < ITEMS_PER_PAGE && (g_listScrollOffset + i) < g_recentEpisodeListCount; i++) {
            Rectangle bounds = {
                PADDING,
                listStartY + i * ITEM_HEIGHT,
                SCREEN_WIDTH - (PADDING * 2) - 10,
                ITEM_HEIGHT - 5
            };

            if (CheckCollisionPointRec(input->tapPosition, bounds)) {
                g_highlightedItem = g_listScrollOffset + i;
                const RecentEpisode *ep = &g_recentEpisodeList[g_highlightedItem];

                printf("Playing episode (tap): %s - %s\n", ep->podcastTitle, ep->title);

                // Send play command and navigate to Now Playing
                SendPlayEpisodeCommand(ep->episodeHash);
                return;
            }
        }
    }

    // Select button - play the episode and transition to Now Playing
    if (input->selectPressed) {
        if (g_highlightedItem >= 0 && g_highlightedItem < g_recentEpisodeListCount) {
            const RecentEpisode *ep = &g_recentEpisodeList[g_highlightedItem];

            printf("Playing episode: %s - %s\n", ep->podcastTitle, ep->title);

            // Send play command and navigate to Now Playing
            SendPlayEpisodeCommand(ep->episodeHash);
        }
    }
}

// ============================================================================
// Plugin Lifecycle
// ============================================================================

static void PluginInit(int width, int height) {
    (void)width;
    (void)height;

    printf("Podcast plugin initialized\n");

    // Load font
    LoadPodcastFont();

    // Reset state
    g_currentScreen = SCREEN_TAB_SELECT;
    g_selectedTab = 0;
    g_selectedPodcast = -1;
    g_selectedPodcastId[0] = '\0';
    g_listScrollOffset = 0;
    g_highlightedItem = 0;
    g_wantsClose = false;
    g_highlightPulse = 0.0f;
    g_smoothScrollOffset = 0.0f;
    g_targetScrollOffset = 0.0f;

    // Reset lazy loading state
    g_podcastListValid = false;
    g_podcastListRequested = false;
    g_podcastChannelCount = 0;

    g_recentEpisodesValid = false;
    g_recentEpisodesRequested = false;
    g_recentEpisodeListCount = 0;

    g_currentEpisodesValid = false;
    g_currentEpisodesRequested = false;
    memset(&g_currentEpisodes, 0, sizeof(g_currentEpisodes));

    // Initialize Redis/Media connection
    MediaInitialize();
}

static void PluginUpdate(const LlzInputState *input, float deltaTime) {
    g_highlightPulse += deltaTime;

    // Poll Redis for podcast data
    MediaPollPodcastData(deltaTime);

    // Poll subscriptions for playstate changes (needed for auto-transition to Now Playing)
    LlzSubscriptionPoll();

    // Update smooth scrolling
    UpdateSmoothScroll(deltaTime);

    // Calculate target scroll based on current screen
    switch (g_currentScreen) {
        case SCREEN_PODCAST_LIST:
            g_targetScrollOffset = CalculateTargetScroll(g_highlightedItem, g_podcastChannelCount, ITEMS_PER_PAGE);
            break;
        case SCREEN_EPISODE_LIST: {
            // Include "Load More" button in item count if there are more episodes
            int totalItems = g_currentEpisodes.loadedCount;
            if (g_currentEpisodes.hasMore) totalItems++;
            g_targetScrollOffset = CalculateTargetScroll(g_highlightedItem, totalItems, ITEMS_PER_PAGE);
            break;
        }
        case SCREEN_RECENT_EPISODES:
            g_targetScrollOffset = CalculateTargetScroll(g_highlightedItem, g_recentEpisodeListCount, ITEMS_PER_PAGE);
            break;
        default:
            g_targetScrollOffset = 0.0f;
            break;
    }

    // Use SDK's backReleased for proper release detection
    bool backJustReleased = input->backReleased;

    // Handle back button based on current screen
    if (backJustReleased) {
        switch (g_currentScreen) {
            case SCREEN_TAB_SELECT:
                // On root screen, exit the plugin
                g_wantsClose = true;
                return;

            case SCREEN_PODCAST_LIST:
                // Go back to tab selection
                g_currentScreen = SCREEN_TAB_SELECT;
                g_listScrollOffset = 0;
                g_highlightedItem = g_selectedTab;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;
                return;

            case SCREEN_EPISODE_LIST:
                // Go back to podcast list
                g_currentScreen = SCREEN_PODCAST_LIST;
                g_listScrollOffset = 0;
                g_highlightedItem = g_selectedPodcast;  // Highlight the podcast we came from
                g_selectedPodcast = -1;
                g_selectedPodcastId[0] = '\0';
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;

                // Clear episode data for the podcast we were viewing
                g_currentEpisodesValid = false;
                g_currentEpisodesRequested = false;
                return;

            case SCREEN_RECENT_EPISODES:
                // Go back to tab selection
                g_currentScreen = SCREEN_TAB_SELECT;
                g_listScrollOffset = 0;
                g_highlightedItem = g_selectedTab;
                g_smoothScrollOffset = 0.0f;
                g_targetScrollOffset = 0.0f;
                return;
        }
    }

    // Screen-specific input handling
    switch (g_currentScreen) {
        case SCREEN_TAB_SELECT:
            UpdateTabSelectScreen(input);
            break;

        case SCREEN_PODCAST_LIST:
            UpdatePodcastListScreen(input);
            break;

        case SCREEN_EPISODE_LIST:
            UpdateEpisodeListScreen(input);
            break;

        case SCREEN_RECENT_EPISODES:
            UpdateRecentEpisodesScreen(input);
            break;
    }
}

static void PluginDraw(void) {
    // Background is drawn by each screen function now
    switch (g_currentScreen) {
        case SCREEN_TAB_SELECT:
            DrawTabSelectScreen();
            break;

        case SCREEN_PODCAST_LIST:
            DrawPodcastListScreen();
            break;

        case SCREEN_EPISODE_LIST:
            DrawEpisodeListScreen();
            break;

        case SCREEN_RECENT_EPISODES:
            DrawRecentEpisodesScreen();
            break;
    }
}

static void PluginShutdown(void) {
    // Unload font
    UnloadPodcastFont();

    // Shutdown Redis/Media connection
    MediaShutdown();

    printf("Podcast plugin shutdown\n");
}

static bool PluginWantsClose(void) {
    return g_wantsClose;
}

// ============================================================================
// Plugin API Export
// ============================================================================

static LlzPluginAPI g_api = {
    .name = "Podcasts",
    .description = "Browse podcasts and episodes",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginWantsClose,
    .handles_back_button = true,  // Plugin handles back for hierarchical navigation
    .category = LLZ_CATEGORY_MEDIA
};

const LlzPluginAPI *LlzGetPlugin(void) {
    return &g_api;
}
