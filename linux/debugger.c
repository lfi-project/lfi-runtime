#include "debugger.h"
#include "linux.h"
#include "lock.h"
#include "elfsym.h"

#include <stdlib.h>

#ifdef HAVE_R_DEBUG
struct link_map {
    uintptr_t l_addr;
    const char *l_name;
    uintptr_t l_ld;
    struct link_map *l_next;
    struct link_map *l_prev;
};

struct r_debug {
    int32_t r_version;
    struct link_map *r_map;
    uintptr_t r_brk;
    enum {
        RT_CONSISTENT,
        RT_ADD,
        RT_DELETE
    } r_state;
    uintptr_t r_ldbase;
};

extern struct r_debug _r_debug;

static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

static void
rtld_db_dlactivity()
{
    ((void (*)()) _r_debug.r_brk)();
}

static void
insert_link_map_into_debug_map(struct link_map *map)
{
    if (!_r_debug.r_map) {
        _r_debug.r_map = map;
        return;
    }
    struct link_map *r_map = _r_debug.r_map;
    while (r_map->l_next) {
        r_map = r_map->l_next;
    }
    r_map->l_next = map;
    map->l_prev = r_map;
}

static void
notify_db_of_load(struct link_map *map)
{
    LOCK_WITH_DEFER(&db_lock, lk_db);

    _r_debug.r_state = RT_ADD;
    rtld_db_dlactivity();

    insert_link_map_into_debug_map(map);

    _r_debug.r_state = RT_CONSISTENT;
    rtld_db_dlactivity();
}
#endif

void
db_register_load(struct LFILinuxProc *proc, const char *filename, uint8_t *prog_data, size_t prog_size, uintptr_t load_addr)
{
#ifdef HAVE_R_DEBUG
    uintptr_t dynsym;
    if (!elf_dynamic(prog_data, prog_size, &dynsym))
        return;
    struct link_map *map = malloc(sizeof(struct link_map));
    if (map) {
        *map = (struct link_map) {
            .l_addr = load_addr,
            .l_name = filename,
            .l_ld = load_addr + dynsym,
        };
        notify_db_of_load(map);
        LOG(proc->engine, "db_register: %s", map->l_name ? map->l_name : "(embedded ELF)");
    }
#else
    LOG(proc->engine, "db_register: gdb/lldb support unavailable because _r_debug was not found");
#endif
}
