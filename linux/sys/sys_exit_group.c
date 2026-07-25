#include "sys/sys.h"

uintptr_t
sys_exit_group(struct LFILinuxThread *t, int code)
{
    // TODO: consider only allowing the main thread to perform exit_group.
    thread_exit(t, LFI_EXIT_PROCESS, code);
}
