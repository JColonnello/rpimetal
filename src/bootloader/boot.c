#include <stdint.h>
#include <stdio.h>
#include "boot.h"
#include <arm/irq.h>
#include <drivers/uart.h>
#include "entry.h"
#include "utils.h"
#include <drivers/irq.h>
#include <drivers/timer.h>
#include "loader.h"
#include <boot/custom.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <attrib.h>

FILE_FROM_SYMBOL_FUNC_DECL(testing_test);
FILE_FROM_SYMBOL_FUNC_DECL(kernel);

void init() {}
void fini() {}

static struct boot_customdata boot_data;
static struct boot_info boot_info;
static union boot_userdata boot_userdata;
static start_function_type kernel_start;

static void noreturn kernel_jump()
{
	//Inline asm equivalent to kernel_start(&boot_info, boot_userdata) without link
	asm (
		"mov x0, %0\n"
		"mov x1, %1\n"
		"mov x2, %2\n"
		"br x2\n"
		:
		: "r"(&boot_info), "r"(boot_userdata), "r"(kernel_start)
	);
	__builtin_unreachable();
}

int main(void)
{
	symbol_data symbols[] = 
	{
		{ .name = "__stack", .address = (void*)0x80000 },
		{ .name = "__bss_start__", .address = NULL },
		{ .name = "__bss_end__", .address = NULL },
		{ .name = "_init", .address = init },
		{ .name = "_fini", .address = fini },
	};

	void *mem_end = sbrk(0);
	loader_init();
	loader_add_starting_symbols(sizeof(symbols)/sizeof(*symbols), symbols);
	// loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(libc_libgcc), "build/modules/libc/libgcc.ko");
	// loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(libc_libc), "build/modules/libc/libc.ko");
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(kernel), "build/modules/kernel.ko");
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(testing_test), "build/modules/testing/test.ko");
	loader_print_tls_layout(tls_schema);
	struct tls_data *tcb = loader_create_tcb();
	// once everything is patched, we should be able to run test_function
	// which should call our callback!

	boot_info = (struct boot_info)
	{
		.boot_memory_end = mem_end,
		.memory_start = sbrk(0),
		.memory_end = (void*)0x3E000000,
	};
	boot_data = (struct boot_customdata)
	{
		.test_function = loader_search_symbol("test_function_02"),
		.module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol("module_data"))
	};
	boot_userdata.custom = &boot_data;
	kernel_start = loader_search_symbol("_start");
	
	loader_switch_tcb(tcb);
	kernel_jump();
	return 0;
}
