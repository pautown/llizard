<p align="center">
  <img src="salamander mushrooms.jpg" width="400" alt="Salamander Mushrooms">
</p>

# Salamanders - Per-Plugin Supporting Files

This directory contains supporting files, resources, and documentation organized by plugin. Each subdirectory corresponds to a plugin in `plugins_src/`.

## Structure

```
salamanders/
├── album_art_viewer/   # Album art viewer plugin resources
├── alchemy/            # Alchemy plugin resources
├── clock/              # Clock plugin resources
├── flashcards/         # Flashcards plugin resources
│   ├── questions/          # Runtime question banks (copied to plugins/ at build)
│   ├── scraped_questions/  # Raw scraped data from OpenTDB
│   ├── legacy_questions/   # Legacy question files
│   └── scrape_opentdb.py   # Python scraper utility
├── llzblocks/          # LLZ Blocks game plugin resources
├── llzsolipskier/      # LLZ Solipskier game plugin resources
├── lyrics/             # Lyrics plugin resources
├── millionaire/        # Millionaire game plugin resources
│   └── questions/          # Runtime question bank (copied to plugins/ at build)
├── nowplaying/         # Now Playing plugin resources
├── podcast/            # Podcast plugin resources
├── redis_status/       # Redis Status plugin resources
├── settings/           # Settings plugin resources
└── swipe_2048/         # Swipe 2048 game plugin resources
```

## Build Integration

The `questions/` directories under `flashcards/` and `millionaire/` are automatically copied to the `plugins/` directory during the build process. This is configured in `CMakeLists.txt`.

## Purpose

Use these directories to store:
- Question banks and game data
- Design assets and mockups
- Plugin-specific documentation
- Test data and fixtures
- Configuration templates
- Utility scripts (scrapers, generators, etc.)
- Development notes and TODOs

## Related Projects

- **salamander** (singular) - Desktop plugin manager for installing/uninstalling plugins on CarThing via SSH/SCP
- **plugins_src/** - Actual plugin source code

## Adding a New Plugin

When adding a new plugin to `plugins_src/`, create a corresponding directory here:

```bash
mkdir supporting_projects/salamanders/your_plugin_name
```

If the plugin needs runtime resources (like question banks), update `CMakeLists.txt` to copy them to the `plugins/` directory during build.
