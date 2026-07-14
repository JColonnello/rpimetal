#include <attrib.h>
#include <stddef.h>

// Adhoc definition of memcpy
weak void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
	char *restrict d = dest;
	const char *restrict s = src;
	while (n-- > 0)
		*d++ = *s++;

	return dest;
}