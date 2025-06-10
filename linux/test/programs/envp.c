#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main(void)
{
    char *var;
    var = getenv("LFI");
    assert(var && strcmp(var, "1") == 0);
    printf("LFI=%s\n", var);
    var = getenv("NOT_EXIST");
    assert(!var);
    var = getenv("USER");
    assert(var && strcmp(var, "sandbox") == 0);
    printf("USER=%s\n", var);
    return 0;
}
