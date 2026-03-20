#ifndef FB_PACK_H
#define FB_PACK_H

#include "types.h"

// Placeholder — pack management will be ported later
// For now the C version focuses on preview + single import

bool fb_add_to_pack(const char *bbmodel_path, const char *pack_root,
                    const char *mc_item_id, const char *model_name,
                    const char *author, FBLog *log);

#endif // FB_PACK_H
