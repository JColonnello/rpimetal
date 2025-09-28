#pragma once
#include <fs/ff.h>

struct dirent
{
	char d_name[255 + 1]; /* zero-terminated file name */
};