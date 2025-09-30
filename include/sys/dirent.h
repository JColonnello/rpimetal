#pragma once

typedef struct DIR DIR;
struct dirent
{
	char d_name[255 + 1]; /* zero-terminated file name */
};