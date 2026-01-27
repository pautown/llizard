cmake_minimum_required(VERSION 3.10)
project(llizardgui-host)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/raylib)

# libwebp for WebP image decoding
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBP REQUIRED libwebp)

# hiredis Redis client
set(HIREDIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/hiredis)
set(HIREDIS_SOURCES
    ${HIREDIS_DIR}/hiredis.c
    ${HIREDIS_DIR}/net.c
    ${HIREDIS_DIR}/read.c
    ${HIREDIS_DIR}/sds.c
    ${HIREDIS_DIR}/alloc.c
    ${HIREDIS_DIR}/sockcompat.c
    ${HIREDIS_DIR}/async.c
    ${HIREDIS_DIR}/dict.c
)
add_library(hiredis_static STATIC ${HIREDIS_SOURCES})
target_include_directories(hiredis_static PUBLIC ${HIREDIS_DIR})
target_compile_definitions(hiredis_static PRIVATE DISABLE_SSL=1)
set_target_properties(hiredis_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(llz_sdk STATIC
    sdk/llz_sdk/display.c
    sdk/llz_sdk/input.c
    sdk/llz_sdk/layout.c
    sdk/llz_sdk/media.c
    sdk/llz_sdk/image.c
    sdk/llz_sdk/config.c
    sdk/llz_sdk/background.c
    sdk/llz_sdk/subscribe.c
    sdk/llz_sdk/navigation.c
    sdk/llz_sdk/font.c
    sdk/llz_sdk/shapes.c
    sdk/llz_sdk/connections.c
    shared/host_input/carthing_input.c
)
set_target_properties(llz_sdk PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(llz_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${WEBP_INCLUDE_DIRS}
)

target_link_libraries(llz_sdk PUBLIC hiredis_static ${WEBP_LIBRARIES})

if(PLATFORM STREQUAL "DRM")
    target_compile_definitions(llz_sdk PUBLIC PLATFORM_DRM GRAPHICS_API_OPENGL_ES2)
endif()

# === Notification System (Shared Library) ===
set(LLZ_NOTIFICATION_SOURCES
    shared/notifications/llz_notification/notification.c
    shared/notifications/llz_notification/notification_render.c
)

add_library(llz_notifications STATIC ${LLZ_NOTIFICATION_SOURCES})
set_target_properties(llz_notifications PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(llz_notifications PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/notifications/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
)

target_link_libraries(llz_notifications PUBLIC llz_sdk)

set(HOST_SOURCES
    src/main.c
    src/plugin_loader.c
    shared/host_input/host_input.c
)

add_executable(llizardgui-host ${HOST_SOURCES})

target_link_libraries(llizardgui-host
    raylib
    llz_sdk
    m
    pthread
    dl
)

target_link_options(llizardgui-host PRIVATE -rdynamic)

target_compile_options(llizardgui-host PRIVATE -Wall -Wextra)

# NowPlaying plugin sources
set(NOWPLAYING_PLUGIN_SOURCES
    # Main plugin entry
    plugins_src/nowplaying/nowplaying_plugin.c
    # Core
    plugins_src/nowplaying/core/np_theme.c
    plugins_src/nowplaying/core/np_effects.c
    # Widgets
    plugins_src/nowplaying/widgets/np_widget_button.c
    plugins_src/nowplaying/widgets/np_widget_progress.c
    plugins_src/nowplaying/widgets/np_widget_panel.c
    plugins_src/nowplaying/widgets/np_widget_label.c
    plugins_src/nowplaying/widgets/np_widget_album_art.c
    # Overlays
    plugins_src/nowplaying/overlays/np_overlay_manager.c
    plugins_src/nowplaying/overlays/np_overlay_template.c
    plugins_src/nowplaying/overlays/np_overlay_clock.c
    plugins_src/nowplaying/overlays/np_overlay_colorpicker.c
    plugins_src/nowplaying/overlays/np_overlay_lyrics.c
    plugins_src/nowplaying/overlays/np_overlay_media_channels.c
    plugins_src/nowplaying/overlays/np_overlay_actions.c
    # Screens
    plugins_src/nowplaying/screens/np_screen_now_playing.c
)

add_library(nowplaying_plugin SHARED ${NOWPLAYING_PLUGIN_SOURCES})
set_target_properties(nowplaying_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(nowplaying_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${WEBP_INCLUDE_DIRS}
)

target_link_libraries(nowplaying_plugin llz_sdk ${WEBP_LIBRARIES})

set_target_properties(nowplaying_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "nowplaying"
)

add_custom_command(TARGET nowplaying_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nowplaying_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying plugin to runtime plugins directory"
)

# Redis Status plugin
set(REDIS_STATUS_PLUGIN_SOURCES
    plugins_src/redis_status/redis_status_plugin.c
)

add_library(redis_status_plugin SHARED ${REDIS_STATUS_PLUGIN_SOURCES})
set_target_properties(redis_status_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(redis_status_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(redis_status_plugin llz_sdk)

set_target_properties(redis_status_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "redis_status"
)

add_custom_command(TARGET redis_status_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:redis_status_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying redis status plugin to runtime plugins directory"
)

# Swipe 2048 plugin
set(SWIPE_2048_PLUGIN_SOURCES
    plugins_src/swipe_2048/swipe_2048_plugin.c
)

add_library(swipe_2048_plugin SHARED ${SWIPE_2048_PLUGIN_SOURCES})
set_target_properties(swipe_2048_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(swipe_2048_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/notifications/include
)

target_link_libraries(swipe_2048_plugin llz_sdk llz_notifications)

set_target_properties(swipe_2048_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "swipe_2048"
)

add_custom_command(TARGET swipe_2048_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:swipe_2048_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying swipe 2048 plugin to runtime plugins directory"
)

# LLZ Blocks (Tetris-style) plugin
set(LLZBLOCKS_PLUGIN_SOURCES
    plugins_src/llzblocks/llzblocks_plugin.c
)

add_library(llzblocks_plugin SHARED ${LLZBLOCKS_PLUGIN_SOURCES})
set_target_properties(llzblocks_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(llzblocks_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(llzblocks_plugin llz_sdk)

set_target_properties(llzblocks_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "llzblocks"
)

add_custom_command(TARGET llzblocks_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:llzblocks_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying llzblocks plugin to runtime plugins directory"
)

# Alchemy (Cauldron Cascade) plugin
set(ALCHEMY_PLUGIN_SOURCES
    plugins_src/alchemy/alchemy_plugin.c
)

add_library(alchemy_plugin SHARED ${ALCHEMY_PLUGIN_SOURCES})
set_target_properties(alchemy_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(alchemy_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(alchemy_plugin llz_sdk)

set_target_properties(alchemy_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "alchemy"
)

add_custom_command(TARGET alchemy_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:alchemy_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying alchemy plugin to runtime plugins directory"
)

# Album Art Viewer plugin
set(ALBUM_ART_VIEWER_PLUGIN_SOURCES
    plugins_src/album_art_viewer/album_art_viewer_plugin.c
)

add_library(album_art_viewer_plugin SHARED ${ALBUM_ART_VIEWER_PLUGIN_SOURCES})
set_target_properties(album_art_viewer_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(album_art_viewer_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${WEBP_INCLUDE_DIRS}
)

target_link_libraries(album_art_viewer_plugin llz_sdk ${WEBP_LIBRARIES})

set_target_properties(album_art_viewer_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "album_art_viewer"
)

add_custom_command(TARGET album_art_viewer_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:album_art_viewer_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying album art viewer plugin to runtime plugins directory"
)

# Settings plugin
set(SETTINGS_PLUGIN_SOURCES
    plugins_src/settings/settings_plugin.c
)

add_library(settings_plugin SHARED ${SETTINGS_PLUGIN_SOURCES})
set_target_properties(settings_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(settings_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(settings_plugin llz_sdk)

set_target_properties(settings_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "settings"
)

add_custom_command(TARGET settings_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:settings_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying settings plugin to runtime plugins directory"
)

# ===== Plugin Manager Plugin =====
set(PLUGIN_MANAGER_SOURCES
    plugins_src/plugin_manager/plugin_manager_plugin.c
)

add_library(plugin_manager_plugin SHARED ${PLUGIN_MANAGER_SOURCES})
set_target_properties(plugin_manager_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(plugin_manager_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(plugin_manager_plugin llz_sdk ${CMAKE_DL_LIBS})

set_target_properties(plugin_manager_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "plugin_manager"
)

add_custom_command(TARGET plugin_manager_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:plugin_manager_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying plugin_manager plugin to runtime plugins directory"
)

# ===== Menu Sorter Plugin =====
set(MENU_SORTER_SOURCES
    plugins_src/menu_sorter/menu_sorter_plugin.c
)

add_library(menu_sorter_plugin SHARED ${MENU_SORTER_SOURCES})
set_target_properties(menu_sorter_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(menu_sorter_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(menu_sorter_plugin llz_sdk ${CMAKE_DL_LIBS})

set_target_properties(menu_sorter_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "menu_sorter"
)

add_custom_command(TARGET menu_sorter_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:menu_sorter_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying menu_sorter plugin to runtime plugins directory"
)

# ===== Media Channels Plugin =====
set(MEDIA_CHANNELS_PLUGIN_SOURCES
    plugins_src/media_channels/media_channels_plugin.c
)

add_library(media_channels_plugin SHARED ${MEDIA_CHANNELS_PLUGIN_SOURCES})
set_target_properties(media_channels_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(media_channels_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(media_channels_plugin llz_sdk)

set_target_properties(media_channels_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "media_channels"
)

add_custom_command(TARGET media_channels_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:media_channels_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying media_channels plugin to runtime plugins directory"
)

# ===== Lyrics Plugin =====
set(LYRICS_PLUGIN_SOURCES
    plugins_src/lyrics/lyrics_plugin.c
)

add_library(lyrics_plugin SHARED ${LYRICS_PLUGIN_SOURCES})
set_target_properties(lyrics_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(lyrics_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${WEBP_INCLUDE_DIRS}
)

target_link_libraries(lyrics_plugin llz_sdk ${WEBP_LIBRARIES})

set_target_properties(lyrics_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "lyrics"
)

add_custom_command(TARGET lyrics_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lyrics_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying lyrics plugin to runtime plugins directory"
)

# ===== Podcast Plugin =====
set(PODCAST_PLUGIN_SOURCES
    plugins_src/podcast/podcast_plugin.c
)

add_library(podcast_plugin SHARED ${PODCAST_PLUGIN_SOURCES})
set_target_properties(podcast_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(podcast_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(podcast_plugin llz_sdk)

set_target_properties(podcast_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "podcast"
)

add_custom_command(TARGET podcast_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:podcast_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying podcast plugin to runtime plugins directory"
)

# ===== Flashcards Plugin =====
set(FLASHCARDS_PLUGIN_SOURCES
    plugins_src/flashcards/flashcards_plugin.c
)

add_library(flashcards_plugin SHARED ${FLASHCARDS_PLUGIN_SOURCES})
set_target_properties(flashcards_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(flashcards_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(flashcards_plugin llz_sdk)

set_target_properties(flashcards_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "flashcards"
)

add_custom_command(TARGET flashcards_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins/flashcards
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins/flashcards/questions
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:flashcards_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/supporting_projects/salamanders/flashcards/questions ${CMAKE_CURRENT_SOURCE_DIR}/plugins/flashcards/questions
    COMMENT "Copying flashcards plugin and questions to runtime plugins directory"
)

# ===== LLZ Solipskier Plugin =====
set(LLZSOLIPSKIER_PLUGIN_SOURCES
    plugins_src/llzsolipskier/llzsolipskier_plugin.c
)

add_library(llzsolipskier_plugin SHARED ${LLZSOLIPSKIER_PLUGIN_SOURCES})
set_target_properties(llzsolipskier_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(llzsolipskier_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(llzsolipskier_plugin llz_sdk)

set_target_properties(llzsolipskier_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "llzsolipskier"
)

add_custom_command(TARGET llzsolipskier_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:llzsolipskier_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying llzsolipskier plugin to runtime plugins directory"
)

# ===== Millionaire Plugin =====
set(MILLIONAIRE_PLUGIN_SOURCES
    plugins_src/millionaire/millionaire_plugin.c
    plugins_src/millionaire/millionaire_questions.c
    plugins_src/millionaire/millionaire_lifelines.c
)

add_library(millionaire_plugin SHARED ${MILLIONAIRE_PLUGIN_SOURCES})
set_target_properties(millionaire_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(millionaire_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src/millionaire
)

target_link_libraries(millionaire_plugin llz_sdk)

set_target_properties(millionaire_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "millionaire"
)

add_custom_command(TARGET millionaire_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins/millionaire
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins/millionaire/questions
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:millionaire_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/supporting_projects/salamanders/millionaire/questions ${CMAKE_CURRENT_SOURCE_DIR}/plugins/millionaire/questions
    COMMENT "Copying millionaire plugin and questions to runtime plugins directory"
)

# ===== Clock Plugin =====
set(CLOCK_PLUGIN_SOURCES
    plugins_src/clock/clock_plugin.c
)

add_library(clock_plugin SHARED ${CLOCK_PLUGIN_SOURCES})
set_target_properties(clock_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(clock_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${WEBP_INCLUDE_DIRS}
)

target_link_libraries(clock_plugin llz_sdk ${WEBP_LIBRARIES})

set_target_properties(clock_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "clock"
)

add_custom_command(TARGET clock_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:clock_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying clock plugin to runtime plugins directory"
)

# ===== Bejeweled (Match-3) Plugin =====
set(BEJEWELED_PLUGIN_SOURCES
    plugins_src/bejeweled/bejeweled_plugin.c
    plugins_src/bejeweled/bejeweled_logic.c
)

add_library(bejeweled_plugin SHARED ${BEJEWELED_PLUGIN_SOURCES})
set_target_properties(bejeweled_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(bejeweled_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src/bejeweled
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/notifications/include
)

target_link_libraries(bejeweled_plugin llz_sdk llz_notifications)

set_target_properties(bejeweled_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "bejeweled"
)

add_custom_command(TARGET bejeweled_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bejeweled_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying bejeweled plugin to runtime plugins directory"
)

# ===== LLZ Survivors Plugin =====
set(LLZSURVIVORS_PLUGIN_SOURCES
    plugins_src/llzsurvivors/llzsurvivors_plugin.c
    plugins_src/llzsurvivors/llzsurvivors_game.c
)

add_library(llzsurvivors_plugin SHARED ${LLZSURVIVORS_PLUGIN_SOURCES})
set_target_properties(llzsurvivors_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(llzsurvivors_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src/llzsurvivors
)

target_link_libraries(llzsurvivors_plugin llz_sdk)

set_target_properties(llzsurvivors_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "llzsurvivors"
)

add_custom_command(TARGET llzsurvivors_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:llzsurvivors_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying llzsurvivors plugin to runtime plugins directory"
)

# ===== Shapes Demo Plugin =====
set(SHAPES_DEMO_PLUGIN_SOURCES
    plugins_src/shapes_demo/shapes_demo_plugin.c
)

add_library(shapes_demo_plugin SHARED ${SHAPES_DEMO_PLUGIN_SOURCES})
set_target_properties(shapes_demo_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(shapes_demo_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(shapes_demo_plugin llz_sdk)

set_target_properties(shapes_demo_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "shapes_demo"
)

add_custom_command(TARGET shapes_demo_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:shapes_demo_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying shapes_demo plugin to runtime plugins directory"
)

# ===== Queue Plugin =====
set(QUEUE_PLUGIN_SOURCES
    plugins_src/queue/queue_plugin.c
)

add_library(queue_plugin SHARED ${QUEUE_PLUGIN_SOURCES})
set_target_properties(queue_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(queue_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(queue_plugin llz_sdk)

set_target_properties(queue_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "queue"
)

add_custom_command(TARGET queue_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:queue_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying queue plugin to runtime plugins directory"
)

# ===== Spotify Plugin =====
set(SPOTIFY_PLUGIN_SOURCES
    plugins_src/spotify/spotify_plugin.c
)

add_library(spotify_plugin SHARED ${SPOTIFY_PLUGIN_SOURCES})
set_target_properties(spotify_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(spotify_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${WEBP_INCLUDE_DIRS}
)

target_link_libraries(spotify_plugin llz_sdk ${WEBP_LIBRARIES})

set_target_properties(spotify_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "spotify"
)

add_custom_command(TARGET spotify_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:spotify_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying spotify plugin to runtime plugins directory"
)

# ===== Albums Plugin =====
set(ALBUMS_PLUGIN_SOURCES
    plugins_src/albums/albums_plugin.c
)

add_library(albums_plugin SHARED ${ALBUMS_PLUGIN_SOURCES})
set_target_properties(albums_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(albums_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${WEBP_INCLUDE_DIRS}
)

target_link_libraries(albums_plugin llz_sdk ${WEBP_LIBRARIES})

set_target_properties(albums_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "albums"
)

add_custom_command(TARGET albums_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:albums_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying albums plugin to runtime plugins directory"
)

# ===== Artists Plugin =====
set(ARTISTS_PLUGIN_SOURCES
    plugins_src/artists/artists_plugin.c
)

add_library(artists_plugin SHARED ${ARTISTS_PLUGIN_SOURCES})
set_target_properties(artists_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(artists_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
    ${WEBP_INCLUDE_DIRS}
)

target_link_libraries(artists_plugin llz_sdk ${WEBP_LIBRARIES})

set_target_properties(artists_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "artists"
)

add_custom_command(TARGET artists_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:artists_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying artists plugin to runtime plugins directory"
)

# ===== Mercury Plugin =====
set(MERCURY_PLUGIN_SOURCES
    plugins_src/mercury/mercury_plugin.c
)

add_library(mercury_plugin SHARED ${MERCURY_PLUGIN_SOURCES})
set_target_properties(mercury_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(mercury_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins_src
)

target_link_libraries(mercury_plugin llz_sdk)

set_target_properties(mercury_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "mercury"
)

add_custom_command(TARGET mercury_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mercury_plugin> ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
    COMMENT "Copying mercury plugin to runtime plugins directory"
)
