#include "raylib.h"
#include <stdio.h>

int main(void) {
    InitWindow(400, 300, "Drop Test");
    SetTargetFPS(60);
    
    char msg[512] = "Drag a file onto this window...";
    int drop_count = 0;
    
    while (!WindowShouldClose()) {
        if (IsFileDropped()) {
            FilePathList files = LoadDroppedFiles();
            drop_count++;
            snprintf(msg, sizeof(msg), "Drop #%d: %d files", drop_count, files.count);
            for (int i = 0; i < (int)files.count; i++) {
                printf("Dropped: %s\n", files.paths[i]);
            }
            fflush(stdout);
            UnloadDroppedFiles(files);
        }
        
        BeginDrawing();
        ClearBackground(DARKGRAY);
        DrawText(msg, 20, 140, 20, WHITE);
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}
