# llizardgui-host

A plugin-based GUI host application for the Spotify CarThing device. Uses raylib for graphics and provides an SDK that plugins use to draw UIs with a unified input/display abstraction.

## Features

- **Plugin Architecture**: Dynamic loading of `.so` plugins from the `./plugins/` directory
- **Unified SDK**: Consistent 800x480 logical canvas across desktop and CarThing DRM
- **Touch & Button Input**: Full gesture recognition (tap, swipe, drag, hold) plus hardware button support
- **Media Integration**: Redis-backed media state from the MediaDash Go client
- **Configuration System**: Global settings (brightness, orientation) and per-plugin configs
- **Image Utilities**: Blur effects and CSS-like cover/contain scaling

## Target Platform

- **Hardware**: Spotify CarThing (Cortex-A7, ARMv7, hard float ABI)
- **Graphics**: raylib with DRM backend for CarThing, standard raylib for desktop
- **Display**: 800x480 logical resolution (SDK handles DRM rotation)

## Quick Start

### Desktop Build (Development)

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./llizardgui-host
```

### Cross-Compilation for CarThing

```bash
mkdir -p build-armv7-drm && cd build-armv7-drm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-armv7.cmake -DPLATFORM=DRM ..
make -j$(nproc)
```

### Deployment to CarThing

Device: IP `172.16.42.2`, user `root`, password `llizardos`

```bash
scp build-armv7-drm/llizardgui-host root@172.16.42.2:/tmp/
scp build-armv7-drm/*.so root@172.16.42.2:/usr/lib/llizard/plugins/
```

## Project Structure

```
llizardgui-host/
├── src/                    # Host application
│   ├── main.c              # Plugin menu and main loop
│   └── plugin_loader.c     # Dynamic plugin loading
├── sdk/                    # llizardgui SDK (10 modules)
│   ├── include/            # Public headers
│   │   ├── llz_sdk.h           # Master include (includes all)
│   │   ├── llz_sdk_display.h   # Display abstraction (800x480 canvas)
│   │   ├── llz_sdk_input.h     # Input/gesture system
│   │   ├── llz_sdk_layout.h    # Layout helpers
│   │   ├── llz_sdk_media.h     # Redis media state, podcasts, lyrics
│   │   ├── llz_sdk_image.h     # Blur effects, cover/contain scaling
│   │   ├── llz_sdk_config.h    # Global and plugin configuration
│   │   ├── llz_sdk_background.h # 9 animated background styles
│   │   ├── llz_sdk_subscribe.h # Event-driven media callbacks
│   │   ├── llz_sdk_navigation.h # Inter-plugin navigation
│   │   └── llz_sdk_font.h      # Font loading and text helpers
│   └── llz_sdk/            # Implementation
├── shared/                 # Shared libraries
│   └── notifications/      # Popup notification system (opt-in)
├── plugins_src/            # Plugin source code (15 plugins)
│   ├── nowplaying/         # Music player UI with themes
│   ├── lyrics/             # Synced lyrics display
│   ├── podcast/            # Podcast browser
│   ├── album_art_viewer/   # Album art browser
│   ├── media_channels/     # Media channel browser/switcher
│   ├── clock/              # Multi-style clock
│   ├── settings/           # System settings
│   ├── redis_status/       # Redis/BLE status display
│   ├── swipe_2048/         # 2048 puzzle game
│   ├── llzblocks/          # Tetris-style block game
│   ├── llzsolipskier/      # Line-drawing ski game
│   ├── bejeweled/          # Match-3 puzzle game
│   ├── millionaire/        # Trivia game
│   ├── flashcards/         # Quiz/flashcard system
│   └── alchemy/            # Cauldron Cascade game
├── plugins/                # Built plugins (auto-populated at build time)
├── include/                # Shared headers
│   └── llizard_plugin.h    # Plugin API definition
├── supporting_projects/    # Related tools and resources
│   ├── salamander/         # Desktop plugin manager (SSH/SCP deploy)
│   └── salamanders/        # Per-plugin resources (see below)
└── external/               # Dependencies (git submodules)
    ├── raylib/             # Graphics library
    ├── raygui/             # Immediate-mode GUI
    └── hiredis/            # Redis C client
```

### Plugin Resources (salamanders/)

Plugin resources (question banks, scraped data, utility scripts) are organized separately from source code in `supporting_projects/salamanders/`:

```
supporting_projects/salamanders/
├── flashcards/
│   ├── questions/           # Runtime question banks (copied to plugins/ at build)
│   ├── scraped_questions/   # Raw OpenTDB scraped data
│   ├── legacy_questions/    # Old question files
│   └── scrape_opentdb.py    # Question scraper utility
├── millionaire/
│   └── questions/           # Runtime question bank (copied to plugins/ at build)
├── nowplaying/              # Now Playing resources
├── clock/                   # Clock resources
└── ...                      # One folder per plugin
```

**Build Flow:** `CMakeLists.txt` copies `salamanders/{plugin}/questions/` → `plugins/{plugin}/questions/` at build time.

**Deploy Flow:** `build-deploy-carthing.sh` copies `plugins/{plugin}/questions/` → `/tmp/{plugin}/questions/` on CarThing.

## Available Plugins

| Plugin | Description |
|--------|-------------|
| **Now Playing** | Now playing screen with clock overlay and theming |
| **Lyrics** | Display synced lyrics for current track |
| **Podcasts** | Browse podcasts and episodes |
| **Album Art Viewer** | Browse cached album art |
| **Media Channels** | Browse and switch between media channels |
| **Clock** | Modern clock with multiple styles |
| **Settings** | System settings - brightness, lyrics |
| **Redis Status** | Displays Redis/MediaDash state |
| **Swipe 2048** | Touch-friendly 2048 clone with swipe + hardware input |
| **LLZ Blocks** | Block-stacking puzzle with Marathon, Sprint, Ultra & Zen modes |
| **LLZ Solipskier** | Draw snow lines for a skier to ride! |
| **Bejeweled** | Match-3 puzzle game with particle effects and animations |
| **Millionaire** | Who Wants to Be a Millionaire trivia game |
| **Flashcards** | Multiple choice quiz tester |
| **Cauldron Cascade** | Gold becoming aware of itself becoming gold |

## SDK Overview

The SDK provides 10 modules that abstract platform differences and provide common functionality:

| Module | Header | Description |
|--------|--------|-------------|
| Display | `llz_sdk_display.h` | 800x480 logical canvas with DRM rotation handling |
| Input | `llz_sdk_input.h` | Unified buttons, touch, gestures, button hold detection |
| Layout | `llz_sdk_layout.h` | Rectangle subdivision helpers |
| Media | `llz_sdk_media.h` | Redis-backed track metadata, podcasts, lyrics, playback commands |
| Image | `llz_sdk_image.h` | Blur effects, CSS-like cover/contain scaling |
| Config | `llz_sdk_config.h` | Global settings (brightness, auto-brightness) + per-plugin configs |
| Background | `llz_sdk_background.h` | 9 animated background styles (Aurora, Bokeh, etc.) |
| Subscribe | `llz_sdk_subscribe.h` | Event callbacks for media changes (no polling needed) |
| Navigation | `llz_sdk_navigation.h` | Inter-plugin navigation requests |
| Font | `llz_sdk_font.h` | Centralized font loading with path resolution |

### Quick Examples

```c
// Display - all plugins draw to 800x480 canvas
LlzDisplayInit();
LlzDisplayBegin();
DrawText("Hello!", 100, 100, 32, WHITE);
LlzDisplayEnd();

// Input - unified gestures and button handling
void PluginUpdate(const LlzInputState *input, float dt) {
    if (input->tap) HandleTap(input->tapPosition);
    if (input->swipeLeft) NextScreen();
    if (input->backPressed) ExitPlugin();
    if (input->selectHold) ShowContextMenu();  // Long press
    if (input->scrollDelta) AdjustVolume(input->scrollDelta);
}

// Media - Redis integration with playback control
LlzMediaState media;
LlzMediaGetState(&media);
printf("Now Playing: %s - %s\n", media.artist, media.track);
LlzMediaSendCommand(LLZ_PLAYBACK_TOGGLE, 0);

// Subscriptions - event-driven updates
LlzSubscribeTrackChanged(OnTrackChanged, NULL);
LlzSubscribePlaystateChanged(OnPlaystateChanged, NULL);

// Config - global and per-plugin settings
LlzConfigSetBrightness(75);
LlzConfigSetAutoBrightness();  // Use ambient sensor

// Image - blur and scaling
Texture2D blurred = LlzTextureBlur(albumArt, 15, 0.4f);
LlzDrawTextureCover(blurred, screenRect, WHITE);

// Fonts - automatic path resolution
LlzDrawTextCentered("Title", 400, 50, 36, GOLD);
```

See [sdk/README.md](sdk/README.md) for complete API documentation.

## Creating a Plugin

1. Create `plugins_src/yourplugin/yourplugin_plugin.c`:

```c
#include "llz_sdk.h"
#include "llizard_plugin.h"

static void PluginInit(int width, int height) { /* setup */ }
static void PluginUpdate(const LlzInputState *input, float dt) { /* logic */ }
static void PluginDraw(void) { /* render */ }
static void PluginShutdown(void) { /* cleanup */ }

static LlzPluginAPI api = {
    .name = "My Plugin",
    .description = "Does something cool",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = NULL
};

const LlzPluginAPI *LlzGetPlugin(void) { return &api; }
```

2. Add to `CMakeLists.txt`:

```cmake
set(MYPLUGIN_SOURCES plugins_src/myplugin/myplugin_plugin.c)
add_library(myplugin_plugin SHARED ${MYPLUGIN_SOURCES})
target_include_directories(myplugin_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
)
target_link_libraries(myplugin_plugin llz_sdk)
set_target_properties(myplugin_plugin PROPERTIES PREFIX "" OUTPUT_NAME "myplugin")

add_custom_command(TARGET myplugin_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:myplugin_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
)
```

3. Build and the plugin will appear in `plugins/`

4. (Optional) For plugins with runtime resources (question banks, data files):
   - Create `supporting_projects/salamanders/yourplugin/`
   - Add resources there (e.g., `questions/`, `data/`)
   - Update `CMakeLists.txt` to copy resources to `plugins/yourplugin/` at build time

## CarThing Redis Setup

The media system requires Redis running on CarThing, populated by the `golang_ble_client` daemon:

```bash
# Check status
ssh root@172.16.42.2 "sv status redis"

# Start Redis
ssh root@172.16.42.2 "sv start redis"

# Test connection
ssh root@172.16.42.2 "redis-cli ping"
```

## Configuration Files

| File | Location (CarThing) | Location (Desktop) | Purpose |
|------|---------------------|-------------------|---------|
| Global config | `/var/llizard/config.ini` | `./llizard_config.ini` | Brightness, orientation |
| Plugin configs | `/var/llizard/<name>_config.ini` | `./<name>_config.ini` | Per-plugin settings |

## Dependencies

- **raylib** - Graphics library (`external/raylib`)
- **hiredis** - Redis C client (`external/hiredis`)
- **libwebp** - WebP image decoding (system package)

## License

See individual component licenses in `external/` directories.
