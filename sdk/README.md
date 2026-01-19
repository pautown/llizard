# llizardgui SDK

This SDK packages the pieces every DRM plugin or host needs so you don't have to re-copy full projects. It currently provides ten layers:

1. **Display (llz_sdk_display.h)** - wraps raylib window/texture setup so both the host and plugins target a logical 800x480 canvas while the SDK handles DRM rotation and render textures under the hood.
2. **Input (llz_sdk_input.h)** - unified CarThing/desktop input module with full gesture recognition and button hold detection. See [Input System](#input-system) below.
3. **Layout helpers (llz_sdk_layout.h)** - a tiny set of functions to carve up the logical canvas without rewriting rectangle math each time. Useful for plugins trying to match the llizardgui look.
4. **Media helpers (llz_sdk_media.h)** - Redis-backed accessors for track metadata, playback progress, BLE connection status, playback commands, podcasts, and lyrics routed through the MediaDash Go client.
5. **Image utilities (llz_sdk_image.h)** - blur effects and CSS-like cover/contain image scaling for album art backgrounds and UI elements.
6. **Configuration (llz_sdk_config.h)** - global system config (brightness, auto-brightness, orientation) and per-plugin configuration with automatic file creation and INI persistence.
7. **Background system (llz_sdk_background.h)** - 9 animated background styles (Pulse, Aurora, Wave, Constellation, Bokeh, etc.) with color palettes and playback-responsive energy levels.
8. **Subscription system (llz_sdk_subscribe.h)** - event-driven callbacks for media changes (track, playstate, volume, position, connection, album art) without polling.
9. **Navigation system (llz_sdk_navigation.h)** - inter-plugin navigation requests allowing plugins to request opening other plugins.
10. **Font system (llz_sdk_font.h)** - centralized font loading with automatic path resolution across CarThing and desktop, plus text drawing helpers.

---

## Input System

The input module (`llz_sdk_input.h`) provides a unified `LlzInputState` struct that abstracts CarThing hardware buttons, rotary encoder, and touchscreen into a consistent interface that also works on desktop.

### Hardware Button Events

| Field | CarThing Button | Desktop Key | Description |
|-------|-----------------|-------------|-------------|
| `backPressed` | Back (preset 4) | ESC | Exit/back navigation (pressed this frame) |
| `backReleased` | Back (preset 4) | ESC | Back button just released |
| `selectPressed` | Front dial click | ENTER | Confirm/select action (quick click) |
| `selectDown` | Front dial click | ENTER | Select button currently held |
| `selectHold` | Front dial click | ENTER | Select held past threshold (0.5s, edge-triggered) |
| `selectHoldTime` | - | - | Duration select has been held (seconds) |
| `playPausePressed` | Front dial click | ENTER | Alias for selectPressed (media control) |
| `upPressed` | Preset 1 | 1 | Up/previous navigation |
| `downPressed` | Preset 2 | 2 | Down/next navigation |
| `displayModeNext` | Preset 3 | 3 or M | Cycle display modes |
| `styleCyclePressed` | Preset 4 | 4 or B | Cycle visual styles |
| `screenshotPressed` | Screenshot | 5 or F1 | Screenshot/special action |

### Generic Button States

Each hardware button has three state fields for flexible handling:

| Field Pattern | Type | Description |
|---------------|------|-------------|
| `buttonNPressed` | `bool` | Quick click detected (released before hold threshold) |
| `buttonNDown` | `bool` | Button currently held down |
| `buttonNHold` | `bool` | Button held past 0.5s threshold (edge-triggered, fires once) |
| `buttonNHoldTime` | `float` | Duration button has been held (seconds) |

Buttons 1-5 map to Preset 1-4 + Screenshot. Button 6 maps to the screenshot button and includes special brightness toggle behavior on quick press.

### Rotary Encoder / Scroll

| Field | Type | Description |
|-------|------|-------------|
| `scrollDelta` | `float` | Rotary encoder delta (positive = clockwise). Desktop: mouse wheel. |

### Touch / Mouse State

| Field | Type | Description |
|-------|------|-------------|
| `mousePos` | `Vector2` | Current touch/mouse position |
| `mousePressed` | `bool` | Touch/mouse currently held down |
| `mouseJustPressed` | `bool` | Touch/mouse pressed this frame |
| `mouseJustReleased` | `bool` | Touch/mouse released this frame |

### Gesture Detection

The SDK automatically detects common gestures from touch/mouse input:

**Tap Detection**
| Field | Type | Description |
|-------|------|-------------|
| `tap` | `bool` | Single tap detected (< 0.3s, < 30px movement) |
| `doubleTap` | `bool` | Double tap detected (two taps within 0.35s) |
| `doubleClick` | `bool` | Alias for `doubleTap` |
| `tapPosition` | `Vector2` | Location of the tap |

**Hold/Long Press**
| Field | Type | Description |
|-------|------|-------------|
| `hold` | `bool` | Long press detected (> 0.7s stationary) |
| `longPress` | `bool` | Alias for `hold` |
| `holdPosition` | `Vector2` | Location of the hold |

**Drag Tracking**
| Field | Type | Description |
|-------|------|-------------|
| `dragActive` | `bool` | Currently dragging (touch/mouse held and moving) |
| `dragStart` | `Vector2` | Position where drag began |
| `dragCurrent` | `Vector2` | Current drag position |
| `dragDelta` | `Vector2` | Frame-to-frame movement delta |

**Swipe Detection**
| Field | Type | Description |
|-------|------|-------------|
| `swipeLeft` | `bool` | Left swipe detected (> 80px horizontal) |
| `swipeRight` | `bool` | Right swipe detected |
| `swipeUp` | `bool` | Up swipe detected (> 80px vertical) |
| `swipeDown` | `bool` | Down swipe detected |
| `swipeDelta` | `Vector2` | Total swipe displacement |
| `swipeStart` | `Vector2` | Swipe start position |
| `swipeEnd` | `Vector2` | Swipe end position |

### Gesture Thresholds

- **Tap**: < 30px movement, < 0.3s duration
- **Double tap**: Two taps within 0.35s, < 30px apart
- **Hold/Long press**: > 0.7s stationary (touch gestures)
- **Button hold**: > 0.5s held (hardware buttons)
- **Swipe**: > 80px movement in a direction

### Input API Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzInputInit()` | `void` | Initialize input subsystem. Starts CarThing input backend on DRM. |
| `LlzInputUpdate(state)` | `void` | Update input state. Call once per frame. Pass NULL to update internal state. |
| `LlzInputShutdown()` | `void` | Shutdown input subsystem. |
| `LlzInputGetState()` | `const LlzInputState*` | Get pointer to current input state. |

### Usage Example

```c
void PluginUpdate(const LlzInputState *input, float deltaTime) {
    // Button navigation
    if (input->backPressed) { /* go back */ }
    if (input->selectPressed) { /* confirm (quick click) */ }
    if (input->selectHold) { /* long press action */ }

    // Rotary/scroll for volume or list navigation
    if (input->scrollDelta != 0) {
        volume += (int)(input->scrollDelta * 5);
    }

    // Touch gestures
    if (input->tap) {
        HandleTapAt(input->tapPosition);
    }
    if (input->swipeLeft) {
        NextScreen();
    }
    if (input->dragActive) {
        // Scrubbing, scrolling, etc.
        UpdateScrubPosition(input->dragCurrent.x);
    }

    // Button hold detection
    if (input->button1Hold) {
        // Button 1 held for 0.5s - trigger alternate action
    }
}
```

### Simulated Input Globals

For advanced use cases (e.g., input injection), the SDK exposes:
- `llzSimulatedMousePressed`, `llzSimulatedMouseJustPressed`, `llzSimulatedMouseJustReleased`
- `llzSimulatedMousePos`
- `llzSimulatedScrollWheel`

---

## Why this SDK?

- **Consistency** - Every plugin sees the exact same coordinate system, input behaviours, and touch mapping, so gestures map 1:1 with the host and legacy apps.
- **Ease of authoring** - Plugin authors can focus on their UI instead of re-implementing DRM rotation, input polling, layout math, or Redis plumbing. They simply include `llz_sdk.h` and link against `llz_sdk`.
- **Future-proofing** - Central fixes (e.g., touch deadzones, display timing, new hardware quirks) land here once and automatically benefit the host and all plugins.

---

## Integrating the SDK

- The CMake target `llz_sdk` builds `sdk/llz_sdk/*.c` along with the CarThing input backend and installs public headers under `sdk/include`.
- The host links against `llz_sdk` and calls:
  - `LlzDisplayInit()/LlzDisplayBegin()/LlzDisplayEnd()/LlzDisplayShutdown()` instead of its ad-hoc render-target logic.
  - `LlzInputInit()/LlzInputUpdate()/LlzInputGetState()` for button/touch polling.
- Plugins should:
  - Include `llz_sdk.h` (or the specific sub-headers) for display constants, input state, layout helpers, and media helpers when syncing with Redis.
  - Update their API to accept `const LlzInputState *` inside the `update` callback so they inherit CarThing gestures immediately.
- If a plugin needs media metadata or wants to issue playback commands, call `LlzMediaInit` once (pointing at the Redis host), then use `LlzMediaGetState`, `LlzMediaGetConnection`, and `LlzMediaSendCommand`/`LlzMediaSetVolume` as needed.

---

## Display System

The display module (`llz_sdk_display.h`) abstracts the differences between desktop development and CarThing DRM deployment. It ensures all plugins draw to a consistent 800x480 logical canvas regardless of the underlying platform.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LLZ_LOGICAL_WIDTH` | 800 | Logical canvas width in pixels |
| `LLZ_LOGICAL_HEIGHT` | 480 | Logical canvas height in pixels |

### API Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzDisplayInit()` | `bool` | Initialize the display subsystem. Returns `true` on success. Handles DRM rotation setup on CarThing. Sets target FPS to 60. |
| `LlzDisplayBegin()` | `void` | Begin a frame. Call before any drawing operations. Clears to BLACK. |
| `LlzDisplayEnd()` | `void` | End a frame. Handles render texture blitting and 90 degree rotation on DRM. |
| `LlzDisplayShutdown()` | `void` | Clean up display resources. Call when exiting. |

### Platform Behavior

| Platform | Native Resolution | Notes |
|----------|-------------------|-------|
| Desktop | 800x480 | Resizable window, direct drawing |
| DRM (CarThing) | 480x800 | Render texture + 90 degree rotation to match physical display orientation |

### Usage Example

```c
// Initialization (once at startup)
if (!LlzDisplayInit()) {
    // Handle error
    return false;
}

// Main loop
while (!WindowShouldClose()) {
    LlzDisplayBegin();

    // Draw to 800x480 logical canvas
    DrawRectangle(0, 0, LLZ_LOGICAL_WIDTH, LLZ_LOGICAL_HEIGHT, BLACK);
    DrawText("Hello!", 100, 100, 32, WHITE);

    LlzDisplayEnd();
}

// Shutdown
LlzDisplayShutdown();
```

---

## Layout System

The layout module (`llz_sdk_layout.h`) provides utility functions for dividing the canvas into regions without manual rectangle math.

### Types

```c
typedef struct {
    Rectangle rect;
    float padding;
} LlzLayout;
```

### API Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzLayoutSection(bounds, top, height)` | `Rectangle` | Extract a horizontal section starting at `top` offset with given `height` |
| `LlzLayoutInset(bounds, inset)` | `Rectangle` | Shrink a rectangle by `inset` pixels on all sides |
| `LlzLayoutSplitHorizontal(bounds, ratio, left, right)` | `void` | Split horizontally at `ratio` (0.0-1.0), output to `left` and `right` |
| `LlzLayoutSplitVertical(bounds, ratio, top, bottom)` | `void` | Split vertically at `ratio` (0.0-1.0), output to `top` and `bottom` |

### Usage Example

```c
void DrawUI(void) {
    Rectangle screen = {0, 0, LLZ_LOGICAL_WIDTH, LLZ_LOGICAL_HEIGHT};
    Rectangle padded = LlzLayoutInset(&screen, 16);  // 16px margin

    Rectangle left, right;
    LlzLayoutSplitHorizontal(&padded, 0.3f, &left, &right);  // 30%/70% split

    Rectangle top, bottom;
    LlzLayoutSplitVertical(&right, 0.2f, &top, &bottom);  // 20%/80% split

    // Draw to each region
    DrawRectangleRec(left, DARKGRAY);      // Sidebar
    DrawRectangleRec(top, GRAY);           // Header
    DrawRectangleRec(bottom, LIGHTGRAY);   // Content area
}
```

---

## Media System

The media module (`llz_sdk_media.h`) provides Redis-backed access to media metadata, playback state, BLE connection status, playback command dispatch, podcast support, and lyrics. It integrates with the `golang_ble_client` daemon running on CarThing.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LLZ_MEDIA_TEXT_MAX` | 128 | Maximum length for text fields (track, artist, album, device name) |
| `LLZ_MEDIA_PATH_MAX` | 256 | Maximum length for file paths (album art) |
| `LLZ_LYRICS_LINE_MAX` | 256 | Maximum length for a lyrics line |
| `LLZ_LYRICS_MAX_LINES` | 500 | Maximum number of lyrics lines |

### Types

#### LlzMediaState
Current media playback information:

| Field | Type | Description |
|-------|------|-------------|
| `track` | `char[128]` | Current track title |
| `artist` | `char[128]` | Artist name |
| `album` | `char[128]` | Album name |
| `albumArtPath` | `char[256]` | Path to cached album art file |
| `isPlaying` | `bool` | Currently playing or paused |
| `durationSeconds` | `int` | Total track duration |
| `positionSeconds` | `int` | Current playback position |
| `volumePercent` | `int` | Volume level (0-100, or -1 if unavailable) |
| `updatedAt` | `int64_t` | Unix timestamp of last update |

#### LlzConnectionStatus
BLE connection state:

| Field | Type | Description |
|-------|------|-------------|
| `connected` | `bool` | Whether BLE device is connected |
| `deviceName` | `char[128]` | Name of connected device |

#### LlzPodcastState
Podcast playback information:

| Field | Type | Description |
|-------|------|-------------|
| `showName` | `char[128]` | Podcast show name |
| `episodeTitle` | `char[128]` | Current episode title |
| `episodeDescription` | `char[512]` | Episode description |
| `author` | `char[128]` | Podcast author |
| `artPath` | `char[256]` | Path to podcast art |
| `episodeCount` | `int` | Total episode count |
| `currentEpisodeIndex` | `int` | Current episode index |
| `durationSeconds` | `int` | Episode duration |
| `positionSeconds` | `int` | Current position |
| `isPlaying` | `bool` | Currently playing |

#### LlzLyricsLine
A single line of lyrics:

| Field | Type | Description |
|-------|------|-------------|
| `timestampMs` | `int64_t` | Timestamp in milliseconds (0 if unsynced) |
| `text` | `char[256]` | Lyrics text |

#### LlzLyricsData
Complete lyrics data:

| Field | Type | Description |
|-------|------|-------------|
| `hash` | `char[64]` | CRC32 hash of "artist|track" |
| `synced` | `bool` | True if lyrics have timestamps |
| `lineCount` | `int` | Number of lines |
| `lines` | `LlzLyricsLine*` | Dynamically allocated array (caller must free) |

#### LlzPlaybackCommand
Commands to send to the media daemon:

| Command | Value | Description |
|---------|-------|-------------|
| `LLZ_PLAYBACK_PLAY` | 0 | Start playback |
| `LLZ_PLAYBACK_PAUSE` | 1 | Pause playback |
| `LLZ_PLAYBACK_TOGGLE` | 2 | Toggle play/pause (uses cached state) |
| `LLZ_PLAYBACK_NEXT` | 3 | Skip to next track |
| `LLZ_PLAYBACK_PREVIOUS` | 4 | Go to previous track |
| `LLZ_PLAYBACK_SEEK_TO` | 5 | Seek to position (value = seconds) |
| `LLZ_PLAYBACK_SET_VOLUME` | 6 | Set volume (value = 0-100) |

#### LlzMediaKeyMap
Optional custom Redis key mapping for non-standard setups. All fields are `const char*` and can be NULL to use defaults.

### Core Media API Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzMediaInit(config)` | `bool` | Connect to Redis. Pass `NULL` for defaults (127.0.0.1:6379). |
| `LlzMediaShutdown()` | `void` | Disconnect and clean up. |
| `LlzMediaGetState(outState)` | `bool` | Fetch current media state from Redis. |
| `LlzMediaGetConnection(outStatus)` | `bool` | Fetch BLE connection status. |
| `LlzMediaGetProgressPercent(state)` | `float` | Calculate progress as 0.0-1.0 from state. |
| `LlzMediaSendCommand(action, value)` | `bool` | Push a playback command to Redis queue. |
| `LlzMediaSeekSeconds(seconds)` | `bool` | Seek to absolute position (shortcut for seek command). |
| `LlzMediaSetVolume(percent)` | `bool` | Set volume 0-100 (shortcut for volume command). |

### Album Art Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzMediaRequestAlbumArt(hash)` | `bool` | Request album art from Android companion. Hash from `LlzMediaGenerateArtHash`. |
| `LlzMediaGenerateArtHash(artist, album)` | `const char*` | Generate CRC32 hash for album art lookup. Returns static buffer. |

### BLE Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzMediaRequestBLEReconnect()` | `bool` | Signal golang_ble_client to attempt BLE reconnection. |

### Podcast Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzMediaRequestPodcastInfo()` | `bool` | Request podcast metadata from Android companion. |
| `LlzMediaGetPodcastState(outState)` | `bool` | Fetch current podcast state from Redis. |
| `LlzMediaGetPodcastEpisodes(outJson, maxLen)` | `bool` | Get episode list as JSON string. |
| `LlzMediaGetPodcastCount()` | `int` | Get number of podcasts in library. |
| `LlzMediaGetPodcastLibrary(outJson, maxLen)` | `bool` | Get full podcast library as JSON. |
| `LlzMediaPlayEpisode(episodeHash)` | `bool` | Play episode by hash (preferred method). |
| `LlzMediaPlayPodcastEpisode(podcastId, episodeIndex)` | `bool` | Play episode by index (DEPRECATED). |

### Lazy Loading Podcast API

For efficient podcast browsing without loading all data upfront:

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzMediaRequestPodcastList()` | `bool` | Request podcast channel list (names only, no episodes). |
| `LlzMediaRequestRecentEpisodes(limit)` | `bool` | Request recent episodes across all podcasts. Default limit 30. |
| `LlzMediaRequestPodcastEpisodes(podcastId, offset, limit)` | `bool` | Request episodes for a specific podcast with pagination. |
| `LlzMediaGetPodcastList(outJson, maxLen)` | `bool` | Get cached podcast list from Redis. |
| `LlzMediaGetRecentEpisodes(outJson, maxLen)` | `bool` | Get cached recent episodes from Redis. |
| `LlzMediaGetPodcastEpisodesForId(podcastId, outJson, maxLen)` | `bool` | Get cached episodes for specific podcast. |

### Lyrics API Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzLyricsIsEnabled()` | `bool` | Check if lyrics feature is enabled. |
| `LlzLyricsSetEnabled(enabled)` | `bool` | Enable/disable lyrics feature. |
| `LlzLyricsGet(outLyrics)` | `bool` | Get parsed lyrics data. Caller must call `LlzLyricsFree`. |
| `LlzLyricsFree(lyrics)` | `void` | Free lyrics data allocated by `LlzLyricsGet`. |
| `LlzLyricsGetJson(outJson, maxLen)` | `bool` | Get raw lyrics JSON string. |
| `LlzLyricsGetHash(outHash, maxLen)` | `bool` | Get current lyrics hash for change detection. |
| `LlzLyricsAreSynced()` | `bool` | Check if current lyrics have timestamps. |
| `LlzLyricsFindCurrentLine(positionMs, lyrics)` | `int` | Find lyrics line index for playback position. Returns -1 if not found. |
| `LlzLyricsGenerateHash(artist, track)` | `const char*` | Generate hash for lyrics lookup. Returns static buffer. |
| `LlzLyricsRequest(artist, track)` | `bool` | Request lyrics from Android companion via BLE. |
| `LlzLyricsStore(lyricsJson, hash, synced)` | `bool` | Store lyrics to Redis (used when lyrics received). |

### Redis Schema

The media module reads/writes these Redis keys (defaults):

```
# Media state (read-only, set by golang_ble_client)
media:track          -> "Track Title"
media:artist         -> "Artist Name"
media:album          -> "Album Name"
media:playing        -> "true" | "false"
media:duration       -> "180"  (seconds)
media:progress       -> "45"   (seconds)
media:album_art_path -> "/var/mediadash/album_art_cache/abc123.jpg"
media:volume         -> "75"

# Connection state (read-only)
system:ble_connected -> "true" | "false"
system:ble_name      -> "iPhone 15 Pro"

# Command queue (write-only, consumed by golang_ble_client)
system:playback_cmd_q -> LPUSH '{"action":"play","value":0,"timestamp":1234567890}'

# Podcast state
podcast:show_name, podcast:episode_title, podcast:author, podcast:art_path, etc.
podcast:list, podcast:recent_episodes, podcast:episodes:<podcastId>

# Lyrics
lyrics:enabled -> "true" | "false"
lyrics:data -> JSON lyrics data
lyrics:hash -> CRC32 hash string
lyrics:synced -> "true" | "false"
```

### Usage Example

```c
// Initialize with defaults
if (!LlzMediaInit(NULL)) {
    printf("Redis not available\n");
    // Proceed without media features
}

// In update loop
LlzMediaState media;
LlzConnectionStatus conn;

if (LlzMediaGetState(&media)) {
    printf("Now Playing: %s - %s\n", media.artist, media.track);
    printf("Progress: %.1f%%\n", LlzMediaGetProgressPercent(&media) * 100);
}

if (LlzMediaGetConnection(&conn)) {
    printf("BLE: %s (%s)\n", conn.connected ? "Connected" : "Disconnected", conn.deviceName);
}

// Send commands
if (input->playPausePressed) {
    LlzMediaSendCommand(LLZ_PLAYBACK_TOGGLE, 0);
}

if (input->scrollDelta != 0) {
    int newVolume = media.volumePercent + (int)(input->scrollDelta * 5);
    LlzMediaSetVolume(newVolume);  // Clamped to 0-100 internally
}

// Seek on drag
if (input->dragActive) {
    float pct = input->dragCurrent.x / LLZ_LOGICAL_WIDTH;
    int seekPos = (int)(pct * media.durationSeconds);
    LlzMediaSeekSeconds(seekPos);
}

// Lyrics example
if (LlzLyricsIsEnabled()) {
    LlzLyricsData lyrics;
    if (LlzLyricsGet(&lyrics)) {
        int currentLine = LlzLyricsFindCurrentLine(media.positionSeconds * 1000, &lyrics);
        if (currentLine >= 0) {
            printf("Lyrics: %s\n", lyrics.lines[currentLine].text);
        }
        LlzLyricsFree(&lyrics);
    }
}

// Shutdown
LlzMediaShutdown();
```

### Custom Redis Configuration

For non-standard Redis setups:

```c
LlzMediaKeyMap customKeys = {
    .trackTitle = "myapp:song",
    .artistName = "myapp:artist",
    // ... other custom keys, or NULL to use defaults
};

LlzMediaConfig config = {
    .host = "192.168.1.100",
    .port = 6380,
    .keyMap = &customKeys
};

LlzMediaInit(&config);
```

---

## Image Utilities

The image module (`llz_sdk_image.h`) provides blur effects and CSS-like image scaling functions useful for creating polished UIs with album art backgrounds.

### API Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzImageBlur(source, blurRadius, darkenAmount)` | `Image` | Apply box blur and darken an image. Returns new image (caller must call `UnloadImage`). |
| `LlzTextureBlur(source, blurRadius, darkenAmount)` | `Texture2D` | Create a blurred texture from a source texture. Returns new texture (caller must call `UnloadTexture`). |
| `LlzDrawTextureCover(texture, destRect, tint)` | `void` | Draw texture stretched to fill rect (like CSS `background-size: cover`). May crop edges. |
| `LlzDrawTextureContain(texture, destRect, tint)` | `void` | Draw texture scaled to fit within rect (like CSS `background-size: contain`). May letterbox. |

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `blurRadius` | `int` | Blur strength (1-20 recommended, higher = more blur). |
| `darkenAmount` | `float` | Darken factor (0.0-1.0). 0.0 = no darkening, 1.0 = fully black. |

### Usage Example

```c
// Create a blurred background from album art (from texture)
Texture2D albumArt = LoadTexture("album.png");
Texture2D blurredBg = LlzTextureBlur(albumArt, 15, 0.4f);  // radius 15, 40% darker

// Or from image
Image img = LoadImage("album.png");
Image blurredImg = LlzImageBlur(img, 15, 0.4f);
Texture2D blurredBg2 = LoadTextureFromImage(blurredImg);
UnloadImage(blurredImg);

void PluginDraw(void) {
    Rectangle screen = {0, 0, LLZ_LOGICAL_WIDTH, LLZ_LOGICAL_HEIGHT};

    // Draw blurred album art stretched to fill background
    LlzDrawTextureCover(blurredBg, screen, WHITE);

    // Draw sharp album art centered on top
    Rectangle artArea = {200, 40, 400, 400};
    LlzDrawTextureContain(albumArt, artArea, WHITE);
}

// Cleanup
UnloadTexture(blurredBg);
UnloadTexture(albumArt);
```

### Cover vs Contain

| Mode | Behavior | Use Case |
|------|----------|----------|
| **Cover** | Scales to fill entire rect, cropping if needed | Full-bleed backgrounds |
| **Contain** | Scales to fit within rect, letterboxing if needed | Album art display, thumbnails |

---

## Configuration System

The config module (`llz_sdk_config.h`) provides two configuration systems:
1. **Global Config** - System-wide settings (brightness, orientation) shared across all plugins
2. **Plugin Config** - Per-plugin settings with automatic file creation

### Global Configuration

The host application initializes the global config on startup. Plugins can read and modify these settings.

#### Global Settings

| Setting | Type | Range | Description |
|---------|------|-------|-------------|
| `brightness` | `int` | 0-100 or `LLZ_BRIGHTNESS_AUTO` | Screen brightness percentage. Applied to backlight on CarThing. |
| `rotation` | `LlzRotation` | 0, 90, 180, 270 | Screen orientation. |

#### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LLZ_BRIGHTNESS_AUTO` | -1 | Special value for automatic brightness mode |
| `LLZ_CONFIG_PATH_CARTHING` | `/var/llizard/config.ini` | Config path on CarThing |
| `LLZ_CONFIG_PATH_DESKTOP` | `./llizard_config.ini` | Config path on desktop |

#### LlzRotation Values

| Value | Constant | Description |
|-------|----------|-------------|
| 0 | `LLZ_ROTATION_0` | Landscape (default) |
| 90 | `LLZ_ROTATION_90` | Portrait |
| 180 | `LLZ_ROTATION_180` | Landscape (inverted) |
| 270 | `LLZ_ROTATION_270` | Portrait (inverted) |

#### Global Config API

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzConfigInit()` | `bool` | Initialize config system. Loads config from file or creates defaults. Called by host. |
| `LlzConfigShutdown()` | `void` | Save pending changes and shutdown. |
| `LlzConfigGet()` | `const LlzConfig*` | Get pointer to current global config |
| `LlzConfigGetBrightness()` | `int` | Get current brightness (0-100 or `LLZ_BRIGHTNESS_AUTO`) |
| `LlzConfigSetBrightness(value)` | `bool` | Set brightness and apply to hardware. Stops auto service when setting manual. |
| `LlzConfigIsAutoBrightness()` | `bool` | Check if brightness is in automatic mode |
| `LlzConfigSetAutoBrightness()` | `bool` | Enable automatic brightness (starts auto_brightness service on CarThing) |
| `LlzConfigReadAmbientLight()` | `int` | Read ambient light level in lux from sensor. Returns -1 if unavailable. |
| `LlzConfigToggleBrightness()` | `bool` | Toggle screen on/off. Returns true if screen is now on. |
| `LlzConfigGetRotation()` | `LlzRotation` | Get current rotation |
| `LlzConfigSetRotation(value)` | `bool` | Set rotation |
| `LlzConfigGetPath()` | `const char*` | Get path to global config file |
| `LlzConfigGetDirectory()` | `const char*` | Get config directory path |
| `LlzConfigSave()` | `bool` | Force save config to disk |
| `LlzConfigReload()` | `bool` | Reload config from disk |
| `LlzConfigApplyBrightness()` | `void` | Apply current brightness to hardware (CarThing backlight) |

#### Config File Locations

| Platform | Path |
|----------|------|
| CarThing | `/var/llizard/config.ini` |
| Desktop | `./llizard_config.ini` |

#### Global Config Example

```c
// Read settings
const LlzConfig *cfg = LlzConfigGet();
printf("Brightness: %d%%\n", cfg->brightness);

// Modify brightness (auto-saves and applies to hardware)
LlzConfigSetBrightness(75);

// Enable auto brightness
LlzConfigSetAutoBrightness();

// Check if auto mode
if (LlzConfigIsAutoBrightness()) {
    printf("Using ambient light sensor\n");
}

// Read ambient light (CarThing only)
int lux = LlzConfigReadAmbientLight();
if (lux >= 0) {
    printf("Ambient light: %d lux\n", lux);
}

// Toggle screen on/off (for hardware button)
bool screenOn = LlzConfigToggleBrightness();

// Check orientation
if (LlzConfigGetRotation() == LLZ_ROTATION_90) {
    printf("Portrait mode\n");
}
```

### Plugin Configuration

Each plugin can have its own config file with custom key-value settings. If the config file doesn't exist, it's automatically created with provided defaults.

#### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LLZ_PLUGIN_CONFIG_MAX_ENTRIES` | 64 | Maximum entries per plugin config |

#### Plugin Config API

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzPluginConfigInit(config, name, defaults, count)` | `bool` | Initialize plugin config. Creates file with defaults if missing. |
| `LlzPluginConfigFree(config)` | `void` | Save and cleanup plugin config |
| `LlzPluginConfigGetString(config, key)` | `const char*` | Get string value (NULL if missing) |
| `LlzPluginConfigGetInt(config, key, default)` | `int` | Get int value with fallback |
| `LlzPluginConfigGetFloat(config, key, default)` | `float` | Get float value with fallback |
| `LlzPluginConfigGetBool(config, key, default)` | `bool` | Get bool value with fallback |
| `LlzPluginConfigSetString(config, key, value)` | `bool` | Set string value |
| `LlzPluginConfigSetInt(config, key, value)` | `bool` | Set int value |
| `LlzPluginConfigSetFloat(config, key, value)` | `bool` | Set float value |
| `LlzPluginConfigSetBool(config, key, value)` | `bool` | Set bool value |
| `LlzPluginConfigSave(config)` | `bool` | Force save to disk |
| `LlzPluginConfigReload(config)` | `bool` | Reload from disk |
| `LlzPluginConfigHasKey(config, key)` | `bool` | Check if key exists |

#### Plugin Config File Format

Plugin configs are stored as INI files named `<pluginname>_config.ini`:
- CarThing: `/var/llizard/myplugin_config.ini`
- Desktop: `./myplugin_config.ini`

```ini
# myplugin plugin configuration
# Auto-generated - edit with care

theme=dark
volume=80
show_clock=true
animation_speed=1.500000
```

#### Plugin Config Example

```c
static LlzPluginConfig g_config;

void PluginInit(int width, int height) {
    // Define default settings
    LlzPluginConfigEntry defaults[] = {
        {"theme", "dark"},
        {"volume", "80"},
        {"show_clock", "true"},
        {"animation_speed", "1.5"},
    };

    // Initialize - creates file with defaults if it doesn't exist
    LlzPluginConfigInit(&g_config, "myplugin", defaults, 4);

    // Read settings
    const char *theme = LlzPluginConfigGetString(&g_config, "theme");
    int volume = LlzPluginConfigGetInt(&g_config, "volume", 50);
    bool showClock = LlzPluginConfigGetBool(&g_config, "show_clock", false);
    float animSpeed = LlzPluginConfigGetFloat(&g_config, "animation_speed", 1.0f);
}

void PluginUpdate(const LlzInputState *input, float dt) {
    // Modify settings
    if (input->scrollDelta != 0) {
        int vol = LlzPluginConfigGetInt(&g_config, "volume", 50);
        vol += (int)(input->scrollDelta * 5);
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        LlzPluginConfigSetInt(&g_config, "volume", vol);
    }
}

void PluginShutdown(void) {
    // Auto-saves if modified
    LlzPluginConfigFree(&g_config);
}
```

#### Boolean Value Parsing

The config system recognizes these boolean values:
- **True**: `"true"`, `"1"`, `"yes"`
- **False**: `"false"`, `"0"`, `"no"`

---

## Background System

The background module (`llz_sdk_background.h`) provides 9 animated background styles that can be used by plugins to create visually engaging UIs. Backgrounds respond to playback state and can use custom color palettes.

### Background Styles

| Style | Constant | Description |
|-------|----------|-------------|
| Pulse Glow | `LLZ_BG_STYLE_PULSE` | Breathing gradient circles with pulsing effect |
| Aurora Sweep | `LLZ_BG_STYLE_AURORA` | Flowing gradient bands, aurora borealis-like |
| Radial Echo | `LLZ_BG_STYLE_RADIAL` | Expanding concentric rings |
| Neon Strands | `LLZ_BG_STYLE_WAVE` | Flowing sine wave patterns |
| Grid Spark | `LLZ_BG_STYLE_GRID` | Animated grid with drifting glow points |
| Blurred Album | `LLZ_BG_STYLE_BLUR` | Blurred texture with crossfade support |
| Constellation | `LLZ_BG_STYLE_CONSTELLATION` | Connected floating stars/points |
| Liquid Gradient | `LLZ_BG_STYLE_LIQUID` | Morphing color blobs with fluid motion |
| Bokeh Lights | `LLZ_BG_STYLE_BOKEH` | Soft floating circles with depth |

### Types

```c
// Color palette for backgrounds (6 colors derived from primary/accent)
typedef struct {
    Color colors[6];
} LlzBackgroundPalette;
```

### API Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzBackgroundInit(width, height)` | `void` | Initialize background system. Call once at startup. |
| `LlzBackgroundShutdown()` | `void` | Free background resources. |
| `LlzBackgroundUpdate(deltaTime)` | `void` | Update animations. Call once per frame. |
| `LlzBackgroundDraw()` | `void` | Draw current background. Call at start of draw phase. |
| `LlzBackgroundDrawIndicator()` | `void` | Draw style name overlay (optional). |
| `LlzBackgroundCycleNext()` | `void` | Cycle to next style with smooth transition. |
| `LlzBackgroundSetStyle(style, animate)` | `void` | Set style directly. If `animate`, smooth transition. |
| `LlzBackgroundGetStyle()` | `LlzBackgroundStyle` | Get current style. |
| `LlzBackgroundIsEnabled()` | `bool` | Check if background rendering is enabled. |
| `LlzBackgroundSetEnabled(enabled)` | `void` | Enable/disable background rendering. |
| `LlzBackgroundSetColors(primary, accent)` | `void` | Set custom palette colors. Generates 6-color palette. |
| `LlzBackgroundClearColors()` | `void` | Revert to default theme colors. |
| `LlzBackgroundSetBlurTexture(tex, prev, alpha, prevAlpha)` | `void` | Set blurred texture for BLUR style with crossfade. |
| `LlzBackgroundSetEnergy(energy)` | `void` | Set energy level (0.0-1.0) for responsive styles. |
| `LlzBackgroundGetStyleName(style)` | `const char*` | Get human-readable name of style. |
| `LlzBackgroundGetStyleCount()` | `int` | Get total number of styles (9). |
| `LlzBackgroundGetPalette()` | `const LlzBackgroundPalette*` | Get current 6-color palette. |

### Usage Example

```c
void PluginInit(int width, int height) {
    LlzBackgroundInit(width, height);
    LlzBackgroundSetEnabled(true);
    LlzBackgroundSetStyle(LLZ_BG_STYLE_AURORA, false);
}

void PluginUpdate(const LlzInputState *input, float dt) {
    LlzBackgroundUpdate(dt);

    // Cycle style on button press
    if (input->styleCyclePressed) {
        LlzBackgroundCycleNext();
    }

    // Respond to playback state
    LlzBackgroundSetEnergy(isPlaying ? 1.0f : 0.0f);
}

void PluginDraw(void) {
    LlzBackgroundDraw();  // Draw animated background first
    // Draw UI on top...
}

void PluginShutdown(void) {
    LlzBackgroundShutdown();
}
```

### Custom Colors from Album Art

```c
// Extract colors from album art and apply to background
Color primary = {88, 166, 255, 255};   // Dominant color
Color accent = {255, 107, 129, 255};   // Vibrant accent
LlzBackgroundSetColors(primary, accent);

// Or revert to theme defaults
LlzBackgroundClearColors();
```

### Blur Style with Album Art

```c
Texture2D albumArt = LoadTexture("album.png");
Texture2D blurred = LlzTextureBlur(albumArt, 20, 0.3f);

// Set blur texture with crossfade support
LlzBackgroundSetStyle(LLZ_BG_STYLE_BLUR, true);
LlzBackgroundSetBlurTexture(blurred, prevBlurred, 1.0f, 0.0f);
```

---

## Subscription System

The subscription module (`llz_sdk_subscribe.h`) provides event-driven callbacks for media state changes. Instead of polling `LlzMediaGetState()` every frame and comparing values, plugins can subscribe to specific events and receive callbacks only when values change.

### Event Types

| Event | Constant | Callback Signature |
|-------|----------|-------------------|
| Track Changed | `LLZ_EVENT_TRACK_CHANGED` | `void(track, artist, album, userData)` |
| Playstate Changed | `LLZ_EVENT_PLAYSTATE_CHANGED` | `void(isPlaying, userData)` |
| Volume Changed | `LLZ_EVENT_VOLUME_CHANGED` | `void(volumePercent, userData)` |
| Position Changed | `LLZ_EVENT_POSITION_CHANGED` | `void(positionSeconds, durationSeconds, userData)` |
| Connection Changed | `LLZ_EVENT_CONNECTION_CHANGED` | `void(connected, deviceName, userData)` |
| Album Art Changed | `LLZ_EVENT_ALBUM_ART_CHANGED` | `void(artPath, userData)` |
| Notification | `LLZ_EVENT_NOTIFICATION` | `void(level, source, message, userData)` |

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LLZ_MAX_SUBSCRIPTIONS` | 8 | Maximum subscriptions per event type |

### Notification Levels

| Level | Constant | Description |
|-------|----------|-------------|
| Info | `LLZ_NOTIFY_INFO` | Informational message |
| Warning | `LLZ_NOTIFY_WARNING` | Warning message |
| Error | `LLZ_NOTIFY_ERROR` | Error message |
| System | `LLZ_NOTIFY_SYSTEM` | System notification |

### Subscribe Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzSubscribeTrackChanged(callback, userData)` | `LlzSubscriptionId` | Subscribe to track changes |
| `LlzSubscribePlaystateChanged(callback, userData)` | `LlzSubscriptionId` | Subscribe to play/pause changes |
| `LlzSubscribeVolumeChanged(callback, userData)` | `LlzSubscriptionId` | Subscribe to volume changes |
| `LlzSubscribePositionChanged(callback, userData)` | `LlzSubscriptionId` | Subscribe to position updates (~1/sec) |
| `LlzSubscribeConnectionChanged(callback, userData)` | `LlzSubscriptionId` | Subscribe to BLE connection changes |
| `LlzSubscribeAlbumArtChanged(callback, userData)` | `LlzSubscriptionId` | Subscribe to album art path changes |
| `LlzSubscribeNotification(callback, userData)` | `LlzSubscriptionId` | Subscribe to system notifications |

### Management Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzUnsubscribe(id)` | `void` | Unsubscribe by ID. Safe with invalid ID. |
| `LlzUnsubscribeAll(eventType)` | `void` | Unsubscribe all callbacks for event type |
| `LlzSubscriptionPoll()` | `void` | Poll Redis and dispatch callbacks. Call in update loop. |
| `LlzPostNotification(level, source, message)` | `void` | Post notification programmatically |
| `LlzGetSubscriptionCount(eventType)` | `int` | Get active subscription count |
| `LlzHasActiveSubscriptions()` | `bool` | Check if any subscriptions are active |

### Usage Example

```c
static LlzSubscriptionId g_trackSub;
static LlzSubscriptionId g_playSub;

void OnTrackChanged(const char *track, const char *artist, const char *album, void *userData) {
    printf("Now Playing: %s - %s\n", artist, track);
    // Update UI, load album art, etc.
}

void OnPlaystateChanged(bool isPlaying, void *userData) {
    printf("Playback: %s\n", isPlaying ? "Playing" : "Paused");
    LlzBackgroundSetEnergy(isPlaying ? 1.0f : 0.0f);
}

void PluginInit(int width, int height) {
    LlzMediaInit(NULL);  // Must init media first

    // Subscribe to events
    g_trackSub = LlzSubscribeTrackChanged(OnTrackChanged, NULL);
    g_playSub = LlzSubscribePlaystateChanged(OnPlaystateChanged, NULL);
}

void PluginUpdate(const LlzInputState *input, float dt) {
    // Poll for changes and dispatch callbacks
    LlzSubscriptionPoll();
}

void PluginShutdown(void) {
    // Unsubscribe
    LlzUnsubscribe(g_trackSub);
    LlzUnsubscribe(g_playSub);

    LlzMediaShutdown();
}
```

### Posting Notifications

Plugins can post notifications to communicate with other plugins or log events:

```c
// Post informational notification
LlzPostNotification(LLZ_NOTIFY_INFO, "MyPlugin", "Initialization complete");

// Post error notification
LlzPostNotification(LLZ_NOTIFY_ERROR, "MyPlugin", "Failed to load resource");
```

### Subscription vs Polling Comparison

| Approach | Pros | Cons |
|----------|------|------|
| **Subscription** | Callbacks only on change, cleaner code | Must call `LlzSubscriptionPoll()` |
| **Polling** | Direct control, simple for one-off reads | Must track previous state manually |

Use subscriptions when you need to react to changes. Use direct polling (`LlzMediaGetState()`) for one-time reads or when you need the full state object.

---

## Navigation System

The navigation module (`llz_sdk_navigation.h`) enables inter-plugin navigation, allowing one plugin to request that the host open a different plugin.

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LLZ_PLUGIN_NAME_MAX` | 128 | Maximum length for plugin names |

### API Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzRequestOpenPlugin(pluginName)` | `bool` | Request host to open a different plugin after current plugin closes. Returns true on success. |
| `LlzGetRequestedPlugin()` | `const char*` | Get the requested plugin name. Returns NULL if no request pending. |
| `LlzClearRequestedPlugin()` | `void` | Clear any pending plugin request. Host should call after handling. |
| `LlzHasRequestedPlugin()` | `bool` | Check if a plugin request is pending. |

### Usage Example

```c
// In a notification tap handler
void OnNotificationTap(void *userData) {
    // Request to open the Now Playing plugin
    LlzRequestOpenPlugin("Now Playing");

    // Signal that this plugin wants to close (let host handle navigation)
    g_wantsClose = true;
}

// Host-side code (in main loop)
if (plugin->wants_close && plugin->wants_close()) {
    // Check if plugin requested to open another plugin
    if (LlzHasRequestedPlugin()) {
        const char *nextPlugin = LlzGetRequestedPlugin();
        // Find and activate the requested plugin
        ActivatePluginByName(nextPlugin);
        LlzClearRequestedPlugin();
    } else {
        // Return to menu
        currentPlugin = NULL;
    }
}
```

---

## Font System

The font module (`llz_sdk_font.h`) provides centralized font loading with automatic path resolution across CarThing and desktop environments. Fonts are cached internally for efficient reuse.

### Font Types

| Type | Constant | Description |
|------|----------|-------------|
| UI | `LLZ_FONT_UI` | Primary UI font (ZegoeUI on CarThing) - good for menus, labels |
| UI Bold | `LLZ_FONT_UI_BOLD` | Bold variant of UI font |
| Mono | `LLZ_FONT_MONO` | Monospace font for code/technical display (DejaVuSansMono) |

### Font Size Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `LLZ_FONT_SIZE_SMALL` | 16 | Small labels, captions |
| `LLZ_FONT_SIZE_NORMAL` | 20 | Default body text |
| `LLZ_FONT_SIZE_LARGE` | 28 | Emphasized text |
| `LLZ_FONT_SIZE_TITLE` | 36 | Section titles |
| `LLZ_FONT_SIZE_HEADING` | 48 | Large headings |

### API Functions

#### Initialization

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzFontInit()` | `bool` | Initialize font system. Called automatically by host. |
| `LlzFontShutdown()` | `void` | Free all loaded fonts. Called at exit. |
| `LlzFontIsReady()` | `bool` | Check if fonts are available. |

#### Font Loading

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzFontGetDefault()` | `Font` | Get default 20px UI font (cached) |
| `LlzFontGet(type, size)` | `Font` | Get font at specific size (cached) |
| `LlzFontLoadCustom(type, size, codepoints, count)` | `Font` | Load custom font (caller must unload with `UnloadFont`) |
| `LlzFontGetPath(type)` | `const char*` | Get path to font file |
| `LlzFontGetDirectory()` | `const char*` | Get fonts directory path |

#### Text Drawing Helpers

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzDrawText(text, x, y, size, color)` | `void` | Draw text using SDK font |
| `LlzDrawTextCentered(text, centerX, y, size, color)` | `void` | Draw horizontally centered text |
| `LlzDrawTextShadow(text, x, y, size, color, shadow)` | `void` | Draw text with shadow effect |
| `LlzMeasureText(text, size)` | `int` | Measure text width |
| `LlzMeasureTextEx(text, size)` | `Vector2` | Measure text size (width, height) |

### Font Search Paths

The SDK searches for fonts in this order:

| Priority | Path | Description |
|----------|------|-------------|
| 1 | `/var/local/fonts/` | CarThing system fonts |
| 2 | `/tmp/fonts/` | Deployed fonts on CarThing |
| 3 | `./fonts/` | Project fonts (desktop dev) |
| 4 | System fonts | DejaVu fallback |

### Usage Example

```c
void PluginInit(int width, int height) {
    // Font system auto-initializes, but safe to call
    LlzFontInit();
}

void PluginDraw(void) {
    // Simple text drawing
    LlzDrawText("Hello World", 100, 100, 24, WHITE);

    // Centered title
    LlzDrawTextCentered("My Plugin", 400, 50, 36, GOLD);

    // Text with shadow for better readability
    LlzDrawTextShadow("Score: 1000", 100, 200, 28, WHITE, BLACK);

    // Get font for custom drawing
    Font titleFont = LlzFontGet(LLZ_FONT_UI, 48);
    DrawTextEx(titleFont, "Custom", (Vector2){100, 300}, 48, 2, BLUE);

    // Measure text for layout
    int width = LlzMeasureText("Some text", 24);
    DrawRectangle(100, 400, width, 30, DARKGRAY);
    LlzDrawText("Some text", 100, 400, 24, WHITE);
}
```

### Setting Up Fonts

1. **Copy fonts from CarThing** (one-time setup):
   ```bash
   ./scripts/copy-carthing-fonts.sh
   ```

2. **Fonts are deployed automatically** by `build-deploy-carthing.sh`

3. **Desktop development** falls back to system DejaVu fonts if project fonts aren't present

### Font Files

| File | Location | Description |
|------|----------|-------------|
| `ZegoeUI-U.ttf` | `fonts/` | Primary UI font (copy from CarThing) |

See `fonts/README.md` for detailed font setup instructions.

---

## Notification System (Shared Library)

The notification system (`shared/notifications/`) is a separate shared library that provides reusable popup notifications for plugins. It's not part of the core SDK but works alongside it.

### Features

- **Three notification styles**: Banner (top/bottom bar), Toast (corner popup), Dialog (modal with buttons)
- **Queue system**: Up to 16 notifications queue and display sequentially
- **Full callback support**: onTap, onDismiss, onTimeout, onButtonPress
- **Navigation integration**: Built-in plugin switching on tap
- **Smooth animations**: Fade in/out with configurable timing

### Including in Your Plugin

Add to your plugin's CMake configuration:

```cmake
target_include_directories(myplugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/notifications/include
)
target_link_libraries(myplugin llz_sdk llz_notifications)
```

Then include in your source:

```c
#include "llz_sdk.h"
#include "llz_notification.h"
```

### Notification Styles

| Style | Constant | Description |
|-------|----------|-------------|
| Banner | `LLZ_NOTIFY_BANNER` | Full-width bar at top or bottom edge |
| Toast | `LLZ_NOTIFY_TOAST` | Small popup in a corner |
| Dialog | `LLZ_NOTIFY_DIALOG` | Centered modal with buttons and backdrop |

### Positions

| Position | Constant | Use With |
|----------|----------|----------|
| Top | `LLZ_NOTIFY_POS_TOP` | Banner, Toast |
| Bottom | `LLZ_NOTIFY_POS_BOTTOM` | Banner, Toast |
| Top Left | `LLZ_NOTIFY_POS_TOP_LEFT` | Toast |
| Top Right | `LLZ_NOTIFY_POS_TOP_RIGHT` | Toast |
| Bottom Left | `LLZ_NOTIFY_POS_BOTTOM_LEFT` | Toast |
| Bottom Right | `LLZ_NOTIFY_POS_BOTTOM_RIGHT` | Toast |

### API Functions

#### Lifecycle

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzNotifyInit(width, height)` | `void` | Initialize notification system with screen dimensions |
| `LlzNotifyShutdown()` | `void` | Shutdown and cleanup |

#### Showing Notifications

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzNotifyShow(config)` | `int` | Show notification with full config. Returns ID (>0) or 0 on failure. |
| `LlzNotifyBanner(message, duration, position)` | `int` | Show simple banner |
| `LlzNotifyBannerWithIcon(message, icon, duration, position)` | `int` | Banner with Unicode icon |
| `LlzNotifyToast(message, duration, position)` | `int` | Show simple toast |
| `LlzNotifyToastWithIcon(message, icon, duration, position)` | `int` | Toast with Unicode icon |
| `LlzNotifyDialog(title, message, buttons, count, callback, userData)` | `int` | Show dialog with buttons |

#### Update & Draw

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzNotifyUpdate(input, deltaTime)` | `bool` | Update animations and handle input. Returns true if visible. |
| `LlzNotifyDraw()` | `void` | Draw current notification overlay |

#### Control

| Function | Returns | Description |
|----------|---------|-------------|
| `LlzNotifyConfigDefault(style)` | `LlzNotifyConfig` | Get default config for a style |
| `LlzNotifyIsVisible()` | `bool` | Check if any notification is visible |
| `LlzNotifyIsBlocking()` | `bool` | Check if a dialog is blocking input |
| `LlzNotifyDismissCurrent()` | `void` | Dismiss current notification |
| `LlzNotifyClearQueue()` | `void` | Clear all queued notifications |
| `LlzNotifyGetAlpha()` | `float` | Get current notification alpha (0.0-1.0) |

### LlzNotifyConfig Structure

```c
typedef struct {
    LlzNotifyStyle style;           // BANNER, TOAST, or DIALOG
    LlzNotifyPosition position;     // TOP, BOTTOM, corners

    // Content
    char title[256];                // Title (dialogs)
    char message[256];              // Main text
    char iconText[8];               // Unicode icon (e.g., "!", "*")

    // Timing
    float duration;                 // Auto-dismiss time (0 = manual only)
    float fadeInDuration;           // Default 0.25s
    float fadeOutDuration;          // Default 0.2s

    // Appearance
    Color bgColor, textColor, accentColor;
    float cornerRadius;

    // Callbacks
    LlzNotifyCallback onTap;        // Called when tapped
    LlzNotifyDismissCallback onDismiss;  // Called when dismissed (wasTapped flag)
    LlzNotifyCallback onTimeout;    // Called on auto-dismiss
    void *callbackUserData;

    // Dialog buttons (max 3)
    LlzNotifyButton buttons[3];
    int buttonCount;
    LlzNotifyButtonCallback onButtonPress;
    bool dismissOnTapOutside;

    // Navigation
    char openPluginOnTap[128];      // Plugin to open when tapped
} LlzNotifyConfig;
```

### Usage Examples

#### Basic Banner Notification

```c
LlzNotifyBanner("Settings saved!", 3.0f, LLZ_NOTIFY_POS_TOP);
```

#### Now Playing Notification with Navigation

```c
void OnTrackChanged(const char *track, const char *artist, const char *album, void *userData) {
    char message[256];
    snprintf(message, sizeof(message), "%s - %s", artist, track);

    LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_BANNER);
    strncpy(config.message, message, 255);
    strncpy(config.iconText, "*", 8);  // Use ASCII for compatibility
    config.duration = 5.0f;
    strncpy(config.openPluginOnTap, "Now Playing", 128);

    LlzNotifyShow(&config);
}
```

#### Toast with Custom Colors

```c
LlzNotifyConfig config = LlzNotifyConfigDefault(LLZ_NOTIFY_TOAST);
strncpy(config.message, "BLE Connected", 255);
strncpy(config.iconText, "+", 8);
config.accentColor = GREEN;
config.duration = 3.0f;
config.position = LLZ_NOTIFY_POS_BOTTOM_RIGHT;

LlzNotifyShow(&config);
```

#### Confirmation Dialog

```c
void OnExitChoice(int buttonIndex, void *userData) {
    if (buttonIndex == 1) {  // "Exit" button
        *(bool*)userData = true;  // Set wants_close flag
    }
}

void ShowExitConfirmation(bool *wantsClose) {
    const char *buttons[] = {"Cancel", "Exit"};
    LlzNotifyDialog("Exit Plugin?", "Are you sure you want to exit?",
                    buttons, 2, OnExitChoice, wantsClose);
}
```

### Plugin Integration Pattern

```c
#include "llz_sdk.h"
#include "llz_notification.h"

static void PluginInit(int width, int height) {
    LlzNotifyInit(width, height);
    // ... other init
}

static void PluginUpdate(const LlzInputState *input, float dt) {
    // Update notifications (handles input and animations)
    bool blocking = LlzNotifyUpdate(input, dt);

    // Skip other input if a dialog is blocking
    if (blocking && LlzNotifyIsBlocking()) {
        return;
    }

    // ... other update logic
}

static void PluginDraw(void) {
    // ... draw your content

    // Draw notifications on top
    LlzNotifyDraw();
}

static void PluginShutdown(void) {
    LlzNotifyShutdown();
    // ... other cleanup
}
```

### File Structure

```
shared/notifications/
├── include/
│   ├── llz_notification.h         # Main API header
│   └── llz_notification_types.h   # Type definitions
└── llz_notification/
    ├── notification.c             # Core logic + queue
    └── notification_render.c      # Drawing implementation
```

---

## Extending the SDK

**Implemented:**
- Display abstraction with DRM rotation handling (`llz_sdk_display.h`)
- Gesture classification (tap, double-tap, hold, swipe, drag) with button hold detection - built into `LlzInputState`
- Layout helpers for rectangle subdivision (`llz_sdk_layout.h`)
- Media state access via Redis with podcast and lyrics support (`llz_sdk_media.h`)
- Image utilities with blur effects and cover/contain scaling (`llz_sdk_image.h`)
- Global configuration system with brightness/auto-brightness/orientation (`llz_sdk_config.h`)
- Per-plugin configuration with automatic file creation (`llz_sdk_config.h`)
- Animated background system with 9 styles (`llz_sdk_background.h`)
- Event subscription system for media changes (`llz_sdk_subscribe.h`)
- Inter-plugin navigation system (`llz_sdk_navigation.h`)
- Font loading with path resolution and text drawing helpers (`llz_sdk_font.h`)

**Planned additions:**
- **Asset helpers**: optional utilities for loading shared icons and caches.

Contributions should live in `sdk/llz_sdk/` (sources) and `sdk/include/` (headers). Keep the API minimal but cohesive - if a helper requires input and display, think about whether it belongs in a higher-level module.

---

## Quick Reference

### Minimal Plugin Template

```c
#include "llz_sdk.h"
#include "llizard_plugin.h"

static void PluginInit(int width, int height) {
    // Setup
}

static void PluginUpdate(const LlzInputState *input, float dt) {
    if (input->backPressed) {
        // Request return to menu
    }
    if (input->tap) {
        // Handle tap at input->tapPosition
    }
}

static void PluginDraw(void) {
    DrawRectangle(0, 0, LLZ_LOGICAL_WIDTH, LLZ_LOGICAL_HEIGHT, DARKGRAY);
    DrawText("My Plugin", 100, 200, 40, WHITE);
}

static void PluginShutdown(void) {
    // Cleanup
}

static LlzPluginAPI api = {
    .name = "My Plugin",
    .version = "1.0",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = NULL
};

LlzPluginAPI *LlzGetPlugin(void) { return &api; }
```

### All SDK Headers

| Header | Purpose |
|--------|---------|
| `llz_sdk.h` | Master include - includes all SDK headers |
| `llz_sdk_display.h` | Display init, begin/end frame, constants |
| `llz_sdk_input.h` | Input state, gestures, button hold detection |
| `llz_sdk_layout.h` | Layout helpers for UI composition |
| `llz_sdk_media.h` | Redis media state, commands, podcasts, lyrics |
| `llz_sdk_image.h` | Blur effects, cover/contain scaling |
| `llz_sdk_config.h` | Global and plugin configuration |
| `llz_sdk_background.h` | Animated background system |
| `llz_sdk_subscribe.h` | Event subscription callbacks |
| `llz_sdk_navigation.h` | Inter-plugin navigation |
| `llz_sdk_font.h` | Font loading and text helpers |
