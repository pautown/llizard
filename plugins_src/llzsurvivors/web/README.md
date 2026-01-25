# LLZ Survivors - Web Build

Play LLZ Survivors in your browser using WebAssembly!

## Prerequisites

1. **Emscripten SDK** - Install from https://emscripten.org/docs/getting_started/downloads.html
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh  # Add to your shell profile
   ```

2. **raylib built for web** - Build raylib with Emscripten
   ```bash
   git clone https://github.com/raysan5/raylib.git ~/raylib
   cd ~/raylib/src
   make PLATFORM=PLATFORM_WEB -B
   ```

## Building

```bash
# Activate Emscripten environment (if not in profile)
source ~/emsdk/emsdk_env.sh

# Build the web version
cd plugins_src/llzsurvivors/web
make

# The output will be in build/llzsurvivors.html
```

## Running Locally

```bash
# Option 1: Python HTTP server
make serve
# Opens http://localhost:8000/llzsurvivors.html

# Option 2: Emscripten's emrun (better WebGL support)
make emrun
```

## Controls

### Mouse (Recommended for Web)

| Action | Input |
|--------|-------|
| Aim | Move mouse - player rotates to face cursor |
| Toggle Movement | Left-click (tap) |
| Auto-Move | Hold left-click and drag |
| Menu Navigation | Hover over buttons |
| Select | Click |

### Keyboard

| Action | Keys |
|--------|------|
| Toggle Movement | Space / Enter |
| Back/Pause | Escape / Backspace |
| Menu Navigate | Arrow keys / WASD |
| Fine-tune Aim | Scroll wheel |

### Touch

| Action | Gesture |
|--------|---------|
| Select | Tap |
| Back | Swipe down |
| Navigate | Swipe left/right |

## Architecture

The web build uses stub implementations of the llizard SDK:

- `llz_sdk_stub.h` - Provides all `Llz*` functions using pure raylib
- `llz_sdk.h` / `llz_sdk_input.h` - Redirect includes to the stub
- `main_web.c` - Web entry point with Emscripten main loop
- `shell.html` - Custom HTML template with styling

## Customization

### Changing Resolution

Edit `main_web.c`:
```c
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
```

### Adding Assets

If you need to bundle fonts or other assets, add to Makefile:
```makefile
EMFLAGS += --preload-file assets/
```

### Memory Settings

Adjust in Makefile if needed:
```makefile
-s TOTAL_MEMORY=67108864    # 64MB initial
-s ALLOW_MEMORY_GROWTH=1    # Can grow if needed
```

## Deployment

The build outputs three files:
- `llzsurvivors.html` - Main HTML page
- `llzsurvivors.js` - JavaScript glue code
- `llzsurvivors.wasm` - WebAssembly binary

Upload all three to your web server. The HTML can be renamed (e.g., `index.html`).

## Known Limitations

- No save/load (localStorage could be added)
- No audio yet (raylib audio works with Emscripten, just needs setup)
- Touch controls are basic (mouse recommended for best experience)

## Deployment to townhaus

The `build_and_deploy.sh` script automatically copies the build to the townhaus pages directory:

```bash
./build_and_deploy.sh
# Deploys to: supporting_projects/townhaus/pages/games/survivors/
```

Then commit and push to the townhaus repo to publish to GitHub Pages.
