#include <stdio.h>

#ifndef thread_local
#define thread_local _Thread_local
#endif

thread_local int i;

int
tls(void)
{
    return i++;
}
