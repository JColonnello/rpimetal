#include <stdint.h>
#include <stdio.h>
#include "boot.h"
#include "irq.h"
#include <drivers/uart.h>
#include "entry.h"
#include "utils.h"
#include <drivers/irq.h>
#include <drivers/timer.h>
#include "loader.h"
#include <boot/custom.h>
#include <stdlib.h>
#include <sys/unistd.h>

FILE_FROM_SYMBOL_FUNC_DECL(testing_test);
FILE_FROM_SYMBOL_FUNC_DECL(kernel);

void init() {}
void fini() {}

struct boot_customdata boot_data;
struct boot_info boot_info;
union boot_userdata boot_userdata;

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
		.memory_start = sbrk(0),
		.memory_end = NULL - 1,
	};
	boot_data = (struct boot_customdata)
	{
		.test_function = loader_search_symbol("test_function_02"),
		.module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol("module_data"))
	};
	boot_userdata.custom = &boot_data;
	start_function_type kernel_start = loader_search_symbol("_start");
	
    loader_switch_tcb(tcb);
	kernel_start(&boot_info, boot_userdata);
	
	return 0;
}
