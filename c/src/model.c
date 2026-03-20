#include "model.h"
#include "pack.h"  // for FB_MODEL_DIR, FB_TEXTURE_DIR
#include "util.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ── Base64 decode (for embedded textures) ───────────────────────────────────

static const unsigned char b64_table[256] = {
    ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
    ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
    ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
    ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
    ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
    ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
    ['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
    ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63,
};

static unsigned char *b64_decode(const char *src, int *out_len) {
    int len = (int)strlen(src);
    int pad = 0;
    if (len > 0 && src[len-1] == '=') pad++;
    if (len > 1 && src[len-2] == '=') pad++;

    int out_size = (len * 3) / 4 - pad;
    unsigned char *out = malloc(out_size + 1);
    if (!out) return NULL;

    int j = 0;
    for (int i = 0; i < len; i += 4) {
        unsigned int n = (b64_table[(unsigned char)src[i]] << 18) |
                         (b64_table[(unsigned char)src[i+1]] << 12) |
                         (b64_table[(unsigned char)src[i+2]] << 6) |
                          b64_table[(unsigned char)src[i+3]];
        if (j < out_size) out[j++] = (n >> 16) & 0xFF;
        if (j < out_size) out[j++] = (n >> 8) & 0xFF;
        if (j < out_size) out[j++] = n & 0xFF;
    }
    *out_len = out_size;
    return out;
}

// ── Face name to index mapping ──────────────────────────────────────────────

static int face_index(const char *name) {
    if (strcmp(name, "north") == 0) return 0;
    if (strcmp(name, "east")  == 0) return 1;
    if (strcmp(name, "south") == 0) return 2;
    if (strcmp(name, "west")  == 0) return 3;
    if (strcmp(name, "up")    == 0) return 4;
    if (strcmp(name, "down")  == 0) return 5;
    return -1;
}

// ── Find most-used texture index ────────────────────────────────────────────

static int find_primary_texture(cJSON *elements) {
    int usage[FB_MAX_TEXTURES] = {0};
    cJSON *el;
    cJSON_ArrayForEach(el, elements) {
        cJSON *faces = cJSON_GetObjectItem(el, "faces");
        if (!faces) continue;
        cJSON *face;
        cJSON_ArrayForEach(face, faces) {
            cJSON *tex = cJSON_GetObjectItem(face, "texture");
            if (tex && cJSON_IsNumber(tex)) {
                int idx = tex->valueint;
                if (idx >= 0 && idx < FB_MAX_TEXTURES)
                    usage[idx]++;
            }
        }
    }
    int best = 0;
    for (int i = 1; i < FB_MAX_TEXTURES; i++) {
        if (usage[i] > usage[best]) best = i;
    }
    return best;
}

// ── Parse bbmodel → FBModel ─────────────────────────────────────────────────

bool fb_parse_bbmodel(const char *path, FBModel *out) {
    memset(out, 0, sizeof(FBModel));

    int file_len;
    char *json_str = fb_read_file(path, &file_len);
    if (!json_str) return false;

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return false;

    // Name
    fb_sanitize_name(path, out->name, FB_MAX_NAME);

    // Resolution → texture_size
    cJSON *res = cJSON_GetObjectItem(root, "resolution");
    if (res) {
        cJSON *w = cJSON_GetObjectItem(res, "width");
        cJSON *h = cJSON_GetObjectItem(res, "height");
        out->texture_size[0] = w ? w->valueint : 16;
        out->texture_size[1] = h ? h->valueint : 16;
    } else {
        out->texture_size[0] = 16;
        out->texture_size[1] = 16;
    }

    float uv_scale_x = 16.0f / out->texture_size[0];
    float uv_scale_y = 16.0f / out->texture_size[1];

    // Count textures
    cJSON *textures = cJSON_GetObjectItem(root, "textures");
    out->texture_count = textures ? cJSON_GetArraySize(textures) : 0;

    // Elements
    cJSON *elements = cJSON_GetObjectItem(root, "elements");
    if (!elements) { cJSON_Delete(root); return false; }

    cJSON *el;
    int ei = 0;
    cJSON_ArrayForEach(el, elements) {
        if (ei >= FB_MAX_ELEMENTS) break;
        FBElement *elem = &out->elements[ei];
        elem->shade = true;

        // from / to
        cJSON *from = cJSON_GetObjectItem(el, "from");
        cJSON *to   = cJSON_GetObjectItem(el, "to");
        if (!from || !to) continue;
        for (int i = 0; i < 3; i++) {
            elem->from[i] = (float)cJSON_GetArrayItem(from, i)->valuedouble;
            elem->to[i]   = (float)cJSON_GetArrayItem(to, i)->valuedouble;
        }

        // Inflate
        cJSON *inflate = cJSON_GetObjectItem(el, "inflate");
        if (inflate && cJSON_IsNumber(inflate)) {
            float inf = (float)inflate->valuedouble;
            for (int i = 0; i < 3; i++) {
                elem->from[i] -= inf;
                elem->to[i]   += inf;
            }
        }

        // Rotation
        cJSON *rot = cJSON_GetObjectItem(el, "rotation");
        cJSON *origin = cJSON_GetObjectItem(el, "origin");
        if (rot && origin && cJSON_IsArray(rot)) {
            float rx = (float)cJSON_GetArrayItem(rot, 0)->valuedouble;
            float ry = (float)cJSON_GetArrayItem(rot, 1)->valuedouble;
            float rz = (float)cJSON_GetArrayItem(rot, 2)->valuedouble;
            // Pick first non-zero axis
            if (rx != 0) { elem->rot_angle = rx; elem->rot_axis = 'x'; }
            else if (ry != 0) { elem->rot_angle = ry; elem->rot_axis = 'y'; }
            else if (rz != 0) { elem->rot_angle = rz; elem->rot_axis = 'z'; }
            for (int i = 0; i < 3; i++)
                elem->rot_origin[i] = (float)cJSON_GetArrayItem(origin, i)->valuedouble;
        }

        // Faces
        cJSON *faces = cJSON_GetObjectItem(el, "faces");
        if (faces) {
            cJSON *face;
            cJSON_ArrayForEach(face, faces) {
                int fi = face_index(face->string);
                if (fi < 0) continue;
                FBFace *f = &elem->faces[fi];

                cJSON *uv = cJSON_GetObjectItem(face, "uv");
                if (uv && cJSON_GetArraySize(uv) == 4) {
                    f->uv[0] = (float)cJSON_GetArrayItem(uv, 0)->valuedouble * uv_scale_x;
                    f->uv[1] = (float)cJSON_GetArrayItem(uv, 1)->valuedouble * uv_scale_y;
                    f->uv[2] = (float)cJSON_GetArrayItem(uv, 2)->valuedouble * uv_scale_x;
                    f->uv[3] = (float)cJSON_GetArrayItem(uv, 3)->valuedouble * uv_scale_y;
                }

                cJSON *tex = cJSON_GetObjectItem(face, "texture");
                if (tex && cJSON_IsNumber(tex)) {
                    f->texture_idx = tex->valueint;
                    f->has_texture = true;
                }

                cJSON *frot = cJSON_GetObjectItem(face, "rotation");
                if (frot && cJSON_IsNumber(frot))
                    f->rotation = frot->valueint;
            }
        }

        // Shade
        cJSON *shade = cJSON_GetObjectItem(el, "shade");
        if (shade && cJSON_IsFalse(shade))
            elem->shade = false;

        ei++;
    }
    out->element_count = ei;

    // Bounds shift (-16 to 32)
    float mins[3] = {999, 999, 999};
    float maxs[3] = {-999, -999, -999};
    for (int i = 0; i < out->element_count; i++) {
        for (int a = 0; a < 3; a++) {
            if (out->elements[i].from[a] < mins[a]) mins[a] = out->elements[i].from[a];
            if (out->elements[i].to[a]   < mins[a]) mins[a] = out->elements[i].to[a];
            if (out->elements[i].from[a] > maxs[a]) maxs[a] = out->elements[i].from[a];
            if (out->elements[i].to[a]   > maxs[a]) maxs[a] = out->elements[i].to[a];
        }
    }
    float shift[3] = {0};
    for (int a = 0; a < 3; a++) {
        if (maxs[a] > 32.0f)  shift[a] = 32.0f - maxs[a];
        if (mins[a] + shift[a] < -16.0f) shift[a] = -16.0f - mins[a];
    }
    if (shift[0] != 0 || shift[1] != 0 || shift[2] != 0) {
        for (int i = 0; i < out->element_count; i++) {
            for (int a = 0; a < 3; a++) {
                out->elements[i].from[a] += shift[a];
                out->elements[i].to[a]   += shift[a];
                out->elements[i].rot_origin[a] += shift[a];
            }
        }
    }

    // Display
    cJSON *display = cJSON_GetObjectItem(root, "display");
    if (display) {
        out->has_display = true;
        // We store display for export but for preview we just use the raw geometry
    }

    cJSON_Delete(root);
    return true;
}

// ── Load textures from bbmodel for GPU preview ──────────────────────────────

bool fb_load_bbmodel_textures(const char *bbmodel_path, FBModel *model) {
    int file_len;
    char *json_str = fb_read_file(bbmodel_path, &file_len);
    if (!json_str) return false;

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return false;

    cJSON *textures = cJSON_GetObjectItem(root, "textures");
    if (!textures) { cJSON_Delete(root); return false; }

    int count = cJSON_GetArraySize(textures);
    if (count > FB_MAX_TEXTURES) count = FB_MAX_TEXTURES;

    for (int i = 0; i < count; i++) {
        cJSON *tex = cJSON_GetArrayItem(textures, i);
        cJSON *source = cJSON_GetObjectItem(tex, "source");
        if (!source || !cJSON_IsString(source)) continue;

        const char *src = source->valuestring;
        const char *prefix = "data:image/png;base64,";
        if (strncmp(src, prefix, strlen(prefix)) != 0) continue;

        const char *b64_data = src + strlen(prefix);
        int png_len;
        unsigned char *png_bytes = b64_decode(b64_data, &png_len);
        if (!png_bytes) continue;

        // Load PNG from memory into raylib texture
        Image img = LoadImageFromMemory(".png", png_bytes, png_len);
        free(png_bytes);

        if (img.data) {
            model->textures[i] = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    }

    model->texture_count = count;
    model->textures_loaded = true;

    cJSON_Delete(root);
    return true;
}

void fb_unload_model_textures(FBModel *model) {
    if (!model->textures_loaded) return;
    for (int i = 0; i < model->texture_count; i++) {
        if (model->textures[i].id > 0)
            UnloadTexture(model->textures[i]);
    }
    model->textures_loaded = false;
}

// ── Load model from pack (MC JSON format + PNG textures on disk) ────────────

bool fb_load_pack_model(const char *pack_root, const char *model_name, FBModel *out) {
    memset(out, 0, sizeof(FBModel));
    strncpy(out->name, model_name, FB_MAX_NAME - 1);

    // Load model JSON
    char model_path[FB_MAX_PATH];
    snprintf(model_path, sizeof(model_path), "%s/%s/%s.json",
             pack_root, FB_MODEL_DIR, model_name);

    int file_len;
    char *json_str = fb_read_file(model_path, &file_len);
    if (!json_str) return false;

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return false;

    // texture_size
    cJSON *ts = cJSON_GetObjectItem(root, "texture_size");
    if (ts && cJSON_GetArraySize(ts) == 2) {
        out->texture_size[0] = cJSON_GetArrayItem(ts, 0)->valueint;
        out->texture_size[1] = cJSON_GetArrayItem(ts, 1)->valueint;
    } else {
        out->texture_size[0] = 16;
        out->texture_size[1] = 16;
    }

    // Parse textures map: {"0": "fruitbowl:item/name", "1": "...", "particle": "..."}
    // We need to: (a) count real texture indices, (b) resolve paths to load PNGs
    cJSON *textures = cJSON_GetObjectItem(root, "textures");
    // Texture paths indexed by number (max FB_MAX_TEXTURES)
    char tex_paths[FB_MAX_TEXTURES][FB_MAX_PATH];
    memset(tex_paths, 0, sizeof(tex_paths));
    int max_tex_idx = -1;

    if (textures) {
        cJSON *tex_entry;
        cJSON_ArrayForEach(tex_entry, textures) {
            const char *key = tex_entry->string;
            if (!key || !cJSON_IsString(tex_entry)) continue;
            // Skip "particle" key — it's not a numbered texture
            // But do process numeric keys: "0", "1", etc.
            char *endp;
            long idx = strtol(key, &endp, 10);
            if (*endp != '\0' || idx < 0 || idx >= FB_MAX_TEXTURES) continue;

            // Value is like "fruitbowl:item/ham" → texture file at textures/item/ham.png
            const char *val = tex_entry->valuestring;
            const char *colon = strchr(val, ':');
            const char *rel_path = colon ? colon + 1 : val;
            // The namespace before colon tells us assets/<namespace>/textures/<rel_path>.png
            char ns[64] = "minecraft";
            if (colon) {
                int ns_len = (int)(colon - val);
                if (ns_len > 0 && ns_len < (int)sizeof(ns)) {
                    strncpy(ns, val, ns_len);
                    ns[ns_len] = '\0';
                }
            }
            snprintf(tex_paths[idx], FB_MAX_PATH, "%s/assets/%s/textures/%s.png",
                     pack_root, ns, rel_path);

            if ((int)idx > max_tex_idx) max_tex_idx = (int)idx;
        }
    }
    out->texture_count = max_tex_idx + 1;

    // Load texture PNGs from disk
    for (int i = 0; i < out->texture_count; i++) {
        if (tex_paths[i][0] && fb_file_exists(tex_paths[i])) {
            Image img = LoadImage(tex_paths[i]);
            if (img.data) {
                out->textures[i] = LoadTextureFromImage(img);
                UnloadImage(img);
            }
        }
    }
    out->textures_loaded = true;

    // Parse elements — MC JSON format (UVs already in 0-16 range)
    cJSON *elements = cJSON_GetObjectItem(root, "elements");
    if (!elements) { cJSON_Delete(root); return true; } // valid model with no elements

    cJSON *el;
    int ei = 0;
    cJSON_ArrayForEach(el, elements) {
        if (ei >= FB_MAX_ELEMENTS) break;
        FBElement *elem = &out->elements[ei];
        elem->shade = true;

        // from / to
        cJSON *from = cJSON_GetObjectItem(el, "from");
        cJSON *to   = cJSON_GetObjectItem(el, "to");
        if (!from || !to) continue;
        for (int i = 0; i < 3; i++) {
            elem->from[i] = (float)cJSON_GetArrayItem(from, i)->valuedouble;
            elem->to[i]   = (float)cJSON_GetArrayItem(to, i)->valuedouble;
        }

        // Rotation — MC format: {"angle": 22.5, "axis": "x", "origin": [8,8,8]}
        cJSON *rot = cJSON_GetObjectItem(el, "rotation");
        if (rot && cJSON_IsObject(rot)) {
            cJSON *angle  = cJSON_GetObjectItem(rot, "angle");
            cJSON *axis   = cJSON_GetObjectItem(rot, "axis");
            cJSON *origin = cJSON_GetObjectItem(rot, "origin");
            if (angle && axis && origin) {
                elem->rot_angle = (float)angle->valuedouble;
                const char *axis_str = axis->valuestring;
                if (axis_str) elem->rot_axis = axis_str[0];
                for (int i = 0; i < 3; i++)
                    elem->rot_origin[i] = (float)cJSON_GetArrayItem(origin, i)->valuedouble;
            }
        }

        // Faces — MC format: texture is "#0" string, UVs already 0-16
        cJSON *faces = cJSON_GetObjectItem(el, "faces");
        if (faces) {
            cJSON *face;
            cJSON_ArrayForEach(face, faces) {
                int fi = face_index(face->string);
                if (fi < 0) continue;
                FBFace *f = &elem->faces[fi];

                cJSON *uv = cJSON_GetObjectItem(face, "uv");
                if (uv && cJSON_GetArraySize(uv) == 4) {
                    // UVs are already in 0-16 range in pack model JSON
                    f->uv[0] = (float)cJSON_GetArrayItem(uv, 0)->valuedouble;
                    f->uv[1] = (float)cJSON_GetArrayItem(uv, 1)->valuedouble;
                    f->uv[2] = (float)cJSON_GetArrayItem(uv, 2)->valuedouble;
                    f->uv[3] = (float)cJSON_GetArrayItem(uv, 3)->valuedouble;
                }

                cJSON *tex = cJSON_GetObjectItem(face, "texture");
                if (tex && cJSON_IsString(tex)) {
                    // Format: "#0", "#1" etc — extract numeric index
                    const char *ts = tex->valuestring;
                    if (ts && ts[0] == '#') {
                        f->texture_idx = atoi(ts + 1);
                        f->has_texture = true;
                    }
                }

                cJSON *frot = cJSON_GetObjectItem(face, "rotation");
                if (frot && cJSON_IsNumber(frot))
                    f->rotation = frot->valueint;
            }
        }

        // Shade
        cJSON *shade = cJSON_GetObjectItem(el, "shade");
        if (shade && cJSON_IsFalse(shade))
            elem->shade = false;

        ei++;
    }
    out->element_count = ei;

    cJSON_Delete(root);
    return true;
}
