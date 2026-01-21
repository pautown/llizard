#ifndef LLZ_SDK_MEDIA_H
#define LLZ_SDK_MEDIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLZ_MEDIA_TEXT_MAX 128
#define LLZ_MEDIA_PATH_MAX 256

typedef struct {
    char track[LLZ_MEDIA_TEXT_MAX];
    char artist[LLZ_MEDIA_TEXT_MAX];
    char album[LLZ_MEDIA_TEXT_MAX];
    char albumArtPath[LLZ_MEDIA_PATH_MAX];
    bool isPlaying;
    int durationSeconds;
    int positionSeconds;
    int volumePercent;
    int64_t updatedAt;
} LlzMediaState;

typedef struct {
    bool connected;
    char deviceName[LLZ_MEDIA_TEXT_MAX];
} LlzConnectionStatus;

typedef struct {
    char showName[LLZ_MEDIA_TEXT_MAX];
    char episodeTitle[LLZ_MEDIA_TEXT_MAX];
    char episodeDescription[512];
    char author[LLZ_MEDIA_TEXT_MAX];
    char artPath[LLZ_MEDIA_PATH_MAX];
    int episodeCount;
    int currentEpisodeIndex;
    int durationSeconds;
    int positionSeconds;
    bool isPlaying;
} LlzPodcastState;

typedef enum {
    LLZ_PLAYBACK_PLAY = 0,
    LLZ_PLAYBACK_PAUSE,
    LLZ_PLAYBACK_TOGGLE,
    LLZ_PLAYBACK_NEXT,
    LLZ_PLAYBACK_PREVIOUS,
    LLZ_PLAYBACK_SEEK_TO,
    LLZ_PLAYBACK_SET_VOLUME
} LlzPlaybackCommand;

typedef struct {
    const char *trackTitle;
    const char *artistName;
    const char *albumName;
    const char *isPlaying;
    const char *durationSeconds;
    const char *progressSeconds;
    const char *albumArtPath;
    const char *volumePercent;
    const char *bleConnected;
    const char *bleName;
    const char *playbackCommandQueue;
    const char *albumArtRequest;
    const char *podcastRequestQueue;
    const char *podcastShowName;
    const char *podcastEpisodeTitle;
    const char *podcastEpisodeDescription;
    const char *podcastEpisodeList;
    const char *podcastEpisodeCount;
    const char *podcastAuthor;
    const char *podcastArtPath;
    const char *lyricsEnabled;
    const char *lyricsData;
    const char *lyricsHash;
    const char *lyricsSynced;
} LlzMediaKeyMap;

typedef struct {
    const char *host;
    int port;
    const LlzMediaKeyMap *keyMap;
} LlzMediaConfig;

bool LlzMediaInit(const LlzMediaConfig *config);
void LlzMediaShutdown(void);

bool LlzMediaGetState(LlzMediaState *outState);
bool LlzMediaGetConnection(LlzConnectionStatus *outStatus);
float LlzMediaGetProgressPercent(const LlzMediaState *state);

bool LlzMediaSendCommand(LlzPlaybackCommand action, int value);
bool LlzMediaSeekSeconds(int seconds);
bool LlzMediaSetVolume(int percent);

// Album art request - sends request to Android companion to fetch/transmit album art
// hash: CRC32 decimal string of "artist.lowercase()|album.lowercase()"
// If hash is NULL, requests art for currently playing track
bool LlzMediaRequestAlbumArt(const char *hash);

// Generates album art hash from artist and album (CRC32 decimal string)
// Returns pointer to static buffer, caller should copy if needed
const char *LlzMediaGenerateArtHash(const char *artist, const char *album);

// Request BLE reconnection - signals golang_ble_client to attempt reconnection
// Sets Redis key "system:ble_reconnect_request" with current timestamp
// Returns true if request was sent successfully
bool LlzMediaRequestBLEReconnect(void);

// Check if BLE service (mercury) is running
// Uses runit sv status command on DRM platform
// Returns true if service is running, false otherwise
bool LlzMediaIsBLEServiceRunning(void);

// Restart BLE service (mercury) via runit
// Uses sv restart command on DRM platform
// Returns true if restart command was executed successfully
bool LlzMediaRestartBLEService(void);

// Podcast support functions

// Request podcast info - sends request to Redis queue to fetch podcast metadata
// Pushes JSON {"action":"request_podcast_info","timestamp":...} to podcast:request_q
// Returns true if request was queued successfully
bool LlzMediaRequestPodcastInfo(void);

// Get current podcast state from Redis
// Fetches podcast metadata including show name, episode title, description, etc.
// Returns true if state was retrieved successfully
bool LlzMediaGetPodcastState(LlzPodcastState *outState);

// Get podcast episode list as JSON string
// Retrieves the episode list from Redis (podcast:episode_list)
// outJson: buffer to store JSON string
// maxLen: maximum buffer size
// Returns true if episode list was retrieved successfully
bool LlzMediaGetPodcastEpisodes(char *outJson, size_t maxLen);

// Get number of podcasts in the library
// Returns the count, or 0 if not available
int LlzMediaGetPodcastCount(void);

// Get full podcast library as JSON string
// Retrieves podcast:library which contains array of all podcasts with episodes
// outJson: buffer to store JSON string
// maxLen: maximum buffer size
// Returns true if library was retrieved successfully
bool LlzMediaGetPodcastLibrary(char *outJson, size_t maxLen);

// Play a specific podcast episode by hash (preferred method)
// Sends play_episode command to Redis queue for golang_ble_client to forward to Android
// episodeHash: CRC32 hash of the episode (feedUrl+pubDate+duration), provided in episode data
// Returns true if command was queued successfully
bool LlzMediaPlayEpisode(const char *episodeHash);

// Play a specific podcast episode by index (DEPRECATED - use LlzMediaPlayEpisode instead)
// Sends play_podcast_episode command to Redis queue for golang_ble_client to forward to Android
// podcastId: The unique ID of the podcast (from podcast library)
// episodeIndex: Index of the episode to play (0-based, within the podcast's episode list)
// Returns true if command was queued successfully
bool LlzMediaPlayPodcastEpisode(const char *podcastId, int episodeIndex);

// ============================================================================
// Lazy Loading Podcast API (new, more efficient approach)
// ============================================================================

// Request podcast channel list (names only, no episodes)
// Used for A-Z podcast list view - episodes are loaded on-demand
// Returns true if request was queued successfully
bool LlzMediaRequestPodcastList(void);

// Request recent episodes across all podcasts
// limit: Maximum number of recent episodes to request (default 30 if 0)
// Returns true if request was queued successfully
bool LlzMediaRequestRecentEpisodes(int limit);

// Request episodes for a specific podcast with pagination
// podcastId: The podcast to get episodes for
// offset: Starting episode index (0-based, for pagination)
// limit: Maximum episodes to return (default 15 if 0)
// Returns true if request was queued successfully
bool LlzMediaRequestPodcastEpisodes(const char *podcastId, int offset, int limit);

// Get cached podcast channel list from Redis
// outJson: buffer to store JSON string (PodcastListResponse format)
// maxLen: maximum buffer size
// Returns true if data was retrieved successfully
bool LlzMediaGetPodcastList(char *outJson, size_t maxLen);

// Get cached recent episodes from Redis
// outJson: buffer to store JSON string (RecentEpisodesResponse format)
// maxLen: maximum buffer size
// Returns true if data was retrieved successfully
bool LlzMediaGetRecentEpisodes(char *outJson, size_t maxLen);

// Get cached podcast episodes for specific podcast from Redis
// podcastId: The podcast ID to get episodes for
// outJson: buffer to store JSON string (PodcastEpisodesResponse format)
// maxLen: maximum buffer size
// Returns true if data was retrieved successfully
bool LlzMediaGetPodcastEpisodesForId(const char *podcastId, char *outJson, size_t maxLen);

// ============================================================================
// Lyrics API
// ============================================================================

#define LLZ_LYRICS_LINE_MAX 256
#define LLZ_LYRICS_MAX_LINES 500

// A single line of lyrics with timestamp
typedef struct {
    int64_t timestampMs;  // Timestamp in milliseconds (0 if unsynced)
    char text[LLZ_LYRICS_LINE_MAX];
} LlzLyricsLine;

// Complete lyrics data
typedef struct {
    char hash[64];        // CRC32 hash of "artist|track"
    bool synced;          // True if lyrics have timestamps
    int lineCount;        // Number of lines
    LlzLyricsLine *lines; // Dynamically allocated array of lines
} LlzLyricsData;

// Check if lyrics feature is enabled
// Returns true if lyrics are enabled in settings
bool LlzLyricsIsEnabled(void);

// Set lyrics feature enabled/disabled
// Stores the setting in Redis for sync with Android companion
// Returns true if setting was stored successfully
bool LlzLyricsSetEnabled(bool enabled);

// Get current lyrics from Redis
// outLyrics: pointer to receive lyrics data (caller must call LlzLyricsFree after use)
// Returns true if lyrics are available
bool LlzLyricsGet(LlzLyricsData *outLyrics);

// Free lyrics data allocated by LlzLyricsGet
void LlzLyricsFree(LlzLyricsData *lyrics);

// Get lyrics as raw JSON string
// outJson: buffer to store JSON string
// maxLen: maximum buffer size
// Returns true if lyrics were retrieved successfully
bool LlzLyricsGetJson(char *outJson, size_t maxLen);

// Get current lyrics hash (for checking if lyrics changed)
// outHash: buffer to store hash (at least 32 bytes)
// maxLen: buffer size
// Returns true if hash was retrieved
bool LlzLyricsGetHash(char *outHash, size_t maxLen);

// Check if lyrics are synced (have timestamps)
// Returns true if current lyrics have timestamps
bool LlzLyricsAreSynced(void);

// Find the current lyrics line for a given playback position
// positionMs: current playback position in milliseconds
// lyrics: the lyrics data
// Returns the index of the current line, or -1 if not found
int LlzLyricsFindCurrentLine(int64_t positionMs, const LlzLyricsData *lyrics);

// Generate lyrics hash from artist and track
// Returns pointer to static buffer, caller should copy if needed
const char *LlzLyricsGenerateHash(const char *artist, const char *track);

// Request lyrics from Android companion via BLE
// artist: the artist name
// track: the track title
// Pushes a request to the playback command queue for golang_ble_client to forward via BLE
// Returns true if request was queued successfully
bool LlzLyricsRequest(const char *artist, const char *track);

// Store lyrics data to Redis (used when lyrics are successfully received)
// This stores the lyrics JSON and sets the hash/synced keys
// lyricsJson: the complete lyrics JSON string
// hash: the lyrics hash (artist|track CRC32)
// synced: whether the lyrics have timestamps
// Returns true if stored successfully
bool LlzLyricsStore(const char *lyricsJson, const char *hash, bool synced);

#ifdef __cplusplus
}
#endif

#endif
