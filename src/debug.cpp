#include "debug.h"
#include <stdio.h>
#include <string>

#if defined(NDEBUG)
#else
void pmesg(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
#ifdef WIN32
	fflush(stderr);
#endif
	va_end(args);
}
#endif
