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

#ifdef __ANDROID__

// This is a weak definition of __android_log_print in case it doesn't exist
// that just prints to stderr. This is used when building for Android but when
// liblog is not linked.
__attribute__((weak)) int
__android_log_print(int prio, const char *tag,  const char *fmt, ...)
{
    (void) prio;

    va_list args;
    va_start(args, fmt);

    fputs(stderr, tag);
    int result = vfprintf(stderr, fmt, args);
    fputs(stderr, "\n");

    va_end(args);
    return result;
}

#endif
