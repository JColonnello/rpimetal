#include "stdlib.h"
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/timer.h>
#include <drivers/uspi.h>
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

static void MouseStatusHandler(unsigned nButtons, int nDisplacementX, int nDisplacementY)
{
	printf(
		"Buttons %c%c%c, X %d, Y %d\n",
		nButtons & MOUSE_BUTTON1 ? 'L' : '-',
		nButtons & MOUSE_BUTTON3 ? 'M' : '-',
		nButtons & MOUSE_BUTTON2 ? 'R' : '-',
		nDisplacementX,
		nDisplacementY
	);
}
extern 
int GetMACAddress(unsigned char Buffer[6]); // "get board MAC address"

int kernel_start(struct boot_info *info, union boot_userdata userdata)
{
	printf("Kernel started\n");

	if (!USPiInitialize())
	{
		fputs("Cannot initialize USPi", stderr);
		return 1;
	}

	if (!USPiMouseAvailable())
	{
		fputs("Mouse not found", stderr);
		return 1;
	}

	USPiMouseRegisterStatusHandler(MouseStatusHandler);

	fputs("Move your mouse!", stderr);

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