#include "renderer.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>

// Minecraft model coordinates: 1 unit = 1/16 of a block.
// MC coordinate system: +X = east, +Y = up, +Z = south
// MC UV: (u1,v1) = top-left, (u2,v2) = bottom-right, in 0-16 range

void fb_draw_model(const FBModel *model) {
    for (int i = 0; i < model->element_count; i++) {
        const FBElement *el = &model->elements[i];

        float x0 = el->from[0], y0 = el->from[1], z0 = el->from[2];
        float x1 = el->to[0],   y1 = el->to[1],   z1 = el->to[2];

        // 8 vertices of the box
        // Indexed so we can build faces easily:
        //   0 = (x0,y0,z0)  1 = (x1,y0,z0)  2 = (x1,y1,z0)  3 = (x0,y1,z0)
        //   4 = (x0,y0,z1)  5 = (x1,y0,z1)  6 = (x1,y1,z1)  7 = (x0,y1,z1)
        Vector3 v[8] = {
            {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
            {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
        };

        // Apply rotation if present
        if (el->rot_axis != 0) {
            float angle_rad = el->rot_angle * DEG2RAD;
            Vector3 origin = {el->rot_origin[0], el->rot_origin[1], el->rot_origin[2]};
            Vector3 axis = {0};
            if (el->rot_axis == 'x') axis = (Vector3){1, 0, 0};
            else if (el->rot_axis == 'y') axis = (Vector3){0, 1, 0};
            else if (el->rot_axis == 'z') axis = (Vector3){0, 0, 1};

            for (int vi = 0; vi < 8; vi++) {
                v[vi] = Vector3Subtract(v[vi], origin);
                v[vi] = Vector3RotateByAxisAngle(v[vi], axis, angle_rad);
                v[vi] = Vector3Add(v[vi], origin);
            }
        }

        // For each face, we need 4 vertices in order such that:
        //   vtx[0] gets UV (u1,v1) = top-left
        //   vtx[1] gets UV (u2,v1) = top-right
        //   vtx[2] gets UV (u2,v2) = bottom-right
        //   vtx[3] gets UV (u1,v2) = bottom-left
        //
        // MC face definitions (looking AT the face from outside):
        //   north (-Z): top-left=(x1,y1,z0) top-right=(x0,y1,z0) bottom-right=(x0,y0,z0) bottom-left=(x1,y0,z0)
        //   south (+Z): top-left=(x0,y1,z1) top-right=(x1,y1,z1) bottom-right=(x1,y0,z1) bottom-left=(x0,y0,z1)
        //   east  (+X): top-left=(x1,y1,z1) top-right=(x1,y1,z0) bottom-right=(x1,y0,z0) bottom-left=(x1,y0,z1)
        //   west  (-X): top-left=(x0,y1,z0) top-right=(x0,y1,z1) bottom-right=(x0,y0,z1) bottom-left=(x0,y0,z0)
        //   up    (+Y): top-left=(x0,y1,z0) top-right=(x1,y1,z0) bottom-right=(x1,y1,z1) bottom-left=(x0,y1,z1)
        //   down  (-Y): top-left=(x0,y0,z1) top-right=(x1,y0,z1) bottom-right=(x1,y0,z0) bottom-left=(x0,y0,z0)

        static const int face_verts[6][4] = {
            // {top-left, top-right, bottom-right, bottom-left}
            {2, 3, 0, 1}, // north: (x1,y1,z0) (x0,y1,z0) (x0,y0,z0) (x1,y0,z0)
            {6, 2, 1, 5}, // east:  (x1,y1,z1) (x1,y1,z0) (x1,y0,z0) (x1,y0,z1)
            {7, 6, 5, 4}, // south: (x0,y1,z1) (x1,y1,z1) (x1,y0,z1) (x0,y0,z1)
            {3, 7, 4, 0}, // west:  (x0,y1,z0) (x0,y1,z1) (x0,y0,z1) (x0,y0,z0)
            {3, 2, 6, 7}, // up:    (x0,y1,z0) (x1,y1,z0) (x1,y1,z1) (x0,y1,z1)
            {4, 5, 1, 0}, // down:  (x0,y0,z1) (x1,y0,z1) (x1,y0,z0) (x0,y0,z0)
        };

        for (int fi = 0; fi < 6; fi++) {
            const FBFace *face = &el->faces[fi];
            if (!face->has_texture) continue;

            int tex_idx = face->texture_idx;
            Texture2D tex = {0};
            if (model->textures_loaded && tex_idx >= 0 && tex_idx < model->texture_count) {
                tex = model->textures[tex_idx];
            }

            // Convert MC UV (0-16 range) to GL (0-1 range)
            float u1 = face->uv[0] / 16.0f;
            float v1 = face->uv[1] / 16.0f;
            float u2 = face->uv[2] / 16.0f;
            float v2 = face->uv[3] / 16.0f;

            if (tex.id > 0) {
                rlSetTexture(tex.id);
            }

            const int *fv = face_verts[fi];
            rlBegin(RL_QUADS);
                rlColor4ub(255, 255, 255, 255);
                // Reverse winding for CCW front faces (GL default)
                // bottom-left
                rlTexCoord2f(u1, v2);
                rlVertex3f(v[fv[3]].x, v[fv[3]].y, v[fv[3]].z);
                // bottom-right
                rlTexCoord2f(u2, v2);
                rlVertex3f(v[fv[2]].x, v[fv[2]].y, v[fv[2]].z);
                // top-right
                rlTexCoord2f(u2, v1);
                rlVertex3f(v[fv[1]].x, v[fv[1]].y, v[fv[1]].z);
                // top-left
                rlTexCoord2f(u1, v1);
                rlVertex3f(v[fv[0]].x, v[fv[0]].y, v[fv[0]].z);
            rlEnd();

            rlSetTexture(0);
        }
    }
}

void fb_init_camera(Camera3D *cam) {
    cam->position = (Vector3){30, 25, 30};
    cam->target   = (Vector3){8, 8, 8};
    cam->up       = (Vector3){0, 1, 0};
    cam->fovy     = 45.0f;
    cam->projection = CAMERA_PERSPECTIVE;
}
