#include <stdio.h>

_Thread_local int i;

int
tls(void)
{
    return i++;
}
