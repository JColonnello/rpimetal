////////////////////////////////////////////////////////////////////////////////
//  change these a little bit for different behavior
//
////////////////////////////////////////////////////////////////////////////////
#include "stdlib.h"
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/sd2.h>
#include <stdio.h>
#include <sys/reent.h>

destructor static void print_exit()
{
	printf("Exitting kernel\n");
}

void _init(struct boot_info *info, union boot_userdata userdata)
{
	stdlib_set_mem_limits(info->memory_start, info->memory_end);
}

#define COUNTER_SECTOR 1

int kernel_start(struct boot_info *info, union boot_userdata userdata)
{
	static unsigned char buffer[512];
	unsigned int *counter = (unsigned int *)(buffer + 508);
	// initialize EMMC and detect SD card type
	if (sd_init() == SD_OK)
	{
		// read the second sector after our bss segment
		if (sd_readblock(COUNTER_SECTOR, buffer, 1))
		{
			// increase boot counter
			(*counter)++;
			// save the sector
			if (sd_writeblock(buffer, COUNTER_SECTOR, 1))
			{
				printf("Boot counter %08X written to SD card.\n", *counter);
			}
		}
	}

	return 0;
}