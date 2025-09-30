#include <stddef.h>

void __real__free_r(void *aptr);
__attribute__((weak)) void __wrap__free_r(void *aptr)
{
	__real__free_r(aptr);
}

void *__real__malloc_r(size_t nbytes);
__attribute__((weak)) void *__wrap__malloc_r(size_t nbytes)
{
	return __real__malloc_r(nbytes);
}
