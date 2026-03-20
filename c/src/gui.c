#include "gui.h"
#include "model.h"
#include "renderer.h"
#include "util.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

// ── Layout constants ────────────────────────────────────────────────────────
#define SIDEBAR_W   320
#define PAD         8

// ── Orbital camera state ────────────────────────────────────────────────────
static float cam_yaw   = 0.8f;   // radians
static float cam_pitch = 0.5f;   // radians
static float cam_dist  = 40.0f;
static Vector3 cam_target = {8, 8, 8};
static bool show_grid = true;

// Compute center and size of model bounding box
static void compute_model_bounds(const FBModel *model, Vector3 *center, float *radius) {
    if (model->element_count == 0) {
        *center = (Vector3){8, 8, 8};
        *radius = 16.0f;
        return;
    }
    float mins[3] = {999, 999, 999};
    float maxs[3] = {-999, -999, -999};
    for (int i = 0; i < model->element_count; i++) {
        for (int a = 0; a < 3; a++) {
            if (model->elements[i].from[a] < mins[a]) mins[a] = model->elements[i].from[a];
            if (model->elements[i].to[a]   < mins[a]) mins[a] = model->elements[i].to[a];
            if (model->elements[i].from[a] > maxs[a]) maxs[a] = model->elements[i].from[a];
            if (model->elements[i].to[a]   > maxs[a]) maxs[a] = model->elements[i].to[a];
        }
    }
    center->x = (mins[0] + maxs[0]) / 2.0f;
    center->y = (mins[1] + maxs[1]) / 2.0f;
    center->z = (mins[2] + maxs[2]) / 2.0f;

    float dx = maxs[0] - mins[0];
    float dy = maxs[1] - mins[1];
    float dz = maxs[2] - mins[2];
    *radius = sqrtf(dx*dx + dy*dy + dz*dz) / 2.0f;
}

static void update_orbital_camera(Camera3D *cam) {
    // Only handle input when mouse is in viewport
    if (GetMouseX() <= SIDEBAR_W) return;

    // Rotate: left mouse drag
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 delta = GetMouseDelta();
        cam_yaw   -= delta.x * 0.005f;
        cam_pitch -= delta.y * 0.005f;
        // Clamp pitch to avoid gimbal lock
        if (cam_pitch > 1.5f) cam_pitch = 1.5f;
        if (cam_pitch < -1.5f) cam_pitch = -1.5f;
    }

    // Pan: middle mouse drag
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 delta = GetMouseDelta();
        // Compute camera right and up vectors for panning
        float cos_yaw = cosf(cam_yaw), sin_yaw = sinf(cam_yaw);
        Vector3 right = {cos_yaw, 0, sin_yaw};
        Vector3 up = {0, 1, 0};
        float pan_speed = cam_dist * 0.003f;
        cam_target.x -= right.x * delta.x * pan_speed;
        cam_target.z -= right.z * delta.x * pan_speed;
        cam_target.y -= up.y * delta.y * pan_speed;
    }

    // Zoom: scroll wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        cam_dist -= wheel * cam_dist * 0.1f;
        if (cam_dist < 5.0f) cam_dist = 5.0f;
        if (cam_dist > 200.0f) cam_dist = 200.0f;
    }

    // Compute camera position from spherical coordinates
    cam->target = cam_target;
    cam->position.x = cam_target.x + cam_dist * cosf(cam_pitch) * sinf(cam_yaw);
    cam->position.y = cam_target.y + cam_dist * sinf(cam_pitch);
    cam->position.z = cam_target.z + cam_dist * cosf(cam_pitch) * cosf(cam_yaw);
    cam->up = (Vector3){0, 1, 0};
}

static void reset_camera_to_model(Camera3D *cam, const FBModel *model) {
    cam_yaw = 0.8f;
    cam_pitch = 0.5f;
    float radius;
    compute_model_bounds(model, &cam_target, &radius);
    cam_dist = radius * 2.5f;
    if (cam_dist < 20.0f) cam_dist = 20.0f;
    update_orbital_camera(cam);
}

// ── Sidebar ─────────────────────────────────────────────────────────────────

static void draw_sidebar(FBAppState *state) {
    int sw = SIDEBAR_W;
    int sh = GetScreenHeight();
    int y = PAD;

    DrawRectangle(0, 0, sw, sh, (Color){30, 30, 30, 255});

    // Title
    DrawText("Fruitbowl Tool", PAD, y, 20, RAYWHITE);
    y += 30;
    DrawLine(0, y, sw, y, (Color){60, 60, 60, 255});
    y += PAD;

    // Instructions
    DrawText("Drag & drop a .bbmodel file", PAD, y, 12, (Color){150, 150, 150, 255});
    y += 18;
    DrawText("onto this window to preview", PAD, y, 12, (Color){150, 150, 150, 255});
    y += 30;

    // Current model info
    if (state->preview_loaded) {
        DrawLine(0, y, sw, y, (Color){60, 60, 60, 255});
        y += PAD;

        DrawText("Loaded Model:", PAD, y, 14, (Color){100, 200, 100, 255});
        y += 20;

        char info[256];
        snprintf(info, sizeof(info), "Name: %s", state->preview_model.name);
        DrawText(info, PAD + 8, y, 12, LIGHTGRAY);
        y += 16;

        snprintf(info, sizeof(info), "Elements: %d", state->preview_model.element_count);
        DrawText(info, PAD + 8, y, 12, LIGHTGRAY);
        y += 16;

        snprintf(info, sizeof(info), "Texture: %dx%d",
                 state->preview_model.texture_size[0],
                 state->preview_model.texture_size[1]);
        DrawText(info, PAD + 8, y, 12, LIGHTGRAY);
        y += 16;

        snprintf(info, sizeof(info), "Textures: %d", state->preview_model.texture_count);
        DrawText(info, PAD + 8, y, 12, LIGHTGRAY);
        y += 24;

        // Texture preview thumbnails
        for (int i = 0; i < state->preview_model.texture_count && i < 4; i++) {
            Texture2D tex = state->preview_model.textures[i];
            if (tex.id > 0) {
                float max_dim = (float)(tex.width > tex.height ? tex.width : tex.height);
                float scale = 64.0f / max_dim;
                DrawTextureEx(tex, (Vector2){(float)(PAD + 8 + i * 72), (float)y}, 0, scale, WHITE);
            }
        }
        y += 72;
    }

    // Controls help
    y = sh - 120;
    DrawLine(0, y, sw, y, (Color){60, 60, 60, 255});
    y += PAD;
    DrawText("Controls:", PAD, y, 14, (Color){180, 180, 180, 255});
    y += 20;
    DrawText("  Left drag   - Rotate", PAD, y, 11, GRAY);
    y += 14;
    DrawText("  Scroll      - Zoom", PAD, y, 11, GRAY);
    y += 14;
    DrawText("  Middle drag - Pan", PAD, y, 11, GRAY);
    y += 14;
    DrawText("  R           - Reset camera", PAD, y, 11, GRAY);
    y += 14;
    DrawText("  G           - Toggle grid", PAD, y, 11, GRAY);
}

// ── 3D Viewport ─────────────────────────────────────────────────────────────

static void draw_preview(FBAppState *state) {
    int sw = SIDEBAR_W;
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    int vp_x = sw;
    int vp_w = screen_w - sw;
    int vp_h = screen_h;

    BeginScissorMode(vp_x, 0, vp_w, vp_h);

    DrawRectangle(vp_x, 0, vp_w, vp_h, (Color){45, 45, 48, 255});

    BeginMode3D(state->camera);

    if (show_grid) {
        DrawGrid(32, 1.0f);
        // Block boundary wireframe (0,0,0 to 16,16,16)
        DrawCubeWires((Vector3){8, 8, 8}, 16, 16, 16, (Color){80, 80, 80, 128});
    }

    if (state->preview_loaded) {
        fb_draw_model(&state->preview_model);
    }

    EndMode3D();

    if (!state->preview_loaded) {
        const char *msg = "Drop a .bbmodel file here to preview";
        int tw = MeasureText(msg, 20);
        DrawText(msg, vp_x + (vp_w - tw) / 2, vp_h / 2 - 10, 20,
                 (Color){100, 100, 100, 255});
    }

    DrawText(TextFormat("%d FPS", GetFPS()), vp_x + vp_w - 70, 8, 14, GRAY);

    EndScissorMode();
}

// ── Main GUI entry point ────────────────────────────────────────────────────

void fb_draw_gui(FBAppState *state) {
    // Handle drag & drop
    if (IsFileDropped()) {
        FilePathList files = LoadDroppedFiles();
        if (files.count > 0) {
            const char *path = files.paths[0];
            int len = (int)strlen(path);
            if (len > 8 && strcmp(path + len - 8, ".bbmodel") == 0) {
                if (state->preview_loaded) {
                    fb_unload_model_textures(&state->preview_model);
                    state->preview_loaded = false;
                }
                if (fb_parse_bbmodel(path, &state->preview_model)) {
                    fb_load_bbmodel_textures(path, &state->preview_model);
                    state->preview_loaded = true;
                    strncpy(state->bbmodel_path, path, FB_MAX_PATH - 1);
                    reset_camera_to_model(&state->camera, &state->preview_model);
                }
            }
        }
        UnloadDroppedFiles(files);
    }

    // Keyboard shortcuts
    if (IsKeyPressed(KEY_R)) {
        reset_camera_to_model(&state->camera, &state->preview_model);
    }
    if (IsKeyPressed(KEY_G)) {
        show_grid = !show_grid;
    }

    // Auto-center on first load
    static bool needs_center = true;
    if (state->preview_loaded && needs_center) {
        reset_camera_to_model(&state->camera, &state->preview_model);
        needs_center = false;
    }

    // Update camera
    update_orbital_camera(&state->camera);

    // Draw
    draw_sidebar(state);
    draw_preview(state);
}
