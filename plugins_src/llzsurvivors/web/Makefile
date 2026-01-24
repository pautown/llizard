# Makefile for LLZ Survivors Web Build (Emscripten)
#
# Prerequisites:
#   - Emscripten SDK installed and activated (source emsdk_env.sh)
#   - raylib built for web (or use the pre-built version)
#
# Usage:
#   make          # Build the web version
#   make clean    # Remove build artifacts
#   make serve    # Build and start local server

# Emscripten compiler
CC = emcc

# Project name
PROJECT = llzsurvivors

# Directories
RAYLIB_PATH ?= $(HOME)/raylib
WEB_DIR = .
GAME_DIR = ..
BUILD_DIR = build

# Source files
SOURCES = \
    main_web.c \
    $(GAME_DIR)/llzsurvivors_game.c

# Include paths (web stubs take priority)
INCLUDES = \
    -I$(WEB_DIR) \
    -I$(GAME_DIR) \
    -I$(RAYLIB_PATH)/src \
    -I$(RAYLIB_PATH)/src/external

# raylib library for web
RAYLIB_LIB = $(RAYLIB_PATH)/src/libraylib.a

# Compiler flags
CFLAGS = -Wall -Os -DPLATFORM_WEB -DNDEBUG $(INCLUDES)

# Emscripten flags
EMFLAGS = \
    -s USE_GLFW=3 \
    -s TOTAL_MEMORY=67108864 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ASYNCIFY \
    -s FORCE_FILESYSTEM=1 \
    -s EXPORTED_FUNCTIONS="['_main','_WebInit']" \
    -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
    --shell-file shell.html

# Output files
OUTPUT = $(BUILD_DIR)/$(PROJECT).html

# Default target
all: $(BUILD_DIR) $(OUTPUT)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUTPUT): $(SOURCES) $(RAYLIB_LIB) shell.html
	$(CC) $(CFLAGS) $(EMFLAGS) -o $@ $(SOURCES) $(RAYLIB_LIB) -lm

# Build raylib for web (if needed)
raylib:
	cd $(RAYLIB_PATH)/src && make PLATFORM=PLATFORM_WEB -B

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Start local server
serve: all
	@echo "Starting server at http://localhost:8000/$(PROJECT).html"
	@cd $(BUILD_DIR) && python3 -m http.server 8000

# Alternative: use emrun for better integration
emrun: all
	emrun $(OUTPUT)

.PHONY: all clean serve emrun raylib
