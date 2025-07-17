#define _GNU_SOURCE
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

int
main()
{
    struct utsname buf;

    // Use the syscall directly instead of uname() wrapper
    long result = syscall(SYS_uname, &buf);

    if (result == -1) {
        perror("syscall SYS_uname failed");
        return 1;
    }

    printf("System name:    %s\n", buf.sysname);
    printf("Node name:      %s\n", buf.nodename);
    printf("Release:        %s\n", buf.release);
    printf("Version:        %s\n", buf.version);

    return 0;
}
