#ifndef DEBUG_H_
#define DEBUG_H_
#include <stdarg.h>

#if defined(NDEBUG)
    #if defined(WIN32)
        #define pmesg(format, ...) ((void)0)
    #else
        #define pmesg(format, args...) ((void)0)
    #endif
#else
    void pmesg(const char *format, ...);
#endif

#endif
