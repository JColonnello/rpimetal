////////////////////////////////////////////////////////////////////////////////
//  change these a little bit for different behavior
//
////////////////////////////////////////////////////////////////////////////////
#include "stdlib.h"
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/display.h>
#include <drivers/timer.h>
#include <drivers/uart.h>
#include <sys/unistd.h>
#include <utils.h>
// #include <resources/zorzal.h>
#include <drivers/irq.h>
#include <drivers/sd.h>
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

void DisplayDirectory(const char *dirName)
{
	HANDLE fh;
	FIND_DATA find;
	char *month[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	fh = sdFindFirstFile(dirName, &find); // Find first file
	if (fh == 0)
	{
		printf("No files found\n");
		return;
	}
	do
	{
		if (find.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)
			printf("%s <DIR>\n", find.cFileName);
		else
			printf(
				"%c%c%c%c%c%c%c%c.%c%c%c Size: %9lu bytes, %2d/%s/%4d, LFN: %s\n",
				find.cAlternateFileName[0],
				find.cAlternateFileName[1],
				find.cAlternateFileName[2],
				find.cAlternateFileName[3],
				find.cAlternateFileName[4],
				find.cAlternateFileName[5],
				find.cAlternateFileName[6],
				find.cAlternateFileName[7],
				find.cAlternateFileName[8],
				find.cAlternateFileName[9],
				find.cAlternateFileName[10],
				(unsigned long)find.nFileSizeLow,
				find.CreateDT.tm_mday,
				month[find.CreateDT.tm_mon],
				find.CreateDT.tm_year + 1900,
				find.cFileName
			); // Display each entry
	} while (sdFindNextFile(fh, &find) != 0); // Loop finding next file
	sdFindClose(fh); // Close the serach handle
}

int kernel_start(struct boot_info *info, union boot_userdata userdata)
{
	SDRESULT sd = sdInitCard(printf, printf, true);
	if (sd != SD_OK)
	{
		printf("Failed to initialize SD card\n");
		return -1;
	}

	/* Display root directory */
	printf("root directory: \n");
	DisplayDirectory("\\*.*");

	/* Display bitmaps directory */
	printf("Bitmap directory: \n");
	DisplayDirectory("\\bitmaps\\*.*");

	return 0;
}