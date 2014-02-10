#include "debug.H"
#include <stdio.h>
#include <string>

extern int msglevel;

#if defined(NDEBUG)
#else
void pmesg(int level, const char* format, ...)
{
    va_list args;

    if (level>msglevel)
        return;

    va_start(args, format);
    vfprintf(stderr, format, args);
#ifdef WINDOWS
    fflush(stderr);
#endif
    va_end(args);
}
#endif
