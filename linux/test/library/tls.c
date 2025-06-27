#include <stdio.h>

_Thread_local int i;

void
tls(char *msg)
{
    printf("%d: %s\n", i, msg);
    i++;
}
