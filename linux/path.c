#include "align.h"
#include "cwalk.h"
#include "linux.h"
#include "proc.h"

#include <assert.h>
#include <string.h>

// Resolves a sandbox path to a host path. Returns true if successful. The
// resolution uses the dir_maps variable from the engine to determine where
// directories in the sandbox are mapped in the host.
bool
path_resolve(struct LFILinuxProc *proc, const char *path, char *buffer,
    size_t buffer_size)
{
    // If the path is relative, append the process's working directory to it.
    if (!cwk_path_is_absolute(path)) {
        assert(!"unimplemented: path_resolve for relative path");
        /* char abs_path[FILENAME_MAX]; */
        /* cwk_path_join(proc->wd, path, abs_path, sizeof(abs_path)); */
        /* path = abs_path; */
    }

    // Normalize the path.
    char normalized[FILENAME_MAX];
    cwk_path_normalize(path, normalized, sizeof(normalized));
    path = normalized;

    const char **dir_maps = proc->engine->opts.dir_maps;
    if (!dir_maps) {
        // No directory mappings means no paths are available.
        return false;
    }

    // Look for intersections between the requested path and the directory maps.
    char box_dir[FILENAME_MAX];
    char *host_dir = NULL;
    size_t common_match = 0;
    while (*dir_maps != NULL) {
        char *eq = strchr(*dir_maps, '=');
        if (!eq) {
            LOG(proc->engine, "not a valid directory mapping: %s", *dir_maps);
            dir_maps++;
            continue;
        }
        strncpy(box_dir, *dir_maps, sizeof(box_dir) - 1);
        box_dir[MIN(eq - *dir_maps, sizeof(box_dir) - 1)] = 0;

        size_t common = cwk_path_get_intersection(path, box_dir);
        if (common > common_match) {
            host_dir = eq + 1;
            common_match = common;
        }
        dir_maps++;
    }

    if (!host_dir)
        return NULL;

    if (common_match >= buffer_size) {
        LOG(proc->engine, "host prefix exceeds buffer size");
        return NULL;
    }

    cwk_path_join(host_dir, path + common_match, buffer, buffer_size);

    LOG(proc->engine, "path_resolve: resolved %s to %s", path, buffer);

    return true;
}
