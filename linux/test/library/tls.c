#include <stdio.h>

_Thread_local int i;

int
tls(void)
{
    printf("i: %d\n", i);
    return i++;
}
