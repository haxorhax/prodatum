#ifndef DEBUG_H_
#define DEBUG_H_
#include <stdarg.h>

#if defined(NDEBUG)
#define pmesg(level, format, args...) ((void)0)

#else
void pmesg(int level, const char *format, ...);

#endif

#endif
