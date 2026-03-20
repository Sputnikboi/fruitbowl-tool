#ifndef FB_MODEL_H
#define FB_MODEL_H

#include "types.h"

// Parse a .bbmodel file and convert to FBModel (MC format)
bool fb_parse_bbmodel(const char *path, FBModel *out);

// Load textures from a bbmodel into GPU (for preview)
// Extracts embedded base64 PNGs and creates raylib Textures
bool fb_load_bbmodel_textures(const char *bbmodel_path, FBModel *model);

// Free GPU textures
void fb_unload_model_textures(FBModel *model);

// Load a model from an existing pack model JSON + texture files
bool fb_load_pack_model(const char *pack_root, const char *model_name, FBModel *out);

#endif // FB_MODEL_H
