#include <fs/ff.h>
#include <time.h>

#if FF_USE_LFN == 3 /* Use dynamic memory allocation */

/*------------------------------------------------------------------------*/
/* Allocate/Free a Memory Block                                           */
/*------------------------------------------------------------------------*/

#include <stdlib.h> /* with POSIX API */

void *ff_memalloc(           /* Returns pointer to the allocated memory block (null if not enough core) */
				  UINT msize /* Number of bytes to allocate */
)
{
	return malloc((size_t)msize); /* Allocate a new memory block */
}

void ff_memfree(void *mblock /* Pointer to the memory block to free (no effect if null) */
)
{
	free(mblock); /* Free the memory block */
}

#endif

#if FF_FS_READONLY == 0

DWORD get_fattime(void)
{
	time_t t;
	struct tm *stm;

	t = time(0);
	stm = localtime(&t);

	return (DWORD)(stm->tm_year - 80) << 25 | (DWORD)(stm->tm_mon + 1) << 21 | (DWORD)stm->tm_mday << 16 |
		   (DWORD)stm->tm_hour << 11 | (DWORD)stm->tm_min << 5 | (DWORD)stm->tm_sec >> 1;
}

#endif