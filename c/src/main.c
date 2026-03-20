#include "types.h"
#include "gui.h"
#include "renderer.h"
#include "model.h"
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv) {
    // Init state
    FBAppState state = {0};
    fb_init_camera(&state.camera);

    // Window — must come before any texture loading
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1100, 700, "Fruitbowl Resource Pack Tool");
    SetTargetFPS(60);
    SetTraceLogLevel(LOG_WARNING);  // Reduce raylib spam

    // If a .bbmodel was passed as argument, load it
    if (argc > 1) {
        const char *path = argv[1];
        int len = (int)strlen(path);
        if (len > 8 && strcmp(path + len - 8, ".bbmodel") == 0) {
            if (fb_parse_bbmodel(path, &state.preview_model)) {
                if (fb_load_bbmodel_textures(path, &state.preview_model)) {
                    state.preview_loaded = true;
                    strncpy(state.bbmodel_path, path, FB_MAX_PATH - 1);
                }
            }
        }
    }

    // Main loop
    while (!WindowShouldClose() && !state.should_close) {
        BeginDrawing();
        ClearBackground((Color){35, 35, 38, 255});
        fb_draw_gui(&state);
        EndDrawing();
    }

    // Cleanup
    if (state.preview_loaded) {
        fb_unload_model_textures(&state.preview_model);
    }
    CloseWindow();

    return 0;
}
