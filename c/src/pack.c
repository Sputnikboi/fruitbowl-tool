#include "pack.h"
#include "util.h"

// Stub — full pack management will be ported incrementally
bool fb_add_to_pack(const char *bbmodel_path, const char *pack_root,
                    const char *mc_item_id, const char *model_name,
                    const char *author, FBLog *log) {
    (void)bbmodel_path; (void)pack_root; (void)mc_item_id;
    (void)model_name; (void)author;
    fb_log(log, FB_LOG_WARN, "Pack management not yet ported to C version");
    fb_log(log, FB_LOG_INFO, "Use the Python tool for add/delete/manage operations");
    return false;
}
