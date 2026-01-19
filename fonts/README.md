# Fonts Directory

This directory contains fonts used by llizardgui-host and its plugins.

## Required Fonts

### ZegoeUI-U.ttf (Primary UI Font)
The main UI font used throughout the application. This is the Spotify CarThing's default system font.

**How to obtain:**
1. Connect to your CarThing device via SSH
2. Copy the font: `scp root@172.16.42.2:/var/local/fonts/ZegoeUI-U.ttf ./fonts/`

Or use the provided script:
```bash
./scripts/copy-carthing-fonts.sh
```

### Fallback Fonts
If ZegoeUI is not available, the SDK will automatically fall back to:
- DejaVuSans-Bold.ttf
- DejaVuSans.ttf
- System raylib default font

## Font Search Paths

The SDK searches for fonts in the following locations (in order):

### On CarThing Device:
1. `/var/local/fonts/` - System fonts
2. `/tmp/fonts/` - Deployed fonts

### On Desktop (Development):
1. `./fonts/` - Project fonts directory
2. `../fonts/` - Parent directory
3. `/usr/share/fonts/truetype/dejavu/` - System DejaVu fonts
4. `/usr/share/fonts/TTF/` - System TrueType fonts

## SDK Font API

Plugins should use the SDK font loader instead of loading fonts directly:

```c
#include "llz_sdk.h"

// Get the default UI font
Font font = LlzFontGetDefault();

// Get a specific font at a specific size
Font titleFont = LlzFontGet(LLZ_FONT_UI, 36);

// Draw text using SDK helpers
LlzDrawText("Hello World", 100, 100, 24, WHITE);
LlzDrawTextCentered("Centered Text", 400, 200, 32, GOLD);
LlzDrawTextShadow("Shadow Text", 100, 300, 28, WHITE, BLACK);

// Measure text
int width = LlzMeasureText("Some text", 24);
```

## Font Types

| Type | Constant | Description |
|------|----------|-------------|
| UI | `LLZ_FONT_UI` | Primary UI font (ZegoeUI) |
| UI Bold | `LLZ_FONT_UI_BOLD` | Bold variant |
| Mono | `LLZ_FONT_MONO` | Monospace (DejaVuSansMono) |

## Font Sizes

Predefined size constants:
- `LLZ_FONT_SIZE_SMALL` = 16
- `LLZ_FONT_SIZE_NORMAL` = 20
- `LLZ_FONT_SIZE_LARGE` = 28
- `LLZ_FONT_SIZE_TITLE` = 36
- `LLZ_FONT_SIZE_HEADING` = 48

## Deployment

The `build-deploy-carthing.sh` script automatically copies fonts from this directory to the CarThing device at `/tmp/fonts/`.
