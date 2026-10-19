// Program used to test the exit syscalls: exits through exit_group or exit
// with the code given on the command line. Compiled with the LFI toolchain
// as a sandboxed program, and natively (with HOST_TEST defined) into the test
// driver, which runs it in a forked child.

#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

int
exit_prog_main(int argc, char **argv)
{
    if (argc < 3)
        return 98;
    long code = atol(argv[1]);
    if (argv[2][0] == 'g')
        syscall(SYS_exit_group, code);
    else
        syscall(SYS_exit, code);
    // Not reached.
    return 99;
}

#ifndef HOST_TEST
int
main(int argc, char **argv)
{
    return exit_prog_main(argc, argv);
}
#endif
