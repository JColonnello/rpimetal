#include "boot.h"
#include "complex-loader.h"
#include <arm/irq.h>
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/irq.h>
#include <drivers/timer.h>
#include <drivers/uart.h>
#include <payload.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>

/* The payload generator creates a numbered table `payloads[]` in
	build/payload.c which provides {path, size, ptr} entries for each
	embedded file. We iterate it below and use `fmemopen` to obtain
	a FILE* for the existing loader APIs. */

extern const struct payload_entry kernel_payload[];
extern const size_t kernel_payload_count;
extern const struct payload_entry extra_payload[];
extern const size_t extra_payload_count;

void fini()
{
}

static struct boot_customdata boot_data;
static struct boot_info boot_info;
static union boot_userdata boot_userdata;
static start_function_type kernel_start;
static struct tls_data *tcb;
struct linkset *linkset;

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

int main(void)
{
	struct start_symbol symbols[] = {
		{.name = "__stack", .address = (void *)0x80000},
		{.name = "__bss_start__", .address = NULL},
		{.name = "__bss_end__", .address = NULL},
		{.name = "_fini", .address = fini},
	};

	linkset = loader_create_linkset();
	void *mem_end = sbrk(0);
	loader_add_starting_symbols(linkset, sizeof(symbols) / sizeof(*symbols), symbols);

	for (size_t i = 0; i < kernel_payload_count; ++i)
	{
		const struct payload_entry *e = &kernel_payload[i];
		FILE *f = fmemopen((void *)e->ptr, e->size, "rb");
		if (!f)
		{
			fprintf(stdout, "Failed to open embedded payload %s\n", e->path);
			continue;
		}
		loader_read_file(linkset, f, e->path);
	}
	if (loader_finish_link(linkset) != LOADER_ERROR_NONE)
	{
		fputs("Linking failed!\n", stdout);
		return -1;
	}

	for (size_t i = 0; i < extra_payload_count; ++i)
	{
		const struct payload_entry *e = &extra_payload[i];
		FILE *f = fmemopen((void *)e->ptr, e->size, "rb");
		if (!f)
		{
			fprintf(stdout, "Failed to open embedded payload %s\n", e->path);
			continue;
		}
		loader_read_file(linkset, f, e->path);
		if (loader_finish_link(linkset) != LOADER_ERROR_NONE)
		{
			fputs("Linking failed!\n", stdout);
			return -1;
		}
	}

	loader_print_tls_layout(linkset);
	tcb = loader_create_tcb(linkset);
	// once everything is patched, we should be able to run test_function
	// which should call our callback!

	boot_info = (struct boot_info){
		.boot_memory_end = mem_end,
		.memory_start = sbrk(0),
		.memory_end = (void *)0x3E000000,
	};
	boot_data = (struct boot_customdata){
		.test_function = loader_search_symbol(linkset, "test_function_02"),
		.module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol(linkset, "module_data")),
	};
	boot_userdata.custom = &boot_data;
	kernel_start = loader_search_symbol(linkset, "_start");

	fputs("Jumping to kernel...\n", stdout);

	return 0;
}
