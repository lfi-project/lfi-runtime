#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static int
xvasprintf(char **strp, const char *fmt, va_list ap)
{
    va_list cp;

    va_copy(cp, ap);
    int len = vsnprintf(NULL, 0, fmt, cp);
    va_end(cp);

    if (len < 0)
        return -1;

    *strp = malloc(len + 1);
    if (*strp == NULL)
        return -1;

    int res = vsnprintf(*strp, len + 1, fmt, ap);
    if (res < 0) {
        free(*strp);
        return -1;
    }
    return res;
}

char *
xasprintf(const char *fmt, ...)
{
    va_list ap;
    char *strp = NULL;

    va_start(ap, fmt);
    int error = xvasprintf(&strp, fmt, ap);
    va_end(ap);

    if (error == -1)
        return NULL;

    return strp;
}
