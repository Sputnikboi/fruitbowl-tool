// Test file drop detection in both positions:
// 1) BEFORE BeginDrawing  (like test_drop.c / standard pattern)
// 2) INSIDE BeginDrawing/EndDrawing (like the actual fruitbowl app)
//
// Build from the build/ dir:
//   cc -fsanitize=address -g -o test_drop2 ../test_drop2.c \
//      -I../_deps/raylib-src/src -I../include \
//      -L_deps/raylib-build/raylib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
//
// Run: ./test_drop2

#include "raylib.h"
#include <stdio.h>

int main(void) {
    InitWindow(500, 350, "Drop Test 2 — inside vs outside BeginDrawing");
    SetTargetFPS(60);

    char msg_outside[256] = "No drop (outside)";
    char msg_inside[256]  = "No drop (inside)";
    int drop_outside = 0;
    int drop_inside  = 0;
    int frame = 0;

    while (!WindowShouldClose()) {
        frame++;

        // ── Check OUTSIDE BeginDrawing/EndDrawing ─────────────
        if (IsFileDropped()) {
            FilePathList files = LoadDroppedFiles();
            drop_outside++;
            snprintf(msg_outside, sizeof(msg_outside),
                     "OUTSIDE drop #%d: %d files (frame %d)", drop_outside, files.count, frame);
            printf("[frame %d] OUTSIDE: %d files\n", frame, files.count);
            for (int i = 0; i < (int)files.count; i++)
                printf("  [%d] %s\n", i, files.paths[i]);
            fflush(stdout);
            UnloadDroppedFiles(files);
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        // ── Check INSIDE BeginDrawing/EndDrawing ──────────────
        if (IsFileDropped()) {
            FilePathList files = LoadDroppedFiles();
            drop_inside++;
            snprintf(msg_inside, sizeof(msg_inside),
                     "INSIDE drop #%d: %d files (frame %d)", drop_inside, files.count, frame);
            printf("[frame %d] INSIDE: %d files\n", frame, files.count);
            for (int i = 0; i < (int)files.count; i++)
                printf("  [%d] %s\n", i, files.paths[i]);
            fflush(stdout);
            UnloadDroppedFiles(files);
        }

        DrawText("Drag a file onto this window...", 20, 20, 20, LIGHTGRAY);
        DrawText(msg_outside, 20, 80, 18, GREEN);
        DrawText(msg_inside, 20, 120, 18, YELLOW);
        DrawText(TextFormat("Frame: %d", frame), 20, 300, 16, GRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
