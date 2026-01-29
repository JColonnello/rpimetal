#pragma once
#include <stddef.h>

struct payload_entry
{
	const char *path;
	size_t size;
	const void *ptr;
};
