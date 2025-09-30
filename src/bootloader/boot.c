#include "boot.h"
#include "loader.h"
#include <arm/irq.h>
#include <attrib.h>
#include <boot/custom.h>
#include <dirent.h>
#include <drivers/irq.h>
#include <drivers/timer.h>
#include <drivers/uart.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>

FILE_FROM_SYMBOL_FUNC_DECL(testing_test);
FILE_FROM_SYMBOL_FUNC_DECL(kernel);

void fini()
{
}

static struct boot_customdata boot_data;
static struct boot_info boot_info;
static union boot_userdata boot_userdata;
static start_function_type kernel_start;
static struct tls_data *tcb;

void noreturn kernel_jump()
{
	loader_switch_tcb(tcb);
	//Inline asm equivalent to kernel_start(&boot_info, boot_userdata) without link
	asm("mov x0, %0\n"
		"ldr x1, %1\n"
		"ldr x2, %2\n"
		// Point LR to inside the start function to avoid GDB crash
		"add x30, x2, #4\n"
		"br x2\n"
		:
		: "r"(&boot_info), "m"(boot_userdata), "m"(kernel_start)
		: "x0", "x1", "x2", "x30");
	__builtin_unreachable();
}

extern noreturn void proc_hang();
static void mount_fs();

int main(void)
{
	symbol_data symbols[] = {
		{.name = "__stack", .address = (void *)0x80000},
		{.name = "__bss_start__", .address = NULL},
		{.name = "__bss_end__", .address = NULL},
		{.name = "_fini", .address = fini},
	};

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

	void *mem_end = sbrk(0);
	loader_init();
	loader_add_starting_symbols(sizeof(symbols) / sizeof(*symbols), symbols);
	// loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(libc_libgcc), "build/modules/libc/libgcc.ko");
	// loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(libc_libc), "build/modules/libc/libc.ko");
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(kernel), "build/modules/kernel.ko");
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(testing_test), "build/modules/testing/test.ko");
	loader_print_tls_layout(tls_schema);
	tcb = loader_create_tcb();
	// once everything is patched, we should be able to run test_function
	// which should call our callback!

	boot_info = (struct boot_info){
		.boot_memory_end = mem_end,
		.memory_start = sbrk(0),
		.memory_end = (void *)0x3E000000,
	};
	boot_data = (struct boot_customdata){
		.test_function = loader_search_symbol("test_function_02"),
		.module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol("module_data")),
	};
	boot_userdata.custom = &boot_data;
	kernel_start = loader_search_symbol("_start");

	fputs("Jumping to kernel...\n", stdout);

	return 0;
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