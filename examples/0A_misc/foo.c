#include "stdlib.h"
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/timer.h>
#include <stdio.h>
#include <sys/unistd.h>

destructor static void print_exit()
{
	printf("Exitting kernel\n");
}

void _init(struct boot_info *info, union boot_userdata userdata)
{
	stdlib_set_mem_limits(info->memory_start, info->memory_end);
}

int kernel_start(struct boot_info *info, union boot_userdata userdata)
{
	for (;;)
	{
		static char s[256];
		size_t n = read(0, s, sizeof(s));
		if (n)
		{
			unsigned long time = timer_monotonic();
			printf("Current time: %lu us\n", time);
			printf("Received %lu bytes: %.*s\n", n, (int)n, s);
		}
		asm("wfi");
	}

	return 0;
}