cmake_minimum_required(VERSION 3.20)
project(fruitbowl_tool C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Fetch raylib automatically ───────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ── Sources ──────────────────────────────────────────────────────────────────
add_executable(fruitbowl_tool
    src/main.c
    src/model.c
    src/pack.c
    src/renderer.c
    src/gui.c
    src/util.c
    lib/cJSON.c
)

target_include_directories(fruitbowl_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fruitbowl_tool PRIVATE raylib)

# ── Windows: don't open a console window ─────────────────────────────────────
if(WIN32)
    target_link_options(fruitbowl_tool PRIVATE -mwindows)
endif()

# ── Copy assets to build dir ─────────────────────────────────────────────────
add_custom_command(TARGET fruitbowl_tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:fruitbowl_tool>/assets
)
