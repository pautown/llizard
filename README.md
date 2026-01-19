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

Device: IP `172.16.42.2`, user `root`, password `nocturne`

```bash
scp build-armv7-drm/llizardgui-host root@172.16.42.2:/tmp/
scp build-armv7-drm/*.so root@172.16.42.2:/tmp/plugins/
```

## Project Structure

```
llizardgui-host/
├── src/                    # Host application
│   ├── main.c              # Plugin menu and main loop
│   └── plugin_loader.c     # Dynamic plugin loading
├── sdk/                    # llizardgui SDK
│   ├── include/            # Public headers
│   │   ├── llz_sdk.h           # Main include
│   │   ├── llz_sdk_display.h   # Display abstraction
│   │   ├── llz_sdk_input.h     # Input/gesture system
│   │   ├── llz_sdk_layout.h    # Layout helpers
│   │   ├── llz_sdk_media.h     # Redis media state
│   │   ├── llz_sdk_image.h     # Blur and scaling
│   │   └── llz_sdk_config.h    # Configuration system
│   └── llz_sdk/            # Implementation
├── plugins_src/            # Plugin source code
│   ├── nowplaying/         # Music player UI
│   ├── settings/           # System settings
│   ├── redis_status/       # Redis/BLE status display
│   ├── swipe_2048/         # 2048 game
│   ├── alchemy/            # Alchemy game
│   └── album_art_viewer/   # Album art display
├── include/                # Shared headers
│   └── llizard_plugin.h    # Plugin API definition
├── plugins/                # Built plugins (auto-populated)
└── external/               # Dependencies (raylib, hiredis)
```

## Available Plugins

| Plugin | Description |
|--------|-------------|
| **nowplaying** | Music player UI with multiple display modes, themes, clock overlay |
| **settings** | System settings (brightness, orientation) |
| **redis_status** | Live Redis/MediaDash state display |
| **swipe_2048** | 2048 puzzle game |
| **alchemy** | Cauldron Cascade alchemy game |
| **album_art_viewer** | Full-screen album art display |

## SDK Overview

The SDK provides a consistent interface for plugins regardless of platform:

### Display (800x480 logical canvas)
```c
LlzDisplayInit();
LlzDisplayBegin();
// Draw your UI here
LlzDisplayEnd();
LlzDisplayShutdown();
```

### Input (buttons, touch, gestures)
```c
void PluginUpdate(const LlzInputState *input, float dt) {
    if (input->tap) HandleTap(input->tapPosition);
    if (input->swipeLeft) NextScreen();
    if (input->backPressed) ExitPlugin();
}
```

### Media (Redis integration)
```c
LlzMediaState media;
LlzMediaGetState(&media);
printf("Now Playing: %s - %s\n", media.artist, media.track);
LlzMediaSendCommand(LLZ_PLAYBACK_TOGGLE, 0);
```

### Configuration
```c
// Global config
LlzConfigSetBrightness(75);

// Plugin config (auto-creates file if missing)
LlzPluginConfigEntry defaults[] = {{"theme", "dark"}};
LlzPluginConfigInit(&config, "myplugin", defaults, 1);
```

### Image Utilities
```c
Texture2D blurred = LlzTextureBlur(albumArt, 15, 0.4f);
LlzDrawTextureCover(blurred, screenRect, WHITE);  // Stretch to fill
LlzDrawTextureContain(albumArt, artRect, WHITE);  // Fit within
```

See [sdk/README.md](sdk/README.md) for full SDK documentation.

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
