#include <boot.h>
#include <dirent.h>
#include <drivers/mbox.h>
#include <stdio.h>
#include <stdlib.h>

static void mount_fs();

extern void stdlib_set_mem_limits(void *start, void *end);
void _init(struct boot_info *info, union boot_userdata userdata)
{
	stdlib_set_mem_limits(info->memory_start, info->memory_end);
}

int kernel_start(void)
{
	mount_fs();

	DIR *dir;
	struct dirent *entry;

	dir = opendir("sd:");
	if (!dir)
	{
		fprintf(stderr, "Error opening directory\n");
		return -1;
	}

	while ((entry = readdir(dir)) != NULL)
	{
		printf("%s\n", entry->d_name);
	}

	closedir(dir);

	FILE *file;
	file = fopen("sd:counter", "r+");
	if (!file)
	{
		fprintf(stderr, "Error opening counter file\n");
		return -1;
	}
	// Read counter (2 bytes) in file, print, increment, and write back
	uint16_t counter = 0;
	int br, bw;
	br = fread(&counter, sizeof(counter), 1, file);
	printf("Counter: %u\n", counter);
	counter++;
	rewind(file);
	bw = fwrite(&counter, sizeof(counter), 1, file);
	fclose(file);
	if (br != 1 || bw != 1)
	{
		printf("Error reading/writing counter\n");
	}

	mbox_power_off();
}

#include <fs/ff.h>
static void mount_fs()
{
	static FATFS FatFs; /* FatFs work area needed for each volume */
	FRESULT fr;
	if ((fr = f_mount(&FatFs, "", 1)) != FR_OK) /* Give a work area to the default drive */
	{
		fputs("Error mounting filesystem\n", stderr);
		exit(1);
	}
}