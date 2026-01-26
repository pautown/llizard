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

// Repeat mode values (matches Spotify API)
typedef enum {
    LLZ_REPEAT_OFF = 0,
    LLZ_REPEAT_TRACK,     // Repeat single track
    LLZ_REPEAT_CONTEXT    // Repeat playlist/album
} LlzRepeatMode;

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
    // Spotify-specific state (requires Spotify auth in companion app)
    bool shuffleEnabled;
    LlzRepeatMode repeatMode;
    bool isLiked;                        // Current track is in user's library
    char spotifyTrackId[LLZ_MEDIA_TEXT_MAX];  // Spotify track ID (for like/unlike)
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
    LLZ_PLAYBACK_SET_VOLUME,
    // Spotify-specific controls (require Spotify auth in companion app)
    LLZ_PLAYBACK_SHUFFLE_ON,
    LLZ_PLAYBACK_SHUFFLE_OFF,
    LLZ_PLAYBACK_SHUFFLE_TOGGLE,
    LLZ_PLAYBACK_REPEAT_OFF,
    LLZ_PLAYBACK_REPEAT_TRACK,
    LLZ_PLAYBACK_REPEAT_CONTEXT,
    LLZ_PLAYBACK_REPEAT_CYCLE,
    LLZ_PLAYBACK_LIKE_TRACK,
    LLZ_PLAYBACK_UNLIKE_TRACK
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

// ============================================================================
// Spotify Playback Controls (require Spotify auth in companion app)
// ============================================================================

// Shuffle control
// Returns true if command was queued successfully
bool LlzMediaSetShuffle(bool enabled);
bool LlzMediaToggleShuffle(void);

// Repeat control
// mode: LLZ_REPEAT_OFF, LLZ_REPEAT_TRACK, or LLZ_REPEAT_CONTEXT
bool LlzMediaSetRepeat(LlzRepeatMode mode);
bool LlzMediaCycleRepeat(void);  // Cycles: off -> track -> context -> off

// Like/Unlike tracks (saves/removes from Spotify library)
// trackId: Spotify track ID, or NULL to use current track
bool LlzMediaLikeTrack(const char *trackId);
bool LlzMediaUnlikeTrack(const char *trackId);

// Request Spotify playback state update (triggers fetch of shuffle/repeat/liked state)
// This causes the companion app to fetch fresh state from Spotify API
// State will be available via LlzMediaGetState() after a short delay
bool LlzMediaRequestSpotifyState(void);

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

// ============================================================================
// Media Channels API
// ============================================================================

#define LLZ_MEDIA_CHANNEL_MAX 32
#define LLZ_MEDIA_CHANNEL_NAME_MAX 64

// Media channels response
typedef struct {
    char channels[LLZ_MEDIA_CHANNEL_MAX][LLZ_MEDIA_CHANNEL_NAME_MAX];
    int count;
    int64_t timestamp;
} LlzMediaChannels;

// Request media channel list from Android companion via BLE
// Returns true if request was queued successfully
bool LlzMediaRequestChannels(void);

// Get cached media channels from Redis
// outChannels: pointer to receive channel list
// Returns true if channels were retrieved successfully
bool LlzMediaGetChannels(LlzMediaChannels *outChannels);

// Get media channels as raw JSON string
// outJson: buffer to store JSON string
// maxLen: maximum buffer size
// Returns true if channels were retrieved successfully
bool LlzMediaGetChannelsJson(char *outJson, size_t maxLen);

// Select which media channel (app) to control
// Sends command to Android companion to switch the controlled media session
// channelName: the display name of the app (e.g., "Spotify", "YouTube Music")
// Returns true if command was queued successfully
bool LlzMediaSelectChannel(const char *channelName);

// Get the currently controlled media channel name
// outChannel: buffer to store channel name
// maxLen: maximum buffer size
// Returns true if controlled channel was retrieved
bool LlzMediaGetControlledChannel(char *outChannel, size_t maxLen);

// ============================================================================
// Queue API (Spotify playback queue)
// ============================================================================

#define LLZ_QUEUE_TRACK_MAX 50
#define LLZ_QUEUE_TITLE_MAX 128
#define LLZ_QUEUE_ARTIST_MAX 128
#define LLZ_QUEUE_ALBUM_MAX 128
#define LLZ_QUEUE_URI_MAX 256

// A single track in the queue
typedef struct {
    char title[LLZ_QUEUE_TITLE_MAX];
    char artist[LLZ_QUEUE_ARTIST_MAX];
    char album[LLZ_QUEUE_ALBUM_MAX];
    int64_t durationMs;
    char uri[LLZ_QUEUE_URI_MAX];
} LlzQueueTrack;

// Complete queue data
typedef struct {
    char service[32];                        // Service name (e.g., "spotify")
    LlzQueueTrack currentlyPlaying;          // Currently playing track
    bool hasCurrentlyPlaying;                // True if currentlyPlaying is valid
    LlzQueueTrack tracks[LLZ_QUEUE_TRACK_MAX]; // Queue tracks
    int trackCount;                          // Number of tracks in queue
    int64_t timestamp;                       // When queue was fetched
} LlzQueueData;

// Request playback queue from Android companion via BLE
// Returns true if request was queued successfully
bool LlzMediaRequestQueue(void);

// Get cached queue from Redis
// outQueue: pointer to receive queue data
// Returns true if queue was retrieved successfully
bool LlzMediaGetQueue(LlzQueueData *outQueue);

// Get queue as raw JSON string
// outJson: buffer to store JSON string
// maxLen: maximum buffer size
// Returns true if queue was retrieved successfully
bool LlzMediaGetQueueJson(char *outJson, size_t maxLen);

// Skip to a specific position in the queue
// queueIndex: 0-based index in the queue (0 = first track in queue)
// Returns true if command was queued successfully
bool LlzMediaQueueShift(int queueIndex);

// ============================================================================
// Spotify Library API (library browsing via BLE)
// ============================================================================

#define LLZ_SPOTIFY_TRACK_NAME_MAX 68
#define LLZ_SPOTIFY_ARTIST_NAME_MAX 52
#define LLZ_SPOTIFY_ALBUM_NAME_MAX 52
#define LLZ_SPOTIFY_PLAYLIST_NAME_MAX 52
#define LLZ_SPOTIFY_URI_MAX 64
#define LLZ_SPOTIFY_ID_MAX 32
#define LLZ_SPOTIFY_IMAGE_URL_MAX 256
#define LLZ_SPOTIFY_LIST_MAX 50

// Spotify library overview stats
typedef struct {
    char userName[LLZ_MEDIA_TEXT_MAX];
    int likedCount;
    int albumsCount;
    int playlistsCount;
    int artistsCount;
    char currentTrack[LLZ_SPOTIFY_TRACK_NAME_MAX];
    char currentArtist[LLZ_SPOTIFY_ARTIST_NAME_MAX];
    bool isPremium;
    int64_t timestamp;
    bool valid;
} LlzSpotifyLibraryOverview;

// A track item from Spotify library (recent, liked, etc.)
typedef struct {
    char id[LLZ_SPOTIFY_ID_MAX];
    char name[LLZ_SPOTIFY_TRACK_NAME_MAX];
    char artist[LLZ_SPOTIFY_ARTIST_NAME_MAX];
    char album[LLZ_SPOTIFY_ALBUM_NAME_MAX];
    int64_t durationMs;
    char uri[LLZ_SPOTIFY_URI_MAX];
    char imageUrl[LLZ_SPOTIFY_IMAGE_URL_MAX];
} LlzSpotifyTrackItem;

// Paginated response for track lists
typedef struct {
    char type[16];                              // "recent" or "liked"
    LlzSpotifyTrackItem items[LLZ_SPOTIFY_LIST_MAX];
    int itemCount;
    int offset;
    int limit;
    int total;
    bool hasMore;
    int64_t timestamp;
    bool valid;
} LlzSpotifyTrackListResponse;

// An album item from Spotify library
typedef struct {
    char id[LLZ_SPOTIFY_ID_MAX];
    char name[LLZ_SPOTIFY_ALBUM_NAME_MAX];
    char artist[LLZ_SPOTIFY_ARTIST_NAME_MAX];
    int trackCount;
    char uri[LLZ_SPOTIFY_URI_MAX];
    char imageUrl[LLZ_SPOTIFY_IMAGE_URL_MAX];
    char year[8];
} LlzSpotifyAlbumItem;

// Paginated response for album lists
typedef struct {
    LlzSpotifyAlbumItem items[LLZ_SPOTIFY_LIST_MAX];
    int itemCount;
    int offset;
    int limit;
    int total;
    bool hasMore;
    int64_t timestamp;
    bool valid;
} LlzSpotifyAlbumListResponse;

// A playlist item from Spotify library
typedef struct {
    char id[LLZ_SPOTIFY_ID_MAX];
    char name[LLZ_SPOTIFY_PLAYLIST_NAME_MAX];
    char owner[LLZ_SPOTIFY_ARTIST_NAME_MAX];
    int trackCount;
    char uri[LLZ_SPOTIFY_URI_MAX];
    char imageUrl[LLZ_SPOTIFY_IMAGE_URL_MAX];
    bool isPublic;
} LlzSpotifyPlaylistItem;

// Paginated response for playlist lists
typedef struct {
    LlzSpotifyPlaylistItem items[LLZ_SPOTIFY_LIST_MAX];
    int itemCount;
    int offset;
    int limit;
    int total;
    bool hasMore;
    int64_t timestamp;
    bool valid;
} LlzSpotifyPlaylistListResponse;

// Request library overview stats from Android companion
// Returns true if request was queued successfully
bool LlzMediaRequestLibraryOverview(void);

// Request recently played tracks
// limit: max tracks to return (default 20 if 0)
// Returns true if request was queued successfully
bool LlzMediaRequestLibraryRecent(int limit);

// Request liked/saved tracks with pagination
// offset: starting index
// limit: max tracks per page (default 20 if 0)
// Returns true if request was queued successfully
bool LlzMediaRequestLibraryLiked(int offset, int limit);

// Request saved albums with pagination
// offset: starting index
// limit: max albums per page (default 20 if 0)
// Returns true if request was queued successfully
bool LlzMediaRequestLibraryAlbums(int offset, int limit);

// Request playlists with pagination
// offset: starting index
// limit: max playlists per page (default 20 if 0)
// Returns true if request was queued successfully
bool LlzMediaRequestLibraryPlaylists(int offset, int limit);

// Play a Spotify URI (track, album, or playlist)
// uri: Spotify URI (e.g., "spotify:track:xxx", "spotify:album:xxx", "spotify:playlist:xxx")
// Returns true if command was queued successfully
bool LlzMediaPlaySpotifyUri(const char *uri);

// Get cached library overview from Redis
// outOverview: pointer to receive overview data
// Returns true if data was retrieved successfully
bool LlzMediaGetLibraryOverview(LlzSpotifyLibraryOverview *outOverview);

// Get cached track list from Redis (recent or liked)
// type: "recent" or "liked"
// outResponse: pointer to receive track list
// Returns true if data was retrieved successfully
bool LlzMediaGetLibraryTracks(const char *type, LlzSpotifyTrackListResponse *outResponse);

// Get cached album list from Redis
// outResponse: pointer to receive album list
// Returns true if data was retrieved successfully
bool LlzMediaGetLibraryAlbums(LlzSpotifyAlbumListResponse *outResponse);

// Get cached playlist list from Redis
// outResponse: pointer to receive playlist list
// Returns true if data was retrieved successfully
bool LlzMediaGetLibraryPlaylists(LlzSpotifyPlaylistListResponse *outResponse);

// Get raw JSON for library data (for debugging or custom parsing)
bool LlzMediaGetLibraryOverviewJson(char *outJson, size_t maxLen);
bool LlzMediaGetLibraryTracksJson(const char *type, char *outJson, size_t maxLen);
bool LlzMediaGetLibraryAlbumsJson(char *outJson, size_t maxLen);
bool LlzMediaGetLibraryPlaylistsJson(char *outJson, size_t maxLen);

// ============================================================================
// Timezone API
// ============================================================================

#define LLZ_TIMEZONE_ID_MAX 64

// Timezone information from phone
typedef struct {
    int offsetMinutes;                    // Offset from UTC in minutes (e.g., -300 for EST)
    char timezoneId[LLZ_TIMEZONE_ID_MAX]; // IANA timezone ID (e.g., "America/New_York")
    bool valid;                           // True if timezone data was successfully retrieved
} LlzTimezone;

// Get phone's timezone from Redis
// outTimezone: pointer to receive timezone info
// Returns true if timezone data was retrieved successfully
bool LlzMediaGetTimezone(LlzTimezone *outTimezone);

// Get current time adjusted to phone's timezone
// This applies the timezone offset to the system time
// hours, minutes, seconds: pointers to receive time components (any can be NULL)
// Returns true if timezone was applied, false if using system local time
bool LlzMediaGetPhoneTime(int *hours, int *minutes, int *seconds);

// Get precise time adjusted to phone's timezone with sub-second accuracy
// hours, minutes, seconds: pointers to receive time components (any can be NULL)
// fractionalSecond: pointer to receive fractional second (0.0-1.0, can be NULL)
// Returns true if timezone was applied, false if using system local time
bool LlzMediaGetPhoneTimePrecise(int *hours, int *minutes, int *seconds, double *fractionalSecond);

#ifdef __cplusplus
}
#endif

#endif
