# Redis Integration Plan for llizardgui SDK

## 1. What the Go BLE client publishes

The MediaDash BLE client we just mirrored under `supporting_projects/golang_ble_client/` pushes every piece of phone state into Redis. The relevant code lives in `internal/redis/store.go` and is configured via `internal/config/config.json`.

### Key map

| Logical field | Default Redis key | Source |
| ------------- | ----------------- | ------ |
| Track title   | `media:track`     | `StoreMediaState` lines 40-68 |
| Artist name   | `media:artist`    | same |
| Album name    | `media:album`     | same |
| Playing flag  | `media:playing`   | stored as literal `"true"/"false"` strings |
| Track duration (seconds) | `media:duration` | milliseconds from BLE are converted to seconds before writing |
| Current position (seconds) | `media:progress` | milliseconds converted to seconds |
| Album art path | `media:album_art_path` | `SetAlbumArtFilePath` |
| BLE connected flag | `system:ble_connected` | `SetBLEConnectionStatus` lines 433-456 |
| BLE device name | `system:ble_name` | same |
| Playback command queue | `system:playback_cmd_q` | `QueuePlaybackCommand` lines 182-208 |
| Album art cache namespace | `media:album_art_cache` | `StoreAlbumArtCache` lines 233-320 |

`config.json` also lets us change these names at runtime, but the defaults above are what the BLE client uses today.

### Data flow summary

* **Media state:** BLE notifications land in `internal/ble`, the decoded fields feed `redis.StoreMediaState`, which pipeline-sets the per-field keys and logs a timestamp.
* **Connection status:** `SetBLEConnectionStatus` flips `system:ble_connected` and writes a friendly device name whenever the BLE link comes up/down. UIs only need to read these two keys to know if the phone is reachable.
* **Commands:** UI controls call `QueuePlaybackCommand` (action strings `play`, `pause`, `next`, `previous`, `seek`, `volume`, value optional). The BLE loop blocks on `BRPOP` and forwards actions to the control characteristic.
* **Album art:** Requests are serialized JSON blobs stored under `media:album_art_request`; binary art is cached under `media:album_art_cache:<hash>` alongside a JSON metadata key (`:meta`). We only need the file path key for now.

## 2. Proposed SDK module: `llz_sdk_media`

To let plugins query/playback without re-implementing Redis plumbing, add a new SDK surface:

### Types

```c
#define LLZ_MEDIA_TEXT_MAX 128
#define LLZ_MEDIA_PATH_MAX 256

typedef struct {
    char track[LLZ_MEDIA_TEXT_MAX];
    char artist[LLZ_MEDIA_TEXT_MAX];
    char album[LLZ_MEDIA_TEXT_MAX];
    char albumArtPath[LLZ_MEDIA_PATH_MAX];
    bool isPlaying;
    int durationSeconds;  // total length
    int positionSeconds;  // current progress
    int volumePercent;    // optional, populated when available
    int64_t updatedAt;    // unix seconds from Redis timestamp
} LlzMediaState;

typedef struct {
    bool connected;
    char deviceName[LLZ_MEDIA_TEXT_MAX];
} LlzConnectionStatus;

typedef enum {
    LLZ_PLAYBACK_PLAY,
    LLZ_PLAYBACK_PAUSE,
    LLZ_PLAYBACK_TOGGLE,
    LLZ_PLAYBACK_NEXT,
    LLZ_PLAYBACK_PREVIOUS,
    LLZ_PLAYBACK_SEEK_TO,   // `value` holds seconds
    LLZ_PLAYBACK_SET_VOLUME // `value` holds percent 0-100
} LlzPlaybackCommand;
```

### API surface

```c
// Initialize/shutdown hiredis context
bool LlzMediaInit(const char *host, int port);
void LlzMediaShutdown(void);

// Getters
bool LlzMediaGetState(LlzMediaState *out);        // reads atomic track keys
bool LlzMediaGetConnection(LlzConnectionStatus *out);

// Derived data helper (optional)
float LlzMediaGetProgressPercent(const LlzMediaState *state);

// Setters/commands -> enqueue into system:playback_cmd_q
bool LlzMediaSendCommand(LlzPlaybackCommand action, int value);
bool LlzMediaSeekSeconds(int seconds);
bool LlzMediaSetVolume(int percent);
```

Internally this module will:

1. Keep a single `redisContext *` (from hiredis) and pipeline `MGET`s so UI calls remain cheap.
2. Use the same default key strings as the Go client, with an optional override struct for advanced users.
3. Serialize commands as `{"action":"pause","value":0,"timestamp":<unix>}` before pushing to `system:playback_cmd_q` (matching `PlaybackCommand` in `supporting_projects/golang_ble_client/internal/redis/store.go` lines 166-214).
4. Provide lightweight caching/dirty checks so the host does not spam Redis every frame (e.g., call into this module at ~10 Hz and stash results in host state).

### Integration plan

1. Vendor hiredis into `llizardgui-host` (reuse the copy already present in `llizardgui-raygui/external/hiredis`). Add a static `hiredis_static` target in our top-level `CMakeLists.txt` so both host and plugins can link against it.
2. Add `sdk/include/llz_sdk_media.h` and `sdk/llz_sdk/llz_sdk_media.c` that implement the API outlined above. Follow the SDK style: keep globals private, expose init/update helpers, avoid raylib dependencies.
3. Extend `sdk/README.md` to mention the new module and the required Redis keys.
4. Update the host to optionally poll `LlzMediaGetState` and route button presses to `LlzMediaSendCommand`, but keep the functions reusable so plugins can call them directly once the SDK target is rebuilt.

By mirroring the Go client’s exact Redis schema we ensure the GUI and the BLE daemon stay in lock-step: all metadata, connection status, and playback commands pass through Redis, so the UI never has to touch BLE directly.
