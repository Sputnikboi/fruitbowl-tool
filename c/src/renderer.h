#ifndef FB_RENDERER_H
#define FB_RENDERER_H

#include "types.h"

// Draw a parsed FBModel in 3D space
void fb_draw_model(const FBModel *model);

// Initialize the preview camera
void fb_init_camera(Camera3D *cam);

#endif // FB_RENDERER_H
