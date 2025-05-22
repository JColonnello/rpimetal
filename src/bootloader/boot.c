#include <stdint.h>
#include <stdio.h>
#include "irq.h"
#include <drivers/uart.h>
#include "entry.h"
#include "utils.h"
#include <drivers/irq.h>
#include <drivers/timer.h>
#include "loader.h"

FILE_FROM_SYMBOL_FUNC_DECL(libc_libc);
FILE_FROM_SYMBOL_FUNC_DECL(testing_test);
FILE_FROM_SYMBOL_FUNC_DECL(kernel);

void init() {}
void fini() {}

int main(void)
{
	irq_vector_init();
	// timer_init();
	enable_interrupt_controller();
	enable_irq();
	uart_init();

	// Exception test
    // unsigned int r=*((volatile unsigned int*)0xFFFFFFFFFF000000);
	// r++;

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
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(libc_libc), "build/modules/libc/libc.ko");
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(kernel), "build/modules/kernel.ko");
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(testing_test), "build/modules/testing/test.ko");
	loader_print_tls_layout(tls_schema);
    struct tls_data *tcb = loader_create_tcb();
	// once everything is patched, we should be able to run test_function
	// which should call our callback!
	void (*test_function)(int, int*) = loader_search_symbol("test_function_02");
	void (*kernel_function)(void (*)(int, int*), int *) = loader_search_symbol("_start");
	int *module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol("module_data"));
	
    loader_switch_tcb(tcb);
	kernel_function(test_function, module_data);
	
	return 0;
}
