#include <stdio.h>
#include <threads.h>

thread_local int i;

int
tls(void)
{
    return i++;
}
