/*
 * Copyright (C) 2019 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "sd.h"
#include <stdio.h>

// get the end of bss segment from linker
static unsigned char buffer[512];

// do not use the first sector (lba 0), could render your card unbootable
// choose a sector which is unused by your partitions
#define COUNTER_SECTOR 1

int main()
{
	// use the last 4 bytes on the second sector as a boot counter
	unsigned int *counter = (unsigned int *)(buffer + 508);
	// set up serial console

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

	return 1;
}
