#pragma once
#include <sys/types.h>

typedef struct DIR DIR;
struct dirent
{
	char d_name[255 + 1]; /* zero-terminated file name */
};