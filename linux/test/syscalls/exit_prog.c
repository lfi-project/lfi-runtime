// Program used to test the exit syscalls: exits through exit_group or exit
// with the code given on the command line, or (mode 't') has a second thread
// exit the process with exit_group while the main thread keeps making
// syscalls, or (mode 's') exits the process with exit_group while a second
// thread is still reading the main thread's stack. Compiled with the LFI
// toolchain as a sandboxed program, and natively (with HOST_TEST defined) into
// the test driver, which runs it in a child that re-executes the driver.

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

static void *
exit_group_thread(void *arg)
{
    syscall(SYS_exit_group, (long) arg);
    return NULL;
}

static atomic_int stack_reader_started;

static void *
stack_reader_thread(void *arg)
{
    volatile long *data = arg;
    long sum = 0;
    atomic_store(&stack_reader_started, 1);
    // Mostly reads the main thread's stack, with an occasional syscall so the
    // thread can leave once the process exits.
    for (;;) {
        for (int n = 0; n < 256; n++)
            for (int i = 0; i < 4096; i++)
                sum += data[i];
        syscall(SYS_gettid);
    }
    return (void *) sum;
}

int
exit_prog_main(int argc, char **argv)
{
    if (argc < 3)
        return 98;
    long code = atol(argv[1]);
    if (argv[2][0] == 't') {
        pthread_t th;
        if (pthread_create(&th, NULL, exit_group_thread, (void *) code) != 0)
            return 96;
        // Not left until the other thread's exit_group takes this one down.
        for (;;)
            syscall(SYS_gettid);
    }
    if (argv[2][0] == 's') {
        long data[4096] = { 1 };
        pthread_t th;
        if (pthread_create(&th, NULL, stack_reader_thread, data) != 0)
            return 96;
        while (!atomic_load(&stack_reader_started))
            ;
        syscall(SYS_exit_group, code);
    }
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
