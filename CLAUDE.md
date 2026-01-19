# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

llizardgui-host is a plugin-based GUI host application for the Spotify CarThing device. It uses raylib/raygui for graphics and provides an SDK that plugins use to draw UIs with a unified input/display abstraction.

**Target Platform:** ARM Linux (Spotify CarThing - Cortex-A7, ARMv7, hard float ABI)
**Graphics:** raylib with DRM backend for CarThing, standard raylib for desktop
**Display:** 800×480 logical resolution (SDK handles DRM rotation)

## Build Commands

### Desktop Build (Development)
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Cross-Compilation for CarThing (DRM)
```bash
mkdir -p build-armv7-drm && cd build-armv7-drm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-armv7.cmake -DPLATFORM=DRM ..
make -j$(nproc)
```

### Build Outputs
- `build/llizardgui-host` - Desktop executable
- `build/nowplaying.so`, `build/redis_status.so` - Plugins copied to `plugins/` automatically
- `build-armv7-drm/llizardgui-host` - ARM binary for CarThing
- `build-armv7-drm/*.so` - ARM plugins

### Deployment
CarThing device: IP `172.16.42.2`, user `root`, password `nocturne`
```bash
scp build-armv7-drm/llizardgui-host root@172.16.42.2:/tmp/
scp build-armv7-drm/nowplaying.so root@172.16.42.2:/tmp/plugins/
```

## Architecture

### Host Application (`src/`)
- **main.c** - Plugin menu and main loop using SDK display/input functions
- **plugin_loader.c** - Dynamic loading of `.so` plugins from `./plugins/` directory via `dlopen`

### Plugin API (`include/llizard_plugin.h`)
Plugins export `LlzGetPlugin()` returning a `LlzPluginAPI` struct with callbacks:
- `init(width, height)` - Called when plugin is selected
- `update(input, deltaTime)` - Per-frame update with input state
- `draw()` - Render plugin content
- `shutdown()` - Cleanup when exiting plugin
- `wants_close()` - Optional signal to return to menu

### SDK (`sdk/`)
Abstracts raylib/DRM platform differences:
- **llz_sdk_display.h** - `LlzDisplayInit/Begin/End/Shutdown` for render target management
- **llz_sdk_input.h** - `LlzInputState` with buttons, touch, gestures, scroll wheel
- **llz_sdk_layout.h** - Helper functions for layout math
- **llz_sdk_media.h** - Redis-backed media state access and playback commands
- **llz_sdk_image.h** - Blur effects and cover/contain image scaling
- **llz_sdk_config.h** - Global and per-plugin configuration
- **llz_sdk_background.h** - 9 animated background styles
- **llz_sdk_subscribe.h** - Event callbacks for media changes
- **llz_sdk_navigation.h** - Plugin-to-plugin navigation requests
- **llz_sdk_font.h** - Centralized font loading with automatic path resolution

Plugins include `llz_sdk.h` and receive `const LlzInputState*` in their update callback.

### Fonts (`fonts/`)
Centralized font storage for consistent text rendering:
- **ZegoeUI-U.ttf** - Primary UI font (CarThing system font)
- Copy fonts from CarThing: `./scripts/copy-carthing-fonts.sh`
- SDK searches: `/var/local/fonts/`, `/tmp/fonts/`, `./fonts/`, system fonts
- See `fonts/README.md` for detailed font API usage

### Shared Libraries (`shared/`)

**Notification System** (`shared/notifications/`)
- Reusable popup notifications that plugins can opt-in to use
- Three styles: Banner (top/bottom bar), Toast (corner popup), Dialog (modal with buttons)
- Queue system for sequential display
- Callbacks for tap, dismiss, timeout, button press
- Built-in plugin navigation on tap
- Usage: `#include "llz_notification.h"` and link with `llz_notifications`

### Available Plugins

**nowplaying** (`plugins_src/nowplaying/`)
- Music player UI with multiple display modes (ADVANCED, NORMAL, BAREBONES, ALBUM_ART)
- Theme system with multiple color variants
- Clock overlay, progress scrubbing, playback controls
- Modular structure: `core/`, `widgets/`, `screens/`, `overlays/`

**redis_status** (`plugins_src/redis_status/`)
- Displays live Redis/MediaDash state from the golang_ble_client daemon
- Two-column layout: Connection status (left), Now Playing media (right)
- Header with Redis/BLE connection indicators
- Requires Redis running on CarThing (`sv start redis`)
- Uses SDK's `LlzMedia*` APIs to fetch state from Redis

### Plugin Structure (nowplaying example)
```
plugins_src/nowplaying/
├── nowplaying_plugin.c     # Plugin entry point with LlzGetPlugin()
├── core/                   # Theme system, effects
├── widgets/                # Reusable UI components
├── screens/                # Screen implementations
└── overlays/               # Overlay system (clock, etc.)
```

## Key Patterns

### Input Handling
Use `LlzInputState` fields, not raw raylib calls, for CarThing hardware compatibility:
- `backPressed`, `selectPressed`, `upPressed`, `downPressed` - Hardware buttons
- `screenshotPressed` - Screenshot button (CT_BUTTON_SCREENSHOT)
- `scrollDelta` - Scroll wheel/rotary encoder
- `tap`, `doubleTap`, `hold`, `swipeLeft/Right/Up/Down` - Touch gestures

### Platform-Specific Code
```c
#ifdef PLATFORM_DRM
    // CarThing-specific behavior
#else
    // Desktop behavior
#endif
```

### Font Loading
Use SDK font loader for consistent text rendering across platforms:
```c
#include "llz_sdk.h"

// Get default UI font
Font font = LlzFontGetDefault();

// Get font at specific size
Font titleFont = LlzFontGet(LLZ_FONT_UI, 36);

// Draw text helpers
LlzDrawText("Hello", 100, 100, 24, WHITE);
LlzDrawTextCentered("Title", 400, 50, 32, GOLD);
LlzDrawTextShadow("Shadow", 100, 200, 28, WHITE, BLACK);

// Measure text
int width = LlzMeasureText("Text", 24);
```

### Adding a New Plugin
1. Create `plugins_src/yourplugin/yourplugin_plugin.c`
2. Implement `LlzGetPlugin()` returning a `LlzPluginAPI*`
3. Add sources and target to `CMakeLists.txt` (see `nowplaying_plugin` example)
4. Plugin `.so` is auto-copied to `plugins/` directory post-build
5. Optional: Add `llz_notifications` to link libraries for popup notifications

## External Dependencies

- **raylib** - Graphics library (in `external/raylib`)
- **raygui** - Immediate-mode GUI (in `external/raygui`)
- **hiredis** - Redis C client (in `external/hiredis`) - used by redis_status plugin

## Internal Libraries

- **llz_sdk** - Core SDK with display, input, media, config, background, subscribe, navigation, font
- **llz_notifications** - Popup notification system (in `shared/notifications/`) - opt-in for plugins

## Scripts

- **scripts/copy-carthing-fonts.sh** - Copy fonts from connected CarThing device to `fonts/`

## CarThing Redis Setup

The redis_status plugin requires Redis running on the CarThing:
```bash
# Check status
sshpass -p nocturne ssh root@172.16.42.2 "sv status redis"

# Start Redis
sshpass -p nocturne ssh root@172.16.42.2 "sv start redis"

# Test connection
sshpass -p nocturne ssh root@172.16.42.2 "redis-cli ping"
```

Redis is populated by the `golang_ble_client` daemon which bridges BLE media data to Redis keys.

## Supporting Projects

### `supporting_projects/llizard_firmware`
Custom firmware build system and flashing tools for CarThing.

**Key Components:**
- `tools/flasher/` - Native raylib GUI flasher for flashing CarThing
- `tools/superbird-tool/` - Python reference flasher (from thinglabsoss)
- `scripts/` - Firmware build pipeline (5-stage Docker build)
- `output/` - Built firmware images (partition files)

**GUI Flasher (`tools/flasher/`)**
Native C/raylib application implementing Amlogic USB boot protocol:
- `src/amlogic_protocol.c` - USB protocol (BL2 boot, AMLC, partition writes)
- `src/flash_protocol.c` - High-level flashing workflow
- `src/main.c` - raylib GUI

**Build & Run:**
```bash
cd supporting_projects/llizard_firmware/tools/flasher
mkdir build && cd build
cmake .. && make -j$(nproc)
sudo ./llizard_flasher
```

**Protocol Details:**
- USB VID:PID `0x1b8e:0xc003` (Boot ROM/Burn mode)
- BL2 loaded to `0xfffa0000`, executed via control transfer `0x05`
- AMLC protocol uploads bootloader in chunks with checksum
- Partition writes use `amlmmc write` commands via USB bulk transfers
- Bootloader writes at 512-byte offset, always timeout (expected)

See `supporting_projects/llizard_firmware/README.md` for full protocol documentation.

### `supporting_projects/carthing_installer`
SSH-based installer for development (doesn't reflash, just copies files over SSH).

**Build & Run:**
```bash
cd supporting_projects/carthing_installer
mkdir build && cd build
cmake .. && make
./carthing_installer
```

### `supporting_projects/golang_ble_client`
- Sourced from `llizardgui-raygui/supporting_projects` and mirrored here for reference/integration work.
- Always-on Go daemon that bridges the Android MediaDash phone and the llizard UI: it speaks BLE on one side and exposes all shared state/commands via Redis on the other.
- **Data Flow**
  - *BLE → Redis*: Metadata, playback progress, connection metrics, and album art notifications are decoded inside `internal/ble`/`internal/albumart` and written into Redis hashes/lists such as `media:track`, `media:progress`, `system:ble_connected`, plus cached blobs under `/var/mediadash/album_art_cache/` with mirror keys (`media:album_art_cache:<hash>`).
  - *Redis → BLE*: UI drops commands (`play`, `pause`, `seek`, `skip`, `volume`) into queues like `system:playback_cmd_q`. `internal/redis` dequeues, validates, and `internal/ble` forwards them to the BLE control characteristic with retry/backoff.
- **Key Packages**
  - `cmd/mediadash-client`: entry point that loads `config.json`, wires goroutines (BLE IO, Redis pump, status writers).
  - `internal/ble`: scanning/pairing, ATT IO, connection metrics, album-art chunking.
  - `internal/redis`: typed helpers (`StoreMediaState`, `StoreConnectionHealth`, `QueuePlaybackCommand`, etc.) that encapsulate the Redis schema.
  - `internal/albumart`: normalizes hashes, persists blobs to `/var/mediadash/album_art_cache`, and tracks cache state in Redis.
- **Documentation & Tooling**: `LLM_OVERVIEW.md`, `COMMAND_PROCESSING.md`, `ALBUM_ART_PROTOCOL_FIXES.md`, and `PERFORMANCE_OPTIMIZATIONS.md` describe the BLE protocol and Redis contract; `build-deploy.sh`, `test_build.sh`, and the Dockerfile provide build/deploy workflows.
- Use this project as the canonical definition for the SDK's upcoming Redis getter/setter APIs (track metadata, progress, connection status, command queues).

### `supporting_projects/llizardOS`
Firmware build system for CarThing. Produces flashable Void Linux images configured to run llizardgui-host natively on DRM with Redis-backed BLE media bridging.

**Key Components:**
- `build.sh` - Main build script running 5-stage pipeline
- `Dockerfile` - Docker build environment with all dependencies
- `scripts/stages/` - Build stages (00: rootfs, 10: system config, 20: llizard stack, 30: cleanup, 40: image)
- `scripts/services/` - runit service definitions (redis, mediadash-client, llizardgui, bluetooth)
- `resources/bins/` - Pre-built ARM binaries (llizardgui-host, mediadash-client, plugins)
- `resources/fonts/` - TTF fonts for llizardgui-host
- `resources/stock-files/` - CarThing stock libraries (libMali)

**Build Prerequisites:**
```bash
cd supporting_projects/llizardOS
# 1. Download stock files
cd resources/stock-files && ./download.sh && cd ../..
# 2. Copy pre-built ARM binaries
cp ../../../build-armv7-drm/llizardgui-host resources/bins/
cp ../../../build-armv7-drm/*.so resources/bins/plugins/
cp ../golang_ble_client/mediadash-client resources/bins/
# 3. Copy fonts
cp ../../../fonts/*.ttf resources/fonts/
```

**Build:**
```bash
just docker-qemu  # Setup ARM emulation (non-ARM hosts)
just docker-run   # Build image
```

**Output:** `output/nocturne_image_vX.X.X.zip` - Flashable via Terbium

**Runtime Stack:**
- `llizardgui` - Native raylib GUI on DRM (no Weston/Chromium)
- `mediadash-client` - BLE media bridge (golang_ble_client)
- `redis` - Data store for media state

See `supporting_projects/llizardOS/CLAUDE.md` for full documentation.

### `supporting_projects/mediadash-android`
Android companion app that provides media metadata over BLE to the CarThing.
